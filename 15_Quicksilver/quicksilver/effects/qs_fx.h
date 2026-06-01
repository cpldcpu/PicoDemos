/* qs_fx.h — shared inner-loop helpers for QUICKSILVER's interpolator scenes.
 * Header-only, builds identically on host (emulator) and RP2350 (raw SIO). */

#ifndef QS_FX_H
#define QS_FX_H

#include "../interp_compat.h"
#include "../rgb565.h"
#include <stdint.h>

/* Bilinear tap of a power-of-two RGB565 texture using interpolator `I`, set up
 * by qs_texmap_setup() (POP self-stepping) with the step loaded via
 * qs_texmap_step(). Reads the current (u,v) fractions, then a single POP returns
 * the current texel offset AND advances accum by (du,dv) — so the caller does
 * NOT call add_accumulator. `texw` = texture width in texels; `bytemask` =
 * texture_size_in_bytes - 1 (power of two) for neighbour wrap. */
static inline uint16_t qs_tap_bilerp(interp_hw_t *I, const uint8_t *base,
                                     int texw, uint32_t bytemask)
{
    int uf = (int)((interp_get_accumulator(I, 0) >> 8) & 0xFF);  /* before POP */
    int vf = (int)((interp_get_accumulator(I, 1) >> 8) & 0xFF);
    uint32_t off = interp_pop_full_result(I);    /* offset now, accum += du,dv */

    uint16_t c00 = *(const uint16_t *)(base + off);
    uint16_t c10 = *(const uint16_t *)(base + ((off + 2)            & bytemask));
    uint16_t c01 = *(const uint16_t *)(base + ((off + texw * 2)     & bytemask));
    uint16_t c11 = *(const uint16_t *)(base + ((off + texw * 2 + 2) & bytemask));

    int r0 = rgb565_r8(c00) + (((rgb565_r8(c10) - rgb565_r8(c00)) * uf) >> 8);
    int g0 = rgb565_g8(c00) + (((rgb565_g8(c10) - rgb565_g8(c00)) * uf) >> 8);
    int b0 = rgb565_b8(c00) + (((rgb565_b8(c10) - rgb565_b8(c00)) * uf) >> 8);
    int r1 = rgb565_r8(c01) + (((rgb565_r8(c11) - rgb565_r8(c01)) * uf) >> 8);
    int g1 = rgb565_g8(c01) + (((rgb565_g8(c11) - rgb565_g8(c01)) * uf) >> 8);
    int b1 = rgb565_b8(c01) + (((rgb565_b8(c11) - rgb565_b8(c01)) * uf) >> 8);
    return rgb565_pack(r0 + (((r1 - r0) * vf) >> 8),
                       g0 + (((g1 - g0) * vf) >> 8),
                       b0 + (((b1 - b0) * vf) >> 8));
}

/* Point (nearest) tap — cheaper; POP returns the offset and advances accum. */
static inline uint16_t qs_tap_point(interp_hw_t *I, const uint8_t *base)
{
    return *(const uint16_t *)(base + interp_pop_full_result(I));
}

/* Ordered 4x4 Bayer dither offset in [-8..+7], ~one 5-bit quantisation step,
 * to break banding on smooth gradients before rgb565_pack truncates. Add it to
 * each 8-bit channel: rgb565_pack(r+qs_dither(x,y), g+..., b+...). */
static const uint8_t qs_bayer4[16] = { 0,8,2,10, 12,4,14,6, 3,11,1,9, 15,7,13,5 };
static inline int qs_dither(int x, int y) { return (int)qs_bayer4[((y & 3) << 2) | (x & 3)] - 8; }

/* lerp two PIO-native RGB565 pixels, a..b by t/256. */
static inline uint16_t qs_lerp565(uint16_t a, uint16_t b, int t)
{
    int r = rgb565_r8(a) + (((rgb565_r8(b) - rgb565_r8(a)) * t) >> 8);
    int g = rgb565_g8(a) + (((rgb565_g8(b) - rgb565_g8(a)) * t) >> 8);
    int bl= rgb565_b8(a) + (((rgb565_b8(b) - rgb565_b8(a)) * t) >> 8);
    return rgb565_pack(r, g, bl);
}

#endif
