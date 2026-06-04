/* mode7.c — infinite reflective mercury plain + chrome sky (QUICKSILVER scene
 * "Mercury Plain").
 *
 * Rendered at QVGA (320x240) into the framebuffer on core 0 (then 2x-doubled to
 * 640x480 at scanout). Beam-racing it at native 640 left no per-pixel budget for
 * filtering, so the far ground aliased badly; at QVGA we can afford BILINEAR
 * sampling AND distance FOG that blends the ground into the sky at the horizon
 * (the fog target is the actual sky pixel for each column, so the plain melts
 * seamlessly into the background). interp0 = affine address-gen (POP);
 * interp1 = CLAMP for the per-row fog weight. */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "../scene_scratch.h"
#include "assets.h"
#include "qs_fx.h"

#include <math.h>

#ifdef PICO_BUILD
#include "pico/platform.h"
#define QS_FAST(n) __not_in_flash_func(n)   /* pin the hot per-pixel loop in SRAM */
#else
#define QS_FAST(n) n
#endif

#define HORIZON 96
#define F120    120.0f
#define CAMH    55.0f
#define GW128   128                            /* ground tile copied to SRAM */
#define GMASK   (GW128 * GW128 * 2 - 1)

static uint16_t s_hz[VGA_HIRES_W];     /* sky colour at the horizon, per column */
static uint16_t *s_ground;             /* 128x128 mercury tile in SRAM          */

/* Warm copper/sunset re-grade for the second pass ("victory lap"), so it reads
 * as a different mood instead of a repeat of the cool-silver groove cruise. */
static inline uint16_t m7_warm(uint16_t c)
{
    int r = rgb565_r8(c), g = rgb565_g8(c), b = rgb565_b8(c);
    r += (r >> 2) + 18; if (r > 255) r = 255;   /* push amber  */
    b -= (b >> 2);      if (b < 0)   b = 0;      /* cool out of the highlights */
    return rgb565_pack(r, g, b);
}

static void m7_init(void)
{
    /* Copy a 128x128 ground tile into SRAM — bilinear from XIP flash (4 taps/px)
     * was the per-pixel cost that broke 60 fps; from RAM it's cheap. */
    s_ground = (uint16_t *)g_scratch.bg_cache;
    const uint16_t *src = (const uint16_t *)asset_ground_data;
    for (int y = 0; y < GW128; y++)
        for (int x = 0; x < GW128; x++)
            s_ground[y * GW128 + x] = src[(y * 2) * ASSET_GROUND_W + x * 2];

    qs_texmap_setup(interp0, 1, 7, 7);                 /* ground affine, POP, 128 */
    interp_config c = interp_default_config();         /* interp1 = CLAMP fog */
    interp_config_set_clamp(&c, true);
    interp_config_set_signed(&c, true);
    interp_set_config(interp1, 0, &c);
    interp_set_base(interp1, 0, 0);
    interp_set_base(interp1, 1, 255);
}

static void QS_FAST(m7_frame)(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    uint16_t *fb = vga_hires_back_buffer();
    const uint8_t  *gbase = (const uint8_t *)s_ground;    /* SRAM 128 tile */
    const uint16_t *sky   = (const uint16_t *)asset_sky_data;

    /* Mercury-plain cruise — appears twice, but the two passes are deliberately
     * DIFFERENT scenes, not a repeat:
     *   v1 GROOVE       : calm silver cruise, gentle heading drift, eye-height cam.
     *   v2 VICTORY LAP  : flown LOW and FAST with hard banking S-curves over the
     *                     opposite face of the sky panorama, and re-graded to a
     *                     warm copper sunset (m7_warm) — a clearly distinct mood. */
    int   v2     = scene_cur_start_ms() > 100000u;
    float spd    = v2 ? 158.0f : 90.0f;
    float swing  = v2 ? 0.46f  : 0.25f;   /* banking amplitude  */
    float swingf = v2 ? 0.34f  : 0.13f;   /* banking frequency  */
    float camh   = v2 ? 33.0f  : CAMH;    /* lower => ground rushes by faster */
    int   skyoff = v2 ? (ASSET_SKY_W / 2) : 0;  /* opposite face of the pano */

    float t    = t_ms * 0.001f;
    float head = swing * sinf(t * swingf);
    float cosH = cosf(head), sinH = sinf(head);
    float camX = 128.0f, camY = t * spd;
    int   scroll = (int)(head * (ASSET_SKY_W / 6.2831853f) + camY * 0.15f) + skyoff;

    /* --- sky (above the horizon); remember the horizon row as the fog target */
    for (int y = 0; y < HORIZON; y++) {
        int srow = y * 70 / HORIZON; if (srow > ASSET_SKY_H - 1) srow = ASSET_SKY_H - 1;
        const uint16_t *src = sky + srow * ASSET_SKY_W;
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            uint16_t c = src[(scroll + x) & (ASSET_SKY_W - 1)];
            if (v2) c = m7_warm(c);
            row[x] = c;
            if (y == HORIZON - 1) s_hz[x] = c;
        }
    }

    /* --- ground (below the horizon): affine + bilinear + fog into the sky --- */
    for (int y = HORIZON; y < VGA_HIRES_H; y++) {
        float p = (float)(y - HORIZON) + 0.75f;
        float dist = (camh * F120) / p;
        float stepx = (dist / F120) * (-sinH), stepy = (dist / F120) * cosH;
        float u0 = camX + dist * cosH + (0 - 160) * stepx;
        float v0 = camY + dist * sinH + (0 - 160) * stepy;
        interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)(u0 * 65536.0f));
        interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)(v0 * 65536.0f));
        qs_texmap_step(interp0, (uint32_t)(int32_t)(stepx * 65536.0f),
                                (uint32_t)(int32_t)(stepy * 65536.0f));

        /* per-row fog weight (heavy near the horizon, gone by mid-screen) */
        interp_set_accumulator(interp1, 0, (uint32_t)(int32_t)(290.0f - p * 2.6f));
        int fog = (int)interp_peek_lane_result(interp1, 0);

        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            uint16_t c = qs_tap_bilerp(interp0, gbase, GW128, GMASK);
            if (v2) c = m7_warm(c);
            if (fog) {
                uint16_t f = s_hz[x];            /* blend toward the sky above */
                c = rgb565_pack(rgb565_r8(c) + (((rgb565_r8(f) - rgb565_r8(c)) * fog) >> 8),
                                rgb565_g8(c) + (((rgb565_g8(f) - rgb565_g8(c)) * fog) >> 8),
                                rgb565_b8(c) + (((rgb565_b8(f) - rgb565_b8(c)) * fog) >> 8));
            }
            row[x] = c;
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
