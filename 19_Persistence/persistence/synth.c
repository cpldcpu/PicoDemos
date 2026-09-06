/* PERSISTENCE -- the synth.
 *
 * A small tracker synth in integer arithmetic, playing song.c. See synth.h
 * for the host/device contract (stereo, 24 kHz, pull model, bit-identical on
 * both targets) and PLANNING.md section 6 for why this has to be a tune.
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
 * Bhaskara's rational approximation evaluated in int64. The alternative --
 * building tables with libm at start-up -- risks an x86 sinf() and a newlib
 * sinf() disagreeing by one ulp in one entry, which would break the
 * host/device hash for a reason nobody could hear.
 *
 * CONTROL RATE. Every 50 samples (480 Hz): the sequencer, filter coefficients
 * and level smoothing. 2500 samples per 16th is exactly 50 ticks, so every
 * note lands on a tick boundary and nothing jitters. Envelopes run per sample
 * because their multiply is one instruction and it keeps the attacks clean.
 *
 * PULL MODEL. synth_render() only ever advances the absolute sample counter;
 * a control tick fires when that counter says so, never when a block starts.
 * Rendering 1, 32 or 1024 frames at a time produces the same bytes, and
 * tools/song_check.py asserts it.
 *
 * ------------------------------------------------------------------- voicing --
 *
 *   kick    pitched sine, 160 Hz sweeping to 45 Hz, with a noise click
 *   snare   band-passed noise plus a short triangle body
 *   hats    high-passed noise, closed and open; the crash is the same noise
 *           with a long tail
 *   bass    saw + square through a lowpass with an envelope on the cutoff
 *   arp     one saw, plucked: amplitude and brightness decay together
 *   lead    three detuned saws, spread hard left / centre / right into two
 *           lowpasses, with a filter envelope; sends to the delay
 *   lead2   two detuned 25% pulses, softer, for the counter-melody
 *   pad     eight detuned saws, four a side, through two slow lowpasses
 *   riser   band-passed noise whose centre sweeps up over eight bars
 *   delay   one 3/16 line with a 2/16 tap: echoes alternate right, left,
 *           right, left on the 16th grid
 *
 * ---------------------------------------------------------------------- RAM --
 *
 * Measured (arm-none-eabi-size, -O2, cortex-m33): synth.o has 17,560 bytes
 * of .bss and 4 of .data; song.o has none -- its tables are const and live
 * in flash, read once per 16th. Of the 17.5 KB: the delay line is 15,000
 * (7500 x int16), the sine table 2,048, the note table 48, and the state
 * struct about 450. The render block also uses ~1 KB of stack for its
 * accumulators. Under the 24 KB the plan allowed.
 *
 * -------------------------------------------------------------------- cycles --
 *
 * Measured from the M33 assembly rather than guessed: the executed path is
 * about 540 instructions per stereo frame (drums 117, bass + arp 79, lead 79,
 * lead2 51, pad 86, riser 26, delay 37, master 65), of which ~100 are
 * multiplies and ~170 loads/stores. On the single-issue M33 that is roughly
 * 570-620 cycles a frame, 14-15 M cycles a second, 4.5-5% of core 0 at
 * 300 MHz -- above the 400-cycle target in the brief. The render is one pass
 * per voice over a 50-frame block (see render_block) precisely because the
 * one-loop version was 900 instructions; the levers that remain all change
 * the sound: the pad at half rate (-45), one filter on the lead instead of
 * two (-15), a one-pole instead of the state-variable filter on the pad
 * (-20). The cost is constant: every voice runs every frame whether or not
 * it is sounding, so the audio pump never has a surprising frame.
 */

#include "synth.h"
#include "song.h"

#include <string.h>

#define CTL_DIV        50                      /* samples per control tick    */
#define STEP_SAMPLES   (PV_SPB / 4)            /* 2500                        */
#define BAR_SAMPLES    (PV_SPB * 4)            /* 40000                       */

/* --------------------------------------------------------------- tables ---- */

static int16_t  g_sin[1024];                   /* Q15, one full cycle         */
static uint32_t g_oct8[12];                    /* phase inc for MIDI 108..119 */

/* 440 * 2^((n-69)/12) * 2^32 / 24000 for n = 108..119, rounded. Computed once
 * in Python and pasted, so this file needs no pow(). */
static const uint32_t k_oct8[12] = {
     749115498u,  793660223u,  840853716u,  890853480u,
     943826385u,  999949222u, 1059409297u, 1122405052u,
    1189146729u, 1259857073u, 1334772074u, 1414141751u,
};

static void build_tables(void)
{
    /* sin(x) ~ 16x(pi-x) / (5pi^2 - 4x(pi-x)) on [0, pi], Bhaskara I. With
     * pi = 512 units and u = x(512-x) this is 4u / (327680 - u); max error
     * 0.0016, and it is integer arithmetic so both targets agree exactly. */
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

static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi)
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
 * small-angle approximation, Q16. 411775 is 2*pi in Q16. Reads a few percent
 * high at 4 kHz, which is as far as the filter is ever pushed. */
static inline int32_t hz_f(uint32_t hz)
{
    if (hz > 4000) hz = 4000;
    return (int32_t)((uint64_t)hz * 411775u / PV_RATE);
}


/* High word of a 32x32 product: one smull on the M33, the high register used
 * directly. With `w` a gain pre-shifted to Q32 (GW below) this is a Q16
 * multiply in one instruction instead of the three qmul() costs. Gains that
 * go through it must be at most 32767 (so `g << 16` fits). */
static inline int32_t mulhi(int32_t a, int32_t w)
{
    return (int32_t)(((int64_t)a * w) >> 32);
}
#define GW(g) ((int32_t)((uint32_t)(g) << 16))

/* ---------------------------------------------------------------- filter ---- */
/* Chamberlin two-pole. No clamps: hz_f() caps f at 1.05 and the smallest
 * damping used is 0.3, so f + q < 1.4 and the filter cannot run away; the
 * clamps the first version carried cost a dozen instructions per filter. */
typedef struct { int32_t lo, band; } svf_t;

static inline void svf(svf_t *s, int32_t in, int32_t f, int32_t q)
{
    s->lo += qmul(f, s->band);
    const int32_t hi = in - s->lo - qmul(q, s->band);
    s->band += qmul(f, hi);
}

/* -------------------------------------------------------------- envelope ---- */
/* ADSR in Q16, per sample. The attack is the only branch; decay and release
 * are the same one-pole toward a target (the sustain, then zero) so the
 * common path is one compare, one multiply, two adds. Full scale is 65535,
 * not 65536, so that an envelope times a gain always fits a gain word. */
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
static const envp_t ep_bass  = { 65536 / 24,    65520, 40000, 65000 };
static const envp_t ep_lead  = { 65536 / 48,    65525, 46000, 65481 };
static const envp_t ep_lead2 = { 65536 / 720,   65530, 52000, 65514 };
static const envp_t ep_pad   = { 65536 / 12000, 65535, 65535, 65528 };

/* ------------------------------------------------------------------ hash ---- */
/* FNV-1a over the emitted int16 stream (each int16 one step), latched once a
 * second under a seqlock so the telemetry core reads a consistent pair. */
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

#define DLY_LEN   7500                 /* 3/16 at 144 BPM                     */
#define DLY_TAP   5000                 /* 2/16, the right-hand tap            */
#define DLY_FB    28000                /* 0.43 feedback                       */

static int16_t g_dly[DLY_LEN];

static struct {
    uint32_t pos;
    int      ctl_left;
    int      mark_left;
    uint32_t rng, rng2;
    int32_t  peak;
    unsigned solo;

    /* kick */
    uint32_t k_ph, k_inc;  int32_t k_env, k_click;
    /* snare */
    uint32_t s_ph;  int32_t s_envn, s_envt;  svf_t s_f;
    /* hats + crash */
    int32_t  h_env, h_dec, h_lp, c_env;
    /* bass */
    uint32_t b_ph, b_inc;  env_t b_env;  int32_t b_fenv;  svf_t b_f;  int32_t b_fc;
    /* arp */
    uint32_t a_ph, a_inc;  int32_t a_env, a_lp, a_pl, a_pr;
    /* lead */
    uint32_t l_ph[3], l_inc[3];  env_t l_env;  int32_t l_fenv;  svf_t l_fl, l_fr;
    int32_t  l_fc, l_lvl;
    /* lead2 */
    uint32_t m_ph[2], m_inc[2];  env_t m_env;  int32_t m_lp, m_lvl;
    /* pad */
    uint32_t p_ph[8], p_inc[8];  env_t p_env;  svf_t p_fl, p_fr;  int32_t p_fc, p_lvl;
    uint8_t  p_chord[4];  uint32_t p_lfo;
    /* riser */
    svf_t    r_f;  int32_t r_fc, r_lvl;
    /* delay */
    int      d_w;  int32_t d_lp;
    /* master */
    int32_t  master;

    /* per-tick mix gains, with solo and song levels folded in. gw_* are
     * Q32 gain words for mulhi(); g_lead / g_pad are Q16 <= 32767 because
     * they are multiplied by an envelope first. */
    int32_t  gw_kick, gw_snare, gw_hat, gw_crash, gw_bass, gw_arp_l, gw_arp_r;
    int32_t  g_lead, gw_lead2_l, gw_lead2_r, gw_lead2_s, g_pad, gw_riser;
    int32_t  gw_dly, mw;
} S;

void synth_solo(unsigned mask) { S.solo = mask; }

/* Levels, all <= 32767 so they fit a gain word. These are what the mix was
 * balanced to, read off tools/song_check.py solo renders: kick 13k, bass 10k,
 * lead 11k, snare 10k, pad 7k, lead2 7k, arp 6k, riser 7k, hats 4.5k; the
 * full mix peaks at 90% of scale after the trim and the knee. */
#define G_KICK   32767
#define G_SNARE  18000
#define G_HAT    3000
#define G_CRASH  5000
#define G_BASS   16000
#define G_ARP    42000     /* folded with the pan (max 28000/65536) -> <= 17944 */
#define G_LEAD   24000
#define G_LEAD2  29000
#define G_PAD    16000
#define G_RISER  5500
#define G_DELAY  24000
#define G_MASTER 47000     /* 0.72: measured full-mix peak 50959 -> 36.5k, then the knee */

/* Soft knee above 75% of scale: out = k + 8191 e / (e + 8191), which starts
 * at slope 1 at the knee and never reaches 32767. Kick, bass and lead land
 * on the same sample a few hundred times in the piece; this rounds those
 * peaks instead of chopping them. Costs a division only above the knee. */
static inline int32_t soft_clip(int32_t v)
{
    const int32_t a = v < 0 ? -v : v;
    if (a <= 24576) return v;
    const int32_t e = a - 24576;
    const int32_t o = 24576 + (8191 * e) / (e + 8191);
    return v < 0 ? -o : o;
}

/* --------------------------------------------------------------- triggers --- */

static void trig_kick(void)
{
    S.k_ph = 0; S.k_inc = 28633115u;          /* 160 Hz */
    S.k_env = 65535; S.k_click = 65535;
}

static void trig_snare(void)
{
    S.s_ph = 0; S.s_envn = 65535; S.s_envt = 65535;
}

static void trig_hat(int open)
{
    S.h_env = 65535; S.h_dec = open ? 65509 : 65349;
}

static void trig_crash(void) { S.c_env = 65535; }

static void bass_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.b_env, &ep_bass); return; }
    S.b_inc = note_inc(e);
    S.b_fenv = 65535;
    env_on(&S.b_env);
}

static void arp_event(int e, uint32_t step)
{
    if (e == 0) return;
    if (e == SONG_OFF) { S.a_env = 0; return; }
    S.a_inc = note_inc(e);
    S.a_ph = 0;
    S.a_env = 65535;
    if (step & 1) { S.a_pl = 12000; S.a_pr = 28000; }
    else          { S.a_pl = 28000; S.a_pr = 12000; }
}

static void lead_event(int e)
{
    if (e == 0) return;
    if (e == SONG_OFF) { env_off(&S.l_env, &ep_lead); return; }
    const uint32_t inc = note_inc(e);
    S.l_inc[0] = inc - (inc >> 8);            /* -7 cents */
    S.l_inc[1] = inc;
    S.l_inc[2] = inc + (inc >> 8);            /* +7 cents */
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
    /* Re-attack from three quarters so a chord change breathes instead of
     * simply being a different set of frequencies from one sample on. */
    if (S.p_env.stage == 2) S.p_env.v = S.p_env.v - (S.p_env.v >> 2);
    env_on(&S.p_env);
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
        if ((step & 15) == 0) pad_bar(bar);

        const uint8_t d = song_drums(step);
        if (d & DR_KICK)  trig_kick();
        if (d & DR_SNARE) trig_snare();
        if (d & DR_OHAT)  trig_hat(1);
        else if (d & DR_HAT) trig_hat(0);
        if (d & DR_CRASH) trig_crash();

        bass_event(song_bass(step));
        arp_event(song_arp(step), step);
        lead_event(song_lead(step));
        lead2_event(song_lead2(step));
    }

    /* --- per-bar parameters, interpolated toward the next bar --- */
    const int32_t lcut = mix(song_lead_cut(bar), song_lead_cut(bar + 1), (int32_t)bar_frac);
    const int32_t rise = mix(song_riser(bar),    song_riser(bar + 1),    (int32_t)bar_frac);

    /* levels: one-pole per tick, ~30 ms */
    const int32_t l_t = song_lead_level(bar)  * 257;
    const int32_t m_t = song_lead2_level(bar) * 257;
    const int32_t p_t = song_pad_level(bar)   * 257;
    const int32_t r_t = rise * 257;
    S.l_lvl += (l_t - S.l_lvl) >> 4;
    S.m_lvl += (m_t - S.m_lvl) >> 4;
    S.p_lvl += (p_t - S.p_lvl) >> 4;
    S.r_lvl += (r_t - S.r_lvl) >> 4;

    /* filter envelopes decay per tick (they only feed coefficients): bass
     * 30 ticks, lead 80 ticks */
    S.b_fenv = qmul(S.b_fenv, 63352);
    S.l_fenv = qmul(S.l_fenv, 64717);

    /* cutoffs, Hz -> coefficient */
    S.l_fc = hz_f(250u + (uint32_t)lcut * 12u + (uint32_t)((S.l_fenv * 2200) >> 16));
    S.b_fc = hz_f(110u + (uint32_t)((S.b_fenv * 1600) >> 16));
    S.r_fc = hz_f(300u + (uint32_t)rise * 16u);
    S.p_lfo += 40;                                      /* ~0.29 Hz */
    S.p_fc = hz_f((uint32_t)(760 + ((g_sin[(S.p_lfo >> 6) & 1023] * 220) >> 15)));

    /* master: the trim, and a fade over the last bar so the final chord's
     * tail is inside the file and not cut by it; zero from the end on */
    int32_t master = G_MASTER;
    if (pos >= PV_TOTAL_SAMPLES - BAR_SAMPLES)
        master = qmul(master, 65536 - ramp(pos, PV_TOTAL_SAMPLES - BAR_SAMPLES, PV_TOTAL_SAMPLES));
    S.master = master;
    S.mw = (int32_t)((uint32_t)master << 15);

    /* mix gains with solo folded in */
    const unsigned so = S.solo;
    S.gw_kick  = (so & SOLO_KICK)  ? GW(G_KICK)  : 0;
    S.gw_snare = (so & SOLO_SNARE) ? GW(G_SNARE) : 0;
    S.gw_hat   = (so & SOLO_HAT)   ? GW(G_HAT)   : 0;
    S.gw_crash = (so & SOLO_HAT)   ? GW(G_CRASH) : 0;
    S.gw_bass  = (so & SOLO_BASS)  ? GW(G_BASS)  : 0;
    S.gw_arp_l = (so & SOLO_ARP)   ? GW(qmul(G_ARP, S.a_pl)) : 0;
    S.gw_arp_r = (so & SOLO_ARP)   ? GW(qmul(G_ARP, S.a_pr)) : 0;
    S.g_lead   = (so & SOLO_LEAD)  ? qmul(G_LEAD, S.l_lvl) : 0;
    const int32_t g2 = (so & SOLO_LEAD2) ? qmul(G_LEAD2, S.m_lvl) : 0;
    S.gw_lead2_l = GW(qmul(g2, 49000));
    S.gw_lead2_r = GW(qmul(g2, 62000));
    S.gw_lead2_s = GW(qmul(g2, 28000));
    S.g_pad    = (so & SOLO_PAD)   ? qmul(G_PAD, S.p_lvl) : 0;
    S.gw_riser = (so & SOLO_FX)    ? GW(qmul(G_RISER, S.r_lvl)) : 0;
    S.gw_dly   = (so & SOLO_FX)    ? GW(G_DELAY) : 0;
}

/* -------------------------------------------------------------- render ------ */
/* One pass per voice over a block of at most CTL_DIV frames, the way a
 * tracker mixer does it. The first version was one loop with every voice in
 * it; with eighty words of live state GCC spilled most of them every sample
 * and the loop was 900 instructions. A pass has a handful of state words,
 * which stay in registers, and pays three or four instructions a sample to
 * go through the accumulator instead. */

static void PV_HOT(render_block)(int16_t *out, int n)
{
    int32_t acc[2 * CTL_DIV];      /* the stereo bus                          */
    int32_t send[CTL_DIV];         /* what goes to the delay                  */
    int32_t padb[2 * CTL_DIV];     /* the pad's oscillators before the filter */

    /* --- drums: writes the bus --- */
    {
        uint32_t rng = S.rng, kph = S.k_ph, kinc = S.k_inc, sph = S.s_ph;
        int32_t kenv = S.k_env, kclick = S.k_click, senvn = S.s_envn, senvt = S.s_envt;
        int32_t slo = S.s_f.lo, sband = S.s_f.band;
        int32_t hlp = S.h_lp, henv = S.h_env, cenv = S.c_env;
        const int32_t hdec = S.h_dec;
        const int32_t gk = S.gw_kick, gs = S.gw_snare, gh = S.gw_hat, gc = S.gw_crash;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int32_t nz = (int32_t)(rng >> 16) - 32768;

            /* kick: sine with a pitch drop, plus a two-millisecond click */
            if (kinc > 8053064u) kinc -= kinc >> 11;
            kph += kinc;
            const int32_t kick = qmul(g_sin[kph >> 22], kenv) + (qmul(nz, kclick) >> 2);
            kenv = qmul(kenv, 65523);
            kclick -= kclick >> 6;

            /* snare: band-passed noise (~2.2 kHz) and a triangle body (~185 Hz) */
            slo += qmul(37745, sband);
            const int32_t shi = nz - slo - qmul(52000, sband);
            sband += qmul(37745, shi);
            sph += 13320000u;
            const int32_t st = (int32_t)(sph >> 16) - 32768;
            const int32_t stri = ((st < 0 ? -st : st) << 1) - 32768;
            const int32_t snare = qmul(sband, senvn) + (qmul(stri, senvt) >> 1);
            senvn = qmul(senvn, 65500);
            senvt = qmul(senvt, 65463);

            /* hats and the crash share one highpass */
            const int32_t hp = nz - hlp;
            hlp += (nz - hlp) >> 1;
            const int32_t hats = mulhi(qmul(hp, henv), gh) + mulhi(qmul(hp, cenv), gc);
            henv = qmul(henv, hdec);
            cenv = qmul(cenv, 65532);

            const int32_t c = mulhi(kick, gk) + mulhi(snare, gs);
            acc[2 * i]     = c + hats - (hats >> 2);
            acc[2 * i + 1] = c + hats;
        }
        S.rng = rng; S.k_ph = kph; S.k_inc = kinc; S.s_ph = sph;
        S.k_env = kenv; S.k_click = kclick; S.s_envn = senvn; S.s_envt = senvt;
        S.s_f.lo = slo; S.s_f.band = sband;
        S.h_lp = hlp; S.h_env = henv; S.c_env = cenv;
    }

    /* --- bass (saw + square, lowpass with envelope) and arp (a plucked
     * saw whose brightness follows its amplitude), one pass: together they
     * still fit in registers, and it saves a trip through the bus --- */
    {
        uint32_t bph = S.b_ph, aph = S.a_ph;
        const uint32_t binc = S.b_inc, ainc = S.a_inc;
        env_t benv = S.b_env;
        svf_t f = S.b_f;
        int32_t aenv = S.a_env, alp = S.a_lp;
        const int32_t fc = S.b_fc, gb = S.gw_bass, gl = S.gw_arp_l, gr = S.gw_arp_r;
        for (int i = 0; i < n; i++) {
            const int32_t bsaw = (int32_t)(bph >> 16) - 32768;
            const int32_t bsq  = (bph & 0x80000000u) ? 16384 : -16384;
            bph += binc;
            svf(&f, (bsaw >> 1) + bsq, fc, 45000);
            const int32_t b = mulhi(qmul(f.lo, env_run(&benv, &ep_bass)), gb);

            const int32_t asaw = (int32_t)(aph >> 16) - 32768;
            aph += ainc;
            alp += (asaw - alp) >> 2;
            const int32_t a = qmul(alp + qmul(asaw - alp, aenv), aenv);
            aenv = qmul(aenv, 65492);

            acc[2 * i]     += b + mulhi(a, gl);
            acc[2 * i + 1] += b + mulhi(a, gr);
        }
        S.b_ph = bph; S.b_env = benv; S.b_f = f;
        S.a_ph = aph; S.a_env = aenv; S.a_lp = alp;
    }

    /* --- lead: three saws, left / centre / right, two lowpasses --- */
    {
        uint32_t p0 = S.l_ph[0], p1 = S.l_ph[1], p2 = S.l_ph[2];
        const uint32_t i0 = S.l_inc[0], i1 = S.l_inc[1], i2 = S.l_inc[2];
        env_t env = S.l_env;
        svf_t fl = S.l_fl, fr = S.l_fr;
        const int32_t fc = S.l_fc, g = S.g_lead;
        for (int i = 0; i < n; i++) {
            const int32_t l0 = (int32_t)(p0 >> 16) - 32768;
            const int32_t l1 = ((int32_t)(p1 >> 16) - 32768) >> 1;
            const int32_t l2 = (int32_t)(p2 >> 16) - 32768;
            p0 += i0; p1 += i1; p2 += i2;
            svf(&fl, (l0 + l1) >> 1, fc, 40000);
            svf(&fr, (l2 + l1) >> 1, fc, 40000);
            const int32_t lgw = GW(qmul(env_run(&env, &ep_lead), g));
            const int32_t ll = mulhi(fl.lo, lgw), lr = mulhi(fr.lo, lgw);
            acc[2 * i] += ll; acc[2 * i + 1] += lr;
            send[i] = (ll + lr) >> 1;
        }
        S.l_ph[0] = p0; S.l_ph[1] = p1; S.l_ph[2] = p2;
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
            lp += (((m0 + m1) >> 1) - lp) >> 1;
            const int32_t t = qmul(lp, env_run(&env, &ep_lead2));
            acc[2 * i] += mulhi(t, gl); acc[2 * i + 1] += mulhi(t, gr);
            send[i] += mulhi(t, gs);
        }
        S.m_ph[0] = p0; S.m_ph[1] = p1; S.m_env = env; S.m_lp = lp;
    }

    /* --- pad: eight saws, four a side (each note twice, detuned across
     * the pair), summed a side at a time, then two slow lowpasses --- */
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
            acc[2 * i] += mulhi(fl.lo, pgw); acc[2 * i + 1] += mulhi(fr.lo, pgw);
        }
        S.p_env = env; S.p_fl = fl; S.p_fr = fr;
    }

    /* --- riser: band-passed noise, sweeping; its own noise generator --- */
    {
        uint32_t rng = S.rng2;
        svf_t f = S.r_f;
        const int32_t fc = S.r_fc, g = S.gw_riser;
        for (int i = 0; i < n; i++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            svf(&f, (int32_t)(rng >> 16) - 32768, fc, 20000);
            const int32_t r = mulhi(f.band, g);
            acc[2 * i] += r; acc[2 * i + 1] += r;
        }
        S.rng2 = rng; S.r_f = f;
    }

    /* --- delay: one 3/16 line; the 2/16 tap goes right and the line's end
     * goes left, so the echoes ping-pong on the 16th grid --- */
    {
        int w = S.d_w;
        int32_t lp = S.d_lp;
        const int32_t g = S.gw_dly, fb = GW(DLY_FB);
        for (int i = 0; i < n; i++) {
            int r5 = w + (DLY_LEN - DLY_TAP); if (r5 >= DLY_LEN) r5 -= DLY_LEN;
            const int32_t tap_r = g_dly[r5];
            const int32_t tap_l = g_dly[w];
            lp += (tap_l - lp) >> 1;
            g_dly[w] = (int16_t)clampi(send[i] + mulhi(lp, fb), -32768, 32767);
            if (++w >= DLY_LEN) w = 0;
            acc[2 * i] += mulhi(tap_l, g); acc[2 * i + 1] += mulhi(tap_r, g);
        }
        S.d_w = w; S.d_lp = lp;
    }

    /* --- master: DC block, trim, knee, peak, clamp, hash --- */
    {
        int32_t peak = S.peak;
        const int32_t mw = S.mw;
        const int past_end = S.pos >= PV_TOTAL_SAMPLES;
        uint32_t h = g_hash;
        int mark = S.mark_left;
        for (int i = 0; i < n; i++) {
            /* No DC blocker. The first version had one, and it was the
             * only source of DC in the mix: (x - s) >> 9 with a truncating
             * shift has a 512-LSB dead zone and parked a 254-LSB offset
             * after the first crash. Every oscillator here is zero-mean by
             * construction (the 25% pulse is +24576/-8192), and
             * tools/song_check.py asserts the per-bar mean stays under 64. */
            int32_t L = acc[2 * i], R = acc[2 * i + 1];
            L = soft_clip(mulhi(L << 1, mw));
            R = soft_clip(mulhi(R << 1, mw));
            if (past_end) { L = 0; R = 0; }

            const int32_t al = L < 0 ? -L : L, ar = R < 0 ? -R : R;
            if (al > peak) peak = al;
            if (ar > peak) peak = ar;

            const int16_t sl = (int16_t)clampi(L, -32768, 32767);
            const int16_t sr = (int16_t)clampi(R, -32768, 32767);
            out[2 * i] = sl; out[2 * i + 1] = sr;

            /* hash the clamped output: the only thing both targets must
             * agree on */
            h = (h ^ (uint16_t)sl) * 16777619u;
            h = (h ^ (uint16_t)sr) * 16777619u;
            if (--mark <= 0) {
                mark = PV_RATE;
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

void PV_HOT(synth_render)(int16_t *out, int frames)
{
    while (frames > 0) {
        if (S.ctl_left == 0) { control_tick(); S.ctl_left = CTL_DIV; }
        const int k = frames < S.ctl_left ? frames : S.ctl_left;
        render_block(out, k);
        out += 2 * k; frames -= k; S.ctl_left -= k;
    }
}

/* --------------------------------------------------------------- lifecycle -- */

void synth_init(void)
{
    build_tables();
    S.solo = SOLO_ALL;
    synth_reset();
}

void synth_reset(void)
{
    const unsigned solo = S.solo ? S.solo : SOLO_ALL;
    memset(&S, 0, sizeof S);
    memset(g_dly, 0, sizeof g_dly);
    S.solo = solo;
    S.rng  = 0x1BADF00Du;
    S.rng2 = 0x5EEDF00Du;
    S.k_inc = 8053064u;
    S.h_dec = 65349;
    S.master = G_MASTER;
    S.mw = (int32_t)((uint32_t)G_MASTER << 15);
    S.mark_left = PV_RATE;
    S.b_inc = note_inc(33);
    S.a_inc = note_inc(45);
    S.a_pl = S.a_pr = 20000;
    for (int i = 0; i < 3; i++) S.l_inc[i] = note_inc(69);
    S.m_inc[0] = S.m_inc[1] = note_inc(76);
    for (int i = 0; i < 8; i++) S.p_inc[i] = note_inc(57);
    g_hash = 2166136261u;
    hash_publish(0);
}

void synth_seek(uint32_t sample)
{
    synth_reset();
    int16_t tmp[512];
    while (S.pos < sample) {
        uint32_t n = sample - S.pos; if (n > 256) n = 256;
        synth_render(tmp, (int)n);
    }
}

uint32_t synth_pos(void)  { return S.pos; }
int32_t  synth_peak(void) { return S.peak; }
