/* HELION -- the synth.
 *
 * The integer tracker engine from PERSISTENCE, COLOSSUS and PELAGIC with a
 * palette designed for a star: an electric reed, a bowed metal ensemble, a
 * struck bronze bar, a rubbery pulse bass, a frame drum with a rim knock and
 * brushed metal, a tam-tam, a solo bowed harmonic, and a thread of air. See
 * helion.h for the public contract and synth.h for what the tools need.
 *
 * ------------------------------------------------------------------ numerics --
 *
 * Signals are int32 with +-32768 as nominal full scale. Gains and envelopes
 * are Q16 (65536 = 1.0); products go through int64 before the shift, which on
 * Cortex-M33 is a single smull and a shift pair. Oscillators are 32-bit phase
 * accumulators (2^32 = one cycle) so a saw is one subtraction and a pulse is
 * one compare.
 *
 * No floating point anywhere, including synth_init(): the note table is
 * twelve integer constants for the top octave shifted down, and the sine is
 * Bhaskara's rational approximation evaluated in int64, so the host and the
 * device cannot disagree by an ulp in a table entry.
 *
 * CONTROL RATE. Every 40 samples (600 Hz): the sequencer, glides, filter
 * coefficients and level smoothing. 3,000 samples per 16th is exactly 75
 * ticks, so every note lands on a tick boundary. Envelopes run per sample.
 *
 * PULL MODEL, IN BLOCKS. The engine only ever renders whole blocks aligned
 * to the absolute sample counter, into a small ring; synth_render() hands
 * frames out of that ring. So the output is a function of the sample index
 * alone -- rendering 1, 8 or 512 frames at a time produces the same bytes --
 * and the device, which asks for one or two frames at a time between
 * scanlines, pays the per-voice state load/store once per block rather than
 * once per call. The block is half a control tick, 20 frames. Two rules keep
 * the bytes identical at every block size: a voice is skipped only on a flag
 * decided at the control tick (never on per-sample state read at a block
 * boundary), and every voice that uses noise owns its generator.
 *
 * ------------------------------------------------------------------- voicing --
 *
 *   drum     the low frame drum: a sine falling from 130 to 62 Hz over
 *            30 ms with a 1.5x membrane partial and a lowpassed skin
 *            transient; a second, softer touch for the ghost notes
 *   rim      a dry wooden knock: a 900 Hz damped tone, 12 ms, and a click
 *   scrape   brushed metal: band-passed noise that rises over 20 ms and
 *            falls over 75 or 250 ms, its centre climbing 2.4 to 3.6 kHz
 *   gong     a tam-tam: six inharmonic partials on 65 Hz decaying over
 *            0.75 to 3.5 s, a low thud, and the shimmer -- band-passed noise
 *            that swells in over 200 ms and climbs as it fades, which is
 *            what makes a tam-tam a tam-tam
 *   bass     rubbery: a saw and a 25% pulse into a resonant lowpass whose
 *            cutoff bites down over 60 ms on every note; the band output
 *            adds the grit; a cubic saturation; a glide when the score
 *            says slide
 *   bronze   the struck bar, two voices in turn: a sine with partials at
 *            2.756x and 5.404x (a free bar's modes) decaying three and
 *            eight times faster, and a lowpassed knock; the decay set per
 *            bar from muted to ringing; one voice left, one right
 *   reed     the electric reed: a pulse whose width breathes slowly around
 *            50% (odd harmonics, the clarinet body, with a little even
 *            content wandering in), a quarter of a saw for the nasal edge,
 *            a burst of breath noise on every attack, into a resonant
 *            lowpass run at 2x whose cutoff follows the score's push and
 *            the envelope (the swell brightens), a cubic saturation driven
 *            by the push (the edge when it is pushed), and a fixed 1,150 Hz
 *            formant; notes inside a phrase glide into each other; vibrato
 *            arrives 250 ms into a note
 *   bow      the bowed metal, four voices: a saw a side per voice, detuned
 *            +-3.4 cents, into a lowpass a side that opens with the 270 ms
 *            attack; an inharmonic partial at 2.756x per voice, lit at each
 *            stroke and settling to a quarter; bow noise band-passed at
 *            2.2 kHz that fades over the first 250 ms of a stroke; a chord
 *            change under the bow dips and relights rather than restrikes
 *   harm     the solo bowed harmonic: a sine with a twelfth of its second
 *            harmonic, a slow wander and a shallow 4.5 Hz vibrato, and bow
 *            noise band-passed at three times the pitch, louder in the
 *            first 300 ms; a 390 ms attack, a 550 ms release
 *   air      band-passed noise drifting 500 to 1,600 Hz over fifteen
 *            seconds, a different centre a side, breathing at 0.15 Hz
 *   delay    one line of a dotted eighth with an eighth tap: echoes fall
 *            left, then right; the reed, the bronze and the harmonic send
 *   reverb   a hall: three long damped combs a side, one allpass a side
 *
 * ---------------------------------------------------------------------- RAM --
 *
 * The delay line is 9,000 int16 (18 KB); the reverb's 6 combs + 2
 * allpasses 8,398 int16 (16.8 KB); the sine table 2 KB; the block ring
 * 80 B; state about 1 KB. About 38 KB, inside the 48 KiB Phase reserved.
 */

#include "synth.h"
#include "song.h"

#include <string.h>

#define CTL_DIV        40                       /* samples per control tick  */
#define BLOCK          20                       /* frames rendered at a time */
#define STEP_SAMPLES   SONG_STEP_SAMPLES        /* 3000  */
#define BAR_SAMPLES    SONG_BAR_SAMPLES         /* 48000 */
#define TOTAL          DURATION_SAMPLES         /* 3840000 */

_Static_assert(TOTAL % CTL_DIV == 0, "the endpoint must sit on a control tick");
_Static_assert(STEP_SAMPLES % CTL_DIV == 0, "a 16th must be whole control ticks");
_Static_assert(SAMPLE_RATE % CTL_DIV == 0, "the hash latch must sit on a control tick");
_Static_assert(CTL_DIV % BLOCK == 0, "blocks must tile the control tick");

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
static const envp_t ep_reed = { 65536 / 2000,  65534, 52000, 65514 };   /* 85 ms in, 124 ms out */
static const envp_t ep_bass = { 65536 / 60,    65510, 50000, 65440 };   /* 2.5 ms in, 28 ms out */
static const envp_t ep_bow  = { 65536 / 6000,  65535, 65535, 65529 };   /* 270 ms in, 390 ms out */
static const envp_t ep_harm = { 65536 / 8400,  65535, 65535, 65531 };   /* 390 ms in, 550 ms out */

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

#define DLY_LEN   9000                 /* a dotted eighth at 120 BPM: the left echo */
#define DLY_TAP   6000                 /* an eighth: the right echo                 */
#define DLY_FB    24000                /* 0.37 feedback                             */

static int16_t g_dly[DLY_LEN];

/* Reverb: a hall. Comb lengths are Freeverb's scaled to 24 kHz and then
 * by 1.5, the right side 37 samples longer; allpasses 449 and 341. */
#define RV_C0 907
#define RV_C1 1039
#define RV_C2 1213
#define RV_SPREAD 37
#define RV_A0 449
#define RV_A1 341
#define RV_FB 57500                    /* 0.88 */
#define RV_DAMP 30000                  /* 0.46 */

static int16_t g_rv_c[6][RV_C2 + RV_SPREAD];
static int16_t g_rv_a[2][RV_A0];
static const int g_rv_len[6] = { RV_C0, RV_C1, RV_C2, RV_C0 + RV_SPREAD, RV_C1 + RV_SPREAD, RV_C2 + RV_SPREAD };
static const int g_rv_alen[2] = { RV_A0, RV_A1 };

/* The rendered block, handed out by synth_render(). */
static int16_t g_ring[2 * BLOCK];

typedef struct {
    uint32_t ph1, ph2, ph3, inc1, inc2, inc3, rng;
    int32_t  a1, a2, a3, kn, lp;
} bronze_t;

static struct {
    uint32_t pos;                  /* rendered, in whole blocks               */
    uint32_t out;                  /* handed out                              */
    int      ring_rd, ring_n, ctl_left;
    int      mark_left;
    int32_t  peak;
    unsigned solo;

    /* drums: the frame drum, the rim, the scrape; one noise stream */
    uint32_t rng_drum;
    uint32_t k_ph, k_ph2, k_inc;  int32_t k_env, k_env2, k_click, k_lp;
    uint32_t m_ph;  int32_t m_env, m_click, m_lp;
    int32_t  c_env, c_rise, c_dec, c_lp, c_fc;  svf_t c_f;
    /* gong */
    uint32_t rng_gong, g_ph[6], g_inc[6];  int32_t g_amp[6], g_sw, g_rise, g_thud, g_lp, g_fc;  svf_t g_f;  int g_on;
    /* bass */
    uint32_t b_ph, b_inc, b_target;  env_t b_env;  int32_t b_fenv, b_fc, b_lvl, b_bite;  svf_t b_f;
    /* bronze */
    bronze_t z[2];  int z_next, z_on[2];  int32_t z_d1, z_d2, z_d3, z_lvl;
    /* reed */
    uint32_t rng_reed, r_ph, r_inc, r_target, r_eff, r_duty, r_lfo, r_dlfo;
    env_t    r_env;  svf_t r_f, r_n;  int32_t r_breath, r_fenv, r_fc, r_lvl, r_push, r_age;  int r_on;
    /* bow */
    uint32_t rng_bow, w_ph[8], w_inc[8], w_pph[4], w_pinc[4];
    env_t    w_env;  svf_t w_fl, w_fr, w_nf;  int32_t w_onset, w_ovt, w_fc, w_lvl, w_open;  uint8_t w_chord[4];  int w_on;
    /* harmonic */
    uint32_t rng_harm, h_ph, h_inc, h_eff, h_lfo, h_lfo2;  env_t h_env;  svf_t h_f;  int32_t h_fc, h_onset, h_noise, h_lvl;  int h_on;
    /* air */
    uint32_t rng_air, a_lfo, a_lfo2;  svf_t a_f[2];  int32_t a_fc[2], a_lvl;  int a_on;
    /* delay */
    int      d_w;  int32_t d_lp;
    /* reverb */
    int      rv_w[6], rv_aw[2];  int32_t rv_lp[6];
    /* master */
    int32_t  master, mw, space, dc_l, dc_r;

    /* per-tick mix gains, with solo and song levels folded in. gw_* are
     * Q32 gain words for mulhi(); g_* are Q16 <= 32767 because they are
     * multiplied by an envelope first. */
    int32_t  gw_drum, gw_rim, gw_scrape, gw_gong, gw_bronze_l[2], gw_bronze_r[2], gw_air;
    int32_t  g_bass, g_reed, g_bow, g_harm;
    int32_t  gw_dly, gw_rv;
} S;

void synth_solo(unsigned mask) { S.solo = mask; }

/* Levels, all <= 32767 so they fit a gain word. */
#define G_DRUM    26000
#define G_RIM     11000
#define G_SCRAPE  5000
#define G_GONG    26000
#define G_BASS    30000
#define G_BRONZE  52000    /* folded with the pan (max 28000/65536) -> <= 22218 */
#define G_REED    22000
#define G_BOW     18000
#define G_HARM    20000
#define G_AIR     3500
#define G_DELAY   20000
#define G_REVERB  17000
#define G_MASTER  42000

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

static void trig_drum(int soft)
{
    S.k_ph = 0; S.k_ph2 = 0;
    S.k_inc = soft ? 17895697u : 23264406u;             /* 100 or 130 Hz */
    const int32_t vel = soft ? 26000 : 65535;
    S.k_env = vel; S.k_env2 = vel; S.k_click = vel;
}

static void trig_rim(void)
{
    S.m_ph = 0; S.m_env = 65535; S.m_click = 65535;
}

static void trig_scrape(int longer)
{
    S.c_env = 65535; S.c_rise = 2000;
    S.c_dec = longer ? 65525 : 65500;
}

static void trig_gong(int soft)
{
    static const uint32_t inc[6]  = { 11632203u, 18378881u, 26056135u, 33966033u, 43155473u, 53857100u };
    static const int32_t  frac[6] = { 65536, 49152, 32768, 24576, 16384, 13107 };
    const int32_t vel = soft ? 24000 : 65535;
    for (int i = 0; i < 6; i++) { S.g_ph[i] = 0; S.g_inc[i] = inc[i]; S.g_amp[i] = qmul(vel, frac[i]); }
    S.g_sw = vel; S.g_rise = 0; S.g_thud = vel; S.g_lp = 0;
    S.g_f.lo = S.g_f.band = 0;
    S.rng_gong = seed_at(S.pos, 0x600D600Du);
    S.g_on = 1;
}

static void bass_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.b_env, &ep_bass); return; }
    const uint32_t inc = note_inc(e & 127);
    S.b_target = inc;
    if (!(e & SONG_SLIDE)) S.b_inc = inc;
    S.b_fenv = 65535;
    env_on(&S.b_env);
}

static void bronze_event(int e)
{
    if (e < 2) return;
    bronze_t *v = &S.z[S.z_next];
    S.z_next ^= 1;
    const uint32_t inc = note_inc(e);
    v->ph1 = v->ph2 = v->ph3 = 0;
    v->inc1 = inc;
    v->inc2 = (uint32_t)(((uint64_t)inc * 11289u) >> 12);     /* 2.756x */
    v->inc3 = (uint32_t)(((uint64_t)inc * 22135u) >> 12);     /* 5.404x */
    v->a1 = 65535; v->a2 = 45000; v->a3 = 30000; v->kn = 65535; v->lp = 0;
    v->rng = seed_at(S.pos, 0xB402E000u + (uint32_t)e);
}

/* Inside a phrase the reed glides to the next note and takes a smaller
 * breath; after a release it starts again with the full attack. */
static void reed_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.r_env, &ep_reed); return; }
    const uint32_t inc = note_inc(e);
    S.r_target = inc;
    if (S.r_env.stage == 1 || S.r_env.stage == 2) {
        S.r_breath = 24000;
    } else {
        S.r_inc = inc; S.r_breath = 65535; S.r_age = 0; S.r_fenv = 65535;
        env_on(&S.r_env);
    }
}

static void harm_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.h_env, &ep_harm); return; }
    S.h_inc = note_inc(e);
    S.h_onset = 65535;
    env_on(&S.h_env);
}

static void bow_bar(uint32_t bar)
{
    uint8_t c[4];
    song_bow_chord(bar, c);
    if (!c[0]) return;                               /* the gate does the release */
    if (memcmp(c, S.w_chord, 4) == 0) return;
    memcpy(S.w_chord, c, 4);
    for (int i = 0; i < 4; i++) {
        const uint32_t inc = note_inc(c[i]);
        S.w_inc[i]     = inc - (inc >> 9);           /* left bank, -3.4 cents  */
        S.w_inc[4 + i] = inc + (inc >> 9);           /* right bank, +3.4 cents */
        S.w_pinc[i]    = (uint32_t)(((uint64_t)inc * 11289u) >> 12);
    }
    /* a chord change under the bow: a dip, and the overtone relit a little */
    if (S.w_env.stage == 2) { S.w_env.v -= S.w_env.v >> 3; if (S.w_ovt < 40000) S.w_ovt = 40000; }
}

static void bow_gate(int g)
{
    if (g == SONG_OFF) { env_off(&S.w_env, &ep_bow); return; }
    if (g != SONG_BOW_ON) return;
    if (S.w_env.stage == 2) S.w_env.v -= S.w_env.v >> 2;
    env_on(&S.w_env);
    S.w_onset = 65535; S.w_ovt = 65535;
}

/* ------------------------------------------------------------- control ----- */

static void control_tick(void)
{
    const uint32_t pos = S.pos;
    const uint32_t bar = pos / BAR_SAMPLES;

    /* --- the sequencer: one row per 16th --- */
    if (pos % STEP_SAMPLES == 0) {
        const uint32_t step = pos / STEP_SAMPLES;
        if ((step & 15) == 0) bow_bar(bar);

        const uint8_t d = song_drums(step);
        if (d & DR_DRUM)        trig_drum(0);
        else if (d & DR_GHOST)  trig_drum(1);
        if (d & DR_RIM)         trig_rim();
        if (d & DR_SCRAPEL)     trig_scrape(1);
        else if (d & DR_SCRAPE) trig_scrape(0);
        if (d & DR_GONG)        trig_gong(0);
        else if (d & DR_GONGSOFT) trig_gong(1);

        bass_event(song_bass(step));
        bronze_event(song_bronze(step));
        reed_event(song_lead(step));
        harm_event(song_harm(step));
        bow_gate(song_bow_gate(step));
    }

    /* --- levels: one-pole per tick. The slow ones (~100-200 ms) are the
     * things that fade; the leads ~25 ms. --- */
    S.r_lvl  += (song_lead_level(bar)   * 257 - S.r_lvl)  >> 4;
    S.r_push += (song_lead_push(bar)    * 257 - S.r_push) >> 5;
    S.w_lvl  += (song_bow_level(bar)    * 257 - S.w_lvl)  >> 6;
    S.w_open += (song_bow_open(bar)     * 257 - S.w_open) >> 6;
    S.z_lvl  += (song_bronze_level(bar) * 257 - S.z_lvl)  >> 5;
    S.b_lvl  += (song_bass_level(bar)   * 257 - S.b_lvl)  >> 4;
    S.b_bite += (song_bass_bite(bar)    * 257 - S.b_bite) >> 5;
    S.h_lvl  += (song_harm_level(bar)   * 257 - S.h_lvl)  >> 5;
    S.a_lvl  += (song_air_level(bar)    * 257 - S.a_lvl)  >> 6;
    S.space  += (song_space(bar)        * 257 - S.space)  >> 6;

    /* --- the bronze's decay: tau from 100 ms (muted) to 600 ms (ringing);
     * the partials three and eight times faster --- */
    {
        const int32_t tau = 2400 + song_bronze_decay(bar) * 47;
        S.z_d1 = 65536 - 65536 / tau;
        S.z_d2 = 65536 - 3 * (65536 - S.z_d1);
        S.z_d3 = 65536 - 8 * (65536 - S.z_d1);
        for (int v = 0; v < 2; v++) S.z_on[v] = S.z[v].a1 != 0 || S.z[v].kn != 0;
    }

    /* --- the bass: the slide, the bite --- */
    S.b_inc += (uint32_t)(((int64_t)S.b_target - (int64_t)S.b_inc) >> 5);
    S.b_fenv = qmul(S.b_fenv, 63802);                             /* 63 ms */
    S.b_fc = hz_f(170u + (uint32_t)(((int64_t)S.b_fenv * (200 + ((S.b_bite * 7) >> 8))) >> 16));

    /* --- the reed: glide, vibrato arriving over 250 ms, the breathing
     * pulse width, the cutoff that follows the push and the swell --- */
    S.r_lfo += 601;                                               /* 5.5 Hz */
    S.r_dlfo += 33;                                               /* 0.3 Hz */
    S.r_age += 1;
    S.r_inc += (uint32_t)(((int64_t)S.r_target - (int64_t)S.r_inc) >> 3);
    {
        const int32_t depth = S.r_age >= 150 ? 65536 : S.r_age * 437;
        const int32_t vib = qmul(g_sin[(S.r_lfo >> 6) & 1023], depth);        /* Q15 */
        S.r_eff = S.r_inc + (uint32_t)(((int64_t)S.r_inc * vib) >> 23);
        S.r_duty = (uint32_t)(0x80000000ll + (((int64_t)g_sin[(S.r_dlfo >> 6) & 1023] * 0x0A000000) >> 15));
        S.r_fenv = qmul(S.r_fenv, 65200);                         /* 325 ms */
        const uint32_t push_hz = (uint32_t)((S.r_push * 7) >> 8);
        const uint32_t env_hz  = (uint32_t)(((int64_t)S.r_env.v * (600 + ((S.r_push * 3) >> 8))) >> 16);
        const uint32_t bloom   = (uint32_t)((S.r_fenv * 600) >> 16);
        uint32_t fc = 300u + push_hz + env_hz + bloom;
        if (fc > 3200) fc = 3200;
        S.r_fc = hz_f(fc) >> 1;                                   /* run at 2x */
    }

    /* --- the bow: the stroke's noise and overtone settle; the filter opens
     * with the envelope --- */
    S.w_onset = qmul(S.w_onset, 65099);                           /* 250 ms */
    if (S.w_ovt > 16000) S.w_ovt = 16000 + qmul(S.w_ovt - 16000, 65399);  /* 800 ms */
    {
        const uint32_t open_hz = (uint32_t)((S.w_open * 9) >> 8);
        S.w_fc = hz_f(250u + (uint32_t)(((int64_t)open_hz * S.w_env.v) >> 16));
    }

    /* --- the harmonic: a wander, a shallow vibrato, the bow noise at three
     * times the pitch --- */
    S.h_lfo += 492;                                               /* 4.5 Hz  */
    S.h_lfo2 += 33;                                               /* 0.3 Hz  */
    S.h_onset = qmul(S.h_onset, 65200);                           /* 325 ms  */
    {
        const int32_t vib = (g_sin[(S.h_lfo >> 6) & 1023] >> 3) + (g_sin[(S.h_lfo2 >> 6) & 1023] >> 2);
        S.h_eff = S.h_inc + (uint32_t)(((int64_t)S.h_inc * vib) >> 23);
        const uint32_t hz = (uint32_t)(((uint64_t)S.h_inc * SAMPLE_RATE) >> 32);
        S.h_fc = hz_f(hz * 3u);
        S.h_noise = 5000 + ((S.h_onset * 14000) >> 16);
    }

    /* --- the air: two centres drifting, breathing --- */
    S.a_lfo += 8;                                                 /* 0.07 Hz */
    S.a_lfo2 += 16;                                               /* 0.15 Hz */
    S.a_fc[0] = hz_f(500u + (uint32_t)(((g_sin[(S.a_lfo >> 6) & 1023] + 32768) * 1000) >> 16));
    S.a_fc[1] = hz_f(600u + (uint32_t)(((g_sin[((S.a_lfo >> 6) + 300) & 1023] + 32768) * 1000) >> 16));

    /* --- the scrape's centre climbs as it fades --- */
    S.c_fc = hz_f(2400u + (uint32_t)(((65535 - S.c_env) * 1200) >> 16));

    /* --- the gong: partials decaying, the shimmer swelling and climbing --- */
    if (S.g_on) {
        static const int32_t dec[6] = { 65505, 65490, 65470, 65450, 65420, 65390 };
        for (int i = 0; i < 6; i++) S.g_amp[i] = qmul(S.g_amp[i], dec[i]);
        S.g_sw = qmul(S.g_sw, 65490);                             /* 2.4 s */
        S.g_rise += (65535 - S.g_rise) >> 7;                      /* 200 ms */
        S.g_fc = hz_f(1200u + (uint32_t)(((65535 - S.g_sw) * 1800) >> 16));
        if (S.g_amp[0] == 0 && S.g_sw == 0 && S.g_thud == 0) S.g_on = 0;
    }

    /* master: the trim, and a fade over the last bar and a half so the tail
     * is inside the file; squared, and done 2,000 samples early, so the
     * last 83 ms are true silence and nothing is cut */
    int32_t master = G_MASTER;
    const uint32_t fade_from = TOTAL - BAR_SAMPLES - BAR_SAMPLES / 2;
    if (pos >= fade_from) {
        const int32_t k = 65536 - ramp(pos, fade_from, TOTAL - 2000);
        master = qmul(master, qmul(k, k));
    }
    S.master = master;
    S.mw = (int32_t)((uint32_t)master << 15);

    /* mix gains with solo folded in */
    const unsigned so = S.solo;
    const int32_t breath = 40000 + ((g_sin[(S.a_lfo2 >> 6) & 1023] * 25000) >> 15);   /* 0.61 .. 1.0 */
    S.gw_drum   = (so & SOLO_DRUM)   ? GW(G_DRUM)   : 0;
    S.gw_rim    = (so & SOLO_RIM)    ? GW(G_RIM)    : 0;
    S.gw_scrape = (so & SOLO_SCRAPE) ? GW(G_SCRAPE) : 0;
    S.gw_gong   = (so & SOLO_GONG)   ? GW(G_GONG)   : 0;
    S.g_bass    = (so & SOLO_BASS)   ? qmul(G_BASS, S.b_lvl) : 0;
    {
        const int32_t gz = (so & SOLO_BRONZE) ? qmul(G_BRONZE, S.z_lvl) : 0;
        S.gw_bronze_l[0] = GW(qmul(gz, 28000)); S.gw_bronze_r[0] = GW(qmul(gz, 14000));
        S.gw_bronze_l[1] = GW(qmul(gz, 14000)); S.gw_bronze_r[1] = GW(qmul(gz, 28000));
    }
    S.g_reed    = (so & SOLO_REED)   ? qmul(G_REED, S.r_lvl) : 0;
    S.g_bow     = (so & SOLO_BOW)    ? qmul(G_BOW, S.w_lvl)  : 0;
    S.g_harm    = (so & SOLO_HARM)   ? qmul(G_HARM, S.h_lvl) : 0;
    S.gw_air    = (so & SOLO_AIR)    ? GW(qmul(qmul(G_AIR, S.a_lvl), breath)) : 0;
    S.gw_dly    = (so & SOLO_FX)     ? GW(qmul(G_DELAY, S.space))  : 0;
    S.gw_rv     = (so & SOLO_FX)     ? GW(qmul(G_REVERB, S.space)) : 0;

    /* which voices render this tick: decided here, never at a block edge */
    S.r_on = S.g_reed != 0 && S.r_env.stage != 0;
    S.w_on = S.g_bow  != 0 && S.w_env.stage != 0;
    S.h_on = S.g_harm != 0 && S.h_env.stage != 0;
    S.a_on = S.gw_air != 0;
}

/* -------------------------------------------------------------- render ------ */
/* One pass per voice over a block of at most CTL_DIV frames, the way a
 * tracker mixer does it: one loop with every voice in it spills everything. */

static void HOT(render_block)(int16_t *out, int n)
{
    int32_t acc[2 * CTL_DIV];      /* the stereo bus                          */
    int32_t send[CTL_DIV];         /* what goes to the delay                  */
    int32_t rvin[CTL_DIV];         /* what goes to the hall                   */

    /* --- drums: writes the bus --- */
    {
        uint32_t rng = S.rng_drum, kph = S.k_ph, kph2 = S.k_ph2, kinc = S.k_inc, mph = S.m_ph;
        int32_t kenv = S.k_env, kenv2 = S.k_env2, kclick = S.k_click, klp = S.k_lp;
        int32_t menv = S.m_env, mclick = S.m_click, mlp = S.m_lp;
        int32_t cenv = S.c_env, crise = S.c_rise, clp = S.c_lp;
        svf_t cf = S.c_f;
        const int32_t cdec = S.c_dec, cfc = S.c_fc;
        const int32_t gk = S.gw_drum, gm = S.gw_rim, gc = S.gw_scrape;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int32_t nz = (int32_t)(rng >> 16) - 32768;

            /* the frame drum: the fall from 130 Hz to 62 Hz over 30 ms */
            if (kinc > 11095332u) kinc -= kinc >> 10;
            kph += kinc; kph2 += kinc + (kinc >> 1);
            klp += (nz - klp) >> 3;
            const int32_t drum = qmul(g_sin[kph >> 22], kenv) + (qmul(g_sin[kph2 >> 22], kenv2) >> 2)
                               + (qmul(klp, kclick) >> 1);
            kenv = qmul(kenv, 65522); kenv2 = qmul(kenv2, 65480); kclick = qmul(kclick, 64999);

            /* the rim: a 900 Hz knock and a click */
            mph += 161061274u;
            mlp += (nz - mlp) >> 2;
            const int32_t rim = qmul(g_sin[mph >> 22], menv) + (qmul(nz - mlp, mclick) >> 1);
            menv = qmul(menv, 65300); mclick = qmul(mclick, 64800);

            /* brushed metal: rises, then falls, the band climbing */
            const int32_t amp = qmul(cenv, crise);
            cenv = qmul(cenv, cdec);
            crise += (65535 - crise) >> 9;
            svf(&cf, nz, cfc, 22000);
            clp += (nz - clp) >> 1;
            const int32_t scr = qmul(cf.band, amp) + (qmul(nz - clp, amp) >> 2);

            const int32_t d = mulhi(drum, gk), r = mulhi(rim, gm), s = mulhi(scr, gc);
            acc[2 * i]     = d + r + s - (s >> 2);
            acc[2 * i + 1] = d + r - (r >> 2) + s;
            rvin[i] = (r >> 2) + (s >> 1) + (d >> 3);
            send[i] = 0;
        }
        S.rng_drum = rng; S.k_ph = kph; S.k_ph2 = kph2; S.k_inc = kinc; S.m_ph = mph;
        S.k_env = kenv; S.k_env2 = kenv2; S.k_click = kclick; S.k_lp = klp;
        S.m_env = menv; S.m_click = mclick; S.m_lp = mlp;
        S.c_env = cenv; S.c_rise = crise; S.c_lp = clp; S.c_f = cf;
    }

    /* --- the tam-tam: six partials, the thud, the shimmer that swells --- */
    if (S.g_on) {
        uint32_t rng = S.rng_gong, ph[6];
        int32_t amp[6];
        for (int k = 0; k < 6; k++) { ph[k] = S.g_ph[k]; amp[k] = S.g_amp[k]; }
        const uint32_t *inc = S.g_inc;
        svf_t f = S.g_f;
        int32_t glp = S.g_lp, thud = S.g_thud;
        const int32_t sw = qmul(S.g_sw, S.g_rise), fc = S.g_fc, g = S.gw_gong;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int32_t nz = (int32_t)(rng >> 16) - 32768;
            int32_t y = 0;
            for (int k = 0; k < 6; k++) { ph[k] += inc[k]; y += qmul(g_sin[ph[k] >> 22], amp[k]); }
            y >>= 2;
            svf(&f, nz, fc, 12000);
            y += qmul(f.band, sw) >> 1;
            glp += (nz - glp) >> 3;
            y += qmul(glp, thud);
            thud = qmul(thud, 65400);
            const int32_t o = mulhi(y, g);
            acc[2 * i] += o; acc[2 * i + 1] += o;
            rvin[i] += o >> 1;
        }
        for (int k = 0; k < 6; k++) S.g_ph[k] = ph[k];
        S.rng_gong = rng; S.g_f = f; S.g_lp = glp; S.g_thud = thud;
    }

    /* --- the bass: saw and pulse, the biting lowpass, the grit --- */
    {
        uint32_t bph = S.b_ph;
        const uint32_t binc = S.b_inc;
        env_t benv = S.b_env;
        svf_t f = S.b_f;
        const int32_t fc = S.b_fc, g = S.g_bass;
        for (int i = 0; i < n; i++) {
            const int32_t saw = (int32_t)(bph >> 16) - 32768;
            const int32_t pulse = bph < 0x40000000u ? 21000 : -7000;      /* 25%, zero-mean */
            bph += binc;
            svf(&f, (saw >> 1) + pulse, fc, 15000);
            int32_t y = clamp32(f.lo + (f.band >> 2), -32767, 32767);
            y = sat(y + (y >> 1));
            y += y >> 1;                                     /* back to full scale after the knee */
            const int32_t b = mulhi(y, GW(qmul(env_run(&benv, &ep_bass), g)));
            acc[2 * i] += b; acc[2 * i + 1] += b;
        }
        S.b_ph = bph; S.b_env = benv; S.b_f = f;
    }

    /* --- the bronze: two voices in turn, each with its own noise --- */
    for (int v = 0; v < 2; v++) {
        if (!S.z_on[v]) continue;
        bronze_t z = S.z[v];
        const int32_t d1 = S.z_d1, d2 = S.z_d2, d3 = S.z_d3;
        const int32_t gl = S.gw_bronze_l[v], gr = S.gw_bronze_r[v];
        for (int i = 0; i < n; i++) {
            z.rng ^= z.rng << 13; z.rng ^= z.rng >> 17; z.rng ^= z.rng << 5;
            const int32_t nz = (int32_t)(z.rng >> 16) - 32768;
            z.ph1 += z.inc1; z.ph2 += z.inc2; z.ph3 += z.inc3;
            int32_t y = qmul(g_sin[z.ph1 >> 22], z.a1) + (qmul(g_sin[z.ph2 >> 22], z.a2) >> 1)
                      + (qmul(g_sin[z.ph3 >> 22], z.a3) >> 2);
            z.lp += (nz - z.lp) >> 1;
            y += qmul(z.lp, z.kn) >> 1;
            z.a1 = qmul(z.a1, d1); z.a2 = qmul(z.a2, d2); z.a3 = qmul(z.a3, d3); z.kn = qmul(z.kn, 65200);
            const int32_t l = mulhi(y, gl), r = mulhi(y, gr);
            acc[2 * i] += l; acc[2 * i + 1] += r;
            send[i] += (l + r) >> 2;
            rvin[i] += (l + r) >> 2;
        }
        S.z[v] = z;
    }

    /* --- the electric reed --- */
    if (S.r_on) {
        uint32_t rng = S.rng_reed, ph = S.r_ph;
        const uint32_t inc = S.r_eff, duty = S.r_duty;
        env_t env = S.r_env;
        svf_t f = S.r_f, nf = S.r_n;
        int32_t breath = S.r_breath;
        const int32_t fc = S.r_fc, drive = S.r_push, g = S.g_reed;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int32_t nz = (int32_t)(rng >> 16) - 32768;
            ph += inc;
            const int32_t pulse = ph < duty ? 18000 : -18000;
            const int32_t saw = ((int32_t)(ph >> 16) - 32768) >> 2;
            const int32_t x = pulse + saw + (qmul(nz, breath) >> 2);
            breath = qmul(breath, 65490);
            svf(&f, x, fc, 14000);                          /* twice: the filter runs at 2x */
            svf(&f, x, fc, 14000);
            int32_t y = clamp32(f.lo, -32767, 32767);
            y = sat(y + qmul(y, drive));
            svf(&nf, y, 19731, 18000);                      /* the 1,150 Hz formant */
            y += nf.band >> 1;
            const int32_t o = mulhi(y, GW(qmul(env_run(&env, &ep_reed), g)));
            acc[2 * i] += o; acc[2 * i + 1] += o - (o >> 3);
            send[i] += o >> 1;
            rvin[i] += o >> 2;
        }
        S.rng_reed = rng; S.r_ph = ph; S.r_env = env; S.r_f = f; S.r_n = nf; S.r_breath = breath;
    }

    /* --- the bowed metal: four voices, a saw a side each, the partials and
     * the bow noise shared, one filter a side --- */
    if (S.w_on) {
        int32_t gwv[CTL_DIV], ex[CTL_DIV];
        env_t env = S.w_env;
        const int32_t g = S.g_bow;
        for (int i = 0; i < n; i++) gwv[i] = GW(qmul(env_run(&env, &ep_bow), g));
        S.w_env = env;
        {
            uint32_t rng = S.rng_bow, p0 = S.w_pph[0], p1 = S.w_pph[1], p2 = S.w_pph[2], p3 = S.w_pph[3];
            const uint32_t j0 = S.w_pinc[0], j1 = S.w_pinc[1], j2 = S.w_pinc[2], j3 = S.w_pinc[3];
            svf_t nf = S.w_nf;
            const int32_t ovt = S.w_ovt, onset = S.w_onset;
            for (int i = 0; i < n; i++) {
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                const int32_t nz = (int32_t)(rng >> 16) - 32768;
                p0 += j0; p1 += j1; p2 += j2; p3 += j3;
                const int32_t pp = (g_sin[p0 >> 22] + g_sin[p1 >> 22] + g_sin[p2 >> 22] + g_sin[p3 >> 22]) >> 4;
                svf(&nf, nz, 37746, 12000);                 /* 2.2 kHz */
                ex[i] = qmul(pp, ovt) + (qmul(nf.band, onset) >> 2);
            }
            S.rng_bow = rng; S.w_pph[0] = p0; S.w_pph[1] = p1; S.w_pph[2] = p2; S.w_pph[3] = p3; S.w_nf = nf;
        }
        for (int side = 0; side < 2; side++) {
            uint32_t *php = S.w_ph + 4 * side;
            const uint32_t *pinc = S.w_inc + 4 * side;
            uint32_t q0 = php[0], q1 = php[1], q2 = php[2], q3 = php[3];
            const uint32_t j0 = pinc[0], j1 = pinc[1], j2 = pinc[2], j3 = pinc[3];
            svf_t f = side ? S.w_fr : S.w_fl;
            const int32_t fc = S.w_fc;
            for (int i = 0; i < n; i++) {
                const int32_t x = ((int32_t)(q0 >> 16) + (int32_t)(q1 >> 16)
                                 + (int32_t)(q2 >> 16) + (int32_t)(q3 >> 16) - 4 * 32768) >> 2;
                q0 += j0; q1 += j1; q2 += j2; q3 += j3;
                svf(&f, x, fc, 30000);
                const int32_t o = mulhi(f.lo + ex[i], gwv[i]);
                acc[2 * i + side] += o;
                rvin[i] += o >> 2;
            }
            php[0] = q0; php[1] = q1; php[2] = q2; php[3] = q3;
            if (side) S.w_fr = f; else S.w_fl = f;
        }
    }

    /* --- the solo bowed harmonic --- */
    if (S.h_on) {
        uint32_t rng = S.rng_harm, ph = S.h_ph;
        const uint32_t inc = S.h_eff;
        env_t env = S.h_env;
        svf_t f = S.h_f;
        const int32_t fc = S.h_fc, hn = S.h_noise, g = S.g_harm;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int32_t nz = (int32_t)(rng >> 16) - 32768;
            ph += inc;
            int32_t y = g_sin[ph >> 22] + (g_sin[(ph * 2u) >> 22] >> 3);
            svf(&f, nz, fc, 9000);
            y += qmul(f.band, hn) >> 1;
            const int32_t o = mulhi(y, GW(qmul(env_run(&env, &ep_harm), g)));
            acc[2 * i] += o; acc[2 * i + 1] += o - (o >> 2);
            rvin[i] += o >> 1;
            send[i] += o >> 3;
        }
        S.rng_harm = rng; S.h_ph = ph; S.h_env = env; S.h_f = f;
    }

    /* --- the air --- */
    if (S.a_on) {
        uint32_t rng = S.rng_air;
        svf_t f0 = S.a_f[0], f1 = S.a_f[1];
        const int32_t c0 = S.a_fc[0], c1 = S.a_fc[1], g = S.gw_air;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int32_t nz = (int32_t)(rng >> 16) - 32768;
            svf(&f0, nz, c0, 14000);
            svf(&f1, nz, c1, 14000);
            const int32_t l = mulhi(f0.band, g), r = mulhi(f1.band, g);
            acc[2 * i] += l; acc[2 * i + 1] += r;
            rvin[i] += (l + r) >> 3;
        }
        S.rng_air = rng; S.a_f[0] = f0; S.a_f[1] = f1;
    }

    /* --- delay: one dotted-eighth line; the end goes left, the eighth tap right --- */
    {
        int w = S.d_w;
        int32_t lp = S.d_lp;
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
        S.d_w = w; S.d_lp = lp;
    }

    /* --- reverb: three damped combs a side in parallel, one allpass a side --- */
    {
        const int32_t g = S.gw_rv, fb = GW(RV_FB);
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
                    line[w] = (int16_t)clamp32((rvin[i] >> 2) + mulhi(lp, fb), -32768, 32767);
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
    memset(g_ring, 0, sizeof g_ring);
    S.solo = solo;
    S.rng_drum = 0x1BADF00Du;
    S.rng_gong = 0x600D600Du;
    S.rng_reed = 0x5EEDF00Du;
    S.rng_bow  = 0xB0B0B0B1u;
    S.rng_harm = 0x0C0FFEE5u;
    S.rng_air  = 0xA1A1A1A1u;
    S.z[0].rng = 0xB402E001u; S.z[1].rng = 0xB402E003u;
    S.k_inc = 11095332u;
    S.c_dec = 65500;
    S.z_d1 = 65520; S.z_d2 = 65488; S.z_d3 = 65408;
    S.master = G_MASTER;
    S.mw = (int32_t)((uint32_t)G_MASTER << 15);
    S.mark_left = SAMPLE_RATE;
    S.b_inc = S.b_target = note_inc(38);
    S.r_inc = S.r_target = S.r_eff = note_inc(74);
    S.r_duty = 0x80000000u;
    S.h_inc = S.h_eff = note_inc(74);
    for (int i = 0; i < 8; i++) S.w_inc[i] = note_inc(50);
    for (int i = 0; i < 4; i++) S.w_pinc[i] = note_inc(50) * 2u;
    for (int v = 0; v < 2; v++) { S.z[v].inc1 = note_inc(62); S.z[v].inc2 = note_inc(62) * 2u; S.z[v].inc3 = note_inc(62) * 5u; }
    g_hash = 2166136261u;
    hash_publish(0);
    g_read_seq = g_mark_seq;
}

void synth_init(void)
{
    build_tables();
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
