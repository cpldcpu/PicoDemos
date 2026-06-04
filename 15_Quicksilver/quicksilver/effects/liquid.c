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

    /* Liquid metal appears TWICE, in two distinct moods (keyed off the scene
     * start, threshold 100 s, like chrome/mode7):
     *   BUILD (0:54, the mid-groove break) — busy, fast, palette churning,
     *     ACCELERATING the whole way: a quicksilver riser into drop 1.
     *   BREAKDOWN (1:49) — dreamy suspended pooling that re-tensions only over
     *     its final third into the climax (chrome B). */
    float dur = (scene_cur_end_ms() - scene_cur_start_ms()) * 0.001f;
    if (dur < 1.0f) dur = 1.0f;
    float prog = t / dur; if (prog > 1.0f) prog = 1.0f;
    int   build = scene_cur_start_ms() < 100000u;

    float sc, ts, cx, cy; int poff;
    if (build) {
        sc   = 0.60f;                  /* finer, busier field                   */
        ts   = 1.5f + 1.4f * prog;     /* already fast, accelerating to the drop */
        cx   = 10.0f; cy = 8.0f;
        poff = (int)(t * 36.0f);       /* palette churn = energy                 */
    } else {
        float rise = prog < 0.66f ? 0.0f : (prog - 0.66f) / 0.34f;  /* 0 -> 1 late */
        sc   = 0.40f;                  /* dreamy, broad pools                    */
        ts   = 1.0f + 0.7f * rise;     /* flow quickens only into the drop       */
        cx   = 8.0f; cy = 6.0f;
        poff = 0;
    }

    /* coarse plasma field — summed travelling sines (the "liquid"). */
    for (int gy = 0; gy < GH; gy++) {
        for (int gx = 0; gx < GW; gx++) {
            float x = gx * sc, y = gy * sc;
            float f = sinf(x + t * 1.3f * ts)
                    + sinf(y * 1.1f - t * 0.9f * ts)
                    + sinf((x + y) * 0.7f + t * 1.7f * ts)
                    + sinf(sqrtf((x-cx)*(x-cx) + (y-cy)*(y-cy)) * 1.5f - t * 2.1f * ts);
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
            uint16_t c = s_chrome[(val + poff) & 0xFF];

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
