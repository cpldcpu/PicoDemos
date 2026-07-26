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

/* ---------------------------------------------------------------- tables -- */

/* The react curve, rebuilt whenever its two thresholds move. 256 iterations at
 * most once a frame is free, and it lets the arc bend the nonlinearity
 * continuously instead of switching between fixed shapes -- switching would be
 * a discontinuity, and this demo cannot afford one. */
static uint8_t g_lut[256];
static int     g_lut_lo = -1, g_lut_hi = -1, g_lut_fold = -1;

static void build_react(int lo, int hi, int fold)
{
    if (lo == g_lut_lo && hi == g_lut_hi && fold == g_lut_fold) return;
    g_lut_lo = lo; g_lut_hi = hi; g_lut_fold = fold;

    if (lo == 0 && hi == 0) {                 /* identity — the control case */
        for (int i = 0; i < 256; i++) g_lut[i] = (uint8_t)i;
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

void field_init(void)
{
    build_sin();
    g_lut_lo = g_lut_hi = -1;      /* force a rebuild on the next step */
}

/* ------------------------------------------------------------------ step -- */

void HYST_HOT(field_step)(uint8_t *dst, const uint8_t *src,
                          const field_params_t *p)
{
    build_react(p->react_lo, p->react_hi, p->react_fold);
    const uint8_t *lut = g_lut;

    /* Inverse transform, computed once per frame. The parameter is visual
     * magnification, so sampling needs its reciprocal: one division per frame
     * is free, one per pixel would not be. */
    int32_t inv = p->zoom > 0 ? (int32_t)(((int64_t)65536 << 16) / p->zoom)
                              : 65536;
    const int32_t c = (icos(p->angle) * inv) >> 15;   /* 16.16 */
    const int32_t s = (isin(p->angle) * inv) >> 15;

    const int32_t cxi = p->cx >> 16, cyi = p->cy >> 16;

    const int32_t blur = p->blur;
    const int32_t gain = p->gain;

    for (int by = 0; by < FIELD_H; by += FIELD_BLOCK) {
        for (int bx = 0; bx < FIELD_W; bx += FIELD_BLOCK) {
            /* One source coordinate for the whole block. The block is then
             * copied rigidly -- zoom shows up as varying offsets BETWEEN
             * blocks, never as scaling within one. The resulting seams are the
             * effect, not an artefact of it. */
            const int32_t ox = bx - cxi, oy = by - cyi;
            int32_t sx = p->cx + c * ox - s * oy + p->drift_x;
            int32_t sy = p->cy + s * ox + c * oy + p->drift_y;

            int ix = sx >> 16, iy = sy >> 16;

            /* Clamp the block origin, not the pixels. One clamp per block
             * keeps every gather inside the buffer including the 1-cell halo,
             * so the inner loop needs no bounds test at all. Edges repeat,
             * which in feedback reads as smearing at the border -- acceptable,
             * and cheaper than any alternative. */
            if (ix < 1) ix = 1;
            if (iy < 1) iy = 1;
            if (ix > FIELD_W - FIELD_BLOCK - 1) ix = FIELD_W - FIELD_BLOCK - 1;
            if (iy > FIELD_H - FIELD_BLOCK - 1) iy = FIELD_H - FIELD_BLOCK - 1;

            for (int y = 0; y < FIELD_BLOCK; y++) {
                const uint8_t *sp = src + (iy + y) * FIELD_W + ix;
                uint8_t       *dp = dst + (by + y) * FIELD_W + bx;
                const uint8_t *dith = g_bayer + (((by + y) & 3) << 2);

                for (int x = 0; x < FIELD_BLOCK; x++) {
                    const int32_t a = sp[x];
                    const int32_t n = sp[x - 1] + sp[x + 1]
                                    + sp[x - FIELD_W] + sp[x + FIELD_W];

                    /* mix toward the neighbourhood mean */
                    int32_t v = a + ((((n >> 2) - a) * blur) >> 8);

                    /* scale, with ordered dither folded into the rounding */
                    v = (v * gain + dith[(bx + x) & 3]) >> 8;

                    if (v < 0)   v = 0;
                    if (v > 255) v = 255;

                    dp[x] = lut[v];
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

uint32_t field_energy(const uint8_t *f)
{
    uint32_t e = 0;
    for (int i = 0; i < FIELD_W * FIELD_H; i++) e += f[i];
    return e;
}
