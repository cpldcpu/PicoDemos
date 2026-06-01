/* chrome.c — QUICKSILVER centerpiece scene. A sequence of high-poly solids
 * (icosphere, torus knot, rounded cube) spinning as polished chrome, reflecting
 * the matcap sphere-map via envmap3d. Objects swap with a scale-punch so the
 * scene keeps morphing. The interpolator does the per-pixel matcap addressing.
 */

#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "assets.h"
#include "envmap3d.h"
#include "qs_fx.h"

#include <math.h>

#define NOBJ 6
static qs_mesh s_obj[NOBJ];
static float   s_objscale[NOBJ];

static void chrome_init(void)
{
    s_obj[0] = (qs_mesh){ ICO_v,   ICO_n,   ICO_t,   ICO_NV,   ICO_NT   }; s_objscale[0] = 1.55f;
    s_obj[1] = (qs_mesh){ KNOT_v,  KNOT_n,  KNOT_t,  KNOT_NV,  KNOT_NT  }; s_objscale[1] = 1.70f;
    s_obj[2] = (qs_mesh){ SPIKE_v, SPIKE_n, SPIKE_t, SPIKE_NV, SPIKE_NT }; s_objscale[2] = 1.30f;
    s_obj[3] = (qs_mesh){ KNOT2_v, KNOT2_n, KNOT2_t, KNOT2_NV, KNOT2_NT }; s_objscale[3] = 1.75f;
    s_obj[4] = (qs_mesh){ TORUS_v, TORUS_n, TORUS_t, TORUS_NV, TORUS_NT }; s_objscale[4] = 1.85f;
    s_obj[5] = (qs_mesh){ CUBE_v,  CUBE_n,  CUBE_t,  CUBE_NV,  CUBE_NT  }; s_objscale[5] = 1.25f;
}

/* dark steel vertical backdrop so the chrome pops */
static void backdrop(uint32_t t_ms)
{
    uint16_t *fb = vga_hires_back_buffer();
    int pulse = (int)(18 + 10 * sinf(t_ms * 0.0011f));
    for (int y = 0; y < VGA_HIRES_H; y++) {
        int v = (y * 38) / VGA_HIRES_H;
        int br = 8 + v + pulse, bg = 12 + v + pulse, bb = 22 + v + (pulse >> 1);
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int d = qs_dither(x, y);
            row[x] = rgb565_pack(br + d, bg + d, bb + d);
        }
    }
}

static void chrome_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    backdrop(t_ms);

    float t = t_ms * 0.001f;

    /* The chrome scene appears twice. Each block shows a DISJOINT set of three
     * objects, exactly once each (block duration / 3), so no object ever
     * repeats across the demo. Block A = objects 0..2, block B (the DROP) =
     * objects 3..5, flown closer and spun faster. */
    int reprise = (t_global > 110000u);
    int baseidx = reprise ? 3 : 0;

    float dur = (scene_cur_end_ms() - scene_cur_start_ms()) * 0.001f;
    float PERIOD = dur / 3.0f; if (PERIOD < 1.0f) PERIOD = 1.0f;
    int k = (int)(t / PERIOD); if (k > 2) k = 2;        /* 0..2, no wrap */
    float local = t - k * PERIOD;
    float in  = local < 0.6f ? (local / 0.6f) : 1.f;
    float out = local > PERIOD - 0.6f ? ((PERIOD - local) / 0.6f) : 1.f;
    float env_scale = in * out;

    int slot = baseidx + k;
    const qs_mesh *m = &s_obj[slot];
    float base = s_objscale[slot];

    qs_env_params p; qs_env_default(&p);
    p.env = (const uint8_t *)asset_envmap_data;
    p.envW = ASSET_ENVMAP_W; p.envH = ASSET_ENVMAP_H;
    p.yaw   = t * (reprise ? 1.0f : 0.6f);
    p.pitch = 0.5f * sinf(t * 0.37f);
    p.roll  = (reprise ? 0.5f : 0.2f) * sinf(t * 0.23f);
    p.oz    = reprise ? 4.2f : 4.8f;
    p.focal = 300.f;
    p.scale = base * (1.0f + 0.06f * sinf(t * 1.7f)) * (0.25f + 0.75f * env_scale);

    qs_envmap_render(m, &p);
}

const effect_t fx_chrome = {
    .name  = "chrome",
    .mode  = MODE_HIRES,
    .init  = chrome_init,
    .frame = chrome_frame,
    .done  = NULL,
};
