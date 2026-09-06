/* Bilinear texture sampling for SUSTAIN.
 *
 * Bilinear on the HEIGHT sample is not a quality upgrade here, it is a
 * correctness requirement. Point-sampling a 256x256 relief map gives faceted
 * terrain, and a morph lerps between two QUANTISED fields — so the facets pop
 * and crawl as the weight moves, which is exactly the kind of frame-to-frame
 * artefact cut_detect.py exists to catch. A smooth morph needs a smooth thing
 * to morph.
 *
 * On device the RP2350 SIO interpolator's BLEND unit does this lerp in
 * hardware (and interp_emu.c reproduces it bit-exactly on host, verified by
 * tools/interp_selftest.c). These C helpers are the reference path; the hot
 * loops get moved onto BLEND once the arc is authored and the frame budget is
 * measured rather than guessed.
 *
 * All textures are power-of-two, so wrapping is a mask and costs nothing.
 */

#ifndef SUSTAIN_TEX_H
#define SUSTAIN_TEX_H

#include <stdint.h>
#include <math.h>
#include "fastmath.h"

/* gray8, returns 0..255 as float. */
static inline float tex_gray_bilin(const uint8_t *t, int wmask, int hmask,
                                   int w, float u, float v)
{
    const float uf = ffloor(u), vf = ffloor(v);
    const float fu = u - uf, fv = v - vf;
    const int   u0 = (int)uf & wmask, v0 = (int)vf & hmask;
    const int   u1 = (u0 + 1) & wmask, v1 = (v0 + 1) & hmask;

    const float a = (float)t[v0 * w + u0], b = (float)t[v0 * w + u1];
    const float c = (float)t[v1 * w + u0], d = (float)t[v1 * w + u1];
    const float top = a + (b - a) * fu;
    const float bot = c + (d - c) * fu;
    return top + (bot - top) * fv;
}

/* RGB565 (the repo's R-at-LSB layout, see rgb565.h), returns 0..255 channels. */
static inline void tex_rgb_bilin(const uint8_t *raw, int wmask, int hmask,
                                 int w, float u, float v,
                                 int *r, int *g, int *b)
{
    const uint16_t *t = (const uint16_t *)raw;
    const float uf = ffloor(u), vf = ffloor(v);
    const float fu = u - uf, fv = v - vf;
    const int   u0 = (int)uf & wmask, v0 = (int)vf & hmask;
    const int   u1 = (u0 + 1) & wmask, v1 = (v0 + 1) & hmask;

    const uint16_t p00 = t[v0 * w + u0], p10 = t[v0 * w + u1];
    const uint16_t p01 = t[v1 * w + u0], p11 = t[v1 * w + u1];

#define CH(SH, MASK)                                                          \
    ({ float a = (float)((p00 >> SH) & MASK), b_ = (float)((p10 >> SH) & MASK), \
              c = (float)((p01 >> SH) & MASK), d = (float)((p11 >> SH) & MASK); \
       float tp = a + (b_ - a) * fu, bt = c + (d - c) * fu;                    \
       (tp + (bt - tp) * fv) * (255.0f / (float)MASK); })

    *r = (int)CH(0,  0x1F);
    *g = (int)CH(6,  0x1F);
    *b = (int)CH(11, 0x1F);
#undef CH
}

#endif
