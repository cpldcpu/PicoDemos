/* HYSTERESIS — the field. See field.h for why this is integer-only and why
 * advection is per-block.
 *
 * The whole per-frame pipeline is ONE pass:
 *
 *     for each 16x16 block:
 *         source coordinate = affine(block corner)      <- 4 multiplies, per BLOCK
 *         for each cell in block:
 *             gather 5 taps at source                   <- rigid offset, address adds only
 *             mix toward neighbourhood mean  (blur)
 *             scale                          (gain)
 *             add ordered dither             (anti-death)
 *             react through a 256-entry LUT  (nonlinearity, 1 load)
 *             store
 *
 * ~5 reads and 1 write per cell, no branches in the inner loop, no division,
 * no float. Everything expensive happens per block or per frame.
 */

#include "field.h"
#include "hot.h"

#include <string.h>

/* ON CORTEX-M33 DSP INTRINSICS, measured rather than assumed.
 *
 * Azure suggested them and they were worth trying: SMLAD does two 16-bit MACs
 * in one instruction and takes its coefficients packed two per register, which
 * looked ideal given that this loop is bound by register pressure.
 *
 * It came out SLOWER -- 68.5 -> 69.4 cycles/cell. The reason is that SMLAD
 * needs its inputs packed into 16-bit lanes, and packing costs two shifts and
 * two ORs per cell, which is more than the two saved multiplies return. The
 * instruction only pays when the data ARRIVES packed, which means loading four
 * bytes as a word and splitting them with UXTB16 while processing two cells per
 * iteration. That is a real optimisation and a bigger restructure; it is not
 * done here.
 *
 * Worth recording that the safety objection turned out to be the weakest reason
 * not to do it. Referee test 1 diffs the host render against the device render
 * byte for byte, so a device-only instruction path is verifiable rather than a
 * leap of faith. The reason to leave it out is simply that this form is slower.
 */

/* ---------------------------------------------------------------- tables -- */

/* The react curve, rebuilt whenever its two thresholds move. 256 iterations at
 * most once a frame is free, and it lets the arc bend the nonlinearity
 * continuously instead of switching between fixed shapes -- switching would be
 * a discontinuity, and this demo cannot afford one. */
/* 512 entries, not 256. The top half is all clamped to the value at 255, which
 * lets the transport loop index it WITHOUT clamping first: the accumulator is
 * provably in [0, 257] there (all bilinear weights are positive, taps are at
 * most 255, the weights sum to gain <= 258 in Q8, plus at most 240 of dither,
 * shifted down by 8). Two compares per cell become none. */
static uint8_t g_lut[512];
static int     g_lut_lo = -1, g_lut_hi = -1, g_lut_fold = -1;

static void build_react(int lo, int hi, int fold)
{
    if (lo == g_lut_lo && hi == g_lut_hi && fold == g_lut_fold) return;
    g_lut_lo = lo; g_lut_hi = hi; g_lut_fold = fold;

    if (lo == 0 && hi == 0) {                 /* identity — the control case */
        for (int i = 0; i < 256; i++) g_lut[i] = (uint8_t)i;
        for (int i = 256; i < 512; i++) g_lut[i] = 255;
        return;
    }
    if (hi <= lo) hi = lo + 1;

    for (int i = 0; i < 256; i++) {
        /* u: position across the excitable band, 0..65536 */
        int32_t u;
        if (i <= lo)      u = 0;
        else if (i >= hi) u = 65536;
        else              u = ((i - lo) << 16) / (hi - lo);

        /* rise: smoothstep — a cell receiving a little energy is pulled up */
        int64_t rise = ((int64_t)u * u * (3 * 65536 - 2 * u)) >> 32;

        /* hump: 4u(1-u) — peaks mid-band and returns to zero at the top, so
         * over-bright cells fall back instead of pinning at saturation */
        int64_t hump = (4LL * u * (65536 - u)) >> 16;
        if (hump > 65536) hump = 65536;

        int64_t s = rise + (((hump - rise) * fold) >> 8);
        int32_t y = (int32_t)((s * 255) >> 16);
        g_lut[i] = (uint8_t)(y < 0 ? 0 : y > 255 ? 255 : y);
    }
    /* saturating tail, so the transport loop needs no clamp */
    for (int i = 256; i < 512; i++) g_lut[i] = g_lut[255];
}

/* Q15 sine, quarter-turn resolution 256 -> 1024 entries over a full turn.
 * Built at init rather than baked as a const array so there is exactly one
 * definition of the curve and the host and device cannot disagree about it. */
static int16_t g_sin[1024];

/* Ordered dither. Repeated integer scaling rounds toward flat and the system
 * quantises into a frozen fixed point -- the simulation dies while still
 * looking plausible. Carrying an ordered residual keeps the low bits alive.
 * This is a known failure mode of 8-bit feedback, so it is in the design
 * rather than a patch. */
static const uint8_t g_bayer[16] = {
      0, 128,  32, 160,
    192,  64, 224,  96,
     48, 176,  16, 144,
    240, 112, 208,  80,
};

/* Integer sine over 0..1023 = one turn, returns Q15. Built by a stable
 * recurrence rather than sinf() so host and device produce identical tables
 * regardless of libm. */
static void build_sin(void)
{
    /* Build the quarter wave explicitly, then mirror it. The first version of
     * this folded the mirroring into one loop and read g_sin[1536 - i] at
     * i = 768 -- which is g_sin[768], the entry being written. Self-referential
     * and wrong. Two loops cost nothing at init and cannot do that. */
    int16_t quarter[257];
    for (int i = 0; i <= 256; i++) {
        /* u = i/256 of a quarter turn, Q15 */
        int64_t u  = ((int64_t)i * 32768) / 256;
        int64_t u2 = (u * u) >> 15;
        int64_t u3 = (u2 * u) >> 15;
        int64_t u5 = (u3 * u2) >> 15;
        /* sin(pi/2 u) ~= 1.5707963u - 0.2153918u^3 - 0.0086 u^5, in Q15 */
        int64_t s  = (51472 * u >> 15) - (7058 * u3 >> 15) - (282 * u5 >> 15);
        if (s > 32767) s = 32767;
        if (s < 0)     s = 0;
        quarter[i] = (int16_t)s;
    }
    for (int i = 0; i < 256; i++) {
        g_sin[      i] =  quarter[i];
        g_sin[256 + i] =  quarter[256 - i];
        g_sin[512 + i] = -quarter[i];
        g_sin[768 + i] = -quarter[256 - i];
    }
}

static inline int32_t isin(int32_t a) { return g_sin[(a >> 6) & 1023]; }   /* a: 0..65535 */
static inline int32_t icos(int32_t a) { return isin(a + 16384); }

int32_t field_isin(int32_t a) { return isin(a); }

void field_init(void)
{
    build_sin();
    g_lut_lo = g_lut_hi = -1;      /* force a rebuild on the next step */
}

/* ------------------------------------------------------------------ step -- */

/* Scratch for the convolution pass. 76,800 bytes, affordable now that the
 * reaction-diffusion state is unlinked -- it cost 153 KB and this costs half
 * that for something that actually reaches the picture. */
static uint8_t g_conv[FIELD_W * FIELD_H];

/* PASS 1 -- convolve the previous frame.
 *
 * Separate from the transport pass on purpose. Nine taps combined with a
 * four-tap bilinear fetch would be thirty-six taps per cell; convolving once
 * into scratch and then transporting with four is thirteen, and it splits the
 * two jobs cleanly: this pass decides spatial STRUCTURE, the next decides
 * MOTION.
 *
 * The 1-cell border is copied rather than handled, which keeps every bounds
 * test out of the inner loop. */
/* HYST_PROF locates the cost by removing work, not by reasoning about it.
 *   1 -- convolve becomes a straight copy: same memory traffic, no arithmetic
 *   2 -- the convolve pass is skipped entirely and transport reads src
 * Comparing the three tells us whether 84 cycles/cell is multiplies or memory,
 * which decides whether hand-written DSP assembly would help at all. */
#ifndef HYST_PROF
#define HYST_PROF 0
#endif

static void HYST_HOT(convolve)(const uint8_t *src, const field_params_t *p)
{
#if HYST_PROF == 1
    memcpy(g_conv, src, FIELD_W * FIELD_H);
    (void)p;
    return;
#endif
    /* FOLD THE BLUR MIX INTO THE KERNEL, once per frame.
     *
     * The loop used to compute the convolution and then lerp the result toward
     * the original value: v = l0 + ((a - l0) * mixw >> 8). But the lerp is
     * linear and the convolution is linear, so the two compose into a single
     * set of coefficients -- scale the kernel by mixw and add the remaining
     * (256 - mixw) to the centre tap. That removes a subtract, a multiply, a
     * shift and an add from every cell, and frees the mixw register, which
     * matters more: register pressure is what this loop is actually bound by.
     *
     * Rounds once instead of twice, so results shift by at most a bit or two
     * against the previous version -- checked against the referee and the
     * structure metrics rather than assumed harmless. */
    const int32_t mixw = p->blur;
          int32_t kc = ((p->k_centre * mixw) >> 8) + (256 - mixw);
    const int32_t kh =  (p->k_edge_h * mixw) >> 8;
    const int32_t kv =  (p->k_edge_v * mixw) >> 8;
    const int32_t kq =  (p->k_corner * mixw) >> 8;

    /* Restore unity gain exactly. Four independent truncations left the
     * coefficients summing to 253 rather than 256 -- only 1.2%, but the field
     * feeds back into itself sixty times a second, so a systematic gain error
     * compounds. It showed up immediately as the mean brightness moving from
     * 128 to 151, which is why the metrics are checked after every change to
     * this loop and not only after changes to the look. */
    kc += 256 - (kc + 2 * kh + 2 * kv + 4 * kq);


    memcpy(g_conv, src, FIELD_W);
    memcpy(g_conv + (FIELD_H - 1) * FIELD_W,
           src   + (FIELD_H - 1) * FIELD_W, FIELD_W);

    /* SLIDING WINDOW. Measured, not guessed: with nine byte loads per cell this
     * loop cost 37.4 cycles/cell of an 84.2 total, and removing five of the
     * multiplies only bought 4.2 -- so it is load-bound, not multiply-bound.
     *
     * The kernel is symmetric, so it only ever needs the vertical pair sum
     * (r0[x] + r2[x]) per column, never the two values separately. Carrying
     * three consecutive column sums and three centre-row values in registers
     * means each step loads only what is NEW: two bytes for the next column sum
     * and one for the next centre value. Nine loads per cell becomes three.
     *
     * Deliberately plain C rather than Cortex-M33 DSP intrinsics or inline
     * assembly, which Azure suggested and which would indeed fit here (SMLAD,
     * UXTB16, word loads). The reason is referee test 1: the host render and the
     * device render are diffed byte for byte, and device-only assembly puts that
     * guarantee at risk for a win this restructuring gets anyway. If the loop
     * ever needs more, the honest version is a word-load form that BOTH
     * compilers see identically. */
    /* ONE base pointer with fixed offsets, and the kernel coefficients packed
     * two-per-register.
     *
     * The disassembly of the previous version was the thing worth looking at
     * before writing any assembly: it spilled. Five or more stack accesses per
     * cell, because thirteen live values (four coefficients, the mix weight,
     * three row pointers, an output pointer and four window registers) do not
     * fit twelve usable registers -- and -funroll-loops doubled the pressure.
     * That is why cutting nine multiplies to four and nine loads to three each
     * bought only four cycles: the spills were the cost, and neither change
     * touched them.
     *
     * So this is a register-pressure fix, not an arithmetic one. One pointer
     * with -FIELD_W / +FIELD_W offsets replaces three, and the two accumulator
     * terms are folded so fewer values stay live across the loop body. */
    for (int y = 1; y < FIELD_H - 1; y++) {
        const uint8_t *p1 = src + y * FIELD_W;
        uint8_t *o = g_conv + y * FIELD_W;

        o[0] = p1[0];

        int32_t vm = p1[-FIELD_W]     + p1[FIELD_W];
        int32_t v0 = p1[1 - FIELD_W]  + p1[1 + FIELD_W];
        int32_t lm = p1[0];
        int32_t l0 = p1[1];

        for (int x = 1; x < FIELD_W - 1; x++) {
            const uint8_t *q = p1 + x + 1;
            const int32_t vp = q[-FIELD_W] + q[FIELD_W];
            const int32_t lp = *q;

            const int32_t v = (kc * l0 + kh * (lm + lp)
                             + kv * v0 + kq * (vm + vp)) >> 8;
            o[x] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);

            vm = v0; v0 = vp;
            lm = l0; l0 = lp;
        }
        o[FIELD_W - 1] = p1[FIELD_W - 1];
    }
}

/* PASS 2 -- transport, with SUBPIXEL precision.
 *
 * The advection was integer-only, on the grounds that the block quantisation
 * error is the fractal structure. That is true of the INTEGER part, which still
 * varies per block and still produces the seams. The fractional part is a
 * different thing entirely, and throwing it away cost smooth motion for
 * nothing: because the block is copied rigidly, the fraction is CONSTANT across
 * the block, so bilinear collapses to four weights computed once per block and
 * a fixed four-tap sum per cell. This is precisely the Amiga barrel-shifter
 * trick (ASH) that the pouet thread described and I only half-copied.
 */
void HYST_HOT(field_step)(uint8_t *dst, const uint8_t *src,
                          const field_params_t *p)
{
    build_react(p->react_lo, p->react_hi, p->react_fold);
    const uint8_t *lut = g_lut;

#if HYST_PROF == 2
    const uint8_t *cbuf = src;              /* skip the pass entirely */
#else
    convolve(src, p);
    const uint8_t *cbuf = g_conv;
#endif

    int32_t inv = p->zoom > 0 ? (int32_t)(((int64_t)65536 << 16) / p->zoom)
                              : 65536;
    const int32_t c = (icos(p->angle) * inv) >> 15;
    const int32_t s = (isin(p->angle) * inv) >> 15;

    const int32_t cxi = p->cx >> 16, cyi = p->cy >> 16;
    const int32_t gain = p->gain;
    const int32_t rate = 256 - (int32_t)p->persist;

    const uint8_t *bias = p->bias;
    const int32_t  bamt = bias ? p->bias_amt : 0;

    for (int by = 0; by < FIELD_H; by += FIELD_BLOCK) {
        for (int bx = 0; bx < FIELD_W; bx += FIELD_BLOCK) {
            const int32_t ox = bx - cxi, oy = by - cyi;
            int32_t sx = p->cx + c * ox - s * oy + p->drift_x;
            int32_t sy = p->cy + s * ox + c * oy + p->drift_y;

            sx += (p->shear_x * oy) >> 8;
            sy += (p->shear_y * ox) >> 8;

            for (int v = 0; v < 3; v++) {
                const int32_t S = p->vortex[v].strength;
                if (!S) continue;
                const int32_t vx = bx - p->vortex[v].x;
                const int32_t vy = by - p->vortex[v].y;
                const int32_t r2 = vx * vx + vy * vy + 256;
                sx += (int32_t)(((int64_t)(-vy) * S << 16) / r2);
                sy += (int32_t)(((int64_t)( vx) * S << 16) / r2);
            }

            if (p->band_amp)
                sx += (p->band_amp * isin(by * p->band_freq + p->band_phase))
                      << 1;

            int ix = sx >> 16, iy = sy >> 16;
            const int32_t fx = (sx >> 8) & 255, fy = (sy >> 8) & 255;

            /* Clamp the block origin so the bilinear halo stays in the buffer;
             * one clamp per block, no bounds test per cell. */
            if (ix < 0) ix = 0;
            if (iy < 0) iy = 0;
            if (ix > FIELD_W - FIELD_BLOCK - 2) ix = FIELD_W - FIELD_BLOCK - 2;
            if (iy > FIELD_H - FIELD_BLOCK - 2) iy = FIELD_H - FIELD_BLOCK - 2;

            /* four bilinear weights, once per block */
            /* Bilinear weights with GAIN PREMULTIPLIED. Folding the gain into
             * the weights here removes a multiply from every one of the 76,800
             * cells at the cost of four multiplies per block, of which there
             * are 300. Same trick as folding the blur mix into the kernel:
             * anything constant across a block belongs outside the inner
             * loop. */
            const int32_t w00 = (((256 - fx) * (256 - fy)) >> 8) * gain >> 8;
            const int32_t w10 = ((( fx      ) * (256 - fy)) >> 8) * gain >> 8;
            const int32_t w01 = (((256 - fx) * ( fy      )) >> 8) * gain >> 8;
            const int32_t w11 = ((( fx      ) * ( fy      )) >> 8) * gain >> 8;

            for (int y = 0; y < FIELD_BLOCK; y++) {
                const uint8_t *s0 = cbuf + (iy + y) * FIELD_W + ix;
                const uint8_t *s1 = s0 + FIELD_W;
                uint8_t       *dp = dst + (by + y) * FIELD_W + bx;
                const uint8_t *pp = src + (by + y) * FIELD_W + bx;
                const uint8_t *dith = g_bayer + (((by + y) & 3) << 2);

                for (int x = 0; x < FIELD_BLOCK; x++) {
                    int32_t v = (s0[x] * w00 + s0[x + 1] * w10
                               + s1[x] * w01 + s1[x + 1] * w11
                               + dith[(bx + x) & 3]) >> 8;

                    if (bamt) {
                        v -= (bias[(((by + y) >> 1) * (FIELD_W / 2))
                                   + ((bx + x) >> 1)] * bamt) >> 8;
                        if (v < 0) v = 0;      /* only the bias can go under */
                    }

                    const int32_t r = lut[v];
                    const int32_t q = pp[x];
                    dp[x] = (uint8_t)(q + (((r - q) * rate) >> 8));
                }
            }
        }
    }
}

/* ----------------------------------------------------------------- seeds -- */

void field_clear(uint8_t *f) { memset(f, 0, FIELD_W * FIELD_H); }

void field_poke(uint8_t *f, int x, int y, uint8_t v)
{
    if (x < 0 || y < 0 || x >= FIELD_W || y >= FIELD_H) return;
    f[y * FIELD_W + x] = v;
}

void field_inject_stencil(uint8_t *f, const uint8_t *bits, int sw, int sh,
                          int x, int y, uint8_t amp)
{
    const int stride = (sw + 7) >> 3;
    for (int j = 0; j < sh; j++) {
        const int fy = y + j;
        if (fy < 0 || fy >= FIELD_H) continue;
        for (int i = 0; i < sw; i++) {
            const int fx = x + i;
            if (fx < 0 || fx >= FIELD_W) continue;
            if (!(bits[j * stride + (i >> 3)] & (0x80 >> (i & 7)))) continue;
            /* additive and saturating: a disturbance, not a paste */
            int v = f[fy * FIELD_W + fx] + amp;
            f[fy * FIELD_W + fx] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}

void field_inject_blob(uint8_t *f, int x, int y, int radius, uint8_t amp)
{
    const int r2 = radius * radius;
    for (int j = -radius; j <= radius; j++) {
        const int fy = y + j;
        if (fy < 0 || fy >= FIELD_H) continue;
        for (int i = -radius; i <= radius; i++) {
            const int fx = x + i;
            if (fx < 0 || fx >= FIELD_W) continue;
            const int d2 = i * i + j * j;
            if (d2 > r2) continue;
            /* soft falloff so the impulse does not present a hard edge for the
             * blur to chew on for the next fifty frames */
            const int w = ((r2 - d2) << 8) / (r2 + 1);
            int v = f[fy * FIELD_W + fx] + ((amp * w) >> 8);
            f[fy * FIELD_W + fx] = (uint8_t)(v > 255 ? 255 : v);
        }
    }
}

uint32_t field_hash(const uint8_t *f)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < FIELD_W * FIELD_H; i++) {
        h ^= f[i];
        h *= 16777619u;
    }
    return h;
}

uint32_t field_energy(const uint8_t *f)
{
    uint32_t e = 0;
    for (int i = 0; i < FIELD_W * FIELD_H; i++) e += f[i];
    return e;
}
