#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sequencer.h"
#include "chunky.h"
#include "palette.h"
#include "mathtab.h"
#include "vector3d.h"
#include "torus.h"
#include "texmap.h"
#include "voxel.h"
#include "effects.h"

typedef enum {
    SC_TEXT_DAWN, SC_TEXT_BY, SC_TEXT_AZURE,
    SC_TORUS_1,   SC_TORUS_2,
    SC_VOXEL,
    SC_TORUS_3,   SC_TORUS_BLUR,
    SC_FINALE,
} scene_t;

static scene_t scene = SC_TEXT_DAWN;
static int frame_count = 0;
static int scene_start_frame = 0;

/* Pending-fade plumbing — when a scene wants to transition, it triggers
 * fadeout and stores what to switch to once the blur finishes. */
static scene_t       fade_next_scene;
static pal_scheme_t  fade_next_palette;

static int light_angle = 0;
static int light_radius = 0;

/* Per-frame projected vertex / UV scratch space. Stored as file-static
 * arrays to avoid touching the heap during the inner update. */
static screen_pt_t   sc_pts [TORUS_VERTS];
static uint8_t       sc_us  [TORUS_VERTS];
static uint8_t       sc_vs  [TORUS_VERTS];

/* Sorted polygon indices for painter's algorithm. */
typedef struct { int16_t depth; uint16_t poly_idx; } vis_t;
static vis_t vis[TORUS_POLYS];

/* Torus parameters per scene — matches the keyframes the web port picks
 * (sequencer.ts:230-251). The original demos picked specific frame indices
 * from a 40-frame morph; we just build the matching parameter set here. */
static void apply_torus_params_for_scene(scene_t s, int scene_frame)
{
    int inner_step, amplitude, phase;
    switch (s) {
    case SC_TORUS_1:    inner_step = 256;       amplitude = 0;     phase = 100; break;
    case SC_TORUS_2:    inner_step = 128;       amplitude = 30000; phase = 100; break;
    case SC_TORUS_3:    inner_step = 256 + 64;  amplitude = 40000; phase = 0;   break;
    case SC_TORUS_BLUR: inner_step = 256;       amplitude = 20000; phase = 100; break;
    case SC_FINALE: {
        /* Amplitude wobbles with sin(scene_frame * 4 / 1024 * 2π). */
        const int idx = (scene_frame << 2) & SIN_TAB_MASK;
        const int s_val = sin_lookup(idx);
        const int norm = (s_val + 32255) >> 1;     /* 0..32255 */
        inner_step = 192;
        amplitude = (norm * 40000) >> 15;
        phase = 100;
        break;
    }
    default:            return;
    }
    torus_build_frame(inner_step, amplitude, phase);
}

static void start_fade(scene_t next_scene, pal_scheme_t next_pal)
{
    if (effects_fadeout_active()) return;
    effects_fadeout_start();
    fade_next_scene = next_scene;
    fade_next_palette = next_pal;
}

/* Successor scene + palette for each scene — matches the start_fade()
 * calls in sequencer_step(). Used by sequencer_skip() to advance on a
 * button press without duplicating the table. */
static void successor_for(scene_t s, scene_t *next, pal_scheme_t *pal)
{
    switch (s) {
    case SC_TEXT_DAWN:  *next = SC_TEXT_BY;     *pal = PAL_SCHEME_2; break;
    case SC_TEXT_BY:    *next = SC_TEXT_AZURE;  *pal = PAL_SCHEME_3; break;
    case SC_TEXT_AZURE: *next = SC_TORUS_1;     *pal = PAL_SCHEME_1; break;
    case SC_TORUS_1:    *next = SC_TORUS_2;     *pal = PAL_SCHEME_1; break;
    case SC_TORUS_2:    *next = SC_VOXEL;       *pal = PAL_SCHEME_1; break;
    case SC_VOXEL:      *next = SC_TORUS_3;     *pal = PAL_SCHEME_5; break;
    case SC_TORUS_3:    *next = SC_TORUS_BLUR;  *pal = PAL_SCHEME_1; break;
    case SC_TORUS_BLUR: *next = SC_FINALE;      *pal = PAL_SCHEME_4; break;
    case SC_FINALE:     *next = SC_TEXT_DAWN;   *pal = PAL_SCHEME_1; break;
    default:            *next = SC_TEXT_DAWN;   *pal = PAL_SCHEME_1; break;
    }
}

void sequencer_skip(void)
{
    if (effects_fadeout_active()) return;   /* already fading */
    scene_t next; pal_scheme_t pal;
    successor_for(scene, &next, &pal);
    start_fade(next, pal);
}

static void set_initial_translation(scene_t s)
{
    switch (s) {
    case SC_TORUS_1:    v3_translation = 3000; break;
    case SC_TORUS_2:    v3_translation = 3000; break;
    case SC_TORUS_3:    v3_translation = 3000; break;
    case SC_TORUS_BLUR: v3_translation = 1000; break;
    default:            v3_translation = 800;  break;
    }
}

static void update_translation(void)
{
    if (v3_translation > 800) v3_translation -= 20;
}

static int cmp_vis(const void *a, const void *b)
{
    const vis_t *va = (const vis_t *)a;
    const vis_t *vb = (const vis_t *)b;
    return vb->depth - va->depth;  /* far-to-near */
}

static int build_visible_polys(void)
{
    int n = 0;
    for (int p = 0; p < torus_poly_count; p++) {
        const int i0 = torus_polys[p].v[0];
        const int i1 = torus_polys[p].v[1];
        const int i2 = torus_polys[p].v[2];

        /* 2D cross product on the projected triangle → backface cull. */
        const int v1x = sc_pts[i0].sx - sc_pts[i1].sx;
        const int v1y = sc_pts[i0].sy - sc_pts[i1].sy;
        const int v2x = sc_pts[i2].sx - sc_pts[i1].sx;
        const int v2y = sc_pts[i2].sy - sc_pts[i1].sy;
        const int cross = v2x * v1y - v1x * v2y;
        if (cross <= 0) continue;

        int dsum = 0;
        for (int k = 0; k < 4; k++) dsum += sc_pts[torus_polys[p].v[k]].depth;
        vis[n].depth = (int16_t)(dsum >> 2);
        vis[n].poly_idx = (uint16_t)p;
        n++;
    }
    qsort(vis, n, sizeof(vis_t), cmp_vis);
    return n;
}

static void draw_torus(bool blur_mode, bool clear_buf)
{
    if (clear_buf) chunky_clear(0);
    const int scene_frame = frame_count - scene_start_frame;

    /* Rotation increments — dawn_final.s:1078-1080. */
    v3_angle_y = (v3_angle_y - 50)  & 0xFFFF;
    v3_angle_x = (v3_angle_x + 80)  & 0xFFFF;
    v3_angle_z = (v3_angle_z + 600) & 0xFFFF;

    if (light_radius != 0) {
        light_angle = (light_angle + 600) & 0xFFFF;
        const int idx = (light_angle >> 6) & SIN_TAB_MASK;
        const int s_val = sin_lookup(idx);
        /* sin_tab peaks at 32255 → divide to get a small offset. */
        tex_light_u_offset = (s_val * light_radius) >> 16;
        tex_light_v_offset = 0;
    } else {
        tex_light_u_offset = 0;
        tex_light_v_offset = 0;
    }

    apply_torus_params_for_scene(scene, scene_frame);

    for (int i = 0; i < TORUS_VERTS; i++) {
        v3_rotate_project(&torus_verts[i], &sc_pts[i]);
        int u, v;
        v3_normal_to_uv(&torus_norms[i], &u, &v);
        sc_us[i] = (uint8_t)u;
        sc_vs[i] = (uint8_t)v;
    }

    const int nvis = build_visible_polys();
    tex_blur_mode = blur_mode;

    for (int i = 0; i < nvis; i++) {
        const int p = vis[i].poly_idx;
        screen_pt_t pp[4];
        uint8_t pu[4], pv[4];
        for (int k = 0; k < 4; k++) {
            const int vi = torus_polys[p].v[k];
            pp[k] = sc_pts[vi];
            pu[k] = sc_us[vi];
            pv[k] = sc_vs[vi];
        }
        texmap_draw_polygon(pp, pu, pv, 4);
    }
}

void sequencer_init(void)
{
    mathtab_init();
    palette_set_scheme(PAL_SCHEME_1);
    torus_init_polys();
    torus_build_frame(256, 0, 100);   /* arbitrary initial */
    voxel_init();

    scene = SC_TEXT_DAWN;
    frame_count = 0;
    scene_start_frame = 0;
    set_initial_translation(scene);
    light_radius = 0;
    env_mode = ENV_SPHERE;
}

static void render_text_scene(text_id_t which, int scene_frame)
{
    if (scene_frame == 0) chunky_clear(0);
    effects_text_render(which);
    chunky_blur_vertical();
}

static void on_fade_complete(void)
{
    scene = fade_next_scene;
    scene_start_frame = frame_count;

    palette_set_scheme(fade_next_palette);
    set_initial_translation(scene);
    light_radius = (scene == SC_TORUS_2) ? 50 : 0;
    if (light_radius == 0) {
        tex_light_u_offset = 0;
        tex_light_v_offset = 0;
    }
    env_mode = (scene == SC_TORUS_3 || scene == SC_TORUS_BLUR)
                   ? ENV_CHECKER : ENV_SPHERE;
}

void sequencer_step(void)
{
    if (effects_fadeout_active()) {
        effects_fadeout_step();
        if (!effects_fadeout_active()) {
            on_fade_complete();
        }
        frame_count++;
        return;
    }

    const int scene_frame = frame_count - scene_start_frame;

    switch (scene) {
    case SC_TEXT_DAWN:
        render_text_scene(TEXT_DAWN, scene_frame);
        if (scene_frame > 150) start_fade(SC_TEXT_BY, PAL_SCHEME_2);
        break;

    case SC_TEXT_BY:
        render_text_scene(TEXT_BY, scene_frame);
        if (scene_frame > 150) start_fade(SC_TEXT_AZURE, PAL_SCHEME_3);
        break;

    case SC_TEXT_AZURE:
        render_text_scene(TEXT_AZURE, scene_frame);
        if (scene_frame > 150) start_fade(SC_TORUS_1, PAL_SCHEME_1);
        break;

    case SC_TORUS_1:
        update_translation();
        draw_torus(false, true);
        if (scene_frame > 15 * 50) start_fade(SC_TORUS_2, PAL_SCHEME_1);
        break;

    case SC_TORUS_2:
        update_translation();
        draw_torus(false, true);
        if (scene_frame > 20 * 50) start_fade(SC_VOXEL, PAL_SCHEME_1);
        break;

    case SC_VOXEL:
        chunky_clear(0);
        voxel_render(frame_count);
        if (scene_frame > 25 * 50) start_fade(SC_TORUS_3, PAL_SCHEME_5);
        break;

    case SC_TORUS_3:
        update_translation();
        draw_torus(false, true);
        if (scene_frame > 15 * 50) start_fade(SC_TORUS_BLUR, PAL_SCHEME_1);
        break;

    case SC_TORUS_BLUR:
        update_translation();
        /* Don't clear — blur trail accumulates. */
        draw_torus(true, false);
        chunky_blur_vertical();
        if (scene_frame > 20 * 50) start_fade(SC_FINALE, PAL_SCHEME_4);
        break;

    case SC_FINALE:
        effects_text_render(TEXT_DAWN);
        draw_torus(false, false);
        chunky_blur_vertical();
        if (scene_frame > 30 * 50) {
            /* Loop back to start. */
            scene = SC_TEXT_DAWN;
            scene_start_frame = frame_count + 1;
            palette_set_scheme(PAL_SCHEME_1);
            set_initial_translation(scene);
            env_mode = ENV_SPHERE;
            light_radius = 0;
        }
        break;
    }

    frame_count++;
}
