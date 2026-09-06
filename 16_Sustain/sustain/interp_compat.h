/* interp_compat.h — one header that gives effect code the RP2350 interpolator
 * API on both targets. On device it's the raw SIO peripheral; on the SDL host
 * it's the bit-exact software emulator in interp_emu.{h,c}.
 *
 * Effect code includes ONLY this header and uses ONLY the function API — never
 * raw interp->peek[] struct access — so the host emulator can intercept every
 * operation and the preview matches the hardware.
 *
 * Portability note: interp_peek_full_result() returns a uint32_t. Device
 * pointers are 32-bit so it can carry a real texel ADDRESS, but host pointers
 * are 64-bit. To keep ONE code path, QUICKSILVER uses BASE-RELATIVE addressing:
 * base[2]=0, so FULL yields a 32-bit byte OFFSET into the texture (with the
 * interpolator doing the col*bpp + row*stride + wrap math in hardware), and the
 * effect adds its own texture base pointer. The RP2350 still does the real
 * interpolator work; only a single constant-pointer add stays in C.
 */

#ifndef QS_INTERP_COMPAT_H
#define QS_INTERP_COMPAT_H

#ifdef HOST_BUILD
#  include "interp_emu.h"
#else
#  include "hardware/interp.h"
#endif

#include <stdint.h>

/* Configure `interp` for affine texture-address (offset) generation. After
 * this, with accum0 = u and accum1 = v in 16.16 fixed point:
 *
 *     uint32_t off = interp_peek_full_result(interp);
 *     uint16_t texel = *(const uint16_t *)((const uint8_t *)tex + off);
 *     interp_add_accumulator(interp, 0, du);
 *     interp_add_accumulator(interp, 1, dv);
 *
 * `off` = ((v>>16 & (H-1)) * W + (u>>16 & (W-1))) * bpp, wrapping on the texture
 * dimensions for free (the mask discards the integer bits above the texture
 * size). Constraints: power-of-two W,H; log2bpp+log2w <= 16.
 *
 *   log2bpp : log2(bytes per texel)   — 1 for RGB565
 *   log2w   : log2(texture width)     — 8 for 256
 *   log2h   : log2(texture height)    — 8 for 256
 *
 * POP self-stepping: both lanes also enable ADD_RAW, so each lane's RESULT is
 * base+accum (the FULL result is unaffected and still uses shift+mask). With
 * the per-pixel step du/dv placed in base0/base1 via qs_texmap_step(), a single
 * interp_pop_full_result() returns the current texel offset AND advances
 * accum0+=du, accum1+=dv — the canonical RP2350 affine-texture loop, one SIO
 * read per pixel and no explicit add_accumulator().
 */
static inline void qs_texmap_setup(interp_hw_t *interp, int log2bpp, int log2w, int log2h)
{
    int log2stride = log2bpp + log2w;

    interp_config c0 = interp_default_config();
    interp_config_set_shift(&c0, (uint)(16 - log2bpp));
    interp_config_set_mask (&c0, (uint)log2bpp, (uint)(log2bpp + log2w - 1));
    interp_config_set_add_raw(&c0, true);    /* lane0 result = du + accum0 (POP step) */
    interp_set_config(interp, 0, &c0);

    interp_config c1 = interp_default_config();
    interp_config_set_shift(&c1, (uint)(16 - log2stride));
    interp_config_set_mask (&c1, (uint)log2stride, (uint)(log2stride + log2h - 1));
    interp_config_set_add_raw(&c1, true);    /* lane1 result = dv + accum1 (POP step) */
    interp_set_config(interp, 1, &c1);

    interp_set_base(interp, 2, 0);   /* base-relative: FULL yields a byte offset */
}

/* Set the per-pixel step (du,dv in 16.16) for the next run of pops. Call once
 * per scanline after seeding accum0=u0, accum1=v0. */
static inline void qs_texmap_step(interp_hw_t *interp, uint32_t du, uint32_t dv)
{
    interp_set_base(interp, 0, du);
    interp_set_base(interp, 1, dv);
}

#endif /* QS_INTERP_COMPAT_H */
