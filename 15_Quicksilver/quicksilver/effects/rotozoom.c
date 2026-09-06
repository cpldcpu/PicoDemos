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

    /* The rotozoomer ACCELERATES over its scene (eases in, slow -> rushing) so
     * it visibly tenses rather than spinning steadily; the white-glint takes it
     * into the next scene. It appears twice: the first is a gentle RISE, the
     * second the hard, fast BUILD that slams into drop 1 — opposite spin and a
     * harder ramp so the reprise reads as a different move, not a repeat. prog
     * is scene-relative; all cheap core-0 setup, the scan loop is untouched. */
    float dur = (scene_cur_end_ms() - scene_cur_start_ms()) * 0.001f;
    if (dur < 1.0f) dur = 1.0f;
    float prog = t / dur; if (prog > 1.0f) prog = 1.0f;
    float acc  = prog * prog;                /* ease-in: rushes at the end     */

    int   build = scene_cur_start_ms() > 50000u;   /* 2nd pass = build into drop */
    float dir   = build ? -1.0f : 1.0f;            /* opposite spin             */
    float ang   = t * dir * (build ? (0.50f + 1.80f * acc) : (0.40f + 0.90f * acc));
    float zoom  = (build ? 1.90f : 1.55f) - (build ? 1.25f : 0.70f) * acc
                  + 0.10f * sinf(t * 0.8f);        /* rushes in (harder if build)*/
    float warp  = (build ? 0.12f : 0.10f) + (build ? 0.40f : 0.22f) * acc;
    float amp   = 30.0f + 30.0f * acc;
    float fspd  = 2.1f + (build ? 2.4f : 1.2f) * acc;
    g_ca0 = cosf(ang) * zoom;
    g_sa0 = sinf(ang) * zoom;
    g_cu  = 128.0f + amp * sinf(t * 0.31f);
    g_cv  = 128.0f + amp * cosf(t * 0.27f);
    for (int y = 0; y < VGA_RACE_H; y++)
        g_flex[y] = 1.0f + warp * sinf(y * 0.022f + t * fspd);   /* rubber */

    g_white = qs_trans_white(t_global, scene_cur_start_ms(), scene_cur_end_ms(), 0);
}

const effect_t fx_rotozoom = {
    .name  = "rotozoom",
    .mode  = MODE_RACE,
    .init  = roto_init,
    .frame = roto_frame,
    .done  = NULL,
};
