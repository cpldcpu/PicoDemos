/* Scene 2 — CRANE FOLD / UNFOLD MORPH (0:25.82–0:54.54, MODE_320).
 *
 * The centrepiece. A flat square of paper folds — wings, neck, beak, tail,
 * in a staged sequence of animated dihedral angles along fixed crease lines
 * — into an origami crane, holds and turns so you read it in 3D, then
 * unfolds back to flat. Pure poly3d: Rodrigues hinge folding + per-face
 * Lambert + painter sort. The hierarchy (beak folds within the risen neck)
 * comes for free from crease ordering.
 */

#include "scene.h"
#include "vga.h"
#include "poly3d.h"
#include "origami_fx.h"
#include "scene_scratch.h"
#include <math.h>

#define SCENE_LEN_MS 28720

/* --- crane, authored FLAT (y=0); the folds build the bird ------------- */
static const p3_vec3 crane_verts[14] = {
    {-40,0, 40},{ 40,0, 40},{ 40,0,-40},{-40,0,-40},   /* 0..3  body square   */
    {-128,0, 40},{-128,0,-40},                         /* 4,5   left wing     */
    { 128,0, 40},{ 128,0,-40},                         /* 6,7   right wing    */
    {-15,0,112},{ 15,0,112},                           /* 8,9   neck tip      */
    {-15,0,156},{ 15,0,156},                           /* 10,11 head/beak     */
    {-15,0,-122},{ 15,0,-122},                         /* 12,13 tail          */
};
static const p3_face crane_faces[6] = {
    {0,1,2,3,    P3_MAT_WHITE,   P3_FACE_DOUBLE_SIDED},  /* body  */
    {0,3,5,4,    P3_MAT_CORAL,   P3_FACE_DOUBLE_SIDED},  /* L wing*/
    {1,6,7,2,    P3_MAT_CORAL,   P3_FACE_DOUBLE_SIDED},  /* R wing*/
    {0,1,9,8,    P3_MAT_SAGE,    P3_FACE_DOUBLE_SIDED},  /* neck  */
    {8,9,11,10,  P3_MAT_MUSTARD, P3_FACE_DOUBLE_SIDED},  /* head  */
    {3,2,13,12,  P3_MAT_CREAM,   P3_FACE_DOUBLE_SIDED},  /* tail  */
};
static const uint16_t mv_lw[]  = {4,5};
static const uint16_t mv_rw[]  = {6,7};
static const uint16_t mv_nk[]  = {8,9,10,11};
static const uint16_t mv_hd[]  = {10,11};
static const uint16_t mv_tl[]  = {12,13};
static const p3_crease crane_creases[5] = {
    {0,3, mv_lw, 2, 0, 0},   /* left wing  (axis FL->BL)  */
    {1,2, mv_rw, 2, 1, 0},   /* right wing (axis FR->BR)  */
    {0,1, mv_nk, 4, 2, 0},   /* neck       (axis FL->FR)  */
    {8,9, mv_hd, 2, 3, 0},   /* head/beak  (within neck)  */
    {3,2, mv_tl, 2, 4, 0},   /* tail       (axis BL->BR)  */
};
static const p3_model crane_model = { crane_verts, 14, crane_faces, 6, crane_creases, 5 };

/* staged ramp: 0 before t0, smooth 0->1 across [t0,t0+dur] */
static float ramp(uint32_t t, uint32_t t0, uint32_t dur)
{
    if (t <= t0) return 0.0f;
    if (t >= t0 + dur) return 1.0f;
    return og_smooth((float)(t - t0) / (float)dur);
}

static void crane_init(void)
{
    og_materials();
}

static void crane_frame(uint32_t t_into, uint32_t t_global)
{
    /* targets */
    const float WING = 0.72f, NECK = 1.46f, HEAD = 1.05f, TAIL = 1.18f;

    /* fold-in (staged), then unfold-out (staged) in the last third */
    float lw = ramp(t_into,  600,3000) * (1.0f - ramp(t_into, 20500,3200));
    float rw = ramp(t_into,  900,3000) * (1.0f - ramp(t_into, 20800,3200));
    float nk = ramp(t_into, 3000,3200) * (1.0f - ramp(t_into, 22500,3000));
    float hd = ramp(t_into, 5400,2000) * (1.0f - ramp(t_into, 21500,2400));
    float tl = ramp(t_into, 4000,3000) * (1.0f - ramp(t_into, 22000,3000));

    og_sky();

    p3_render_params rp; p3_params_default(&rp);
    rp.focal = 330.0f;
    rp.oz = 330.0f;
    rp.oy = 6.0f;
    rp.ambient = 0.40f;
    rp.light.x = -0.42f; rp.light.y = 0.80f; rp.light.z = -0.44f;

    /* tilt the bird toward a level camera (3/4 "tabletop" view) and slowly
     * turn it on a turntable once it has formed, settling before the unfold */
    float spin = ramp(t_into, 3500, 9000) * (1.0f - ramp(t_into, 19500, 2500));
    rp.yaw   = spin * (0.9f * sinf(t_into * 0.00045f));
    rp.pitch = -0.62f + 0.05f * sinf(t_into * 0.0010f);

    rp.fold_angle[0] =  WING * lw;
    rp.fold_angle[1] = -WING * rw;        /* mirror */
    rp.fold_angle[2] = -NECK * nk;        /* neck rises */
    rp.fold_angle[3] =  HEAD * hd;        /* beak bends */
    rp.fold_angle[4] =  TAIL * tl;

    /* ground shadow under the bird */
    p3_render_shadow(&crane_model, &rp, -70.0f, 140, g_scratch.bg_cache);
    p3_render(&crane_model, &rp);

    /* a quiet label */
    if (t_into > 9000 && t_into < 19000)
        og_text_centred("ONE SQUARE - ONE BIRD", 214, 1);
    (void)t_global;
}

static void crane_done(void) {}

const effect_t fx_crane_real = {
    .name = "crane", .mode = MODE_HIRES,
    .init = crane_init, .frame = crane_frame, .done = crane_done,
};
