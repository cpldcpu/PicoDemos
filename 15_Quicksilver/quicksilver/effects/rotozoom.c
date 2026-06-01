/* rotozoom.c — fullscreen bilinear rotozoomer driven by the RP2350
 * interpolator (QUICKSILVER scene "Rubber Rotozoomer").
 *
 * The interpolator (interp0) is configured by qs_texmap_setup() as an affine
 * texture-address generator: with accum0=u, accum1=v in 16.16 fixed point,
 * interp_peek_full_result() returns the byte OFFSET of the texel in one read,
 * with free power-of-two wrap. We add the per-pixel du/dv with
 * interp_add_accumulator() — the inner loop never computes a texture address
 * in C. Bilinear weights come straight from the accumulator low bits.
 *
 * "Rubber": each scanline's affine scale is wobbled by a travelling sine, so
 * the plane flexes like a rubber sheet. Motion blur: each pixel is blended with
 * what the back buffer already holds (the frame from two flips ago).
 *
 * Identical C on host (emulator) and RP2350 (raw SIO). MODE_HIRES, 320x240.
 */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "assets.h"

#include <math.h>

#define TW   ASSET_ROTO_W                  /* 256 */
#define TH   ASSET_ROTO_H                  /* 256 */
#define TMASK (TW * TH * 2 - 1)            /* byte-offset wrap mask (pow2) */

static void roto_init(void)
{
    qs_texmap_setup(interp0, 1, 8, 8);     /* RGB565 (log2bpp=1), 256x256 */
}

/* bilinear tap of the roto texture at the interpolator's current (u,v). */
/* fracs must be read by the caller BEFORE this (POP advances the accumulators) */
static inline uint16_t roto_tap(const uint8_t *base, int uf, int vf)
{
    uint32_t off = interp_pop_full_result(interp0);   /* offset now, accum += du,dv */
    uint16_t c00 = *(const uint16_t *)(base + off);
    uint16_t c10 = *(const uint16_t *)(base + ((off + 2)       & TMASK));
    uint16_t c01 = *(const uint16_t *)(base + ((off + TW * 2)  & TMASK));
    uint16_t c11 = *(const uint16_t *)(base + ((off + TW * 2 + 2) & TMASK));

    int r0 = rgb565_r8(c00) + (((rgb565_r8(c10) - rgb565_r8(c00)) * uf) >> 8);
    int g0 = rgb565_g8(c00) + (((rgb565_g8(c10) - rgb565_g8(c00)) * uf) >> 8);
    int b0 = rgb565_b8(c00) + (((rgb565_b8(c10) - rgb565_b8(c00)) * uf) >> 8);
    int r1 = rgb565_r8(c01) + (((rgb565_r8(c11) - rgb565_r8(c01)) * uf) >> 8);
    int g1 = rgb565_g8(c01) + (((rgb565_g8(c11) - rgb565_g8(c01)) * uf) >> 8);
    int b1 = rgb565_b8(c01) + (((rgb565_b8(c11) - rgb565_b8(c01)) * uf) >> 8);
    int r = r0 + (((r1 - r0) * vf) >> 8);
    int g = g0 + (((g1 - g0) * vf) >> 8);
    int b = b0 + (((b1 - b0) * vf) >> 8);
    return rgb565_pack(r, g, b);
}

static void roto_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    uint16_t *fb = vga_hires_back_buffer();
    const uint8_t *base = (const uint8_t *)asset_roto_data;

    float t = t_ms * 0.001f;
    float ang   = t * 0.55f;                     /* steady spin            */
    float zoom  = 1.30f + 0.85f * sinf(t * 0.40f);/* breathing zoom         */
    float ctu   = 128.0f + 40.0f * sinf(t * 0.23f);
    float ctv   = 128.0f + 40.0f * cosf(t * 0.19f);
    int   blur  = 110;                            /* 0..255 trail strength  */

    float cosA = cosf(ang), sinA = sinf(ang);

    for (int y = 0; y < VGA_HIRES_H; y++) {
        /* rubber: travelling sine flexes this scanline's scale */
        float flex  = 1.0f + 0.18f * sinf(y * 0.045f + t * 2.1f);
        float scale = zoom * flex;
        float ca = cosA * scale;
        float sa = sinA * scale;

        float dys = (float)(y - 120);
        float u0 = ctu + (0 - 160) * ca - dys * sa;
        float v0 = ctv + (0 - 160) * sa + dys * ca;

        interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)(u0 * 65536.0f));
        interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)(v0 * 65536.0f));
        qs_texmap_step(interp0, (uint32_t)(int32_t)(ca * 65536.0f),
                                (uint32_t)(int32_t)(sa * 65536.0f));

        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int uf = (interp_get_accumulator(interp0, 0) >> 8) & 0xFF;
            int vf = (interp_get_accumulator(interp0, 1) >> 8) & 0xFF;
            uint16_t c = roto_tap(base, uf, vf);

            if (blur) {
                uint16_t o = row[x];
                int r = rgb565_r8(c) + (((rgb565_r8(o) - rgb565_r8(c)) * blur) >> 8);
                int g = rgb565_g8(c) + (((rgb565_g8(o) - rgb565_g8(c)) * blur) >> 8);
                int b = rgb565_b8(c) + (((rgb565_b8(o) - rgb565_b8(c)) * blur) >> 8);
                c = rgb565_pack(r, g, b);
            }
            row[x] = c;     /* roto_tap POPs: advances accum by (du,dv) */
        }
    }
}

const effect_t fx_rotozoom = {
    .name  = "rotozoom",
    .mode  = MODE_HIRES,
    .init  = roto_init,
    .frame = roto_frame,
    .done  = NULL,
};
