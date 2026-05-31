/* Scene 3 — POP-UP PAPER CITY (0:54.54–1:17.11, MODE_320).
 *
 * A page turns and a paper skyline hinges upright: flat facades rise from
 * the page about their base creases (the classic pop-up-book mechanic),
 * staggered front-to-back, each a different pastel, casting soft shadows on
 * the page. Painter's-algorithm layering sells the depth.
 */

#include "scene.h"
#include "vga.h"
#include "poly3d.h"
#include "origami_fx.h"
#include "scene_scratch.h"
#include <math.h>

#define SCENE_LEN_MS 22570
#define NB 12

/* config: base centre (x,z), width, height, material */
static const struct { float x, z, w, h; uint8_t mat; } bcfg[NB] = {
    {-150,  40,  70, 150, P3_MAT_CORAL  },
    { -80, -30,  60, 220, P3_MAT_CREAM  },
    {   0,  60,  84, 120, P3_MAT_SKYBLUE},
    {  90, -10,  64, 200, P3_MAT_MUSTARD},
    { 165,  50,  72, 140, P3_MAT_SAGE   },
    {-200, -70,  60, 180, P3_MAT_WHITE  },
    {-120,-110,  74, 250, P3_MAT_SKYBLUE},
    { -20,-100,  66, 160, P3_MAT_CORAL  },
    {  60,-120,  80, 210, P3_MAT_WHITE  },
    { 150,-90,   62, 175, P3_MAT_CREAM  },
    { 215,-40,   58, 230, P3_MAT_CORAL  },
    {-260,  20,  56, 130, P3_MAT_MUSTARD},
};

static p3_vec3   city_verts[NB*4];
static p3_face   city_faces[NB];
static uint16_t  city_moves[NB][2];
static p3_crease city_creases[NB];
static p3_model  city_model;

/* the page (ground) — kept entirely in front of the camera so the whole
 * quad survives near-clipping (a face is skipped if any vertex is behind). */
static const p3_vec3 page_verts[4] = {
    {-1100,0,-690},{1100,0,-690},{1100,0,560},{-1100,0,560},
};
static const p3_face page_faces[1] = { {0,1,2,3, P3_MAT_CREAM, P3_FACE_DOUBLE_SIDED} };
static const p3_model page_model = { page_verts, 4, page_faces, 1, 0, 0 };

static void city_init(void)
{
    og_materials();
    /* warmer page */
    p3_set_material(P3_MAT_CREAM, 238,226,198);
    for (int b = 0; b < NB; b++) {
        float x = bcfg[b].x, z = bcfg[b].z, w = bcfg[b].w, h = bcfg[b].h;
        p3_vec3 *v = &city_verts[b*4];
        v[0] = (p3_vec3){ x - w*0.5f, 0, z      };   /* BL (base) */
        v[1] = (p3_vec3){ x + w*0.5f, 0, z      };   /* BR (base) */
        v[2] = (p3_vec3){ x + w*0.5f, 0, z + h  };   /* TR (flat) */
        v[3] = (p3_vec3){ x - w*0.5f, 0, z + h  };   /* TL (flat) */
        city_faces[b] = (p3_face){ (uint16_t)(b*4+0),(uint16_t)(b*4+1),
                                   (uint16_t)(b*4+2),(uint16_t)(b*4+3),
                                   bcfg[b].mat, P3_FACE_DOUBLE_SIDED };
        /* hinge the top edge (TR,TL) up about the base edge (BL,BR) */
        city_moves[b][0] = (uint16_t)(b*4+2);
        city_moves[b][1] = (uint16_t)(b*4+3);
        city_creases[b] = (p3_crease){ (uint16_t)(b*4+0), (uint16_t)(b*4+1),
                                       city_moves[b], 2, (uint8_t)b, 0 };
    }
    city_model = (p3_model){ city_verts, NB*4, city_faces, NB,
                             city_creases, NB };
}

static float ramp(uint32_t t, uint32_t t0, uint32_t dur)
{
    if (t <= t0) return 0.0f;
    if (t >= t0 + dur) return 1.0f;
    return og_smooth((float)(t - t0) / (float)dur);
}

static void city_frame(uint32_t t_into, uint32_t t_global)
{
    og_sky();

    p3_render_params rp; p3_params_default(&rp);
    rp.focal = 300.0f;
    rp.oz = 730.0f;
    rp.oy = -60.0f;             /* drop the skyline toward screen centre */
    rp.cam_y = 64.0f;           /* near table height, looking gently down */
    rp.cam_pitch = -0.17f;
    rp.ambient = 0.40f;
    rp.light.x = -0.40f; rp.light.y = 0.80f; rp.light.z = -0.45f;

    /* gentle camera-translation sway for parallax depth. (Rotating the model
     * would swing the big near page plane behind the near-clip plane and drop
     * the whole face — so we move the camera sideways instead, never rotate.) */
    rp.cam_x = 70.0f * sinf(t_into * 0.00030f);

    /* page first (it is the ground all shadows fall on) */
    p3_render(&page_model, &rp);

    /* buildings hinge up, staggered by row (front rows first); a little
     * beat-driven sway once standing. -pi/2 = fully upright. */
    float beat = og_beat_pulse(t_global);
    for (int b = 0; b < NB; b++) {
        uint32_t on = 400 + (uint32_t)((bcfg[b].z + 130.0f) * 12.0f); /* back rises later */
        float up = ramp(t_into, on, 2600);
        float sway = 0.05f * beat * up * sinf(b * 1.7f);
        rp.fold_angle[b] = -1.5708f * up + sway;
    }

    /* cast shadows of the standing facades onto the page. The page sits at
     * world y = rp.oy (model y=0 placed by oy), so the shadow ground must
     * match it — otherwise the shadow floats off toward the horizon. */
    p3_render_shadow(&city_model, &rp, rp.oy, 130, g_scratch.bg_cache);
    p3_render(&city_model, &rp);

    if (t_into > 4000)
        og_text_centred("PAPER CITY", 18, 2);
}

static void city_done(void) {}

const effect_t fx_city_real = {
    .name = "city", .mode = MODE_HIRES,
    .init = city_init, .frame = city_frame, .done = city_done,
};
