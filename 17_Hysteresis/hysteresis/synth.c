/* HYSTERESIS -- the music.
 *
 * A small subtractive/resonator synth in integer arithmetic, playing the score
 * in score.c. See synth.h for the host/device contract and PLANNING.md section 6
 * for why this demo generates its soundtrack rather than carrying a recording.
 *
 * ------------------------------------------------------------------ numerics --
 *
 * Signals are int32 with +-32768 as nominal full scale -- the same units the
 * output uses, which keeps the level design readable. Coefficients are Q16
 * unless labelled, and products go through int64 before the shift. On
 * Cortex-M33 that is `smull` plus a shift pair, about three cycles; the
 * alternative is guessing at headroom, and a resonator with a Q in the
 * thousands is not a good place to guess.
 *
 * CONTROL RATE. Envelopes, cutoffs, the arc and the sequencer run once per 64
 * samples (344.53 Hz); only oscillators, filters and the reverb run per sample.
 * That is partly cost and mostly precision: a 45-second decay is a per-sample
 * multiplier of 0.99999, which does not exist in Q16 -- it rounds to 1.0 and
 * never decays at all. At control rate the same decay is 0.9992, which has real
 * slope. This is the 8-bit reaction-diffusion mistake from rd.c in a different
 * costume: the precision has to live where the small differences are.
 *
 * ------------------------------------------------------------------- voicing --
 *
 *   bass    three oscillators on a D pedal through a slow lowpass. Never moves.
 *   pad     two banks of four notes, crossfaded on a chord change, so no
 *           oscillator is ever retuned while it is audible.
 *   noise   xorshift through a swept bandpass -- the brief's "restrained
 *           spectral noise", and the only voice with no pitch.
 *   impact  six slots, each two tuned resonators plus a sine thud.
 *   reverb  four combs and two allpasses, so that "resonances accumulate" is
 *           something the piece does rather than something it is described as.
 *
 * All six impact slots are computed every sample whether or not they are
 * ringing. A struck resonator decays to zero on its own, so there is nothing to
 * switch off, and the cost of the music is then exactly constant -- the same
 * property the field has, for the same reason: a frame that sometimes costs
 * more is a frame that sometimes arrives late.
 */

#include "synth.h"
#include "score.h"
#include "synth_tables.h"
#include "hot.h"

#include <string.h>

/* ------------------------------------------------------------- small maths -- */

static int16_t g_sin[1024];          /* Q15, full period, for LFOs and the thud */

static void build_sin(void)
{
    for (int i = 0; i < 256; i++) {
        g_sin[      i] =  g_quarter[i];
        g_sin[256 + i] =  g_quarter[256 - i];
        g_sin[512 + i] = -g_quarter[i];
        g_sin[768 + i] = -g_quarter[256 - i];
    }
}

/* turn is a 16-bit angle: 65536 = one cycle. */
static inline int32_t isin(uint32_t turn) { return g_sin[(turn >> 6) & 1023]; }

static inline int32_t qmul(int32_t a, int32_t b)          /* Q16 */
{
    return (int32_t)(((int64_t)a * b) >> 16);
}

static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

/* 0 at or before a, 65536 at or after b. Same shape as sim.c's, in ms. */
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

static int note_clamp(int n) { return clampi(n, SYNTH_NOTE_LO, SYNTH_NOTE_HI); }
static uint32_t note_inc(int n) { return g_note_inc[note_clamp(n) - SYNTH_NOTE_LO]; }

static unsigned g_solo = SOLO_ALL;
void synth_solo(unsigned mask) { g_solo = mask; }

/* ------------------------------------------------------- the sample hash ---- */
/* FNV-1a over the int16 stream, latched at each exact second so the value can be
 * compared across the host/device boundary. See synth.h.
 *
 * Published as a seqlock rather than two plain stores: the writer is core 1 and
 * the reader is core 0, and a (pos, hash) pair caught mid-update would read as a
 * determinism failure. Chasing an imaginary one of those is more expensive than
 * these four lines. */
static uint32_t          g_hash = 2166136261u;
static volatile uint32_t g_mark_seq, g_mark_pos, g_mark_hash;

static inline void hash_byte(uint8_t b)
{
    g_hash = (g_hash ^ b) * 16777619u;
}

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
        if (s & 1u) continue;                   /* a write is in progress */
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        const uint32_t p = g_mark_pos, h = g_mark_hash;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (__atomic_load_n(&g_mark_seq, __ATOMIC_RELAXED) != s) continue;
        if (!p) return 0;
        *pos = p; *hash = h;
        return 1;
    }
}

/* xorshift32. Deterministic, which is the only requirement -- this is the one
 * place in the demo that wants to sound random, and referee test 1 still has to
 * get the same bytes twice. */
static uint32_t g_rng;
static inline int32_t noise(void)
{
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return (int32_t)(g_rng >> 17) - 16384;      /* +-16384 */
}

/* --------------------------------------------------------- state-variable ---- */
/* Chamberlin, two poles. `f` is 2*sin(pi*fc/fs) in Q16, `q` is 1/Q in Q16;
 * stable while f + q < 2. States are clamped rather than trusted: the input is
 * a sum of sixteen oscillators, and a fixed-point filter that blows up does not
 * come back on its own. */
typedef struct { int32_t lo, band; } svf_t;

static inline void svf(svf_t *s, int32_t in, int32_t f, int32_t q)
{
    s->lo   = clampi(s->lo   + qmul(f, s->band), -(1 << 26), (1 << 26));
    const int32_t hi = in - s->lo - qmul(q, s->band);
    s->band = clampi(s->band + qmul(f, hi),      -(1 << 26), (1 << 26));
}

/* Cutoff coefficient from a frequency in Hz: f = 2*sin(pi*fc/fs), small-angle.
 *
 * 2*pi*fc/fs in Q16 is fc * 2^16 * 2*pi / fs, and 411775 is 2*pi in Q16. The
 * approximation is exact to four digits below 1 kHz and reads about 3% high at
 * the top of the range used here (3.4 kHz), which lands as a slightly bright
 * cutoff and nothing else.
 *
 * Deliberately NOT the sine table: at a 170 Hz bass cutoff the angle is under
 * four of the table's 1024 steps, so a lookup would quantise the cutoff into a
 * staircase. Same defect that put the resonators on baked per-note coefficients
 * (synth_tables.h) -- small angles and coarse tables do not mix. */
static inline int32_t hz_f(uint32_t hz)
{
    return (int32_t)((uint64_t)hz * 411775u / SYNTH_RATE);
}

/* -------------------------------------------------------------- resonators -- */
/* A rotation by w per sample, scaled by the decay r:
 *
 *      x' = x*(r cos w) - y*(r sin w)
 *      y' = x*(r sin w) + y*(r cos w)
 *
 * The pair (c,s) has magnitude r, so the amplitude decays as r^n while the
 * phase advances by exactly w. Struck by writing x directly, which makes the
 * peak output equal to the value written -- the level of an impact is therefore
 * a number chosen in strike(), not something that emerges from how much energy a
 * noise burst happened to deposit near the resonance.
 *
 * Coefficients in Q30 from synth_tables.h, which explains why this is a
 * rotation and not the cheaper two-pole recursion. */
typedef struct { int32_t x, y, c, s; } reso_t;

static inline int32_t reso(reso_t *r, int32_t in)
{
    const int32_t x = r->x + in;
    const int32_t y = r->y;
    r->x = clampi((int32_t)(((int64_t)x * r->c - (int64_t)y * r->s) >> 30),
                  -(1 << 24), (1 << 24));
    r->y = clampi((int32_t)(((int64_t)x * r->s + (int64_t)y * r->c) >> 30),
                  -(1 << 24), (1 << 24));
    return r->x;
}

/* Tune to a MIDI note, -60 dB after nsamp samples.
 *
 * r = 0.001^(1/n) = exp(-6.908/n). For every decay length in this piece
 * 6.908/n is under 1e-3, so the first-order term is the whole story: the
 * x^2/2 correction is below one part in 2^30 above about a tenth of a second.
 * Short strikes decay a hair fast, which is inaudible and in the safe
 * direction. */
static void reso_tune(reso_t *v, int note, uint32_t nsamp)
{
    if (nsamp < 128) nsamp = 128;
    note = note_clamp(note);

    const int32_t one = 1 << 30;
    int32_t d = (int32_t)(((uint64_t)6908u << 30) / (1000ull * nsamp));
    if (d > one - 1) d = one - 1;
    const int32_t r = one - d;

    v->c = (int32_t)(((int64_t)r * g_note_cos[note - SYNTH_NOTE_LO]) >> 30);
    v->s = (int32_t)(((int64_t)r * g_note_sin[note - SYNTH_NOTE_LO]) >> 30);
}

/* ------------------------------------------------------------------ reverb -- */
/* Freeverb's shape at half its sample rate: four damped feedback combs in
 * parallel into two allpasses. Buffers are int16 because 12 KB of tail is
 * affordable on a device with 200 KB spare and 24 KB is starting not to be;
 * the requantisation noise recirculates around -70 dB, below the PWM's floor. */
#define NCOMB 4
#define NAP   2

/* NOT const, deliberately. A const array lands in flash, and on RP2350 flash is
 * reached through the XIP cache -- so every sample would take a cached read from
 * the same memory core 0 is streaming 76,800 field bytes through, on the core
 * that has a scanline deadline. Dropping const puts these in .data, which is
 * copied to SRAM at boot. Same reason field.c's hot path is __not_in_flash_func
 * (hot.h); six bytes of table, but they are read 132,000 times a second. */
static uint16_t comb_len[NCOMB] = { 1153, 1327, 1481, 1613 };
static uint16_t ap_len[NAP]     = {  331,  127 };

static int16_t  g_comb[1153 + 1327 + 1481 + 1613];
static int16_t  g_ap[331 + 127];
static uint16_t g_comb_off[NCOMB], g_ap_off[NAP];
static uint16_t g_comb_ix[NCOMB],  g_ap_ix[NAP];
static int32_t  g_comb_lp[NCOMB];

#define REV_FB   62259      /* 0.95 -- about 3.4 s of tail */
#define REV_DAMP 26214      /* 0.40 of lowpass inside the feedback path */
#define REV_APG  32768      /* 0.50 */

static void reverb_reset(void)
{
    uint16_t o = 0;
    for (int i = 0; i < NCOMB; i++) {
        g_comb_off[i] = o; o = (uint16_t)(o + comb_len[i]);
        g_comb_ix[i] = 0; g_comb_lp[i] = 0;
    }
    o = 0;
    for (int i = 0; i < NAP; i++) {
        g_ap_off[i] = o; o = (uint16_t)(o + ap_len[i]);
        g_ap_ix[i] = 0;
    }
    memset(g_comb, 0, sizeof g_comb);
    memset(g_ap,   0, sizeof g_ap);
}

static inline int32_t HYST_HOT(reverb)(int32_t in)
{
    int32_t acc = 0;
    for (int i = 0; i < NCOMB; i++) {
        int16_t *b = g_comb + g_comb_off[i];
        const int32_t out = b[g_comb_ix[i]];
        g_comb_lp[i] += qmul(REV_DAMP, out - g_comb_lp[i]);
        b[g_comb_ix[i]] = (int16_t)clampi(in + qmul(REV_FB, g_comb_lp[i]),
                                          -32768, 32767);
        if (++g_comb_ix[i] >= comb_len[i]) g_comb_ix[i] = 0;
        acc += out;
    }
    acc >>= 2;

    for (int i = 0; i < NAP; i++) {
        int16_t *b = g_ap + g_ap_off[i];
        const int32_t bo = b[g_ap_ix[i]];
        b[g_ap_ix[i]] = (int16_t)clampi(acc + qmul(REV_APG, bo), -32768, 32767);
        if (++g_ap_ix[i] >= ap_len[i]) g_ap_ix[i] = 0;
        acc = bo - acc;
    }
    return acc;
}

/* ------------------------------------------------------------------- voices -- */

#define NPAD    4          /* notes in a chord */
#define NPOSC   3          /* oscillators per note: unison, detuned, octave */
#define NIMPACT 6

typedef struct {
    reso_t   r[2];             /* fundamental, and a near-third partial */
    int32_t  bgain;            /* noise excitation, decaying per sample */
    uint32_t th_ph, th_inc;    /* the thud */
    int32_t  th_env, th_dec;   /* Q16, decayed per control tick */
    int32_t  gain;             /* Q16 */
} impact_t;

static struct {
    uint32_t b_ph[3], b_inc[3];
    svf_t    b_f;
    int32_t  b_cut, b_lvl;

    uint32_t p_ph[2][NPAD][NPOSC], p_inc[2][NPAD][NPOSC];
    int32_t  p_fade;           /* Q16, 0 = bank 0 audible, 65536 = bank 1 */
    int      p_bank, p_section;
    svf_t    p_f;
    int32_t  p_cut, p_lvl;

    svf_t    n_f;
    int32_t  n_cut, n_lvl;

    impact_t imp[NIMPACT];
    int      imp_next;

    uint32_t lfo_slow, lfo_mid;
    int32_t  wet, hp;

    uint32_t pos;
    int      ctl_left;
    int      mark_left;        /* samples until the next hash latch */
    unsigned next_hit;
    int32_t  peak;
} S;

/* There is no master trim. The voices were levelled from measured solo renders
 * (synth.h, synth_solo) until the full mix peaked at 24860 of 32767 -- 76% of
 * scale, 2.4 dB of headroom -- so a trim multiply would be a per-sample no-op.
 * If that peak ever moves, this is where the one line goes. */

/* ------------------------------------------------------------------- chords -- */
/* Upper voices only. The bass pedal never moves (score.c says why), so these
 * are a slow re-voicing of one mode rather than a progression -- there is no
 * dominant anywhere in the piece and nothing ever resolves. A field with memory
 * has one long shape and one attractor; a key change would be the loudest
 * possible claim that something outside the system decided to move.
 *
 * A section change crossfades over six seconds into the idle bank, so the pad
 * is always eight notes of oscillator and four notes of harmony, at constant
 * cost, and no note is ever retuned while you can hear it. */
static const struct { uint32_t at_ms; uint8_t n[NPAD]; } chords[] = {
    {      0, { 50, 57, 62, 65 } },   /* Dm   -- under the near-silence */
    {  30000, { 50, 57, 64, 65 } },   /* Dm9  -- the 9th arrives with the field */
    {  62000, { 53, 58, 62, 65 } },   /* Bb   */
    {  94000, { 55, 58, 62, 67 } },   /* Gm   */
    { 126000, { 57, 62, 65, 69 } },   /* Dm, high voicing -- the peak */
    { 158000, { 53, 58, 62, 65 } },   /* Bb   -- the decay begins */
    { 186000, { 50, 53, 57, 62 } },   /* Dm, closed and low */
};
#define NCHORD ((int)(sizeof chords / sizeof chords[0]))

/* Three oscillators per chord note: unison, unison detuned +3.4 cents, and an
 * octave up a shade flat.
 *
 * The octave is not decoration. Measured, the mix had 97.6% of its energy below
 * 250 Hz at 62 s -- all pedal and low pad, nothing in the band a small speaker
 * can reproduce at all. Adding an octave per note costs four more phase
 * accumulators and moves real energy into the midrange, which a higher filter
 * cutoff alone could not do: opening the lowpass finds harmonics only if the
 * oscillators put some up there. */
static void pad_set_bank(int bank, const uint8_t *n)
{
    for (int i = 0; i < NPAD; i++) {
        const uint32_t inc = note_inc(n[i]);
        S.p_inc[bank][i][0] = inc;
        S.p_inc[bank][i][1] = inc + (inc >> 9) + 1;    /* +3.4 cents */
        S.p_inc[bank][i][2] = (inc << 1) - (inc >> 9); /* octave, -3.4 cents */
    }
}

/* -------------------------------------------------------------------- strike -- */

static void strike(const score_hit_t *h)
{
    impact_t *v = &S.imp[S.imp_next];
    S.imp_next = (S.imp_next + 1) % NIMPACT;

    /* Lower and harder strikes ring longer. The bottom D at 152 s gets about
     * nine seconds, which is what carries the piece into the long decay. */
    const uint32_t ms = 2600u + (uint32_t)h->weight * 14u
                      + (uint32_t)(70 - note_clamp(h->note)) * 60u;
    const uint32_t n  = ms * (SYNTH_RATE / 1000u);

    /* +19 semitones is 2.997x: the near-third partial a struck bar has and a
     * struck string does not. Shorter decay than the fundamental, so the strike
     * is bright and the tail is not. */
    reso_tune(&v->r[0], h->note,      n);
    reso_tune(&v->r[1], h->note + 19, n / 2);

    /* Struck, not blown: x carries the whole amplitude, so the ring peaks at
     * exactly this value. The noise burst underneath it is texture. */
    v->r[0].x = 1 << 14; v->r[0].y = 0;
    v->r[1].x = 1 << 13; v->r[1].y = 0;
    v->bgain  = 2600;

    v->th_ph  = 0;
    v->th_inc = note_inc(h->note - 12);
    v->th_env = 65536;
    v->th_dec = 55000;                     /* ~140 ms */

    v->gain   = (int32_t)h->weight * 130;  /* 255 -> 0.506 */
}

/* --------------------------------------------------------------------- arc --- */

static void control_tick(void)
{
    const uint32_t ms = (uint32_t)((uint64_t)S.pos * 1000u / SYNTH_RATE);

    /* --- the sequencer. Events are in beats; a beat is 11025 samples. --- */
    while (S.next_hit < score_hit_count) {
        const uint32_t at = (uint32_t)score_hits[S.next_hit].beat
                          * SCORE_SAMPLES_PER_BEAT;
        if (at > S.pos) break;
        if (score_hits[S.next_hit].weight) strike(&score_hits[S.next_hit]);
        S.next_hit++;
    }

    /* --- chord section, and its crossfade --- */
    for (int i = NCHORD; i-- > 0; ) {
        if (ms >= chords[i].at_ms) {
            if (i != S.p_section) {
                S.p_section = i;
                S.p_bank   ^= 1;
                pad_set_bank(S.p_bank, chords[i].n);
            }
            break;
        }
    }
    {
        const int32_t target = S.p_bank ? 65536 : 0;
        const int32_t step   = 65536 / (6 * SYNTH_RATE / SYNTH_CTL_DIV);
        if      (S.p_fade < target) S.p_fade += step;
        else if (S.p_fade > target) S.p_fade -= step;
        S.p_fade = clampi(S.p_fade, 0, 65536);
    }

    S.lfo_slow += 21;      /* ~0.11 Hz */
    S.lfo_mid  += 97;      /* ~0.51 Hz */

    /* --- the bed's dynamic arc, mirroring the picture ---
     *
     * Each line takes over once its ramp opens, the same edit-list idiom sim.c
     * uses on the field parameters. Bass first and alone (the brief's "one
     * barely audible sustained tone"), pad from twenty seconds, noise from
     * forty, everything peaking where the zoom does, then forty-five seconds of
     * decay and a one-second cut at the collapse. */
    int32_t bass = mix(0, 10400, ramp(ms,      0,   9000));
    bass = mix(bass,      28100, ramp(ms,   9000,  40000));
    bass = mix(bass,      40000, ramp(ms,  40000, 126000));
    bass = mix(bass,       8900, ramp(ms, 158000, 203000));
    bass = mix(bass,          0, ramp(ms, 203000, 204200));

    int32_t pad = mix(0,   4400, ramp(ms,  14000,  46000));
    pad  = mix(pad,       15300, ramp(ms,  46000, 100000));
    pad  = mix(pad,       23000, ramp(ms, 100000, 138000));
    pad  = mix(pad,        5500, ramp(ms, 158000, 203000));
    pad  = mix(pad,           0, ramp(ms, 203000, 204200));

    int32_t nz = mix(0,    1900, ramp(ms,  26000,  90000));
    nz   = mix(nz,         5200, ramp(ms,  90000, 132000));
    nz   = mix(nz,          750, ramp(ms, 150000, 200000));
    nz   = mix(nz,            0, ramp(ms, 200000, 203600));

    /* Reverb opens as the material thins: the room is most audible when there
     * is least in it, which is the whole trick of the last minute. */
    int32_t wet = mix(13000, 22000, ramp(ms,  30000, 126000));
    wet = mix(wet,           38000, ramp(ms, 150000, 204000));

    /* Cutoffs. The pad opens through the growth and closes again in the outro,
     * so the last minute is darker and not merely quieter -- the palette does
     * the same thing on the picture side (PAL_BLOOM -> PAL_ASH). */
    int32_t pcut = mix(700, 2600, ramp(ms,  14000, 110000));
    pcut = mix(pcut,        800, ramp(ms, 150000, 204000));
    pcut += (isin(S.lfo_slow) * 110) >> 15;

    int32_t ncut = mix(900, 3400, ramp(ms,  26000, 130000));
    ncut = mix(ncut,       1100, ramp(ms, 150000, 200000));
    ncut += (isin(S.lfo_mid) * 300) >> 15;

    const int32_t bcut = 210 + ((isin(S.lfo_slow + 20000) * 45) >> 15);

    if (!(g_solo & SOLO_BASS))   bass = 0;
    if (!(g_solo & SOLO_PAD))    pad  = 0;
    if (!(g_solo & SOLO_NOISE))  nz   = 0;
    if (!(g_solo & SOLO_REVERB)) wet  = 0;

    /* One-pole smoothing on everything the per-sample loop reads. Without it
     * each of these steps once per 64 samples, which is a 344 Hz buzz sitting
     * on the music -- the audio spelling of the ordered-dither speckle the
     * field had. */
    S.b_lvl += (bass - S.b_lvl) >> 3;
    S.p_lvl += (pad  - S.p_lvl) >> 3;
    S.n_lvl += (nz   - S.n_lvl) >> 3;
    S.wet   += (wet  - S.wet)   >> 3;
    S.b_cut += (bcut - S.b_cut) >> 3;
    S.p_cut += (pcut - S.p_cut) >> 3;
    S.n_cut += (ncut - S.n_cut) >> 3;

    for (int i = 0; i < NIMPACT; i++)
        S.imp[i].th_env = qmul(S.imp[i].th_env, S.imp[i].th_dec);
}

/* ------------------------------------------------------------------ render --- */

static void HYST_HOT(render_block)(int16_t *out, int n)
{
    const int32_t bf = hz_f((uint32_t)S.b_cut), bq = 30000;
    const int32_t pf = hz_f((uint32_t)S.p_cut), pq = 40000;
    const int32_t nf = hz_f((uint32_t)S.n_cut), nq = 20000;
    const int32_t fade = S.p_fade, wet = S.wet;

    for (int i = 0; i < n; i++) {
        /* --- bass: two triangles a few cents apart, plus a saw an octave up
         * for the part of 37 Hz a 3.5 mm jack can actually carry.
         *
         * The second triangle is at HALF amplitude, which is not a taste
         * decision. Two equal detuned oscillators of the same waveform cancel
         * completely twice per beat period, and measured that way the bass RMS
         * fell from 2458 to 911 every 6.7 seconds -- a drone with a hole in it.
         * Unequal amplitudes turn the null into a 30% swell, which is the
         * breathing that was wanted in the first place. */
        int32_t b = 0;
        for (int k = 0; k < 2; k++) {
            S.b_ph[k] += S.b_inc[k];
            const int32_t s = (int32_t)(S.b_ph[k] >> 17) - 16384;
            const int32_t tri = ((s < 0 ? -s : s) << 1) - 16384;
            b += k ? (tri >> 1) : tri;
        }
        S.b_ph[2] += S.b_inc[2];
        b += ((int32_t)(S.b_ph[2] >> 17) - 16384) >> 1;        /* saw, -6 dB */
        svf(&S.b_f, b >> 1, bf, bq);
        const int32_t bass = qmul(S.b_f.lo, S.b_lvl);

        /* --- pad: twelve detuned saws, two banks crossfading --- */
        int32_t pa = 0, pb = 0;
        for (int k = 0; k < NPAD; k++) {
            for (int d = 0; d < NPOSC; d++) {
                S.p_ph[0][k][d] += S.p_inc[0][k][d];
                S.p_ph[1][k][d] += S.p_inc[1][k][d];
                const int32_t sa = (int32_t)(S.p_ph[0][k][d] >> 17) - 16384;
                const int32_t sb = (int32_t)(S.p_ph[1][k][d] >> 17) - 16384;
                pa += (d == 2) ? (sa >> 1) : sa;       /* octave at -6 dB */
                pb += (d == 2) ? (sb >> 1) : sb;
            }
        }
        /* >>2, not >>3. The first version scaled by the worst case -- eight
         * saws lining up in phase -- which never happens with them detuned, so
         * the pad played about eighteen dB below where it was written. Level is
         * set from a measured solo render now, not from an overflow bound. */
        svf(&S.p_f, mix(pa, pb, fade) >> 2, pf, pq);
        const int32_t pad = qmul(S.p_f.lo, S.p_lvl);

        /* --- noise bed: bandpass, so it reads as air and not as hiss --- */
        svf(&S.n_f, noise(), nf, nq);
        const int32_t nz = qmul(S.n_f.band, S.n_lvl);

        /* --- impacts --- */
        int32_t im = 0;
        for (int k = 0; k < NIMPACT; k++) {
            impact_t *v = &S.imp[k];
            int32_t x = 0;
            if (v->bgain > 48) {
                x = qmul(noise(), v->bgain);
                v->bgain = qmul(v->bgain, 64854);              /* ~30 ms */
            }
            int32_t r = reso(&v->r[0], x) + (reso(&v->r[1], x) >> 1);
            v->th_ph += v->th_inc;
            r += qmul(isin(v->th_ph >> 16) >> 1, v->th_env);
            im += qmul(r, v->gain);
        }
        if (!(g_solo & SOLO_IMPACT)) im = 0;

        /* --- bus --- */
        const int32_t dry = bass + pad + nz + im;
        int32_t v = dry + qmul(reverb(dry >> 1), wet);

        /* One-pole highpass around 14 Hz. The resonators and the reverb both
         * accumulate a slow offset, and on an 11-bit PWM a wandering DC term is
         * headroom spent on nothing audible. Well below the 37 Hz pedal. */
        S.hp += (v - S.hp) >> 8;
        v -= S.hp;

        const int32_t a = v < 0 ? -v : v;
        if (a > S.peak) S.peak = a;

        const int16_t s = (int16_t)clampi(v, -32768, 32767);
        out[i] = s;

        /* Hash the CLAMPED OUTPUT, which is the only thing both targets are
         * guaranteed to agree about byte for byte. */
        hash_byte((uint8_t)((uint16_t)s & 0xFFu));
        hash_byte((uint8_t)((uint16_t)s >> 8));
        if (--S.mark_left <= 0) {
            S.mark_left = SYNTH_RATE;
            hash_publish(S.pos + (uint32_t)i + 1u);
        }
    }
    S.pos += (uint32_t)n;
}

void HYST_HOT(synth_render)(int16_t *out, int n)
{
    while (n > 0) {
        if (S.ctl_left == 0) { control_tick(); S.ctl_left = SYNTH_CTL_DIV; }
        const int k = n < S.ctl_left ? n : S.ctl_left;
        render_block(out, k);
        out += k; n -= k; S.ctl_left -= k;
    }
}

/* -------------------------------------------------------------------- reset -- */

void synth_reset(void)
{
    build_sin();
    reverb_reset();
    memset(&S, 0, sizeof S);
    g_rng  = 0x1BADF00Du;
    g_hash = 2166136261u;
    S.mark_left = SYNTH_RATE;
    hash_publish(0);                 /* pos 0 means "nothing latched yet" */

    /* The pedal: D1, a companion detuned about seven cents so the drone
     * breathes on a seven-second period, and D2 for the part a jack can carry. */
    const uint32_t d1 = note_inc(26);
    S.b_inc[0] = d1;
    S.b_inc[1] = d1 + (d1 >> 8) + 1;
    S.b_inc[2] = note_inc(38);

    S.b_cut = 210;
    S.p_cut = 700;
    S.n_cut = 900;
    S.wet   = 13000;

    pad_set_bank(0, chords[0].n);
    pad_set_bank(1, chords[0].n);

    for (int k = 0; k < NIMPACT; k++) S.imp[k].th_dec = 65536;
}

uint32_t synth_pos(void)           { return S.pos; }
uint32_t synth_total_samples(void) { return SCORE_BEATS * SCORE_SAMPLES_PER_BEAT; }
int32_t  synth_peak(void)          { return S.peak; }
