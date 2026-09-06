/* tunnel.c - QUICKSILVER "Chrome Conduit". A raycast tube (see qs_tunnel.h) the
 * camera flies through and banks around. The wall is raymarched through a
 * dedicated grayscale height map, then shaded from a packed height-gradient map
 * with coloured moving rings and specular mercury highlights.
 *
 * The 128x128 height field, packed gradient map, and compact angle table live
 * in g_scratch; no permanent static bulk data. */

#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "../scene_scratch.h"
#include "assets.h"
#include "qs_tunnel.h"

#include <math.h>

#ifdef PICO_BUILD
#include "pico/platform.h"
#define QS_FAST(n) __not_in_flash_func(n)
#else
#define QS_FAST(n) n
#endif

#define TUW 128                         /* texture/height width  = tube angle      */
#define TVH 128                         /* texture/height height = tube depth      */
#define TDISP_TILE 2.0f                 /* relief height repeats around/along tube */

#if defined(ASSET_CONDUIT_BUMP_W)
static uint8_t  *s_hgt;                 /* 128x128 grayscale height field          */
static int16_t  *s_grad;                /* packed height gradient for lighting     */
static int16_t  *s_ang;                 /* normalized tube angle -> 8.8 texture U  */

static void tunnel_init(void)
{
    s_hgt = (uint8_t *)g_scratch.bg_cache;
    qs_tunnel_build_height_g8(asset_conduit_bump_data,
                              ASSET_CONDUIT_BUMP_W, ASSET_CONDUIT_BUMP_H,
                              TUW, TVH, s_hgt, /*smooth=*/1);

    s_grad = (int16_t *)(s_hgt + TUW * TVH);
    qs_tunnel_build_grad_g8(s_hgt, TUW, TVH, TUW, TVH, s_grad,
                            /*smooth=*/0, /*baseline=*/1);

    s_ang = s_grad + TUW * TVH;
    qs_tunnel_build_angle_u_lut(s_ang, QT_ANGLE_LUT_N, TUW, TDISP_TILE);
}

static void QS_FAST(tunnel_frame)(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    static const qs_tun_params P = {
        .fwd = 2.6f, .ell_amp = 0.30f, .cam_k = 0.55f, .twist = 0.20f,
        .fog_range = 6.0f, .bright = 10, .pulse_amp = 32, .pulse_hz = 3.0f,
        .bump = 1.2f, .light_speed = 1.4f, .light_span = 2.8f,
        .light_range = 0.42f, .light_amb = 0.38f, .key_amp = 0.75f,
        .spec = 620.0f, .disp = 0.16f, .disp_tile = TDISP_TILE,
    };
    qs_tunnel_render_relief(s_hgt, s_grad, s_ang, TUW, TVH, t_ms * 0.001f, &P);
}

#else  /* ---- fallback: shading-only bump map from colour luma ---- */
static uint16_t *s_tex;
static int16_t  *s_grad;

static void tunnel_init(void)
{
    s_tex = (uint16_t *)g_scratch.bg_cache;
    const uint16_t *src = (const uint16_t *)asset_conduit_data;
    int ustep = ASSET_CONDUIT_W / TUW, vstep = ASSET_CONDUIT_H / TVH;
    for (int v = 0; v < TVH; v++)
        for (int u = 0; u < TUW; u++)
            s_tex[v * TUW + u] = src[(v * vstep) * ASSET_CONDUIT_W + u * ustep];
    s_grad = (int16_t *)(s_tex + TUW * TVH);
    qs_tunnel_build_grad(s_tex, TUW, TVH, s_grad);
}

static void QS_FAST(tunnel_frame)(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    static const qs_tun_params P = {
        .fwd = 2.6f, .ell_amp = 0.30f, .cam_k = 0.55f, .twist = 0.20f,
        .fog_range = 6.0f, .bright = 10, .pulse_amp = 32, .pulse_hz = 3.0f,
        .bump = 1.0f, .light_speed = 1.4f, .light_span = 2.2f,
        .light_range = 0.7f, .light_amb = 0.35f, .key_amp = 0.6f,
        .bump_tile = 2.0f, .spec = 500.0f, .bump_lerp = 1.0f,
    };
    float *cs = (float *)(s_grad + TUW * TVH);
    qs_tunnel_render_lit(s_tex, s_grad, TUW, TVH, t_ms * 0.001f, &P, cs);
}
#endif

const effect_t fx_tunnel = {
    .name  = "tunnel",
    .mode  = MODE_HIRES,
    .init  = tunnel_init,
    .frame = tunnel_frame,
    .done  = NULL,
};
