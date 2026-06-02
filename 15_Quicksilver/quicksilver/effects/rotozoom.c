/* rotozoom.c — fullscreen bilinear/point rotozoomer, now NATIVE FULL VGA
 * (640x480) and beam-raced: no framebuffer. The scene's frame() (core 0)
 * updates the affine basis + per-row "rubber" flex; the registered per-scanline
 * generator (core 1 on device / the present loop on host) turns each row into
 * 640 pixels via the interpolator in POP self-stepping mode — one pop_full per
 * pixel returns the texel offset and advances u,v.
 *
 * Beam-racing discipline: texture copied to SRAM (vga_race_sram, aliases the
 * framebuffer arena), generator pinned in SRAM, no transcendentals on the hot
 * path. Identical on host (emulator) and RP2350 (raw SIO). */

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

#define TW ASSET_ROTO_W            /* 256 */
#define TBYTES (ASSET_ROTO_W * ASSET_ROTO_H * 2)

static uint16_t     *s_tex;                  /* roto in race SRAM */
static volatile float g_ca0, g_sa0, g_cu, g_cv;
static volatile int   g_white;               /* chrome-glint transition amount */
static float          g_flex[VGA_RACE_H];

static void roto_setup(void) { qs_texmap_setup(interp0, 1, 8, 8); }  /* once, on scanout core */

static void QS_FAST(roto_scan)(uint16_t *dst, int y)
{
    /* Cheap chrome-white flash near a boundary — a fill, NOT a per-pixel lerp
     * (the lerp blew the 640-wide per-line budget and caused underrun exactly
     * at scene start). */
    if (g_white > 200) {
        uint16_t w = rgb565_pack(232, 240, 255);
        for (int x = 0; x < VGA_RACE_W; x++) dst[x] = w;
        return;
    }
    float fl = g_flex[y];
    float ca = g_ca0 * fl, sa = g_sa0 * fl;
    float dys = (float)(y - VGA_RACE_H / 2);
    float u0 = g_cu + (float)(-VGA_RACE_W / 2) * ca - dys * sa;
    float v0 = g_cv + (float)(-VGA_RACE_W / 2) * sa + dys * ca;
    interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)(u0 * 65536.0f));
    interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)(v0 * 65536.0f));
    qs_texmap_step(interp0, (uint32_t)(int32_t)(ca * 65536.0f),
                            (uint32_t)(int32_t)(sa * 65536.0f));
    const uint8_t *base = (const uint8_t *)s_tex;
    for (int x = 0; x < VGA_RACE_W; x++)
        dst[x] = qs_tap_point(interp0, base);   /* POP: offset + auto-advance */
}

static void roto_init(void)
{
    s_tex = (uint16_t *)vga_race_sram();
    memcpy(s_tex, asset_roto_data, TBYTES);    /* flash -> SRAM (arena alias) */
    vga_set_race_fn(roto_scan, roto_setup);
}

static void roto_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    float t = t_ms * 0.001f;

    /* The rotozoomer recurs; the second pass spins the OTHER way, faster, more
     * zoomed-in, with a deeper rubber warp so it doesn't read as a repeat.
     * (All cheap core-0 setup; the beam-raced scan loop is untouched.) */
    int   v1   = scene_cur_start_ms() > 50000u;
    float ang  = t * (v1 ? -0.85f : 0.55f);
    float zoom = v1 ? (1.75f + 0.55f * sinf(t * 0.55f))
                    : (1.30f + 0.85f * sinf(t * 0.40f));
    float amp  = v1 ? 60.0f : 40.0f;
    float warp = v1 ? 0.30f : 0.18f;
    g_ca0 = cosf(ang) * zoom;
    g_sa0 = sinf(ang) * zoom;
    g_cu  = 128.0f + amp * sinf(t * (v1 ? 0.31f : 0.23f));
    g_cv  = 128.0f + amp * cosf(t * (v1 ? 0.27f : 0.19f));
    for (int y = 0; y < VGA_RACE_H; y++)
        g_flex[y] = 1.0f + warp * sinf(y * 0.022f + t * 2.1f);   /* rubber */

    g_white = qs_trans_white(t_global, scene_cur_start_ms(), scene_cur_end_ms(), 0);
}

const effect_t fx_rotozoom = {
    .name  = "rotozoom",
    .mode  = MODE_RACE,
    .init  = roto_init,
    .frame = roto_frame,
    .done  = NULL,
};
