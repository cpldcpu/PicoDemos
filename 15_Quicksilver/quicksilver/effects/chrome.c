/* chrome.c — QUICKSILVER centerpiece scene. A sequence of high-poly solids
 * (icosphere, torus knot, rounded cube) spinning as polished chrome, reflecting
 * the matcap sphere-map via envmap3d. Objects swap with a scale-punch so the
 * scene keeps morphing. The interpolator does the per-pixel matcap addressing.
 */

#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "../scene_scratch.h"
#include "assets.h"
#include "envmap3d.h"
#include "qs_fx.h"

#include <math.h>

/* Matcap copied to SRAM (g_scratch) — env mapping reads the matcap per pixel
 * with random access; from flash (XIP) that thrashes the cache and halves the
 * framerate, so we keep a copy in fast RAM.
 *
 * EACH OBJECT carries its OWN matcap, so a scene morphs through several lighting
 * looks instead of one flat reflection: neutral studio chrome on the icosphere
 * (it reads best on a true sphere), violet iridescent (envmap3) and warm gold
 * (envmap2) on the knots/torus/spike. Only ONE 128x128 map (32 KB) lives in SRAM
 * at a time; we reload it from flash whenever the object switches. That switch
 * always lands on the scale-punch boundary (env_scale dips to ~0, the object is
 * tiny), so the one-frame reload is hidden. Per-slot source + width: envmap is
 * 256 wide (halved on load), envmap2/3 are native 128 (straight copy). */
#define ENV128 128
#define NOBJ 6
static uint16_t *s_env;                 /* the one live matcap in SRAM */
static const uint16_t *s_env_src[NOBJ]; /* flash source per object slot */
static int             s_env_sw[NOBJ];  /* source width (256 or 128) */
static int             s_loaded;        /* slot currently in s_env (-1 = none) */

static qs_mesh s_obj[NOBJ];
static float   s_objscale[NOBJ];

static void load_env(int slot)
{
    const uint16_t *src = s_env_src[slot];
    int sw = s_env_sw[slot], step = sw / ENV128;   /* 2 for 256, 1 for 128 */
    for (int y = 0; y < ENV128; y++)
        for (int x = 0; x < ENV128; x++)
            s_env[y * ENV128 + x] = src[(y * step) * sw + x * step];
}

static void chrome_init(void)
{
    s_obj[0] = (qs_mesh){ ICO_v,   ICO_n,   ICO_t,   ICO_NV,   ICO_NT   }; s_objscale[0] = 1.55f;
    s_obj[1] = (qs_mesh){ KNOT_v,  KNOT_n,  KNOT_t,  KNOT_NV,  KNOT_NT  }; s_objscale[1] = 1.70f;
    s_obj[2] = (qs_mesh){ SPIKE_v, SPIKE_n, SPIKE_t, SPIKE_NV, SPIKE_NT }; s_objscale[2] = 1.30f;
    s_obj[3] = (qs_mesh){ KNOT2_v, KNOT2_n, KNOT2_t, KNOT2_NV, KNOT2_NT }; s_objscale[3] = 1.75f;
    s_obj[4] = (qs_mesh){ TORUS_v, TORUS_n, TORUS_t, TORUS_NV, TORUS_NT }; s_objscale[4] = 1.85f;
    s_obj[5] = (qs_mesh){ CUBE_v,  CUBE_n,  CUBE_t,  CUBE_NV,  CUBE_NT  }; s_objscale[5] = 1.25f;

    /* per-object matcap (neutral chrome on the sphere; violet/gold on the rest) */
    const uint16_t *NEU = (const uint16_t *)asset_envmap_data;    /* 256, neutral */
    const uint16_t *VIO = (const uint16_t *)asset_envmap3_data;   /* 128, violet  */
    const uint16_t *GLD = (const uint16_t *)asset_envmap2_data;   /* 128, gold    */
    s_env_src[0] = NEU; s_env_sw[0] = ASSET_ENVMAP_W;   /* ICO sphere -> neutral */
    s_env_src[1] = VIO; s_env_sw[1] = ASSET_ENVMAP3_W;  /* KNOT       -> violet  */
    s_env_src[2] = GLD; s_env_sw[2] = ASSET_ENVMAP2_W;  /* SPIKE      -> gold    */
    s_env_src[3] = VIO; s_env_sw[3] = ASSET_ENVMAP3_W;  /* KNOT2      -> violet  */
    s_env_src[4] = GLD; s_env_sw[4] = ASSET_ENVMAP2_W;  /* TORUS      -> gold    */
    s_env_src[5] = NEU; s_env_sw[5] = ASSET_ENVMAP_W;   /* CUBE       -> neutral */

    s_env = (uint16_t *)g_scratch.bg_cache;
    s_loaded = -1;                       /* force a load on the first frame */
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

    /* The chrome centrepiece appears TWICE — once on each musical drop — with
     * DISJOINT object sets, escalating in complexity. Drop 1 is the long scene
     * (it spans the build hit and the big onset), so it shows THREE objects; the
     * climax is short and punchy, so it shows two:
     *   A (drop 1, long) = ICO + KNOT + TORUS — the clean, recognisable reveal
     *   B (climax)       = SPIKE + KNOT2       — the intricate payoff
     * Block keyed off the SCENE start; the threshold sits between the two chrome
     * starts (see timeline.c: chrome A ~68 s, chrome B ~122 s — keep it between
     * them if re-timed). Block B is the bigger drop: flown closer, spun faster. */
    static const int A_objs[] = { 0, 1, 4 };   /* ICO, KNOT, TORUS */
    static const int B_objs[] = { 2, 3 };      /* SPIKE, KNOT2     */
    int isB     = scene_cur_start_ms() >= 100000u;
    const int *objs = isB ? B_objs : A_objs;
    int nobj    = isB ? 2 : 3;
    int reprise = isB;                   /* B is the louder reprise */

    float dur = (scene_cur_end_ms() - scene_cur_start_ms()) * 0.001f;
    float PERIOD = dur / nobj; if (PERIOD < 1.0f) PERIOD = 1.0f;
    int k = (int)(t / PERIOD); if (k >= nobj) k = nobj - 1;     /* which object */
    float local = t - k * PERIOD;
    float in  = local < 0.6f ? (local / 0.6f) : 1.f;
    float out = local > PERIOD - 0.6f ? ((PERIOD - local) / 0.6f) : 1.f;
    float env_scale = in * out;

    int slot = objs[k];
    if (slot != s_loaded) { load_env(slot); s_loaded = slot; }   /* swap matcap on object change */
    const qs_mesh *m = &s_obj[slot];
    float base = s_objscale[slot];

    qs_env_params p; qs_env_default(&p);
    p.env = (const uint8_t *)s_env;            /* this object's matcap, live in SRAM */
    p.envW = ENV128; p.envH = ENV128;
    p.log2bpp = 1; p.log2w = 7; p.log2h = 7;
    p.bilinear = (slot == 0 || slot == 5);
                                /* bilinear for the icosphere (a true sphere shows
                                 * matcap blockiness most) and the cube (flat
                                 * faces). The intricate knots/spike/torus hide it
                                 * and point-sample for speed. */
    p.yaw   = t * (reprise ? 1.5f : 0.6f);   /* B spins notably faster        */
    p.pitch = (reprise ? 0.7f : 0.5f) * sinf(t * (reprise ? 0.5f : 0.37f));
    p.roll  = (reprise ? 0.6f : 0.2f) * sinf(t * 0.23f);
    p.oz    = reprise ? 3.9f : 4.8f;         /* B flown closer                */
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
