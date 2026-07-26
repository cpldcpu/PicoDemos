/* Fast replacements for the libm calls buried in the ray walk's inner loop.
 *
 * On-device profiling put 95% of the frame inside the march, at roughly 1,700
 * cycles per sample — far more than a handful of SRAM bilinear taps should
 * cost. The reason is that each tap calls floorf() twice, and the mip lookup
 * calls log2f(); on Cortex-M33 those are library calls, not instructions, and
 * a sample makes six to ten of them.
 *
 * These are exact enough for their jobs and cost a few cycles each:
 *   ffloor  — exact for the range texture coordinates ever reach
 *   flog2   — ~0.04 max error, used only to pick a mip level and blend weight
 */
#ifndef SUSTAIN_FASTMATH_H
#define SUSTAIN_FASTMATH_H

#include <stdint.h>

/* Truncation rounds toward zero; floor rounds toward -inf. They differ only
 * for negatives, and world coordinates do go negative. */
static inline float ffloor(float x)
{
    const int i = (int)x;
    return (float)((x < 0.0f && (float)i != x) ? i - 1 : i);
}

static inline int ifloor(float x)
{
    const int i = (int)x;
    return (x < 0.0f && (float)i != x) ? i - 1 : i;
}

/* log2 via the IEEE-754 exponent, plus a linear fit across the mantissa.
 * Only ever used to choose between mip levels and blend adjacent ones, where
 * a few hundredths of a level is invisible. */
static inline float flog2(float x)
{
    union { float f; uint32_t i; } u;
    u.f = x;
    const float e = (float)(int)(((u.i >> 23) & 0xFFu) - 127u);
    u.i = (u.i & 0x007FFFFFu) | 0x3F800000u;      /* mantissa into [1,2) */
    /* Minimax-ish linear fit of log2 over [1,2): max error ~0.04. */
    return e + (u.f - 1.0f) * (1.4187f - 0.4187f * (u.f - 1.0f));
}

#endif
