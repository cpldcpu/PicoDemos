/* liquid.c — QUICKSILVER "Liquid Metal" scene. A plasma intensity field is
 * computed on a coarse grid and bilinearly upscaled to full 320x240 using the
 * interpolator's BLEND unit (hardware linear interpolation) — three BLEND ops
 * per pixel (two horizontal, one vertical). The upscaled intensity is colourised
 * through a chrome gradient and motion-blurred against the previous frame, so
 * the field reads as flowing, pooling quicksilver.
 *
 * This is the demo's showcase of interp0 BLEND mode (the address-gen and CLAMP
 * units are shown in the rotozoom/Mode-7/chrome scenes). Builds identically on
 * host (emulator) and RP2350 (raw SIO).
 */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "qs_fx.h"

#include <math.h>

#define GW 41            /* coarse grid: (GW-1)=40 divides 320 -> 8px cells */
#define GH 31            /* (GH-1)=30 divides 240 -> 8px cells              */
#define CELL 8

static uint8_t  s_grid[GW * GH];
static uint16_t s_chrome[256];

static void liquid_init(void)
{
    /* interp0 BLEND: lane1 result = base0 + (base1-base0)*alpha/256, alpha =
     * 8 LSBs of accum1. Configure once; we just feed base0/base1/accum1. */
    interp_config c0 = interp_default_config();
    interp_config_set_blend(&c0, true);
    interp_set_config(interp0, 0, &c0);
    interp_config c1 = interp_default_config();
    interp_set_config(interp0, 1, &c1);

    /* chrome gradient: steel shadow -> bright silver, with sharp specular
     * sheen bands so the pooling plasma reads as molten mercury. */
    for (int i = 0; i < 256; i++) {
        float s = i / 255.0f;
        float base = powf(s, 0.7f);                       /* lift midtones    */
        float sb = 0.5f + 0.5f * sinf(s * 11.0f);
        float sheen = sb * sb * sb;                       /* sharp bright bands */
        int r = (int)(35 + 175 * base + 110 * sheen);
        int g = (int)(50 + 180 * base + 110 * sheen);
        int b = (int)(80 + 170 * base +  95 * sheen);
        s_chrome[i] = rgb565_pack(r, g, b);
    }
}

static inline int hw_blend(int a, int b, int alpha)
{
    interp_set_base(interp0, 0, (uint32_t)a);
    interp_set_base(interp0, 1, (uint32_t)b);
    interp_set_accumulator(interp0, 1, (uint32_t)alpha);
    return (int)interp_peek_lane_result(interp0, 1);
}

static void liquid_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    float t = t_ms * 0.001f;

    /* coarse plasma field — summed travelling sines (the "liquid"). */
    for (int gy = 0; gy < GH; gy++) {
        for (int gx = 0; gx < GW; gx++) {
            float x = gx * 0.40f, y = gy * 0.40f;
            float f = sinf(x + t * 1.3f)
                    + sinf(y * 1.1f - t * 0.9f)
                    + sinf((x + y) * 0.7f + t * 1.7f)
                    + sinf(sqrtf((x-8)*(x-8) + (y-6)*(y-6)) * 1.5f - t * 2.1f);
            int v = (int)((f * 0.25f + 0.5f) * 255.0f);
            s_grid[gy * GW + gx] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    }

    uint16_t *fb = vga_hires_back_buffer();
    for (int y = 0; y < VGA_HIRES_H; y++) {
        int gy = y / CELL, fy = (y % CELL) * (256 / CELL);
        const uint8_t *r0 = &s_grid[gy * GW];
        const uint8_t *r1 = &s_grid[(gy + 1 < GH ? gy + 1 : gy) * GW];
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int gx = x / CELL, fx = (x % CELL) * (256 / CELL);
            int top = hw_blend(r0[gx], r0[gx + 1], fx);     /* HW bilinear: H */
            int bot = hw_blend(r1[gx], r1[gx + 1], fx);
            int val = hw_blend(top, bot, fy);               /* HW bilinear: V */
            uint16_t c = s_chrome[val & 0xFF];

            uint16_t o = row[x];                            /* motion blur trail */
            int d = qs_dither(x, y);
            int r = rgb565_r8(c) + (((rgb565_r8(o) - rgb565_r8(c)) * 96) >> 8) + d;
            int g = rgb565_g8(c) + (((rgb565_g8(o) - rgb565_g8(c)) * 96) >> 8) + d;
            int b = rgb565_b8(c) + (((rgb565_b8(o) - rgb565_b8(c)) * 96) >> 8) + d;
            row[x] = rgb565_pack(r, g, b);
        }
    }
}

const effect_t fx_liquid = {
    .name  = "liquid",
    .mode  = MODE_HIRES,
    .init  = liquid_init,
    .frame = liquid_frame,
    .done  = NULL,
};
