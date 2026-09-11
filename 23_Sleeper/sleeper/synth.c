/* SLEEPER -- the synth. Phosphor.
 *
 * The integer tracker engine from PERSISTENCE, COLOSSUS, PELAGIC and HELION
 * with a palette for a night train: a drum and bass break, the rail-joint
 * clack driven by the timetable's distance rather than the note grid, a
 * Reese bass, a two-operator electric piano, a detuned pad and an "oo"
 * choir through a chorus, a soft reed lead, and the railway's own sounds --
 * the horn with its Doppler, two bells, the door chime, the brake, the
 * points and the split-flap board. See sleeper.h for the public contract
 * and synth.h for what the tools need.
 *
 * ------------------------------------------------------------------ numerics --
 *
 * Signals are int32 with +-32768 as nominal full scale. Gains and envelopes
 * are Q16 (65536 = 1.0); products go through int64 before the shift, which on
 * Cortex-M33 is a single smull and a shift pair. Oscillators are 32-bit phase
 * accumulators (2^32 = one cycle) so a saw is one subtraction and a pulse is
 * one compare. No floating point anywhere, including synth_init().
 *
 * CONTROL RATE. Every 50 samples (480 Hz): the sequencer, the timetable,
 * glides, filter coefficients and level smoothing. 2,250 samples per 16th is
 * exactly 45 ticks, and a second is exactly 480, so every note and the hash
 * latch land on a tick boundary. Envelopes run per sample.
 *
 * PULL MODEL, IN BLOCKS. The engine only ever renders whole blocks aligned
 * to the absolute sample counter into a small ring; synth_render() hands
 * frames out of that ring, so the output is a function of the sample index
 * alone. Two rules keep the bytes identical at every block size: a voice is
 * skipped only on a flag decided at the control tick, and every voice that
 * uses noise owns its generator.
 *
 * ------------------------------------------------------------------- voicing --
 *
 *   drums    kick: a sine falling 150 to 48 Hz with a click; snare: band-
 *            passed noise at 1.6 kHz and a 190 Hz triangle body, softer and
 *            shorter for the ghosts and for the roll; hats: high-passed
 *            noise, closed and open; ride: the same with a 5.2 kHz ring;
 *            crash: a long high-passed wash
 *   clack    the rail joint: two clicks, "da-dum", a lowpassed noise snap
 *            and a 95 Hz thump each, the second duller; the pair's spacing
 *            widens as the train slows; fired by song_distance() crossing a
 *            joint, so it accelerates with the departure and is silent in
 *            the station. The points fire a decelerating burst of them.
 *   bass     the Reese: two saws 13 cents apart and a sine at the root into
 *            a resonant lowpass that bites on every note and opens with the
 *            score's energy; a cubic saturation
 *   piano    the electric piano, four voices: a two-operator FM body (1:1,
 *            the index falling over 100 ms, the bark of a tine) and a seventh
 *            partial that dies in 30 ms; a stereo tremolo at 4.8 Hz
 *   pad      four notes, two saws each 3.4 cents apart, one bank a side,
 *            into a lowpass a side that opens with the score; 400 ms in
 *   lead     the reed's cousin: a pulse breathing around 45%, a quarter of a
 *            saw, a sine at the root, breath on the attack, into a resonant
 *            lowpass run at 2x, a light saturation and a 1,150 Hz formant;
 *            legato inside a phrase, a slower slide where the score says so
 *   choir    "oo": two saws a side 7 cents apart on one note through three
 *            formant band-passes at 320, 800 and 2,500 Hz; 250 ms in
 *   horn     three saws (a minor-seventh chord) through a 900 Hz lowpass,
 *            swelling to the middle of the pass, pitch 1.03 to 0.97 and pan
 *            right to left as the other train goes by
 *   bells    the platform bell: two partials at 2.4 and 3.9 kHz struck
 *            every 125 ms for two seconds; the crossing bell: 1.1 and
 *            1.6 kHz every 190 ms for four beats, passing right to left
 *   chime    the doors: G5 then E5, sine with a third harmonic
 *   brake    a sine with its octave sliding 3 kHz to 800 Hz over two bars,
 *            band-passed noise riding it, a shudder at 7 Hz, a rumble below
 *   riser    band-passed noise sweeping 300 Hz to 3 kHz over two bars
 *   flaps    the board: a 3 ms noise snap through a 2.8 kHz band-pass and
 *            a 900 Hz knock, one per column that changes, on the board's
 *            own stagger, from a pool of four
 *   chorus   the pad and the choir through two modulated taps
 *   delay    a 3/16 line with a 1/8 tap: echoes fall left, then right
 *   plate    three damped combs a side and an allpass a side; its feedback
 *            follows the score's space, short in the tunnel
 *   master   a lowpass a side that the tunnel closes; a DC blocker; a knee
 *
 * ---------------------------------------------------------------------- RAM --
 *
 * The delay line is 6,750 int16 (13.5 KB); the plate's 6 combs + 2
 * allpasses 8,398 int16 (16.8 KB); the chorus 1,024 int16 (2 KB); the sine
 * table 2 KB; the block ring 100 B; state about 2 KB. About 37 KB, inside
 * the 48 KiB reserved in the ledger.
 */

#include "synth.h"
#include "song.h"

#include <string.h>

#define CTL_DIV        50                       /* samples per control tick  */
#define BLOCK          25                       /* frames rendered at a time */
#define TOTAL          DURATION_SAMPLES         /* 4,608,000 */

_Static_assert(TOTAL % CTL_DIV == 0, "the endpoint must sit on a control tick");
_Static_assert(STEP_SAMPLES % CTL_DIV == 0, "a 16th must be whole control ticks");
_Static_assert(SAMPLE_RATE % CTL_DIV == 0, "the hash latch must sit on a control tick");
_Static_assert(CTL_DIV % BLOCK == 0, "blocks must tile the control tick");
_Static_assert(BEAT_SAMPLES % CTL_DIV == 0, "a beat must be whole control ticks");

/* --------------------------------------------------------------- tables ---- */

static int16_t  g_sin[1024];                   /* Q15, one full cycle         */
static uint32_t g_oct8[12];                    /* phase inc for MIDI 108..119 */

/* 440 * 2^((n-69)/12) * 2^32 / 24000 for n = 108..119, rounded. */
static const uint32_t k_oct8[12] = {
     749115498u,  793660223u,  840853716u,  890853480u,
     943826385u,  999949222u, 1059409297u, 1122405052u,
    1189146729u, 1259857073u, 1334772074u, 1414141751u,
};

static void build_tables(void)
{
    for (int x = 0; x < 512; x++) {
        const int64_t u = (int64_t)x * (512 - x);
        const int32_t v = (int32_t)((4 * u * 32767) / (327680 - u));
        g_sin[x] = (int16_t)v;
        g_sin[x + 512] = (int16_t)-v;
    }
    memcpy(g_oct8, k_oct8, sizeof g_oct8);
}

static inline uint32_t note_inc(int n)
{
    if (n < 24) n = 24;
    if (n > 119) n = 119;
    return g_oct8[n % 12] >> (9 - n / 12);
}

/* Phase increment for a frequency in Hz: hz * 2^32 / 24000. */
static inline uint32_t hz_inc(uint32_t hz) { return (uint32_t)(((uint64_t)hz << 32) / SAMPLE_RATE); }

static inline int32_t clamp32(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static inline int32_t qmul(int32_t a, int32_t b)          /* Q16 */
{
    return (int32_t)(((int64_t)a * b) >> 16);
}

/* 0 at or before a, 65536 at or after b. */
static int32_t ramp(uint32_t t, uint32_t a, uint32_t b)
{
    if (t <= a) return 0;
    if (t >= b) return 65536;
    return (int32_t)(((uint64_t)(t - a) << 16) / (b - a));
}

/* Cutoff coefficient for the state-variable filter: 2*sin(pi*fc/fs) with the
 * small-angle approximation, Q16. 411775 is 2*pi in Q16. */
static inline int32_t hz_f(uint32_t hz)
{
    if (hz > 3800) hz = 3800;
    return (int32_t)((uint64_t)hz * 411775u / SAMPLE_RATE);
}

/* High word of a 32x32 product: one smull on the M33. With `w` a gain
 * pre-shifted to Q32 (GW below) this is a Q16 multiply in one instruction.
 * Gains that go through it must be at most 32767. */
static inline int32_t mulhi(int32_t a, int32_t w)
{
    return (int32_t)(((int64_t)a * w) >> 32);
}
#define GW(g) ((int32_t)((uint32_t)(g) << 16))

/* The cubic soft saturation x - x^3/3 on +-32767, clamped first so that it
 * cannot fold back. */
static inline int32_t sat(int32_t d)
{
    d = clamp32(d, -32767, 32767);
    const int32_t d2 = (d * d) >> 15;
    return d - ((d2 * d) >> 15) / 3;
}

static inline uint32_t xs(uint32_t *r) { uint32_t x = *r; x ^= x << 13; x ^= x >> 17; x ^= x << 5; *r = x; return x; }
#define NOISE(r) ((int32_t)(xs(&(r)) >> 16) - 32768)

/* ---------------------------------------------------------------- filter ---- */
typedef struct { int32_t lo, band; } svf_t;

static inline void svf(svf_t *s, int32_t in, int32_t f, int32_t q)
{
    s->lo += qmul(f, s->band);
    const int32_t hi = in - s->lo - qmul(q, s->band);
    s->band += qmul(f, hi);
}

/* -------------------------------------------------------------- envelope ---- */
typedef struct { int32_t v, sus, rate, stage; } env_t;
typedef struct { int32_t atk, dec, sus, rel; } envp_t;

static inline int32_t env_run(env_t *e, const envp_t *p)
{
    if (e->stage == 1) {
        e->v += p->atk;
        if (e->v >= 65535) { e->v = 65535; e->stage = 2; e->sus = p->sus; e->rate = p->dec; }
    } else {
        e->v = e->sus + qmul(e->v - e->sus, e->rate);
    }
    return e->v;
}

static inline void env_on(env_t *e)  { e->stage = 1; }
static inline void env_off(env_t *e, const envp_t *p)
{
    if (e->stage) { e->stage = 3; e->sus = 0; e->rate = p->rel; }
}

/* Q16 per-sample multipliers: 65536 * (1 - 1/tau_samples). */
static const envp_t ep_bass  = { 65536 / 50,   65530, 52000, 65470 };   /* 2 ms in, 15 ms out    */
static const envp_t ep_piano = { 65536 / 50,   65520, 18000, 65500 };   /* 2 ms in, 55 ms out    */
static const envp_t ep_pad   = { 65536 / 9600, 65535, 65535, 65530 };   /* 400 ms in, 450 ms out */
static const envp_t ep_lead  = { 65536 / 1500, 65534, 54000, 65510 };   /* 62 ms in, 105 ms out  */
static const envp_t ep_choir = { 65536 / 6000, 65535, 65535, 65531 };   /* 250 ms in, 550 ms out */

/* ------------------------------------------------------------------ hash ---- */
static uint32_t          g_hash = 2166136261u;
static volatile uint32_t g_mark_seq, g_mark_pos, g_mark_hash;
static uint32_t          g_read_seq;

static void hash_publish(uint32_t pos)
{
    __atomic_store_n(&g_mark_seq, g_mark_seq + 1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_RELEASE);
    g_mark_pos  = pos;
    g_mark_hash = g_hash;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&g_mark_seq, g_mark_seq + 1, __ATOMIC_RELAXED);
}

int synth_hash_latch(uint32_t *pos, uint32_t *hash)
{
    for (;;) {
        const uint32_t s = __atomic_load_n(&g_mark_seq, __ATOMIC_RELAXED);
        if (s & 1u) continue;
        if (s == g_read_seq) return 0;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        const uint32_t p = g_mark_pos, h = g_mark_hash;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_mark_seq, __ATOMIC_RELAXED) != s) continue;
        g_read_seq = s;
        if (!p) return 0;
        *pos = p; *hash = h;
        return 1;
    }
}

/* ----------------------------------------------------------------- state ---- */

#define DLY_LEN   6750                 /* a dotted eighth at 160 BPM: the left echo */
#define DLY_TAP   4500                 /* an eighth: the right echo                 */
#define DLY_FB    23000                /* 0.35 feedback                             */

static int16_t g_dly[DLY_LEN];

/* The plate: Freeverb's comb lengths scaled to 24 kHz and then by 1.5, the
 * right side 37 samples longer; allpasses 449 and 341. */
#define RV_C0 907
#define RV_C1 1039
#define RV_C2 1213
#define RV_SPREAD 37
#define RV_A0 449
#define RV_A1 341
#define RV_DAMP 30000                  /* 0.46 */

static int16_t g_rv_c[6][RV_C2 + RV_SPREAD];
static int16_t g_rv_a[2][RV_A0];
static const int g_rv_len[6] = { RV_C0, RV_C1, RV_C2, RV_C0 + RV_SPREAD, RV_C1 + RV_SPREAD, RV_C2 + RV_SPREAD };
static const int g_rv_alen[2] = { RV_A0, RV_A1 };

#define CHORUS_LEN 512
static int16_t g_chorus[2][CHORUS_LEN];

/* The rendered block, handed out by synth_render(). */
static int16_t g_ring[2 * BLOCK];

typedef struct { uint32_t rng, ph; int32_t snap, thump, lp; svf_t f; int32_t pan; } click_t;
typedef struct { uint32_t cph, mph, tph, inc; int32_t idx, tenv; env_t env; } piano_t;
typedef struct { uint32_t rng; int32_t snap, knock; svf_t f; uint32_t kph; int32_t lvl; } flap_t;

static struct {
    uint32_t pos;                  /* rendered, in whole blocks               */
    uint32_t out;                  /* handed out                              */
    int      ring_rd, ring_n, ctl_left;
    int      mark_left;
    int32_t  peak;
    unsigned solo;

    /* drums: one noise stream */
    uint32_t rng_drum;
    uint32_t k_ph, k_inc;  int32_t k_env, k_click;
    uint32_t s_ph;  int32_t s_envn, s_envt, s_ndec;  svf_t s_f;
    int32_t  h_env, h_dec, h_lp, c_env;
    uint32_t r_ph;  int32_t r_env, r_ring;
    /* the clack: two clicks, and the points burst */
    click_t  ck[2];  int ck_next;  uint32_t ck_last_joint;  int32_t ck_second, ck_lvl, ck_dull;
    int32_t  pt_age;  int pt_on;
    /* bass */
    uint32_t b_ph[2], b_sph, b_inc;  env_t b_env;  int32_t b_fenv, b_fc, b_open;  svf_t b_f;
    /* piano */
    piano_t  p[4];  uint32_t p_lfo;  int32_t p_gl, p_gr;  int p_on[4];
    /* pad */
    uint32_t d_ph[8], d_inc[8];  env_t d_env;  svf_t d_fl, d_fr;  int32_t d_fc, d_lvl;  int8_t d_chord[4];  int d_on;
    /* lead */
    uint32_t rng_lead, l_ph, l_inc, l_target, l_eff, l_duty, l_lfo, l_dlfo;
    env_t    l_env;  svf_t l_f, l_n;  int32_t l_breath, l_fenv, l_fc, l_age, l_drive, l_glide;  int l_on;
    /* choir */
    uint32_t v_ph[4], v_inc[4], v_target, v_lfo;  env_t v_env;  svf_t v_f[2][3];  int32_t v_lvl, v_age;  int v_on;
    /* horn */
    uint32_t hn_ph[3], hn_inc[3];  int32_t hn_age, hn_amp, hn_pan, hn_mul;  svf_t hn_f;  int hn_on;
    /* bells */
    uint32_t rng_bell, bl_ph1, bl_ph2, bl_inc1, bl_inc2;  int32_t bl_amp, bl_dec, bl_left, bl_space, bl_count, bl_pan, bl_click;  int bl_on;
    /* chime */
    uint32_t ch_ph, ch_inc;  int32_t ch_amp, ch_age;  int ch_on;
    /* brake */
    uint32_t rng_brake, br_ph, br_inc;  int32_t br_age, br_amp, br_fc;  svf_t br_f;  int32_t br_lp;  int br_on;
    /* riser */
    uint32_t rng_riser;  svf_t ri_f;  int32_t ri_fc, ri_amp;  int ri_on;
    /* flaps */
    flap_t   fl[4];  int fl_next;  int fl_on[4];
    /* chorus */
    int      w_w;  uint32_t w_lfo;
    /* delay */
    int      dl_w;  int32_t dl_lp;
    /* plate */
    int      rv_w[6], rv_aw[2];  int32_t rv_lp[6], rv_fb;
    /* master */
    int32_t  master, mw, space, energy, filt, m_fc, dc_l, dc_r;  svf_t m_fl, m_fr;  int m_on, m_prime;

    /* per-tick mix gains, with solo and song levels folded in. gw_* are
     * Q32 gain words for mulhi(); g_* are Q16 <= 32767 because they are
     * multiplied by an envelope first. */
    int32_t  gw_kick, gw_snare, gw_hat, gw_ride, gw_crash, gw_clack, gw_horn, gw_bell, gw_chime, gw_brake, gw_riser, gw_flap;
    int32_t  g_bass, g_piano, g_pad, g_lead, g_choir;
    int32_t  gw_dly, gw_rv;
} S;

void synth_solo(unsigned mask) { S.solo = mask; }

/* Levels, all <= 32767 so they fit a gain word. */
#define G_KICK    30000
#define G_SNARE   19000
#define G_HAT     3600
#define G_RIDE    4500
#define G_CRASH   5500
#define G_CLACK   15000
#define G_BASS    17000
#define G_PIANO   12000
#define G_PAD     15000
#define G_LEAD    17000
#define G_CHOIR   8000
#define G_HORN    12000
#define G_BELL    6000
#define G_CHIME   11000
#define G_BRAKE   6000
#define G_RISER   7000
#define G_FLAP    11000
#define G_DELAY   17000
#define G_REVERB  15000
#define G_MASTER  40000

/* Soft knee above 20000; the output never exceeds 26000 (-2.0 dBFS). */
static inline int32_t soft_clip(int32_t v)
{
    const int32_t a = v < 0 ? -v : v;
    if (a <= 20000) return v;
    const int32_t e = a - 20000;
    const int32_t o = 20000 + (6000 * e) / (e + 6000);
    return v < 0 ? -o : o;
}

static inline uint32_t seed_at(uint32_t pos, uint32_t salt)
{
    return (pos * 2654435761u) ^ salt ^ 1u;
}

/* --------------------------------------------------------------- triggers --- */

static void trig_kick(void)
{
    S.k_ph = 0; S.k_inc = hz_inc(150);
    S.k_env = 65535; S.k_click = 65535;
}

static void trig_snare(int32_t vel, int ghost)
{
    S.s_ph = 0; S.s_envn = vel; S.s_envt = ghost ? vel >> 1 : vel;
    S.s_ndec = ghost ? 65380 : 65480;
}

static void trig_hat(int open)
{
    S.h_env = 65535; S.h_dec = open ? 65509 : 65349;
}

static void trig_ride(void)  { S.r_env = 65535; S.r_ring = 65535; }
static void trig_crash(void) { S.c_env = 65535; }

/* A rail-joint click; `dull` for the second of the pair and for the points. */
static void trig_click(int dull, int32_t pan)
{
    click_t *c = &S.ck[S.ck_next];
    S.ck_next ^= 1;
    c->rng = seed_at(S.pos, 0xC1AC0000u + (uint32_t)S.ck_next);
    c->ph = 0;
    c->snap = dull ? 42000 : 65535;
    c->thump = dull ? 30000 : 50000;
    c->pan = pan;
    c->f.lo = c->f.band = 0;
}

static void bass_event(int e)
{
    if (e == 0) return;
    if (e == N_OFF) { env_off(&S.b_env, &ep_bass); return; }
    S.b_inc = note_inc(e);
    S.b_fenv = 65535;
    env_on(&S.b_env);
}

static void piano_event(int v, int e)
{
    piano_t *p = &S.p[v];
    if (e == 0) return;
    if (e == N_OFF) { env_off(&p->env, &ep_piano); return; }
    p->inc = note_inc(e);
    if (!p->env.stage) { p->cph = p->mph = p->tph = 0; p->env.v = 0; }
    p->idx = 190; p->tenv = 65535;
    env_on(&p->env);
}

static void pad_bar(uint32_t bar)
{
    int8_t c[4];
    for (int i = 0; i < 4; i++) c[i] = (int8_t)song_pad(bar, i);
    if (!c[0]) { env_off(&S.d_env, &ep_pad); memset(S.d_chord, 0, 4); return; }
    if (memcmp(c, S.d_chord, 4) == 0) { if (!S.d_env.stage) env_on(&S.d_env); return; }
    memcpy(S.d_chord, c, 4);
    for (int i = 0; i < 4; i++) {
        const uint32_t inc = note_inc(c[i]);
        S.d_inc[i]     = inc - (inc >> 9);           /* left bank, -3.4 cents  */
        S.d_inc[4 + i] = inc + (inc >> 9);           /* right bank, +3.4 cents */
    }
    if (S.d_env.stage == 2) S.d_env.v -= S.d_env.v >> 3;   /* a dip under the change */
    env_on(&S.d_env);
}

/* Inside a phrase the lead glides to the next note and takes a smaller
 * breath; after a release it starts again with the full attack. */
static void lead_event(int e, int slide)
{
    if (e == 0) return;
    if (e == N_OFF) { env_off(&S.l_env, &ep_lead); return; }
    const uint32_t inc = note_inc(e);
    S.l_target = inc;
    S.l_glide = slide ? 5 : 2;
    if (S.l_env.stage == 1 || S.l_env.stage == 2) {
        S.l_breath = 20000;
    } else {
        S.l_inc = inc; S.l_breath = 65535; S.l_age = 0; S.l_fenv = 65535;
        env_on(&S.l_env);
    }
}

static void choir_event(int e)
{
    if (e == 0) return;
    if (e == N_OFF) { env_off(&S.v_env, &ep_choir); return; }
    const uint32_t inc = note_inc(e);
    S.v_target = inc;
    if (S.v_env.stage != 1 && S.v_env.stage != 2) {
        for (int i = 0; i < 4; i++) S.v_inc[i] = inc;
        S.v_age = 0;
        env_on(&S.v_env);
    }
}

static void trig_horn(void)
{
    static const uint8_t n[3] = { 62, 65, 69 };                /* D4 F4 A4 */
    for (int i = 0; i < 3; i++) { S.hn_ph[i] = 0; S.hn_inc[i] = note_inc(n[i]); }
    S.hn_age = 0; S.hn_on = 1; S.hn_f.lo = S.hn_f.band = 0;
}

static void trig_bell(int crossing)
{
    S.bl_inc1 = hz_inc(crossing ? 1100 : 2400);
    S.bl_inc2 = hz_inc(crossing ? 1600 : 3900);
    S.bl_dec = crossing ? 65510 : 65460;
    S.bl_space = crossing ? 4500 : 3000;
    S.bl_count = crossing ? 8 : 16;
    S.bl_left = 0; S.bl_amp = 0; S.bl_click = 0;
    S.bl_pan = crossing ? 20000 : -6000;                        /* right, or a little left */
    S.rng_bell = seed_at(S.pos, 0xBE11BE11u);
    S.bl_on = 1;
}

static void trig_chime(void)
{
    S.ch_inc = note_inc(79); S.ch_ph = 0; S.ch_amp = 65535; S.ch_age = 0; S.ch_on = 1;
}

static void trig_brake(void)
{
    S.br_age = 0; S.br_inc = hz_inc(3000); S.br_amp = 0; S.br_on = 1;
    S.br_f.lo = S.br_f.band = 0; S.br_lp = 0;
    S.rng_brake = seed_at(S.pos, 0xB2AEB2AEu);
}

static void trig_flap(int rows)
{
    flap_t *f = &S.fl[S.fl_next];
    S.fl_next = (S.fl_next + 1) & 3;
    f->rng = seed_at(S.pos, 0xF1A90000u + (uint32_t)S.fl_next);
    f->snap = 65535; f->knock = 60000; f->kph = 0;
    f->f.lo = f->f.band = 0;
    f->lvl = rows >= 3 ? 65535 : rows == 2 ? 52000 : 40000;
}

/* The points: a decelerating burst of clicks over one beat, tick offsets. */
static const uint8_t k_points[] = { 0, 2, 3, 5, 8, 10, 11, 13, 16, 20, 21, 25, 30, 31, 36, 42, 50, 60, 72, 90, 110, 135, 165 };

/* ------------------------------------------------------------- control ----- */

static void control_tick(void)
{
    const uint32_t pos = S.pos;
    const uint32_t bar = pos / BAR_SAMPLES;
    const bar_t *B = song_bar(bar < SONG_BARS ? bar : SONG_BARS - 1);
    const int past = pos >= TOTAL;

    /* --- the sequencer: one row per 16th --- */
    if (pos % STEP_SAMPLES == 0 && !past) {
        const unsigned step = (pos / STEP_SAMPLES) & 15;
        if (step == 0) pad_bar(bar);

        const unsigned e = song_events(bar, step);
        if (e & EV_KICK)   trig_kick();
        if (e & EV_ROLL) {
            const int32_t k = (int32_t)((bar - 78) * 16 + step);          /* 0..31 */
            trig_snare(22000 + k * 1350, 1);
        } else if (e & EV_SNARE) trig_snare(65535, 0);
        else if (e & EV_GHOST)   trig_snare(24000, 1);
        if (e & EV_OPEN)   trig_hat(1);
        else if (e & EV_HAT) trig_hat(0);
        if (e & EV_RIDE)   trig_ride();
        if (e & EV_CRASH)  trig_crash();
        if (e & EV_BELL)   trig_bell(0);
        if ((e & EV_XING) && !S.bl_on) trig_bell(1);
        if (e & EV_CHIME)  trig_chime();
        if (e & EV_HORN)   trig_horn();
        if (e & EV_BRAKE)  trig_brake();
        if (e & EV_POINTS) { S.pt_on = 1; S.pt_age = 0; }

        bass_event(song_bass(bar, step));
        for (int v = 0; v < 4; v++) piano_event(v, song_piano(bar, step, v));
        lead_event(song_lead(bar, step), song_lead_slide(bar, step));
        choir_event(song_choir(bar, step));
    }

    /* --- the timetable: a joint crossed fires the clack pair; the points
     * fire their burst --- */
    if (!past) {
        const uint32_t joint = song_distance(pos) / JOINT_UNITS;
        const int32_t speed = song_speed(pos);
        if (joint != S.ck_last_joint) {
            S.ck_last_joint = joint;
            if (S.ck_lvl > 1500) {
                trig_click(0, -8000);
                /* the second axle: 50 ms at cruise, wider as the train slows */
                int32_t gap = speed > 4096 ? (int32_t)((1200ll << 16) / speed) : 19200;
                if (gap > 6000) gap = 6000;
                S.ck_second = gap;
            }
        }
        if (S.ck_second > 0) {
            S.ck_second -= CTL_DIV;
            if (S.ck_second <= 0) { trig_click(1, 8000); S.ck_second = 0; }
        }
        if (S.pt_on) {
            const int t = S.pt_age / CTL_DIV;
            for (unsigned i = 0; i < sizeof k_points; i++)
                if (k_points[i] == t) trig_click(i & 1, (i & 1) ? 12000 : -12000);
            S.pt_age += CTL_DIV;
            if (S.pt_age > 9000) S.pt_on = 0;
        }
        /* the clack's level: the score's, scaled by speed up to half cruise */
        {
            int32_t sp = speed > 32768 ? 65536 : speed * 2;
            S.ck_lvl += (qmul(B->clack * 257, sp) - S.ck_lvl) >> 3;
        }
    }

    /* --- levels: one-pole per tick --- */
    S.d_lvl  += (B->pad   * 257 - S.d_lvl)  >> 5;
    S.v_lvl  += (B->choir * 257 - S.v_lvl)  >> 6;
    S.space  += (B->space * 257 - S.space)  >> 6;
    S.energy += (B->energy * 257 - S.energy) >> 5;
    S.filt   += (B->filter * 257 - S.filt)  >> 5;

    /* --- the bass: the bite, the opening --- */
    S.b_fenv = qmul(S.b_fenv, 63898);                             /* 83 ms */
    {
        const uint32_t open = 90u + (uint32_t)((S.energy * 5) >> 12);           /* +0 .. +80 Hz */
        const uint32_t bite = (uint32_t)(((int64_t)S.b_fenv * (500 + ((S.energy * 3) >> 8))) >> 16);
        S.b_fc = hz_f(open + bite);
    }

    /* --- the piano: the tremolo, the FM index and the tine settling --- */
    S.p_lfo += 655;                                               /* 4.8 Hz */
    {
        const int32_t t = g_sin[(S.p_lfo >> 6) & 1023] >> 2;          /* +-8192 */
        S.p_gl = 32768 - t; S.p_gr = 32768 + t;                        /* Q15 */
        for (int v = 0; v < 4; v++) {
            piano_t *p = &S.p[v];
            if (p->idx > 28) p->idx -= (p->idx - 28) >> 3;             /* ~100 ms to the body */
            p->tenv = qmul(p->tenv, 61000);                             /* 30 ms */
            S.p_on[v] = p->env.stage != 0;
        }
    }

    /* --- the pad: the filter opens with the score --- */
    S.d_fc = hz_f(200u + (uint32_t)((S.d_lvl * 3) >> 8) + (uint32_t)((S.energy * 3) >> 9));

    /* --- the lead: glide, vibrato arriving over 200 ms, the breathing
     * pulse width, the cutoff that follows the energy and the swell --- */
    S.l_lfo += 568;                                               /* 5.2 Hz */
    S.l_dlfo += 30;                                               /* 0.27 Hz */
    S.l_age += 1;
    S.l_inc += (uint32_t)(((int64_t)S.l_target - (int64_t)S.l_inc) >> S.l_glide);
    {
        const int32_t depth = S.l_age >= 96 ? 65536 : S.l_age * 682;
        const int32_t vib = qmul(g_sin[(S.l_lfo >> 6) & 1023], depth);        /* Q15 */
        S.l_eff = S.l_inc + (uint32_t)(((int64_t)S.l_inc * vib) >> 24);
        S.l_duty = (uint32_t)(0x73333333ll + (((int64_t)g_sin[(S.l_dlfo >> 6) & 1023] * 0x08000000) >> 15));
        S.l_fenv = qmul(S.l_fenv, 65300);                         /* 210 ms */
        const uint32_t env_hz = (uint32_t)(((int64_t)S.l_env.v * (500 + ((S.energy * 3) >> 8))) >> 16);
        const uint32_t bloom  = (uint32_t)((S.l_fenv * 700) >> 16);
        uint32_t fc = 350u + (uint32_t)((S.energy * 3) >> 8) + env_hz + bloom;
        if (fc > 3200) fc = 3200;
        S.l_fc = hz_f(fc) >> 1;                                   /* run at 2x */
        S.l_drive = 6000 + ((S.energy * 40) >> 8);
    }

    /* --- the choir: a slide between long notes, vibrato after 300 ms --- */
    S.v_lfo += 546;                                               /* 5 Hz */
    S.v_age += 1;
    {
        const uint32_t base = S.v_inc[0] + (uint32_t)(((int64_t)S.v_target - (int64_t)S.v_inc[0]) >> 4);
        const int32_t depth = S.v_age >= 144 ? 65536 : S.v_age * 455;
        const int32_t vib = qmul(g_sin[(S.v_lfo >> 6) & 1023], depth) >> 1;
        const uint32_t eff = base + (uint32_t)(((int64_t)base * vib) >> 24);
        S.v_inc[0] = base;
        S.v_inc[1] = eff - (eff >> 8);                            /* -7 cents, left  */
        S.v_inc[2] = eff + (eff >> 8);                            /* +7 cents, right */
        S.v_inc[3] = eff + (eff >> 7);                            /* +14, right */
    }

    /* --- the horn: the pass takes four beats; pitch, pan and swell from the
     * age --- */
    if (S.hn_on) {
        const int32_t a = ramp((uint32_t)S.hn_age, 0, 4 * BEAT_SAMPLES);      /* 0..65536 */
        const int32_t s = qmul(a, a) * 3 - 2 * qmul(qmul(a, a), a);           /* smoothstep */
        S.hn_mul = 65536 + 1966 - ((s * 3932) >> 16);                         /* 1.03 -> 0.97 */
        S.hn_pan = 24000 - (int32_t)(((int64_t)s * 48000) >> 16);            /* right -> left */
        const int32_t swell = a < 32768 ? a * 2 : (65536 - a) * 2;
        S.hn_amp = qmul(ramp((uint32_t)S.hn_age, 0, 1500), 16000 + ((swell * 3) >> 2));
        S.hn_age += CTL_DIV;
        if (S.hn_age >= (int32_t)(4 * BEAT_SAMPLES)) S.hn_on = 0;
    }

    /* --- the bells: struck every bl_space samples, bl_count times --- */
    if (S.bl_on) {
        if (S.bl_left <= 0) {
            if (S.bl_count > 0) { S.bl_count--; S.bl_amp = 65535; S.bl_click = 65535; S.bl_left = S.bl_space; S.bl_ph1 = S.bl_ph2 = 0; }
            else if (S.bl_amp < 200) S.bl_on = 0;
        }
        S.bl_left -= CTL_DIV;
        if (S.bl_space == 4500 && S.bl_pan > -20000) S.bl_pan -= 56;   /* the crossing passes, right to left */
    }

    /* --- the chime: the second note 300 ms in --- */
    if (S.ch_on) {
        S.ch_age += CTL_DIV;
        if (S.ch_age == 7200) { S.ch_inc = note_inc(76); S.ch_amp = 65535; }
        if (S.ch_age > 40000 && S.ch_amp < 100) S.ch_on = 0;
    }

    /* --- the brake: the slide, the swell and the fade over two bars --- */
    if (S.br_on) {
        S.br_inc = (uint32_t)(((uint64_t)S.br_inc * 65476u) >> 16);            /* 3 kHz -> 800 Hz */
        const uint32_t hz = (uint32_t)(((uint64_t)S.br_inc * SAMPLE_RATE) >> 32);
        S.br_fc = hz_f(hz);
        const int32_t in = ramp((uint32_t)S.br_age, 0, 9000);
        const int32_t out = 65536 - ramp((uint32_t)S.br_age, 54000, 72000);
        const int32_t shudder = 52000 + ((g_sin[((uint32_t)S.br_age * 3u / 10u) & 1023] * 13000) >> 15);   /* 7 Hz */
        S.br_amp = qmul(qmul(in, out), shudder);
        S.br_age += CTL_DIV;
        if (S.br_age >= 72000) S.br_on = 0;
    }

    /* --- the riser: bars 30-31 and 78-79 --- */
    S.ri_on = !past && ((bar >= 30 && bar < 32) || (bar >= 78 && bar < 80));
    if (S.ri_on) {
        const uint32_t from = bar >= 78 ? 78u * BAR_SAMPLES : 30u * BAR_SAMPLES;
        const int32_t a = ramp(pos, from, from + 2 * BAR_SAMPLES);
        S.ri_fc = hz_f(300u + (uint32_t)((a * 2700) >> 16));
        S.ri_amp = 8000 + (int32_t)(((int64_t)a * 57000) >> 16);            /* a*57000 passes 2^31: int64 */
    }

    /* --- the board: a flap per column that changes, on the board's stagger --- */
    if (!past) {
        const board_t *prev = NULL; uint32_t since = 0;
        const board_t *cur = song_board(pos, &prev, &since);
        if (cur && since < BOARD_COLS * BOARD_STAGGER_SAMPLES + CTL_DIV) {
            for (int col = 0; col < BOARD_COLS; col++) {
                const uint32_t start = (uint32_t)col * BOARD_STAGGER_SAMPLES;
                if (since > start || start >= since + CTL_DIV) continue;
                int rows = 0;
                for (int r = 0; r < BOARD_ROWS; r++) {
                    const char *a = cur->row[r], *b = prev ? prev->row[r] : NULL;
                    const char ca = a && (int)strlen(a) > col ? a[col] : ' ';
                    const char cb = b && (int)strlen(b) > col ? b[col] : ' ';
                    if (ca != cb) rows++;
                }
                if (rows) trig_flap(rows);
            }
        }
        for (int i = 0; i < 4; i++) S.fl_on[i] = S.fl[i].snap > 30 || S.fl[i].knock > 30;
    }

    /* --- the plate's feedback follows the space; the master lowpass the
     * filter --- */
    S.rv_fb = 44000 + ((S.space * 17000) >> 16);                /* Q16: 0.67 .. 0.93 */
    {
        const int on = S.filt < 64000;
        if (on && !S.m_on) S.m_prime = 1;
        S.m_on = on;
    }
    S.m_fc = hz_f(250u + (uint32_t)((S.filt * 14) >> 8));      /* 250 .. 3800 Hz */

    /* master: the trim, and a fade after the last flap so the plate's tail
     * is inside the file; the last 1,000 samples are true silence */
    int32_t master = G_MASTER;
    const uint32_t fade_from = 127u * BAR_SAMPLES + 2u * BEAT_SAMPLES + BOARD_COLS * BOARD_STAGGER_SAMPLES + BOARD_FLIP_SAMPLES;
    if (pos >= fade_from) {
        const int32_t k = 65536 - ramp(pos, fade_from, TOTAL - 1000);
        master = qmul(master, qmul(k, k));
    }
    S.master = master;
    S.mw = (int32_t)((uint32_t)master << 15);

    /* mix gains with solo folded in */
    const unsigned so = S.solo;
    S.gw_kick  = (so & SOLO_DRUMS) ? GW(G_KICK)  : 0;
    S.gw_snare = (so & SOLO_DRUMS) ? GW(G_SNARE) : 0;
    S.gw_hat   = (so & SOLO_DRUMS) ? GW(G_HAT)   : 0;
    S.gw_ride  = (so & SOLO_DRUMS) ? GW(G_RIDE)  : 0;
    S.gw_crash = (so & SOLO_DRUMS) ? GW(G_CRASH) : 0;
    S.gw_clack = (so & SOLO_CLACK) ? GW(qmul(G_CLACK, S.ck_lvl > 65535 ? 65535 : S.ck_lvl)) : 0;
    S.g_bass   = (so & SOLO_BASS)  ? G_BASS : 0;
    S.g_piano  = (so & SOLO_PIANO) ? G_PIANO : 0;
    S.g_pad    = (so & SOLO_PAD)   ? qmul(G_PAD, S.d_lvl) : 0;
    S.g_lead   = (so & SOLO_LEAD)  ? G_LEAD : 0;
    S.g_choir  = (so & SOLO_CHOIR) ? qmul(G_CHOIR, S.v_lvl) : 0;
    S.gw_horn  = (so & SOLO_HORN)  ? GW(qmul(G_HORN, S.hn_amp)) : 0;
    S.gw_bell  = (so & SOLO_BELLS) ? GW(G_BELL)  : 0;
    S.gw_chime = (so & SOLO_BELLS) ? GW(G_CHIME) : 0;
    S.gw_flap  = (so & SOLO_BELLS) ? GW(G_FLAP)  : 0;
    S.gw_brake = (so & SOLO_BRAKE) ? GW(qmul(G_BRAKE, S.br_amp)) : 0;
    S.gw_riser = (so & SOLO_BRAKE) ? GW(qmul(G_RISER, S.ri_amp)) : 0;
    S.gw_dly   = (so & SOLO_FX)    ? GW(qmul(G_DELAY, S.space))  : 0;
    S.gw_rv    = (so & SOLO_FX)    ? GW(qmul(G_REVERB, 30000 + (S.space >> 1))) : 0;

    /* which voices render this tick: decided here, never at a block edge */
    S.l_on = S.g_lead  != 0 && S.l_env.stage != 0;
    S.d_on = S.g_pad   != 0 && S.d_env.stage != 0;
    S.v_on = S.g_choir != 0 && S.v_env.stage != 0;
    for (int v = 0; v < 4; v++) S.p_on[v] = S.p_on[v] && S.g_piano != 0;
}

/* -------------------------------------------------------------- render ------ */
/* One pass per voice over a block of at most CTL_DIV frames, the way a
 * tracker mixer does it: one loop with every voice in it spills everything. */

static void HOT(render_block)(int16_t *out, int n)
{
    int32_t acc[2 * CTL_DIV];      /* the stereo bus                          */
    int32_t send[CTL_DIV];         /* what goes to the delay                  */
    int32_t rvin[CTL_DIV];         /* what goes to the plate                  */
    int32_t wide[2 * CTL_DIV];     /* the pad and the choir, before the chorus */

    /* --- drums: writes the bus --- */
    {
        uint32_t rng = S.rng_drum, kph = S.k_ph, kinc = S.k_inc, sph = S.s_ph, rph = S.r_ph;
        int32_t kenv = S.k_env, kclick = S.k_click, senvn = S.s_envn, senvt = S.s_envt;
        svf_t sf = S.s_f;
        int32_t hlp = S.h_lp, henv = S.h_env, cenv = S.c_env, renv = S.r_env, rring = S.r_ring;
        const int32_t hdec = S.h_dec, sndec = S.s_ndec;
        const int32_t gk = S.gw_kick, gs = S.gw_snare, gh = S.gw_hat, gr = S.gw_ride, gc = S.gw_crash;
        const uint32_t kfloor = 8589935u, rinc = 930542913u;             /* 48 Hz, 5.2 kHz */
        for (int i = 0; i < n; i++) {
            const int32_t nz = NOISE(rng);

            if (kinc > kfloor) kinc -= kinc >> 9;
            kph += kinc;
            const int32_t kick = qmul(g_sin[kph >> 22], kenv) + (qmul(nz, kclick) >> 2);
            kenv = qmul(kenv, 65510);
            kclick -= kclick >> 5;

            svf(&sf, nz, 27451, 40000);                          /* 1.6 kHz */
            sph += 34001825u;                                    /* 190 Hz */
            const int32_t st = (int32_t)(sph >> 16) - 32768;
            const int32_t stri = ((st < 0 ? -st : st) << 1) - 32768;
            const int32_t snare = qmul(sf.band, senvn) + (qmul(stri, senvt) >> 1);
            senvn = qmul(senvn, sndec);
            senvt = qmul(senvt, 65400);

            const int32_t hp = nz - hlp;
            hlp += (nz - hlp) >> 1;
            rph += rinc;
            const int32_t ride = qmul(hp, renv) + (qmul(g_sin[rph >> 22], rring) >> 2);
            const int32_t hats = mulhi(qmul(hp, henv), gh) + mulhi(qmul(hp, cenv), gc) + mulhi(ride, gr);
            henv = qmul(henv, hdec);
            cenv = qmul(cenv, 65533);
            renv = qmul(renv, 65500);
            rring = qmul(rring, 65470);

            const int32_t c = mulhi(kick, gk) + mulhi(snare, gs);
            acc[2 * i]     = c + hats - (hats >> 2);
            acc[2 * i + 1] = c + hats;
            rvin[i] = mulhi(snare, gs) >> 2;
            send[i] = 0;
            wide[2 * i] = wide[2 * i + 1] = 0;
        }
        S.rng_drum = rng; S.k_ph = kph; S.k_inc = kinc; S.s_ph = sph; S.r_ph = rph;
        S.k_env = kenv; S.k_click = kclick; S.s_envn = senvn; S.s_envt = senvt; S.s_f = sf;
        S.h_lp = hlp; S.h_env = henv; S.c_env = cenv; S.r_env = renv; S.r_ring = rring;
    }

    /* --- the clack: two click generators, each with its own noise --- */
    for (int k = 0; k < 2; k++) {
        click_t c = S.ck[k];
        if (c.snap < 20 && c.thump < 20) continue;
        const int32_t g = S.gw_clack, thump_inc = 17000454;                 /* 95 Hz */
        const int32_t gl = 32768 - c.pan, grr = 32768 + c.pan;
        for (int i = 0; i < n; i++) {
            const int32_t nz = NOISE(c.rng);
            svf(&c.f, nz, 30000, 30000);                         /* ~1.8 kHz */
            c.lp += (nz - c.lp) >> 2;
            c.ph += (uint32_t)thump_inc;
            int32_t y = qmul(c.f.lo + (c.lp >> 1), c.snap) + (qmul(g_sin[c.ph >> 22], c.thump) >> 1);
            c.snap = qmul(c.snap, 64400);                        /* 2.3 ms */
            c.thump = qmul(c.thump, 65380);                      /* 17 ms */
            const int32_t o = mulhi(y, g);
            acc[2 * i] += (o * gl) >> 15; acc[2 * i + 1] += (o * grr) >> 15;
            rvin[i] += o >> 2;
        }
        S.ck[k] = c;
    }

    /* --- the Reese: two saws and a sine into the biting lowpass --- */
    {
        uint32_t p0 = S.b_ph[0], p1 = S.b_ph[1], sp = S.b_sph;
        const uint32_t inc = S.b_inc, i0 = inc - (inc >> 7), i1 = inc + (inc >> 7);
        env_t benv = S.b_env;
        svf_t f = S.b_f;
        const int32_t fc = S.b_fc, g = S.g_bass;
        for (int i = 0; i < n; i++) {
            const int32_t saw = (((int32_t)(p0 >> 16) - 32768) + ((int32_t)(p1 >> 16) - 32768)) >> 2;
            const int32_t sub = g_sin[sp >> 22] >> 1;
            p0 += i0; p1 += i1; sp += inc;
            svf(&f, saw + sub, fc, 17000);
            int32_t y = clamp32(f.lo + (f.band >> 3), -32767, 32767);
            y = sat(y + (y >> 1));
            y += y >> 1;
            const int32_t b = mulhi(y, GW(qmul(env_run(&benv, &ep_bass), g)));
            acc[2 * i] += b; acc[2 * i + 1] += b;
        }
        S.b_ph[0] = p0; S.b_ph[1] = p1; S.b_sph = sp; S.b_env = benv; S.b_f = f;
    }

    /* --- the electric piano: four FM voices, spread and trembling --- */
    for (int v = 0; v < 4; v++) {
        if (!S.p_on[v]) continue;
        piano_t p = S.p[v];
        const int32_t g = S.g_piano, idx = p.idx, tenv = p.tenv;
        static const int32_t panl[4] = { 28000, 21000, 12000, 6000 };
        const int32_t pl = (panl[v] * S.p_gl) >> 15, pr = ((32768 - panl[v]) * S.p_gr) >> 15;
        const uint32_t tinc = p.inc * 7u;
        for (int i = 0; i < n; i++) {
            p.mph += p.inc; p.cph += p.inc; p.tph += tinc;
            const int32_t m = (g_sin[p.mph >> 22] * idx) >> 15;
            int32_t y = g_sin[((p.cph >> 22) + (uint32_t)m) & 1023];
            y += qmul(g_sin[p.tph >> 22], tenv) >> 3;
            const int32_t o = mulhi(y, GW(qmul(env_run(&p.env, &ep_piano), g)));
            acc[2 * i] += (o * pl) >> 15; acc[2 * i + 1] += (o * pr) >> 15;
            send[i] += o >> 2;
            rvin[i] += o >> 2;
        }
        S.p[v] = p;
    }

    /* --- the pad: four notes, a saw a side each, a filter a side --- */
    if (S.d_on) {
        int32_t gwv[CTL_DIV];
        env_t env = S.d_env;
        const int32_t g = S.g_pad;
        for (int i = 0; i < n; i++) gwv[i] = GW(qmul(env_run(&env, &ep_pad), g));
        S.d_env = env;
        for (int side = 0; side < 2; side++) {
            uint32_t *php = S.d_ph + 4 * side;
            const uint32_t *pinc = S.d_inc + 4 * side;
            uint32_t q0 = php[0], q1 = php[1], q2 = php[2], q3 = php[3];
            const uint32_t j0 = pinc[0], j1 = pinc[1], j2 = pinc[2], j3 = pinc[3];
            svf_t f = side ? S.d_fr : S.d_fl;
            const int32_t fc = S.d_fc;
            for (int i = 0; i < n; i++) {
                const int32_t x = ((int32_t)(q0 >> 16) + (int32_t)(q1 >> 16)
                                 + (int32_t)(q2 >> 16) + (int32_t)(q3 >> 16) - 4 * 32768) >> 2;
                q0 += j0; q1 += j1; q2 += j2; q3 += j3;
                svf(&f, x, fc, 30000);
                wide[2 * i + side] += mulhi(f.lo, gwv[i]);
            }
            php[0] = q0; php[1] = q1; php[2] = q2; php[3] = q3;
            if (side) S.d_fr = f; else S.d_fl = f;
        }
    }

    /* --- the lead --- */
    if (S.l_on) {
        uint32_t rng = S.rng_lead, ph = S.l_ph;
        const uint32_t inc = S.l_eff, duty = S.l_duty;
        env_t env = S.l_env;
        svf_t f = S.l_f, nf = S.l_n;
        int32_t breath = S.l_breath;
        const int32_t fc = S.l_fc, drive = S.l_drive, g = S.g_lead;
        for (int i = 0; i < n; i++) {
            const int32_t nz = NOISE(rng);
            ph += inc;
            const int32_t pulse = ph < duty ? 16000 : -16000;
            const int32_t saw = ((int32_t)(ph >> 16) - 32768) >> 2;
            const int32_t x = pulse + saw + (g_sin[ph >> 22] >> 2) + (qmul(nz, breath) >> 2);
            breath = qmul(breath, 65490);
            svf(&f, x, fc, 15000);                          /* twice: the filter runs at 2x */
            svf(&f, x, fc, 15000);
            int32_t y = clamp32(f.lo, -32767, 32767);
            y = sat(y + qmul(y, drive));
            svf(&nf, y, 19731, 18000);                      /* the 1,150 Hz formant */
            y += nf.band >> 2;
            const int32_t o = mulhi(y, GW(qmul(env_run(&env, &ep_lead), g)));
            acc[2 * i] += o; acc[2 * i + 1] += o - (o >> 3);
            send[i] += o >> 1;
            rvin[i] += o >> 2;
        }
        S.rng_lead = rng; S.l_ph = ph; S.l_env = env; S.l_f = f; S.l_n = nf; S.l_breath = breath;
    }

    /* --- the choir: two saws a side into the "oo" formants --- */
    if (S.v_on) {
        int32_t gwv[CTL_DIV];
        env_t env = S.v_env;
        const int32_t g = S.g_choir;
        for (int i = 0; i < n; i++) gwv[i] = GW(qmul(env_run(&env, &ep_choir), g));
        S.v_env = env;
        for (int side = 0; side < 2; side++) {
            uint32_t q0 = S.v_ph[2 * side], q1 = S.v_ph[2 * side + 1];
            const uint32_t j0 = S.v_inc[side ? 2 : 0], j1 = S.v_inc[side ? 3 : 1];
            svf_t f0 = S.v_f[side][0], f1 = S.v_f[side][1], f2 = S.v_f[side][2];
            for (int i = 0; i < n; i++) {
                const int32_t x = ((int32_t)(q0 >> 16) + (int32_t)(q1 >> 16) - 2 * 32768) >> 1;
                q0 += j0; q1 += j1;
                svf(&f0, x, 5490, 20000);                    /*  320 Hz */
                svf(&f1, x, 13725, 16000);                   /*  800 Hz */
                svf(&f2, x, 42893, 16000);                   /* 2500 Hz */
                const int32_t y = f0.band + (f1.band >> 2) + (f2.band >> 3);
                wide[2 * i + side] += mulhi(y, gwv[i]);
            }
            S.v_ph[2 * side] = q0; S.v_ph[2 * side + 1] = q1;
            S.v_f[side][0] = f0; S.v_f[side][1] = f1; S.v_f[side][2] = f2;
        }
    }

    /* --- the horn --- */
    if (S.hn_on) {
        uint32_t p0 = S.hn_ph[0], p1 = S.hn_ph[1], p2 = S.hn_ph[2];
        const uint32_t j0 = (uint32_t)(((uint64_t)S.hn_inc[0] * (uint32_t)S.hn_mul) >> 16);
        const uint32_t j1 = (uint32_t)(((uint64_t)S.hn_inc[1] * (uint32_t)S.hn_mul) >> 16);
        const uint32_t j2 = (uint32_t)(((uint64_t)S.hn_inc[2] * (uint32_t)S.hn_mul) >> 16);
        svf_t f = S.hn_f;
        const int32_t g = S.gw_horn, gl = 32768 - S.hn_pan, grr = 32768 + S.hn_pan, fc = 15441;   /* 900 Hz */
        for (int i = 0; i < n; i++) {
            const int32_t x = (((int32_t)(p0 >> 16) - 32768) + ((int32_t)(p1 >> 16) - 32768) + ((int32_t)(p2 >> 16) - 32768)) / 3;
            p0 += j0; p1 += j1; p2 += j2;
            svf(&f, x, fc, 20000);
            const int32_t o = mulhi(f.lo, g);
            acc[2 * i] += (o * gl) >> 15; acc[2 * i + 1] += (o * grr) >> 15;
            rvin[i] += o >> 1;
        }
        S.hn_ph[0] = p0; S.hn_ph[1] = p1; S.hn_ph[2] = p2; S.hn_f = f;
    }

    /* --- the bells --- */
    if (S.bl_on) {
        uint32_t rng = S.rng_bell, p1 = S.bl_ph1, p2 = S.bl_ph2;
        int32_t amp = S.bl_amp, click = S.bl_click;
        const int32_t dec = S.bl_dec, g = S.gw_bell, gl = 32768 - S.bl_pan, grr = 32768 + S.bl_pan;
        for (int i = 0; i < n; i++) {
            const int32_t nz = NOISE(rng);
            p1 += S.bl_inc1; p2 += S.bl_inc2;
            int32_t y = qmul(g_sin[p1 >> 22] + (g_sin[p2 >> 22] >> 1), amp) + (qmul(nz, click) >> 3);
            amp = qmul(amp, dec); click = qmul(click, 62000);
            const int32_t o = mulhi(y, g);
            acc[2 * i] += (o * gl) >> 15; acc[2 * i + 1] += (o * grr) >> 15;
            rvin[i] += o >> 1;
        }
        S.rng_bell = rng; S.bl_ph1 = p1; S.bl_ph2 = p2; S.bl_amp = amp; S.bl_click = click;
    }

    /* --- the chime --- */
    if (S.ch_on) {
        uint32_t ph = S.ch_ph;
        int32_t amp = S.ch_amp;
        const uint32_t inc = S.ch_inc;
        const int32_t g = S.gw_chime;
        for (int i = 0; i < n; i++) {
            ph += inc;
            const int32_t y = qmul(g_sin[ph >> 22] + (g_sin[(ph * 3u) >> 22] >> 3), amp);
            amp = qmul(amp, 65528);                              /* 330 ms */
            const int32_t o = mulhi(y, g);
            acc[2 * i] += o; acc[2 * i + 1] += o - (o >> 2);
            send[i] += o >> 2;
            rvin[i] += o >> 1;
        }
        S.ch_ph = ph; S.ch_amp = amp;
    }

    /* --- the brake --- */
    if (S.br_on) {
        uint32_t rng = S.rng_brake, ph = S.br_ph;
        const uint32_t inc = S.br_inc;
        svf_t f = S.br_f;
        int32_t lp = S.br_lp;
        const int32_t fc = S.br_fc, g = S.gw_brake;
        for (int i = 0; i < n; i++) {
            const int32_t nz = NOISE(rng);
            ph += inc;
            svf(&f, nz, fc, 6000);
            lp += (nz - lp) >> 6;
            const int32_t y = (g_sin[ph >> 22] >> 1) + (g_sin[(ph * 2u) >> 22] >> 3) + (f.band >> 1) + lp;
            const int32_t o = mulhi(y, g);
            acc[2 * i] += o; acc[2 * i + 1] += o;
            rvin[i] += o >> 1;
        }
        S.rng_brake = rng; S.br_ph = ph; S.br_f = f; S.br_lp = lp;
    }

    /* --- the riser --- */
    if (S.ri_on) {
        uint32_t rng = S.rng_riser;
        svf_t f = S.ri_f;
        const int32_t fc = S.ri_fc, g = S.gw_riser;
        for (int i = 0; i < n; i++) {
            svf(&f, NOISE(rng), fc, 20000);
            const int32_t o = mulhi(f.band, g);
            acc[2 * i] += o; acc[2 * i + 1] += o;
        }
        S.rng_riser = rng; S.ri_f = f;
    }

    /* --- the flaps: a pool of four --- */
    for (int k = 0; k < 4; k++) {
        if (!S.fl_on[k]) continue;
        flap_t f = S.fl[k];
        const int32_t g = S.gw_flap, kinc = 161061274;                      /* 900 Hz */
        const int32_t gl = 32768 + ((k & 1) ? 6000 : -6000), grr = 65536 - gl;
        for (int i = 0; i < n; i++) {
            const int32_t nz = NOISE(f.rng);
            svf(&f.f, nz, 47000, 30000);                         /* ~2.8 kHz */
            f.kph += (uint32_t)kinc;
            int32_t y = qmul(f.f.band, f.snap) + (qmul(g_sin[f.kph >> 22], f.knock) >> 1);
            f.snap = qmul(f.snap, 64700);                        /* 3 ms */
            f.knock = qmul(f.knock, 65200);                      /* 8 ms */
            const int32_t o = mulhi(qmul(y, f.lvl), g);
            acc[2 * i] += (o * gl) >> 15; acc[2 * i + 1] += (o * grr) >> 15;
            rvin[i] += o >> 3;
        }
        S.fl[k] = f;
    }

    /* --- chorus on the wide bus: two taps modulated in opposite phase,
     * linearly interpolated; dry plus 5/8 wet to the bus, and to the plate --- */
    {
        int w = S.w_w;
        const int32_t lfo = g_sin[(S.w_lfo >> 6) & 1023];
        const int32_t d[2] = { (300 << 8) + ((lfo * 96) >> 7), (300 << 8) - ((lfo * 96) >> 7) };
        for (int i = 0; i < n; i++) {
            for (int side = 0; side < 2; side++) {
                int16_t *line = g_chorus[side];
                const int32_t x = wide[2 * i + side];
                line[w] = (int16_t)clamp32(x, -32768, 32767);
                const int idx = w - (d[side] >> 8);
                const int32_t s0 = line[idx & (CHORUS_LEN - 1)], s1 = line[(idx - 1) & (CHORUS_LEN - 1)];
                const int32_t tap = s0 + (((s1 - s0) * (d[side] & 255)) >> 8);
                const int32_t wet = tap - (tap >> 2) - (tap >> 3);
                acc[2 * i + side] += x + wet;
                rvin[i] += (x + wet) >> 2;
            }
            w = (w + 1) & (CHORUS_LEN - 1);
        }
        S.w_w = w;
    }

    /* --- delay: one 3/16 line; the end goes left, the 1/8 tap right --- */
    {
        int w = S.dl_w;
        int32_t lp = S.dl_lp;
        const int32_t g = S.gw_dly, fb = GW(DLY_FB);
        for (int i = 0; i < n; i++) {
            int r5 = w + (DLY_LEN - DLY_TAP); if (r5 >= DLY_LEN) r5 -= DLY_LEN;
            const int32_t tap_r = g_dly[r5];
            const int32_t tap_l = g_dly[w];
            lp += (tap_l - lp) >> 1;
            g_dly[w] = (int16_t)clamp32(send[i] + mulhi(lp, fb), -32768, 32767);
            if (++w >= DLY_LEN) w = 0;
            acc[2 * i] += mulhi(tap_l, g); acc[2 * i + 1] += mulhi(tap_r, g);
            rvin[i] += (tap_l + tap_r) >> 3;
        }
        S.dl_w = w; S.dl_lp = lp;
    }

    /* --- the plate: three damped combs a side in parallel, one allpass a side --- */
    {
        const int32_t g = S.gw_rv, fb = S.rv_fb;
        for (int side = 0; side < 2; side++) {
            int32_t sum[CTL_DIV];
            for (int i = 0; i < n; i++) sum[i] = 0;
            for (int c = 0; c < 3; c++) {
                const int k = side * 3 + c;
                int16_t *line = g_rv_c[k];
                const int len = g_rv_len[k];
                int w = S.rv_w[k];
                int32_t lp = S.rv_lp[k];
                for (int i = 0; i < n; i++) {
                    const int32_t y = line[w];
                    lp += qmul(y - lp, 65536 - RV_DAMP);
                    line[w] = (int16_t)clamp32((rvin[i] >> 2) + qmul(lp, fb), -32768, 32767);
                    if (++w >= len) w = 0;
                    sum[i] += y;
                }
                S.rv_w[k] = w; S.rv_lp[k] = lp;
            }
            int16_t *ap = g_rv_a[side];
            const int alen = g_rv_alen[side];
            int aw = S.rv_aw[side];
            for (int i = 0; i < n; i++) {
                const int32_t x = sum[i] / 3;
                const int32_t d = ap[aw];
                const int32_t y = d - (x >> 1);
                ap[aw] = (int16_t)clamp32(x + (d >> 1), -32768, 32767);
                if (++aw >= alen) aw = 0;
                acc[2 * i + side] += mulhi(y, g);
            }
            S.rv_aw[side] = aw;
        }
    }

    /* --- the master lowpass: the tunnel --- */
    if (S.m_on) {
        svf_t fl = S.m_fl, fr = S.m_fr;
        const int32_t fc = S.m_fc;
        if (S.m_prime) { S.m_prime = 0; fl.lo = clamp32(acc[0], -65535, 65535); fr.lo = clamp32(acc[1], -65535, 65535); fl.band = fr.band = 0; }
        for (int i = 0; i < n; i++) {
            svf(&fl, clamp32(acc[2 * i], -65535, 65535), fc, 40000);
            svf(&fr, clamp32(acc[2 * i + 1], -65535, 65535), fc, 40000);
            acc[2 * i] = fl.lo; acc[2 * i + 1] = fr.lo;
        }
        S.m_fl = fl; S.m_fr = fr;
    }

    /* --- master: DC blocker (3.7 Hz), trim, knee, peak, clamp, hash --- */
    {
        int32_t peak = S.peak, dcl = S.dc_l, dcr = S.dc_r;
        const int32_t mw = S.mw;
        const int past_end = S.pos >= TOTAL;
        uint32_t h = g_hash;
        int mark = S.mark_left;
        for (int i = 0; i < n; i++) {
            int32_t L = acc[2 * i], R = acc[2 * i + 1];
            dcl += ((L << 8) - dcl) >> 10; L -= dcl >> 8;        /* state in Q8: no deadband */
            dcr += ((R << 8) - dcr) >> 10; R -= dcr >> 8;
            L = soft_clip(mulhi(L * 2, mw));
            R = soft_clip(mulhi(R * 2, mw));
            if (past_end) { L = 0; R = 0; }

            const int32_t al = L < 0 ? -L : L, ar = R < 0 ? -R : R;
            if (al > peak) peak = al;
            if (ar > peak) peak = ar;

            const int16_t sl = (int16_t)clamp32(L, -32768, 32767);
            const int16_t sr = (int16_t)clamp32(R, -32768, 32767);
            out[2 * i] = sl; out[2 * i + 1] = sr;

            h = (h ^ (uint16_t)sl) * 16777619u;
            h = (h ^ (uint16_t)sr) * 16777619u;
            if (--mark <= 0) {
                mark = SAMPLE_RATE;
                g_hash = h;
                hash_publish(S.pos + (uint32_t)i + 1u);
            }
        }
        g_hash = h;
        S.mark_left = mark;
        S.peak = peak; S.dc_l = dcl; S.dc_r = dcr;
    }
    S.pos += (uint32_t)n;
}

void HOT(synth_render)(int16_t *stereo, unsigned frames)
{
    while (frames > 0) {
        if (S.ring_n == 0) {
            if (S.ctl_left == 0) { control_tick(); S.ctl_left = CTL_DIV; }
            const int k = S.ctl_left < BLOCK ? S.ctl_left : BLOCK;
            render_block(g_ring, k);
            S.ctl_left -= k;
            S.ring_rd = 0; S.ring_n = k;
        }
        unsigned k = frames < (unsigned)S.ring_n ? frames : (unsigned)S.ring_n;
        memcpy(stereo, g_ring + 2 * S.ring_rd, k * 4u);
        stereo += 2 * k; frames -= k;
        S.ring_rd += (int)k; S.ring_n -= (int)k; S.out += k;
    }
}

/* --------------------------------------------------------------- lifecycle -- */

static void reset(void)
{
    const unsigned solo = S.solo ? S.solo : SOLO_ALL;
    memset(&S, 0, sizeof S);
    memset(g_dly, 0, sizeof g_dly);
    memset(g_rv_c, 0, sizeof g_rv_c);
    memset(g_rv_a, 0, sizeof g_rv_a);
    memset(g_chorus, 0, sizeof g_chorus);
    memset(g_ring, 0, sizeof g_ring);
    S.solo = solo;
    S.rng_drum  = 0x1BADF00Du;
    S.rng_lead  = 0x5EEDF00Du;
    S.rng_bell  = 0xBE11BE11u;
    S.rng_brake = 0xB2AEB2AEu;
    S.rng_riser = 0x0C0FFEE5u;
    S.ck[0].rng = 0xC1AC0001u; S.ck[1].rng = 0xC1AC0003u;
    for (int i = 0; i < 4; i++) S.fl[i].rng = 0xF1A90001u + 2u * (uint32_t)i;
    S.k_inc = hz_inc(48);
    S.h_dec = 65349; S.s_ndec = 65480;
    S.master = G_MASTER;
    S.mw = (int32_t)((uint32_t)G_MASTER << 15);
    S.mark_left = SAMPLE_RATE;
    S.filt = 65535; S.m_on = 0; S.m_fc = hz_f(3800);
    S.rv_fb = 55000;
    S.b_inc = note_inc(40);
    S.l_inc = S.l_target = S.l_eff = note_inc(76); S.l_glide = 2;
    S.l_duty = 0x73333333u;
    for (int i = 0; i < 4; i++) { S.v_inc[i] = note_inc(76); S.p[i].inc = note_inc(64); S.p[i].idx = 28; }
    S.v_target = note_inc(76);
    for (int i = 0; i < 8; i++) S.d_inc[i] = note_inc(52);
    for (int i = 0; i < 3; i++) S.hn_inc[i] = note_inc(62);
    S.hn_mul = 65536;
    S.br_inc = hz_inc(3000);
    S.bl_inc1 = hz_inc(2400); S.bl_inc2 = hz_inc(3900); S.bl_dec = 65460; S.bl_space = 3000;
    S.ch_inc = note_inc(79);
    S.ri_fc = hz_f(300);
    g_hash = 2166136261u;
    hash_publish(0);
    g_read_seq = g_mark_seq;
}

void synth_init(void)
{
    build_tables();
    song_init();
    S.solo = SOLO_ALL;
    reset();
}

void synth_seek(uint32_t sample)
{
    reset();
    int16_t tmp[2 * 256];
    while (S.out < sample) {
        uint32_t n = sample - S.out; if (n > 256) n = 256;
        synth_render(tmp, n);
    }
    g_read_seq = g_mark_seq;
}

uint32_t synth_position(void) { return S.out; }
int32_t  synth_peak(void)     { return S.peak; }
