/* mode7.c — infinite reflective mercury plain + chrome sky, now NATIVE FULL VGA
 * (640x480) and beam-raced (no framebuffer). Per scanline core 1 either samples
 * the sky panorama (above the horizon) or runs the interpolator POP affine over
 * the ground (below), with interp1 CLAMP folding the distance-haze weight in
 * hardware. Textures live in race SRAM; the generator is pinned in SRAM.
 * Identical on host (emulator) and RP2350 (raw SIO). */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "assets.h"
#include "qs_fx.h"
#include "transition.h"

#include <math.h>
#include <string.h>

#ifdef PICO_BUILD
#include "pico/platform.h"
#define QS_FAST(n) __not_in_flash_func(n)
#else
#define QS_FAST(n) n
#endif

#define HZ     200            /* horizon row in 640x480 */
#define F640   240.0f
#define CAMH   55.0f
#define GBYTES (ASSET_GROUND_W * ASSET_GROUND_H * 2)   /* 131072 */
#define HAZE_R 150
#define HAZE_G 170
#define HAZE_B 205

static uint16_t      *s_ground, *s_sky;
static volatile float g_cosH, g_sinH, g_camX, g_camY;
static volatile int   g_scroll;
static volatile int   g_white;          /* chrome-glint transition amount */

/* one-time config on the scanout core: interp0 affine (POP). */
static void m7_setup(void)
{
    qs_texmap_setup(interp0, 1, 8, 8);
}

static void QS_FAST(m7_scan)(uint16_t *dst, int y)
{
    if (g_white > 200) {                           /* cheap chrome-white flash */
        uint16_t w = rgb565_pack(232, 240, 255);
        for (int x = 0; x < VGA_RACE_W; x++) dst[x] = w;
        return;
    }
    if (y < HZ) {                                  /* --- sky panorama --- */
        int srow = y * 100 / HZ; if (srow > ASSET_SKY_H - 1) srow = ASSET_SKY_H - 1;
        const uint16_t *src = s_sky + srow * ASSET_SKY_W;
        int sc = g_scroll;
        for (int x = 0; x < VGA_RACE_W; x++) dst[x] = src[(sc + x) & (ASSET_SKY_W - 1)];
        return;
    }
    /* --- ground: interpolator affine (interp0) + CLAMP haze (interp1) --- */
    float p = (float)(y - HZ) + 0.75f;
    float dist = (CAMH * F640) / p;
    float cosH = g_cosH, sinH = g_sinH;
    float stepx = (dist / F640) * (-sinH), stepy = (dist / F640) * cosH;
    float u0 = g_camX + dist * cosH + (float)(-VGA_RACE_W / 2) * stepx;
    float v0 = g_camY + dist * sinH + (float)(-VGA_RACE_W / 2) * stepy;
    interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)(u0 * 65536.0f));
    interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)(v0 * 65536.0f));
    qs_texmap_step(interp0, (uint32_t)(int32_t)(stepx * 65536.0f),
                            (uint32_t)(int32_t)(stepy * 65536.0f));

    const uint8_t *base = (const uint8_t *)s_ground;
    for (int x = 0; x < VGA_RACE_W; x++)
        dst[x] = qs_tap_point(interp0, base);   /* POP self-steps; SRAM ground */
}

static void m7_init(void)
{
    uint8_t *sram = vga_race_sram();
    s_ground = (uint16_t *)sram;
    s_sky    = (uint16_t *)(sram + GBYTES);
    memcpy(s_ground, asset_ground_data, GBYTES);
    memcpy(s_sky,    asset_sky_data,    ASSET_SKY_W * ASSET_SKY_H * 2);
    vga_set_race_fn(m7_scan, m7_setup);
}

static void m7_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    float t = t_ms * 0.001f;
    float head = 0.25f * sinf(t * 0.13f);
    g_cosH = cosf(head);
    g_sinH = sinf(head);
    g_camX = 128.0f;
    g_camY = t * 90.0f;
    g_scroll = (int)(head * (ASSET_SKY_W / 6.2831853f) + g_camY * 0.15f);
    g_white = qs_trans_white(t_global, scene_cur_start_ms(), scene_cur_end_ms(), 0);
}

const effect_t fx_mode7 = {
    .name  = "mode7",
    .mode  = MODE_RACE,
    .init  = m7_init,
    .frame = m7_frame,
    .done  = NULL,
};
