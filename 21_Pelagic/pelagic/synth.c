/* PELAGIC -- the synth.
 *
 * The integer tracker synth from PERSISTENCE and COLOSSUS, voiced for water:
 * a rounded sine bass, droplet plucks with a long stereo echo, an airy lead
 * through a chorus, a glass organ of pure harmonics, an "oo" formant choir,
 * a band-passed shimmer that breathes, and a hall behind all of it. See
 * pelagic.h for the public contract and synth.h for what the tools need.
 *
 * ------------------------------------------------------------------ numerics --
 *
 * Signals are int32 with +-32768 as nominal full scale. Gains and envelopes
 * are Q16 (65536 = 1.0); products go through int64 before the shift, which on
 * Cortex-M33 is a single smull and a shift pair. Oscillators are 32-bit phase
 * accumulators (2^32 = one cycle) so a saw is one subtraction.
 *
 * No floating point anywhere, including synth_init(): the note table is
 * twelve integer constants for the top octave shifted down, and the sine is
 * Bhaskara's rational approximation evaluated in int64, so the host and the
 * device cannot disagree by an ulp in a table entry.
 *
 * CONTROL RATE. Every 48 samples (500 Hz): the sequencer, filter coefficients
 * and level smoothing. 2,880 samples per 16th is exactly 60 ticks, so every
 * note lands on a tick boundary. Envelopes run per sample.
 *
 * PULL MODEL, IN BLOCKS. The engine only ever renders whole blocks aligned
 * to the absolute sample counter, into a small ring; synth_render() hands
 * frames out of that ring. So the output is a function of the sample index
 * alone -- rendering 1, 8 or 512 frames at a time produces the same bytes --
 * and the device, which asks for one or two frames at a time between
 * scanlines, pays the per-voice state load/store once per block rather than
 * once per call. The control tick is every 48 samples; the block is half of
 * one, 24 frames, so the one call in ~15 that renders a block costs core 1
 * about 250 us and never more than the scanline queue absorbs (Overscan
 * measured 512 us for a 48-frame block once the plate staging DMA competed
 * for the bus). The two halves see exactly the sequence of per-sample
 * operations one 48-frame block would, so the bytes are the same.
 *
 * ------------------------------------------------------------------- voicing --
 *
 *   kick     a soft thud: sine 110 Hz sweeping to 40 Hz, a whisper of click
 *   brush    band-passed noise at 1.8 kHz with a slow tail, a little body
 *   shaker   high-passed noise, closed and open, quiet
 *   whoosh   low-passed noise with a 1.4 s tail; the water moving
 *   boom     a 48 Hz sine with a 0.9 s tail
 *   bass     a sine with a quarter of a saw, through a lowpass whose cutoff
 *            follows a short envelope: rounded, with a soft edge on the attack
 *   pluck    a plucked string, Karplus-Strong: a one-period loop of
 *            lowpassed noise through an averaging filter, with a fractional
 *            delay so it is in tune; the decay set per bar (a droplet in the
 *            reef, a harp in the abyss); pans alternately; sends hard to the
 *            echo
 *   lead     three detuned saws and the octave (its level set per bar),
 *            two lowpasses with a filter envelope, a 67 ms attack, and a
 *            vibrato that arrives 300 ms into the note; through the chorus
 *   lead2    two detuned 25% pulses, hollow, for the hint and the descent
 *   pad      eight detuned saws, four a side, through two slow lowpasses
 *            whose cutoff the score sets per bar; through the chorus
 *   drone    two triangles an octave apart with a slow vibrato, lowpassed;
 *            its note set per bar
 *   glass    five notes as organs of harmonics 1, 2, 3, 4 (8:4:1:2), from
 *            one phase accumulator each; a slow tremolo in opposite phase
 *            left and right
 *   choir    four notes an octave above the pad, two detuned saws each with
 *            a shared vibrato, through three band-passes at the formants of
 *            an "oo" (350, 800, 2400 Hz); through the chorus
 *   shimmer  band-passed noise whose centre drifts between 0.8 and 3.2 kHz
 *            over twenty seconds and whose level breathes at 0.2 Hz
 *   chorus   two taps modulated in opposite phase (12.5 ms +- 4 ms), dry
 *            plus 5/8 wet, on the pad, the choir, the lead and the shimmer
 *   delay    one line of 3/8 with a 1/4 tap: echoes fall right, then left
 *   reverb   a hall: three long damped combs a side, one allpass a side
 *
 * ---------------------------------------------------------------------- RAM --
 *
 * The delay line is 17,280 int16 (34.5 KB); the reverb's 6 combs + 2
 * allpasses 8,398 int16 (16.8 KB); the chorus 1,024 int16 (2 KB); the sine
 * table 2 KB; the string 1 KB; the block ring 192 B; state about 1 KB.
 * About 57.5 KB, inside the 64 KiB Phase reserved.
 */

#include "synth.h"
#include "song.h"

#include <string.h>

#define CTL_DIV        48                       /* samples per control tick  */
#define BLOCK          24                       /* frames rendered at a time */
#define STEP_SAMPLES   SONG_STEP_SAMPLES        /* 2880  */
#define BAR_SAMPLES    SONG_BAR_SAMPLES         /* 46080 */
#define TOTAL          DURATION_SAMPLES         /* 3686400, a multiple of 48 */

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

static int32_t mix(int32_t a, int32_t b, int32_t w)
{
    return a + (int32_t)(((int64_t)(b - a) * w) >> 16);
}

/* Cutoff coefficient for the state-variable filter: 2*sin(pi*fc/fs) with the
 * small-angle approximation, Q16. 411775 is 2*pi in Q16. */
static inline int32_t hz_f(uint32_t hz)
{
    if (hz > 4000) hz = 4000;
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
static const envp_t ep_bass  = { 65536 / 40,    65525, 42000, 65490 };
static const envp_t ep_lead  = { 65536 / 1600,  65530, 56000, 65515 };
static const envp_t ep_lead2 = { 65536 / 1200,  65531, 54000, 65520 };
static const envp_t ep_pad   = { 65536 / 14000, 65535, 65535, 65530 };
static const envp_t ep_choir = { 65536 / 8000,  65535, 65535, 65531 };

/* ------------------------------------------------------------------ hash ---- */
static uint32_t          g_hash = 2166136261u;
static volatile uint32_t g_mark_seq, g_mark_pos, g_mark_hash;

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
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        const uint32_t p = g_mark_pos, h = g_mark_hash;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_mark_seq, __ATOMIC_RELAXED) != s) continue;
        if (!p) return 0;
        *pos = p; *hash = h;
        return 1;
    }
}

/* ----------------------------------------------------------------- state ---- */

#define DLY_LEN   17280                /* 3/8 at 125 BPM: the left echo       */
#define DLY_TAP   11520                /* 1/4: the right echo                 */
#define DLY_FB    27000                /* 0.41 feedback                       */

static int16_t g_dly[DLY_LEN];

/* Reverb: a hall. Comb lengths are Freeverb's scaled to 24 kHz and then
 * by 1.5, the right side 37 samples longer; allpasses 449 and 341. */
#define RV_C0 907
#define RV_C1 1039
#define RV_C2 1213
#define RV_SPREAD 37
#define RV_A0 449
#define RV_A1 341
#define RV_FB 59500                    /* 0.91 */
#define RV_DAMP 26000                  /* 0.40 */

static int16_t g_rv_c[6][RV_C2 + RV_SPREAD];
static int16_t g_rv_a[2][RV_A0];

/* Chorus lines: the longest tap is 300 + 96 + 1 samples, so 512 is enough. */
#define CHORUS_LEN 512
static int16_t g_chorus[2][CHORUS_LEN];
static const int g_rv_len[6] = { RV_C0, RV_C1, RV_C2, RV_C0 + RV_SPREAD, RV_C1 + RV_SPREAD, RV_C2 + RV_SPREAD };
static const int g_rv_alen[2] = { RV_A0, RV_A1 };

/* The string: the longest period is 512 samples (47 Hz). */
#define KS_LEN 512
static int16_t g_ks[KS_LEN];

/* The rendered block, handed out by synth_render(). */
static int16_t g_ring[2 * BLOCK];

static struct {
    uint32_t pos;                  /* rendered, in whole blocks               */
    uint32_t out;                  /* handed out                              */
    int      ring_rd, ring_n, ctl_left;
    int      mark_left;
    uint32_t rng, rng2;
    int32_t  peak;
    unsigned solo;

    /* kick */
    uint32_t k_ph, k_inc;  int32_t k_env, k_click;
    /* brush */
    uint32_t s_ph;  int32_t s_envn, s_envt;  svf_t s_f;
    /* shaker + whoosh */
    int32_t  h_env, h_dec, h_lp, c_env, w_lp;
    /* boom */
    uint32_t x_ph;  int32_t x_env;
    /* bass */
    uint32_t b_ph, b_inc;  env_t b_env;  int32_t b_fenv;  svf_t b_f;  int32_t b_fc;
    /* pluck: the string */
    int      ks_w;  uint32_t ks_d, rng3;  int32_t ks_g, ks_prev, ks_dc, a_tau, a_lp, a_pl, a_pr, a_lvl;  int a_side;
    /* lead */
    uint32_t l_ph[4], l_inc[4], l_base[4], l_lfo;  env_t l_env;  int32_t l_fenv;  svf_t l_fl, l_fr;
    int32_t  l_fc, l_lvl, l_oct, l_age;
    /* lead2 */
    uint32_t m_ph[2], m_inc[2];  env_t m_env;  int32_t m_lp, m_lvl;
    /* pad */
    uint32_t p_ph[8], p_inc[8];  env_t p_env;  svf_t p_fl, p_fr;  int32_t p_fc, p_lvl;
    uint8_t  p_chord[4];  uint32_t p_lfo;
    /* drone */
    uint32_t t_ph[2], t_lfo;  int32_t t_lp, t_lvl;  int t_note;
    /* glass */
    uint32_t o_ph[5], o_inc[5], o_lfo;  int32_t o_lvl;
    /* choir */
    uint32_t v_ph[8], v_inc[8], v_lfo;  env_t v_env;  svf_t v_f[2][3];  int32_t v_lvl;
    uint8_t  v_chord[4];
    /* shimmer */
    svf_t    r_f;  int32_t r_fc, r_lvl;  uint32_t r_lfo, r_lfo2;
    /* chorus */
    int      w_w;  uint32_t w_lfo;
    /* delay */
    int      d_w;  int32_t d_lp;
    /* reverb */
    int      rv_w[6], rv_aw[2];  int32_t rv_lp[6];
    /* master */
    int32_t  master;

    /* per-tick mix gains, with solo and song levels folded in. gw_* are
     * Q32 gain words for mulhi(); g_* are Q16 <= 32767 because they are
     * multiplied by an envelope first. */
    int32_t  gw_kick, gw_brush, gw_shaker, gw_whoosh, gw_boom, gw_bass, gw_pluck_l, gw_pluck_r;
    int32_t  g_lead, gw_lead2_l, gw_lead2_r, gw_lead2_s, g_pad, gw_drone, gw_shim, gw_glass, g_choir;
    int32_t  gw_dly, gw_rv, mw;
} S;

void synth_solo(unsigned mask) { S.solo = mask; }

/* Levels, all <= 32767 so they fit a gain word. */
#define G_KICK    26000
#define G_BRUSH   9000
#define G_SHAKER  2200
#define G_WHOOSH  6000
#define G_BOOM    20000
#define G_BASS    17000
#define G_PLUCK   54000    /* folded with the pan (max 28000/65536) -> <= 23072 */
#define G_LEAD    22000
#define G_LEAD2   24000
#define G_PAD     15000
#define G_DRONE   13000
#define G_GLASS   9000
#define G_CHOIR   11000
#define G_SHIM    4000
#define G_DELAY   22000
#define G_REVERB  18000
#define G_MASTER  45000

/* Soft knee above 20000; the output never exceeds 26000 (-2.0 dBFS). */
static inline int32_t soft_clip(int32_t v)
{
    const int32_t a = v < 0 ? -v : v;
    if (a <= 20000) return v;
    const int32_t e = a - 20000;
    const int32_t o = 20000 + (6000 * e) / (e + 6000);
    return v < 0 ? -o : o;
}

/* --------------------------------------------------------------- triggers --- */

static void trig_kick(void)
{
    S.k_ph = 0; S.k_inc = 19685266u;          /* 110 Hz */
    S.k_env = 65535; S.k_click = 65535;
}

static void trig_brush(void)
{
    S.s_ph = 0; S.s_envn = 65535; S.s_envt = 65535;
}

static void trig_shaker(int open)
{
    S.h_env = 65535; S.h_dec = open ? 65515 : 65450;
}

static void trig_whoosh(void) { S.c_env = 65535; }
static void trig_boom(void)   { S.x_ph = 0; S.x_env = 65535; }

static void bass_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.b_env, &ep_bass); return; }
    S.b_inc = note_inc(e);
    S.b_fenv = 65535;
    env_on(&S.b_env);
}

/* The droplets ignore note-offs: each one rings out on its own. A pluck
 * fills the loop with lowpassed noise and sets its length to the period in
 * Q8, less the half sample the averaging filter adds; the loop gain is
 * chosen so the note decays over the bar's tau whatever its pitch. */
static void pluck_event(int e)
{
    if (e < 2) return;
    const uint32_t inc = note_inc(e);
    uint32_t dq = (uint32_t)((1ull << 40) / inc);
    if (dq > (uint32_t)((KS_LEN - 2) << 8)) dq = (KS_LEN - 2) << 8;
    const int32_t period = (int32_t)((dq + 128) >> 8);
    S.ks_d = dq - 128;
    S.ks_g = 65536 - (period * 65536) / S.a_tau;
    if (S.ks_g < 40000) S.ks_g = 40000;
    uint32_t r = S.rng3;
    int32_t lp = 0, sum = 0;
    for (int k = 0; k < KS_LEN; k++) {
        r ^= r << 13; r ^= r >> 17; r ^= r << 5;
        const int32_t nz = (int32_t)(r >> 16) - 32768;
        lp += (nz - lp) >> 1;
        g_ks[k] = (int16_t)lp;
        sum += lp;
    }
    /* zero-mean, or the loop would hold the burst's DC for the whole note */
    const int32_t mean = sum / KS_LEN;
    for (int k = 0; k < KS_LEN; k++) g_ks[k] = (int16_t)clamp32(g_ks[k] - mean, -32768, 32767);
    S.rng3 = r;
    S.ks_w = 0; S.ks_prev = 0; S.ks_dc = 0;
    S.a_side ^= 1;
    if (S.a_side) { S.a_pl = 12000; S.a_pr = 28000; }
    else          { S.a_pl = 28000; S.a_pr = 12000; }
}

static void lead_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.l_env, &ep_lead); return; }
    const uint32_t inc = note_inc(e);
    S.l_base[0] = inc - (inc >> 8);           /* -7 cents */
    S.l_base[1] = inc;
    S.l_base[2] = inc + (inc >> 8);           /* +7 cents */
    S.l_base[3] = inc << 1;                   /* the octave */
    for (int k = 0; k < 4; k++) S.l_inc[k] = S.l_base[k];
    S.l_age = 0;
    S.l_fenv = 65535;
    env_on(&S.l_env);
}

static void lead2_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.m_env, &ep_lead2); return; }
    const uint32_t inc = note_inc(e);
    S.m_inc[0] = inc - (inc >> 9);
    S.m_inc[1] = inc + (inc >> 9);
    env_on(&S.m_env);
}

static void pad_bar(uint32_t bar)
{
    uint8_t c[4];
    song_pad_chord(bar, c);
    if (!c[0]) { env_off(&S.p_env, &ep_pad); memset(S.p_chord, 0, 4); return; }
    if (memcmp(c, S.p_chord, 4) == 0) return;
    memcpy(S.p_chord, c, 4);
    for (int i = 0; i < 4; i++) {
        const uint32_t inc = note_inc(c[i]);
        S.p_inc[i]     = inc - (inc >> 9);    /* left bank, -3.4 cents  */
        S.p_inc[4 + i] = inc + (inc >> 9);    /* right bank, +3.4 cents */
    }
    if (S.p_env.stage == 2) S.p_env.v = S.p_env.v - (S.p_env.v >> 2);
    env_on(&S.p_env);
}

static void glass_choir_bar(uint32_t bar)
{
    uint8_t o[5];
    song_glass_chord(bar, o);
    if (o[0]) for (int i = 0; i < 5; i++) S.o_inc[i] = note_inc(o[i]);

    uint8_t v[4];
    song_choir_chord(bar, v);
    if (!v[0]) { env_off(&S.v_env, &ep_choir); memset(S.v_chord, 0, 4); return; }
    if (memcmp(v, S.v_chord, 4) == 0) return;
    memcpy(S.v_chord, v, 4);
    for (int i = 0; i < 4; i++) {
        const uint32_t inc = note_inc(v[i]);
        S.v_inc[i]     = inc - (inc >> 8);    /* left bank, -7 cents  */
        S.v_inc[4 + i] = inc + (inc >> 8);    /* right bank, +7 cents */
    }
    if (S.v_env.stage == 2) S.v_env.v = S.v_env.v - (S.v_env.v >> 2);
    env_on(&S.v_env);
}

/* ------------------------------------------------------------- control ----- */

static void control_tick(void)
{
    const uint32_t pos = S.pos;
    const uint32_t bar = pos / BAR_SAMPLES;
    const uint32_t bar_frac = ramp(pos - bar * BAR_SAMPLES, 0, BAR_SAMPLES);

    /* --- the sequencer: one row per 16th --- */
    if (pos % STEP_SAMPLES == 0) {
        const uint32_t step = pos / STEP_SAMPLES;
        if ((step & 15) == 0) {
            pad_bar(bar); glass_choir_bar(bar);
            const int dn = song_drone_note(bar);
            if (dn) S.t_note = dn;
        }

        const uint8_t d = song_drums(step);
        if (d & DR_KICK)    trig_kick();
        if (d & DR_BRUSH)   trig_brush();
        if (d & DR_OSHAKER) trig_shaker(1);
        else if (d & DR_SHAKER) trig_shaker(0);
        if (d & DR_WHOOSH)  trig_whoosh();
        if (d & DR_BOOM)    trig_boom();

        bass_event(song_bass(step));
        pluck_event(song_pluck(step));
        lead_event(song_lead(step));
        lead2_event(song_lead2(step));
    }

    /* --- per-bar parameters, interpolated toward the next bar --- */
    const int32_t lcut = mix(song_lead_cut(bar), song_lead_cut(bar + 1), (int32_t)bar_frac);
    const int32_t pcut = mix(song_pad_cut(bar),  song_pad_cut(bar + 1),  (int32_t)bar_frac);
    const int32_t shim = mix(song_shimmer(bar),  song_shimmer(bar + 1),  (int32_t)bar_frac);
    const int32_t pdec = song_pluck_decay(bar);

    /* levels: one-pole per tick. The slow ones (~250 ms) are the things that
     * fade; the leads ~30 ms. */
    S.l_lvl += (song_lead_level(bar)  * 257 - S.l_lvl) >> 4;
    S.l_oct += (song_lead_octave(bar) * 257 - S.l_oct) >> 6;
    S.m_lvl += (song_lead2_level(bar) * 257 - S.m_lvl) >> 4;
    S.p_lvl += (song_pad_level(bar)   * 257 - S.p_lvl) >> 7;
    S.t_lvl += (song_drone_level(bar) * 257 - S.t_lvl) >> 7;
    S.r_lvl += (shim * 257 - S.r_lvl) >> 6;
    S.o_lvl += (song_glass_level(bar) * 257 - S.o_lvl) >> 7;
    S.v_lvl += (song_choir_level(bar) * 257 - S.v_lvl) >> 7;
    S.a_lvl += (song_pluck_level(bar) * 257 - S.a_lvl) >> 5;

    /* the string's decay: tau from 1,000 samples (42 ms) to 26,500 (1.1 s) */
    S.a_tau = 1000 + pdec * 100;

    /* the lead's vibrato: 5 Hz, +-0.4%, arriving over the first 300 ms */
    S.l_lfo += 655;
    S.l_age += 1;
    {
        const int32_t depth = S.l_age >= 150 ? 65536 : S.l_age * 437;
        const int32_t vib = qmul(g_sin[(S.l_lfo >> 6) & 1023], depth);        /* Q15 */
        for (int k = 0; k < 4; k++)
            S.l_inc[k] = S.l_base[k] + (uint32_t)(((int64_t)S.l_base[k] * vib) >> 23);
    }

    S.o_lfo += 118;                                     /* ~0.9 Hz tremolo  */
    S.v_lfo += 655;                                     /* ~5 Hz vibrato    */
    S.w_lfo += 46;                                      /* ~0.35 Hz chorus  */
    S.t_lfo += 590;                                     /* ~4.5 Hz drone    */
    S.r_lfo += 7;                                       /* ~0.05 Hz sweep   */
    S.r_lfo2 += 26;                                     /* ~0.2 Hz breath   */

    /* filter envelopes decay per tick (they only feed coefficients) */
    S.b_fenv = qmul(S.b_fenv, 63352);
    S.l_fenv = qmul(S.l_fenv, 64900);

    /* cutoffs, Hz -> coefficient */
    S.l_fc = hz_f(200u + (uint32_t)lcut * 10u + (uint32_t)((S.l_fenv * 1500) >> 16));
    S.b_fc = hz_f(90u + (uint32_t)((S.b_fenv * 700) >> 16));
    S.p_lfo += 36;                                      /* ~0.27 Hz */
    S.p_fc = hz_f((uint32_t)(250 + pcut * 7 + ((g_sin[(S.p_lfo >> 6) & 1023] * (120 + pcut)) >> 15)));
    S.r_fc = hz_f(800u + (uint32_t)(((g_sin[(S.r_lfo >> 6) & 1023] + 32768) * 2400) >> 16));

    /* master: the trim, and a fade over the last 1.75 bars so the tail is
     * inside the file; squared, and done 2,000 samples early, so the last
     * 83 ms are true silence and nothing is cut */
    int32_t master = G_MASTER;
    const uint32_t fade_from = TOTAL - BAR_SAMPLES - BAR_SAMPLES * 3 / 4;
    if (pos >= fade_from) {
        const int32_t k = 65536 - ramp(pos, fade_from, TOTAL - 2000);
        master = qmul(master, qmul(k, k));
    }
    S.master = master;
    S.mw = (int32_t)((uint32_t)master << 15);

    /* mix gains with solo folded in */
    const unsigned so = S.solo;
    const int32_t breath = 36000 + ((g_sin[(S.r_lfo2 >> 6) & 1023] * 29000) >> 15);   /* 0.55 .. 1.0 */
    S.gw_kick   = (so & SOLO_KICK)    ? GW(G_KICK)   : 0;
    S.gw_brush  = (so & SOLO_BRUSH)   ? GW(G_BRUSH)  : 0;
    S.gw_shaker = (so & SOLO_SHAKER)  ? GW(G_SHAKER) : 0;
    S.gw_whoosh = (so & SOLO_SHAKER)  ? GW(G_WHOOSH) : 0;
    S.gw_boom   = (so & SOLO_BOOM)    ? GW(G_BOOM)   : 0;
    S.gw_bass   = (so & SOLO_BASS)    ? GW(G_BASS)   : 0;
    const int32_t gp = (so & SOLO_PLUCK) ? qmul(G_PLUCK, S.a_lvl) : 0;
    S.gw_pluck_l = GW(qmul(gp, S.a_pl));
    S.gw_pluck_r = GW(qmul(gp, S.a_pr));
    S.g_lead    = (so & SOLO_LEAD)    ? qmul(G_LEAD, S.l_lvl) : 0;
    const int32_t g2 = (so & SOLO_LEAD2) ? qmul(G_LEAD2, S.m_lvl) : 0;
    S.gw_lead2_l = GW(qmul(g2, 49000));
    S.gw_lead2_r = GW(qmul(g2, 62000));
    S.gw_lead2_s = GW(qmul(g2, 28000));
    S.g_pad     = (so & SOLO_PAD)     ? qmul(G_PAD, S.p_lvl) : 0;
    S.gw_drone  = (so & SOLO_DRONE)   ? GW(qmul(G_DRONE, S.t_lvl)) : 0;
    S.gw_shim   = (so & SOLO_SHIMMER) ? GW(qmul(qmul(G_SHIM, S.r_lvl), breath)) : 0;
    S.gw_glass  = (so & SOLO_GLASS)   ? GW(qmul(G_GLASS, S.o_lvl)) : 0;
    S.g_choir   = (so & SOLO_CHOIR)   ? qmul(G_CHOIR, S.v_lvl) : 0;
    S.gw_dly    = (so & SOLO_FX)      ? GW(G_DELAY) : 0;
    S.gw_rv     = (so & SOLO_FX)      ? GW(G_REVERB) : 0;
}

/* -------------------------------------------------------------- render ------ */
/* One pass per voice over a block of CTL_DIV frames, the way a tracker mixer
 * does it: one loop with every voice in it spills everything. */

static void HOT(render_block)(int16_t *out, int n)
{
    int32_t acc[2 * CTL_DIV];      /* the stereo bus                          */
    int32_t send[CTL_DIV];         /* what goes to the delay                  */
    int32_t rvin[CTL_DIV];         /* what goes to the reverb                 */
    int32_t padb[2 * CTL_DIV];     /* the pad's oscillators before the filter */
    int32_t wide[2 * CTL_DIV];     /* everything that goes through the chorus */
    for (int i = 0; i < 2 * n; i++) wide[i] = 0;

    /* --- drums: writes the bus --- */
    {
        uint32_t rng = S.rng, kph = S.k_ph, kinc = S.k_inc, sph = S.s_ph;
        int32_t kenv = S.k_env, kclick = S.k_click, senvn = S.s_envn, senvt = S.s_envt;
        int32_t slo = S.s_f.lo, sband = S.s_f.band;
        int32_t hlp = S.h_lp, henv = S.h_env, cenv = S.c_env, wlp = S.w_lp;
        uint32_t xph = S.x_ph;  int32_t xenv = S.x_env;
        const int32_t hdec = S.h_dec, gx = S.gw_boom;
        const int32_t gk = S.gw_kick, gs = S.gw_brush, gh = S.gw_shaker, gc = S.gw_whoosh;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int32_t nz = (int32_t)(rng >> 16) - 32768;

            if (kinc > 7158279u) kinc -= kinc >> 10;
            kph += kinc;
            const int32_t kick = qmul(g_sin[kph >> 22], kenv) + (qmul(nz, kclick) >> 3);
            kenv = qmul(kenv, 65523);
            kclick -= kclick >> 6;

            slo += qmul(30883, sband);                        /* 1.8 kHz */
            const int32_t shi = nz - slo - qmul(40000, sband);
            sband += qmul(30883, shi);
            sph += 13320000u;
            const int32_t st = (int32_t)(sph >> 16) - 32768;
            const int32_t stri = ((st < 0 ? -st : st) << 1) - 32768;
            const int32_t brush = qmul(sband, senvn) + (qmul(stri, senvt) >> 3);
            senvn = qmul(senvn, 65515);
            senvt = qmul(senvt, 65463);

            const int32_t hp = nz - hlp;
            hlp += (nz - hlp) >> 1;
            wlp += (nz - wlp) >> 2;
            const int32_t shaker = mulhi(qmul(hp, henv), gh);
            const int32_t whoosh = mulhi(qmul(wlp, cenv), gc);
            henv = qmul(henv, hdec);
            cenv = qmul(cenv, 65534);

            xph += 8589934u;                                  /* 48 Hz */
            const int32_t boom = mulhi(qmul(g_sin[xph >> 22], xenv), gx);
            xenv = qmul(xenv, 65533);

            const int32_t c = mulhi(kick, gk) + mulhi(brush, gs) + boom;
            acc[2 * i]     = c + shaker - (shaker >> 2) + whoosh;
            acc[2 * i + 1] = c + shaker + whoosh - (whoosh >> 3);
            rvin[i] = (mulhi(brush, gs) >> 2) + (whoosh >> 1);
        }
        S.rng = rng; S.k_ph = kph; S.k_inc = kinc; S.s_ph = sph;
        S.k_env = kenv; S.k_click = kclick; S.s_envn = senvn; S.s_envt = senvt;
        S.s_f.lo = slo; S.s_f.band = sband;
        S.h_lp = hlp; S.h_env = henv; S.c_env = cenv; S.w_lp = wlp;
        S.x_ph = xph; S.x_env = xenv;
    }

    /* --- bass and the droplets --- */
    {
        uint32_t bph = S.b_ph;
        const uint32_t binc = S.b_inc;
        env_t benv = S.b_env;
        svf_t f = S.b_f;
        int w = S.ks_w;
        const int di = (int)(S.ks_d >> 8), frac = (int)(S.ks_d & 255);
        int32_t prev = S.ks_prev, dc = S.ks_dc, alp = S.a_lp;
        const int32_t kg = S.ks_g;
        const int32_t fc = S.b_fc, gb = S.gw_bass, gl = S.gw_pluck_l, gr = S.gw_pluck_r;
        for (int i = 0; i < n; i++) {
            const int32_t bsaw = (int32_t)(bph >> 16) - 32768;
            const int32_t bsin = g_sin[bph >> 22];
            bph += binc;
            svf(&f, bsin - (bsin >> 2) + (bsaw >> 2), fc, 40000);
            const int32_t b = mulhi(qmul(f.lo, env_run(&benv, &ep_bass)), gb);

            const int32_t s0 = g_ks[(w - di) & (KS_LEN - 1)], s1 = g_ks[(w - di - 1) & (KS_LEN - 1)];
            const int32_t tap = s0 + (((s1 - s0) * frac) >> 8);
            const int32_t y = (tap + prev) >> 1;
            prev = tap;
            dc += (y - dc) >> 8;                                 /* a slow DC tracker in the loop */
            g_ks[w] = (int16_t)clamp32(qmul(y - dc, kg), -32768, 32767);
            w = (w + 1) & (KS_LEN - 1);
            alp += (y - alp) >> 1;
            const int32_t a = alp;

            const int32_t al = mulhi(a, gl), ar = mulhi(a, gr);
            acc[2 * i]     += b + al;
            acc[2 * i + 1] += b + ar;
            send[i] = (al + ar) >> 1;
            rvin[i] += (al + ar) >> 2;
        }
        S.b_ph = bph; S.b_env = benv; S.b_f = f;
        S.ks_w = w; S.ks_prev = prev; S.ks_dc = dc; S.a_lp = alp;
    }

    /* --- lead: three saws and the octave, left / centre / right, two lowpasses;
     * to the chorus, the echo and the hall --- */
    {
        uint32_t p0 = S.l_ph[0], p1 = S.l_ph[1], p2 = S.l_ph[2], p3 = S.l_ph[3];
        const uint32_t i0 = S.l_inc[0], i1 = S.l_inc[1], i2 = S.l_inc[2], i3 = S.l_inc[3];
        env_t env = S.l_env;
        svf_t fl = S.l_fl, fr = S.l_fr;
        const int32_t fc = S.l_fc, g = S.g_lead, oct = S.l_oct;
        for (int i = 0; i < n; i++) {
            const int32_t l0 = (int32_t)(p0 >> 16) - 32768;
            const int32_t l1 = ((int32_t)(p1 >> 16) - 32768) >> 1;
            const int32_t l2 = (int32_t)(p2 >> 16) - 32768;
            const int32_t l3 = qmul(((int32_t)(p3 >> 16) - 32768) >> 2, oct);
            p0 += i0; p1 += i1; p2 += i2; p3 += i3;
            svf(&fl, (l0 + l1 + l3) >> 1, fc, 40000);
            svf(&fr, (l2 + l1 + l3) >> 1, fc, 40000);
            const int32_t lgw = GW(qmul(env_run(&env, &ep_lead), g));
            const int32_t ll = mulhi(fl.lo, lgw), lr = mulhi(fr.lo, lgw);
            wide[2 * i] += ll; wide[2 * i + 1] += lr;
            send[i] += (ll + lr) >> 2;
            rvin[i] += (ll + lr) >> 2;
        }
        S.l_ph[0] = p0; S.l_ph[1] = p1; S.l_ph[2] = p2; S.l_ph[3] = p3;
        S.l_env = env; S.l_fl = fl; S.l_fr = fr;
    }

    /* --- lead2: two 25% pulses, one-pole lowpass, a little to the right --- */
    {
        uint32_t p0 = S.m_ph[0], p1 = S.m_ph[1];
        const uint32_t i0 = S.m_inc[0], i1 = S.m_inc[1];
        env_t env = S.m_env;
        int32_t lp = S.m_lp;
        const int32_t gl = S.gw_lead2_l, gr = S.gw_lead2_r, gs = S.gw_lead2_s;
        for (int i = 0; i < n; i++) {
            const int32_t m0 = (p0 < 0x40000000u) ? 24576 : -8192;
            const int32_t m1 = (p1 < 0x40000000u) ? 24576 : -8192;
            p0 += i0; p1 += i1;
            lp += (((m0 + m1) >> 1) - lp) >> 2;
            const int32_t t = qmul(lp, env_run(&env, &ep_lead2));
            acc[2 * i] += mulhi(t, gl); acc[2 * i + 1] += mulhi(t, gr);
            send[i] += mulhi(t, gs);
            rvin[i] += mulhi(t, gs);
        }
        S.m_ph[0] = p0; S.m_ph[1] = p1; S.m_env = env; S.m_lp = lp;
    }

    /* --- pad: eight saws, four a side, then two slow lowpasses --- */
    for (int side = 0; side < 2; side++) {
        uint32_t *php = S.p_ph + 4 * side;
        const uint32_t *pinc = S.p_inc + 4 * side;
        uint32_t q0 = php[0], q1 = php[1], q2 = php[2], q3 = php[3];
        const uint32_t j0 = pinc[0], j1 = pinc[1], j2 = pinc[2], j3 = pinc[3];
        int32_t *pb = padb + side;
        for (int i = 0; i < n; i++) {
            pb[2 * i] = (int32_t)(q0 >> 16) + (int32_t)(q1 >> 16)
                      + (int32_t)(q2 >> 16) + (int32_t)(q3 >> 16) - 4 * 32768;
            q0 += j0; q1 += j1; q2 += j2; q3 += j3;
        }
        php[0] = q0; php[1] = q1; php[2] = q2; php[3] = q3;
    }
    {
        env_t env = S.p_env;
        svf_t fl = S.p_fl, fr = S.p_fr;
        const int32_t fc = S.p_fc, g = S.g_pad;
        for (int i = 0; i < n; i++) {
            svf(&fl, padb[2 * i] >> 2, fc, 52000);
            svf(&fr, padb[2 * i + 1] >> 2, fc, 52000);
            const int32_t pgw = GW(qmul(env_run(&env, &ep_pad), g));
            wide[2 * i] += mulhi(fl.lo, pgw); wide[2 * i + 1] += mulhi(fr.lo, pgw);
        }
        S.p_env = env; S.p_fl = fl; S.p_fr = fr;
    }

    /* --- drone: two triangles an octave apart, vibrato, one-pole lowpass --- */
    {
        uint32_t p0 = S.t_ph[0], p1 = S.t_ph[1];
        const uint32_t base0 = note_inc(S.t_note), base1 = note_inc(S.t_note + 12);
        const int32_t vib = g_sin[(S.t_lfo >> 6) & 1023];          /* Q15 */
        const uint32_t i0 = base0 + (uint32_t)(((int64_t)base0 * vib) >> 22);
        const uint32_t i1 = base1 + (uint32_t)(((int64_t)base1 * vib) >> 22);
        int32_t lp = S.t_lp;
        const int32_t g = S.gw_drone;
        for (int i = 0; i < n; i++) {
            const int32_t s0 = (int32_t)(p0 >> 16) - 32768;
            const int32_t s1 = (int32_t)(p1 >> 16) - 32768;
            p0 += i0; p1 += i1;
            const int32_t t0 = ((s0 < 0 ? -s0 : s0) << 1) - 32768;
            const int32_t t1 = ((s1 < 0 ? -s1 : s1) << 1) - 32768;
            lp += ((t0 + (t1 >> 1)) - lp) >> 3;
            const int32_t d = mulhi(lp, g);
            acc[2 * i] += d; acc[2 * i + 1] += d;
            rvin[i] += d >> 1;
        }
        S.t_ph[0] = p0; S.t_ph[1] = p1; S.t_lp = lp;
    }

    /* --- glass: five notes of pure harmonics, tremolo in opposite phase L/R.
     * Skipped while its gain is zero, which is a function of state and so
     * is safe for the pull model --- */
    {
        const int32_t g = S.gw_glass;
        if (g) {
            uint32_t ph[5], inc[5];
            for (int k = 0; k < 5; k++) { ph[k] = S.o_ph[k]; inc[k] = S.o_inc[k]; }
            const int32_t trem = g_sin[(S.o_lfo >> 6) & 1023];
            const int32_t gl = g + (int32_t)(((int64_t)g * trem) >> 18);   /* +-12.5% */
            const int32_t gr = g - (int32_t)(((int64_t)g * trem) >> 18);
            for (int i = 0; i < n; i++) {
                int32_t sum = 0;
                for (int k = 0; k < 5; k++) {
                    ph[k] += inc[k];
                    const uint32_t p = ph[k];
                    sum += g_sin[p >> 22] * 8 + g_sin[(p * 2u) >> 22] * 4
                         + g_sin[(p * 3u) >> 22] * 1 + g_sin[(p * 4u) >> 22] * 2;
                }
                sum >>= 4;
                acc[2 * i] += mulhi(sum, gl); acc[2 * i + 1] += mulhi(sum, gr);
                rvin[i] += mulhi(sum, g) >> 1;
            }
            for (int k = 0; k < 5; k++) S.o_ph[k] = ph[k];
        }
    }

    /* --- choir: four notes a side, two saws each, into three formant
     * band-passes; the envelope runs once and both sides read it --- */
    {
        const int32_t g = S.g_choir;
        env_t env = S.v_env;
        if (g && env.stage) {
            int32_t gwv[CTL_DIV];
            for (int i = 0; i < n; i++) gwv[i] = GW(qmul(env_run(&env, &ep_choir), g));
            const int32_t vib = g_sin[(S.v_lfo >> 6) & 1023];
            for (int side = 0; side < 2; side++) {
                uint32_t *php = S.v_ph + 4 * side;
                const uint32_t *pinc = S.v_inc + 4 * side;
                uint32_t q0 = php[0], q1 = php[1], q2 = php[2], q3 = php[3];
                const uint32_t j0 = pinc[0] + (uint32_t)(((int64_t)pinc[0] * vib) >> 23);
                const uint32_t j1 = pinc[1] + (uint32_t)(((int64_t)pinc[1] * vib) >> 23);
                const uint32_t j2 = pinc[2] + (uint32_t)(((int64_t)pinc[2] * vib) >> 23);
                const uint32_t j3 = pinc[3] + (uint32_t)(((int64_t)pinc[3] * vib) >> 23);
                svf_t f0 = S.v_f[side][0], f1 = S.v_f[side][1], f2 = S.v_f[side][2];
                for (int i = 0; i < n; i++) {
                    const int32_t x = ((int32_t)(q0 >> 16) + (int32_t)(q1 >> 16)
                                     + (int32_t)(q2 >> 16) + (int32_t)(q3 >> 16) - 4 * 32768) >> 2;
                    q0 += j0; q1 += j1; q2 += j2; q3 += j3;
                    svf(&f0, x, 6001, 20000);                /*  350 Hz */
                    svf(&f1, x, 13701, 16000);               /*  800 Hz */
                    svf(&f2, x, 40504, 16000);               /* 2400 Hz */
                    const int32_t y = f0.band + (f1.band >> 1) + (f2.band >> 3);
                    wide[2 * i + side] += mulhi(y, gwv[i]);
                }
                php[0] = q0; php[1] = q1; php[2] = q2; php[3] = q3;
                S.v_f[side][0] = f0; S.v_f[side][1] = f1; S.v_f[side][2] = f2;
            }
        }
        S.v_env = env;
    }

    /* --- shimmer: band-passed noise, drifting; its own noise generator --- */
    {
        uint32_t rng = S.rng2;
        svf_t f = S.r_f;
        const int32_t fc = S.r_fc, g = S.gw_shim;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            svf(&f, (int32_t)(rng >> 16) - 32768, fc, 14000);
            const int32_t r = mulhi(f.band, g);
            wide[2 * i] += r; wide[2 * i + 1] += r;
        }
        S.rng2 = rng; S.r_f = f;
    }

    /* --- chorus on the wide bus: two taps modulated in opposite phase,
     * linearly interpolated; dry plus 5/8 wet to the bus, and to the hall --- */
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
                rvin[i] += (x + wet) >> 1;
            }
            w = (w + 1) & (CHORUS_LEN - 1);
        }
        S.w_w = w;
    }

    /* --- delay: one 3/8 line; the 1/4 tap goes right, the end goes left --- */
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

    /* --- master: trim, knee, peak, clamp, hash --- */
    {
        int32_t peak = S.peak;
        const int32_t mw = S.mw;
        const int past_end = S.pos >= TOTAL;
        uint32_t h = g_hash;
        int mark = S.mark_left;
        for (int i = 0; i < n; i++) {
            int32_t L = acc[2 * i], R = acc[2 * i + 1];
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
        S.peak = peak;
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
    memset(g_ks, 0, sizeof g_ks);
    memset(g_ring, 0, sizeof g_ring);
    S.solo = solo;
    S.rng  = 0x1BADF00Du;
    S.rng2 = 0x5EEDF00Du;
    S.rng3 = 0x0C0FFEE5u;
    S.k_inc = 7158279u;
    S.h_dec = 65450;
    S.a_tau = 12000;
    S.ks_d = 36 << 8; S.ks_g = 60000;
    S.master = G_MASTER;
    S.mw = (int32_t)((uint32_t)G_MASTER << 15);
    S.mark_left = SAMPLE_RATE;
    S.b_inc = note_inc(40);
    S.a_pl = S.a_pr = 20000;
    S.t_note = 40;
    for (int i = 0; i < 4; i++) S.l_inc[i] = S.l_base[i] = note_inc(76);
    for (int i = 0; i < 5; i++) S.o_inc[i] = note_inc(52);
    for (int i = 0; i < 8; i++) S.v_inc[i] = note_inc(76);
    S.m_inc[0] = S.m_inc[1] = note_inc(64);
    for (int i = 0; i < 8; i++) S.p_inc[i] = note_inc(64);
    g_hash = 2166136261u;
    hash_publish(0);
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
}

uint32_t synth_position(void) { return S.out; }
int32_t  synth_peak(void)     { return S.peak; }
