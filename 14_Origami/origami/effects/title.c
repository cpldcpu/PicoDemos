/* Scene 0 — TITLE (0:00–0:10.45, MODE_320).
 *
 * "ORIGAMI" unfolds from a single creased sheet. A 6-panel accordion-folded
 * strip of cream paper opens flat (animated dihedral fold angles along its
 * crease columns), gently breathing; then the title + subtitle stamp on.
 * This is also the first proof of the flat-shaded poly engine: real folding,
 * per-face Lambert shade, painter fill — over a pastel sky.
 */

#include "scene.h"
#include "vga.h"
#include "poly3d.h"
#include "origami_fx.h"
#include "scene_scratch.h"
#include <math.h>

#define SCENE_LEN_MS 10450

/* --- accordion strip: 7 columns x 2 rows = 14 verts, 6 panels --------- */
#define PW  44.0f      /* panel width  (world) */
#define PH  72.0f      /* half-height  (world) */

static const p3_vec3 strip_verts[14] = {
    {-3*PW,-PH,0},{-3*PW,PH,0}, {-2*PW,-PH,0},{-2*PW,PH,0},
    {-1*PW,-PH,0},{-1*PW,PH,0}, { 0*PW,-PH,0},{ 0*PW,PH,0},
    { 1*PW,-PH,0},{ 1*PW,PH,0}, { 2*PW,-PH,0},{ 2*PW,PH,0},
    { 3*PW,-PH,0},{ 3*PW,PH,0},
};

/* panel i = cols i,i+1 ; vertex id = col*2 + row */
static const p3_face strip_faces[6] = {
    {0,1,3,2,   P3_MAT_CREAM, P3_FACE_DOUBLE_SIDED},
    {2,3,5,4,   P3_MAT_WHITE, P3_FACE_DOUBLE_SIDED},
    {4,5,7,6,   P3_MAT_CREAM, P3_FACE_DOUBLE_SIDED},
    {6,7,9,8,   P3_MAT_WHITE, P3_FACE_DOUBLE_SIDED},
    {8,9,11,10, P3_MAT_CREAM, P3_FACE_DOUBLE_SIDED},
    {10,11,13,12,P3_MAT_WHITE, P3_FACE_DOUBLE_SIDED},
};

/* crease c (c=1..5) pivots about column-c edge, moving all cols > c */
static const uint16_t mv1[] = {4,5,6,7,8,9,10,11,12,13};
static const uint16_t mv2[] = {6,7,8,9,10,11,12,13};
static const uint16_t mv3[] = {8,9,10,11,12,13};
static const uint16_t mv4[] = {10,11,12,13};
static const uint16_t mv5[] = {12,13};

static const p3_crease strip_creases[5] = {
    {2,3,   mv1, 10, 0, 0},
    {4,5,   mv2,  8, 1, 0},
    {6,7,   mv3,  6, 2, 0},
    {8,9,   mv4,  4, 3, 0},
    {10,11, mv5,  2, 4, 0},
};

static const p3_model strip_model = {
    strip_verts, 14, strip_faces, 6, strip_creases, 5,
};

static void title_init(void)
{
    og_materials();
}

static void title_frame(uint32_t t_into, uint32_t t_global)
{
    og_sky();

    /* unfold 0..4 s: fold angle 1.45 -> 0, eased */
    float u = og_smooth(t_into / 4000.0f);          /* 0..1 open */
    float fold = (1.0f - u) * 1.45f;

    p3_render_params rp; p3_params_default(&rp);
    rp.focal = 320.0f;
    rp.oz = 470.0f;
    rp.oy = 18.0f;
    /* gentle breathing sway */
    rp.yaw   = 0.18f * sinf(t_global * 0.0009f) * (0.4f + 0.6f*u);
    rp.pitch = -0.10f + 0.05f * sinf(t_global * 0.0012f);
    rp.cam_pitch = 0.0f;
    rp.ambient = 0.40f;
    for (int i = 0; i < 5; i++)
        rp.fold_angle[i] = (i & 1) ? -fold : fold;   /* accordion alternation */

    /* soft shadow on a ground plane below once mostly open */
    if (u > 0.15f) {
        float gy = -PH - 40.0f;
        p3_render_shadow(&strip_model, &rp, gy, (int)(150 * u), g_scratch.bg_cache);
    }
    p3_render(&strip_model, &rp);

    /* title + subtitle stamp on after it opens */
    if (t_into > 3800) {
        og_logo_centred("ORIGAMI", 150, 40);
    }
    if (t_into > 5200) {
        og_text_centred("A FOLDED PAPER WORLD", 206, 1);
    }
    (void)t_global;
}

static void title_done(void) {}

const effect_t fx_title_real = {
    .name = "title", .mode = MODE_HIRES,
    .init = title_init, .frame = title_frame, .done = title_done,
};
