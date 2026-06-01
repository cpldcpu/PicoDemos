/* credits.c — QUICKSILVER grand finale. A single hero shot that stacks THREE
 * interpolator techniques: a Mode-7 mercury plain under the chrome sky panorama,
 * a slowly rotating chrome hero object (matcap env-map) floating above the
 * horizon, and the chrome wordmark + credits scrolling up over it — then a
 * gentle fade to black to end the demo.
 */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "assets.h"
#include "qs_text.h"
#include "qs_fx.h"
#include "envmap3d.h"

#include <math.h>

#define HORIZON 132
#define GMASK   (ASSET_GROUND_W * ASSET_GROUND_H * 2 - 1)

static qs_mesh s_hero;

static const char *lines[] = {
    "A RP2350 INTERPOLATOR DEMO",
    "",
    "CODE AND DIRECTION",
    "   CLAUDE OPUS 4.8",
    "CRITIC     AZURE (HUMAN)",
    "GRAPHICS   GEMINI 3.5 FLASH",
    "           WITH NANO BANANA 2",
    "MUSIC      SUNO 4.5", 
    "           PROMPTED BY OPUS 4.8",
    "",
    "EVERY PIXEL HW-ACCELERATED",
    "BY THE SIO INTERPOLATOR",
    "",
    "AFFINE . BILINEAR BLEND . CLAMP",
    "ROTOZOOM . MODE-7 . CHROME",
    "",
    "GREETINGS TO THE SCENE",
    "",
    "MERCURY",
    "2026",
    NULL,
};

static void credits_init(void)
{
    qs_texmap_setup(interp0, 1, 8, 8);
    s_hero = (qs_mesh){ KNOT_v, KNOT_n, KNOT_t, KNOT_NV, KNOT_NT };
}

/* Mode-7 mercury sunset backdrop (sky pano + reflective floor). */
static void backdrop(float t)
{
    uint16_t *fb = vga_hires_back_buffer();
    const uint8_t  *g   = (const uint8_t *)asset_ground_data;
    const uint16_t *sky = (const uint16_t *)asset_sky_data;

    int scroll = (int)(t * 6.0f);
    for (int y = 0; y < HORIZON; y++) {
        int srow = y * 84 / HORIZON; if (srow > ASSET_SKY_H - 1) srow = ASSET_SKY_H - 1;
        const uint16_t *src = sky + srow * ASSET_SKY_W;
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) row[x] = src[(scroll + x) & (ASSET_SKY_W - 1)];
    }
    float camY = t * 18.0f;
    for (int y = HORIZON; y < VGA_HIRES_H; y++) {
        float p = (float)(y - HORIZON) + 0.75f;
        float dist = (40.0f * 120.0f) / p;
        float stepx = dist / 120.0f;
        float u0 = 128.0f + dist + (0 - 160) * stepx;
        float v0 = camY + (0 - 160) * 0.0f;
        interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)(u0 * 65536.0f));
        interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)((camY + dist) * 65536.0f));
        qs_texmap_step(interp0, (uint32_t)(int32_t)(stepx * 65536.0f), 0);  /* u steps, v fixed */
        int dim = 90 + (y - HORIZON) * 90 / (VGA_HIRES_H - HORIZON);
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            uint16_t c = qs_tap_point(interp0, g);   /* POPs: advances accum0 by du */
            int d = qs_dither(x, y);
            row[x] = rgb565_pack((rgb565_r8(c) * dim >> 8) + d,
                                 (rgb565_g8(c) * dim >> 8) + d,
                                 (rgb565_b8(c) * dim >> 8) + d);
        }
    }
}

static void credits_frame(uint32_t t_ms, uint32_t t_global)
{
    float t = t_ms * 0.001f;

    backdrop(t);

    /* chrome hero object floating above the horizon, slow majestic spin */
    qs_env_params p; qs_env_default(&p);
    p.env = (const uint8_t *)asset_envmap_data;
    p.envW = ASSET_ENVMAP_W; p.envH = ASSET_ENVMAP_H;
    p.yaw = t * 0.45f; p.pitch = 0.35f * sinf(t * 0.3f); p.roll = t * 0.15f;
    p.oy = 0.85f; p.oz = 4.6f; p.focal = 300.f;
    p.scale = 1.35f;
    qs_envmap_render(&s_hero, &p);

    /* scrolling reel: wordmark, then credit lines. Slow, and CLAMPED so the
     * final card (MERCURY / 2026) rises to centre and HOLDS rather than
     * scrolling off into emptiness — then the whole scene fades to black. */
    int scroll = (int)(t_ms * 0.0085f);
    if (scroll > 470) scroll = 470;
    int y = VGA_HIRES_H + 30 - scroll;
    if (y > -QS_LOGO_H && y < VGA_HIRES_H) {
        int sweepx = (int)(fmodf(t * 200.0f, (float)(VGA_HIRES_W + 120))) - 60;
        qs_logo_blit(0, y, sweepx);
    }
    y += QS_LOGO_H + 16;
    for (int i = 0; lines[i]; i++) {
        const char *s = lines[i];
        if (*s) {
            int w = qs_text_w(s, 1);
            if (y > -10 && y < VGA_HIRES_H)
                qs_text_chrome(s, (VGA_HIRES_W - w) / 2, y, 1, 50);
            y += 8 + 9;
        } else {
            y += 11;
        }
    }

    /* gentle fade to black over the last 5 s of the demo — a clear ending */
    uint32_t end = scene_cur_end_ms();
    uint32_t left = end > t_global ? end - t_global : 0;
    if (left < 5000) {
        int k = 256 - (int)(left * 256 / 5000);     /* 0..256 darkness */
        uint16_t *fb = vga_hires_back_buffer();
        int n = VGA_HIRES_W * VGA_HIRES_H;
        for (int i = 0; i < n; i++) {
            uint16_t c = fb[i];
            fb[i] = rgb565_pack(rgb565_r8(c) * (256 - k) >> 8,
                                rgb565_g8(c) * (256 - k) >> 8,
                                rgb565_b8(c) * (256 - k) >> 8);
        }
    }
}

const effect_t fx_credits = {
    .name  = "credits",
    .mode  = MODE_HIRES,
    .init  = credits_init,
    .frame = credits_frame,
    .done  = NULL,
};
