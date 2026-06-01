/* mode7.c — infinite reflective mercury plain receding to a horizon, with a
 * scrolling chrome sky panorama above it (QUICKSILVER scene "Mercury Plain").
 *
 * The textbook interpolator use: per scanline below the horizon we compute the
 * affine basis (u0,v0,du,dv) for that row's perspective depth, load it into
 * interp0, then the inner loop is one peek_full_result() per pixel for the
 * tiled-ground texel offset (free pow2 wrap) plus a bilinear tap. interp1 runs
 * in CLAMP mode to fold the distance-haze factor toward the horizon in hardware.
 *
 * MODE_HIRES, 320x240. Identical on host (emulator) and RP2350 (raw SIO).
 */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "assets.h"
#include "qs_fx.h"

#include <math.h>

#define HORIZON     96
#define GROUND_BYTES (ASSET_GROUND_W * ASSET_GROUND_H * 2)
#define GMASK        (GROUND_BYTES - 1)

/* horizon haze colour (cool steel toward infinity) */
#define HAZE_R 150
#define HAZE_G 170
#define HAZE_B 205

static void m7_init(void)
{
    qs_texmap_setup(interp0, 1, 8, 8);   /* ground RGB565 256x256 */

    /* interp1 CLAMP: maps a row's raw "near-ness" (0..255) into a fog blend
     * weight clamped to [0,255]. Showcases the CLAMP unit alongside interp0. */
    interp_config c = interp_default_config();
    interp_config_set_clamp(&c, true);
    interp_config_set_signed(&c, true);
    interp_set_config(interp1, 0, &c);
    interp_set_base(interp1, 0, 0);
    interp_set_base(interp1, 1, 255);
}

static void m7_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    uint16_t *fb = vga_hires_back_buffer();
    const uint8_t *gbase = (const uint8_t *)asset_ground_data;
    const uint16_t *sky  = (const uint16_t *)asset_sky_data;

    float t      = t_ms * 0.001f;
    float camX   = 128.0f;
    float camY   = t * 90.0f;                 /* fly forward over the mercury */
    float head   = 0.25f * sinf(t * 0.13f);   /* gentle weave                 */
    float cosH = cosf(head), sinH = sinf(head);
    const float F = 120.0f, camH = 55.0f;

    /* --- sky panorama (rows above the horizon), point-sampled scroll ----- */
    int scroll = (int)(head * (ASSET_SKY_W / 6.2831853f) + camY * 0.15f);
    for (int y = 0; y < HORIZON; y++) {
        int srow = y * 70 / HORIZON;                  /* upper band of the pano */
        if (srow > ASSET_SKY_H - 1) srow = ASSET_SKY_H - 1;
        const uint16_t *srcrow = sky + srow * ASSET_SKY_W;
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++)
            row[x] = srcrow[(scroll + x) & (ASSET_SKY_W - 1)];
    }

    /* --- ground (rows below the horizon), interpolator affine per scanline */
    for (int y = HORIZON; y < VGA_HIRES_H; y++) {
        float p = (float)(y - HORIZON) + 0.75f;
        float dist = (camH * F) / p;                  /* depth of this row     */

        float gx = camX + dist * cosH;
        float gy = camY + dist * sinH;
        float stepx = (dist / F) * (-sinH);           /* per-pixel right step  */
        float stepy = (dist / F) * ( cosH);
        float u0 = gx + (0 - 160) * stepx;
        float v0 = gy + (0 - 160) * stepy;

        interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)(u0 * 65536.0f));
        interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)(v0 * 65536.0f));
        qs_texmap_step(interp0, (uint32_t)(int32_t)(stepx * 65536.0f),
                                (uint32_t)(int32_t)(stepy * 65536.0f));

        /* haze weight for this row via interp1 CLAMP: near rows -> 0 (no haze),
         * far rows (small p) -> up to 255 (full haze). */
        interp_set_accumulator(interp1, 0, (uint32_t)(int32_t)(255 - (p * 3.2f)));
        int haze = (int)interp_peek_lane_result(interp1, 0);

        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            uint16_t c = qs_tap_bilerp(interp0, gbase, ASSET_GROUND_W, GMASK);
            if (haze) {
                int r = rgb565_r8(c) + (((HAZE_R - rgb565_r8(c)) * haze) >> 8);
                int g = rgb565_g8(c) + (((HAZE_G - rgb565_g8(c)) * haze) >> 8);
                int b = rgb565_b8(c) + (((HAZE_B - rgb565_b8(c)) * haze) >> 8);
                c = rgb565_pack(r, g, b);
            }
            row[x] = c;     /* qs_tap_bilerp POPs: advances accum by (du,dv) */
        }
    }
}

const effect_t fx_mode7 = {
    .name  = "mode7",
    .mode  = MODE_HIRES,
    .init  = m7_init,
    .frame = m7_frame,
    .done  = NULL,
};
