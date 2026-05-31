/* Scene 1 — PAPER-PLANE FLIGHT (0:10.45–0:25.82, MODE_320).
 *
 * A folded paper dart banks through a pastel paper-craft sky of cut-paper
 * clouds, casting a soft drop shadow on a paper ground plane. Flat-shaded
 * fuselage facets (Lambert) + painter fill — the engine's first moving 3D
 * model over a real ground/horizon.
 */

#include "scene.h"
#include "vga.h"
#include "poly3d.h"
#include "origami_fx.h"
#include "scene_scratch.h"
#include <math.h>

#define SCENE_LEN_MS 15370

/* --- paper dart (nose along +z) --------------------------------------- */
static const p3_vec3 plane_verts[5] = {
    {  0,  0,  78},   /* 0 nose            */
    {-62,  4, -48},   /* 1 left wingtip    */
    { 62,  4, -48},   /* 2 right wingtip   */
    {  0,  0, -42},   /* 3 tail centre     */
    {  0,-28, -44},   /* 4 keel bottom     */
};
static const p3_face plane_faces[3] = {
    {0,3,1, P3_NO_VERT, P3_MAT_WHITE, P3_FACE_DOUBLE_SIDED},  /* left wing  */
    {0,2,3, P3_NO_VERT, P3_MAT_CREAM, P3_FACE_DOUBLE_SIDED},  /* right wing */
    {0,4,3, P3_NO_VERT, P3_MAT_WHITE, P3_FACE_DOUBLE_SIDED},  /* keel fin   */
};
static const p3_model plane_model = { plane_verts, 5, plane_faces, 3, 0, 0 };

/* --- a big paper ground plane (sage field) ---------------------------- */
static const p3_vec3 ground_verts[4] = {
    {-4000,-95,  60},{4000,-95,  60},{4000,-95,4000},{-4000,-95,4000},
};
static const p3_face ground_faces[1] = {
    {0,1,2,3, P3_MAT_SAGE, P3_FACE_DOUBLE_SIDED},
};
static const p3_model ground_model = { ground_verts, 4, ground_faces, 1, 0, 0 };

/* cut-paper cloud: an angular white convex blob with a soft drop shadow */
static void draw_cloud(float cx, float cy, float s)
{
    float px[7], py[7];
    static const float ox[7] = {-1.7f,-0.9f, 0.0f, 0.9f, 1.7f, 0.8f,-0.8f};
    static const float oy[7] = { 0.15f,-0.55f,-0.8f,-0.55f,0.15f,0.6f, 0.6f};
    /* shadow (offset down, soft blue-grey) */
    for (int i = 0; i < 7; i++) { px[i] = cx + ox[i]*s; py[i] = cy + oy[i]*s + 0.18f*s; }
    p3_fill_convex(px, py, 7, 176, 190, 206);
    /* body (warm paper white) */
    for (int i = 0; i < 7; i++) { px[i] = cx + ox[i]*s; py[i] = cy + oy[i]*s; }
    p3_fill_convex(px, py, 7, 248, 246, 238);
}

static void plane_init(void)
{
    og_materials();
    /* a touch greener ground than the default sage */
    p3_set_material(P3_MAT_SAGE, 150,188,128);
}

static void plane_frame(uint32_t t_into, uint32_t t_global)
{
    float p = t_into / (float)SCENE_LEN_MS;       /* 0..1 over the scene */

    og_sky();

    /* clouds drift slowly left as the plane crosses right */
    float drift = t_global * 0.010f;
    draw_cloud(fmodf(300 - drift, 460.0f) - 70, 52, 34);
    draw_cloud(fmodf(540 - drift*0.7f, 520.0f) - 90, 84, 26);
    draw_cloud(fmodf(160 - drift*1.3f, 480.0f) - 60, 38, 20);

    p3_render_params rp; p3_params_default(&rp);
    rp.focal = 300.0f;
    rp.cam_pitch = 0.16f;       /* look slightly down to reveal the ground */
    rp.ambient = 0.42f;
    rp.light.x = -0.45f; rp.light.y = 0.74f; rp.light.z = -0.50f;

    /* ground first (overdraws sky below the horizon) */
    p3_render(&ground_model, &rp);

    /* flight path: sweep across, weaving in depth + height, banking */
    float sweep = (p - 0.5f);                       /* -0.5 .. 0.5 */
    rp.ox = sweep * 720.0f;
    rp.oy = 30.0f + 24.0f * sinf(t_into * 0.0016f);
    rp.oz = 430.0f + 120.0f * sinf(t_into * 0.0011f + 1.0f);
    float turn = cosf(t_into * 0.0016f);            /* derivative of height weave */
    rp.roll  = 0.55f * turn;                        /* bank into the turn  */
    rp.yaw   = -0.5f * sweep + 0.25f * sinf(t_into*0.0011f+1.0f);
    rp.pitch = -0.12f + 0.10f * sinf(t_into * 0.0016f);

    /* soft shadow on the ground, then the plane over it */
    p3_render_shadow(&plane_model, &rp, -95.0f, 150, g_scratch.bg_cache);
    p3_render(&plane_model, &rp);
}

static void plane_done(void) {}

const effect_t fx_plane_real = {
    .name = "plane", .mode = MODE_HIRES,
    .init = plane_init, .frame = plane_frame, .done = plane_done,
};
