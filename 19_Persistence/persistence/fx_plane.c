/* The plane: an infinite floor, and a solid object hanging over it with its
 * reflection.
 *
 * Three layers per row, all cheap: the affine floor (interpolator), the
 * reflection's spans (dimmed, inserted first), the object's spans. The camera
 * for the object is the one the floor already implies -- principal point at
 * (320, horizon), focal length 320, camera height H above the plane -- so an
 * object at height h sits at camera-space y = H - h and its mirror image at
 * y = H + h. Nothing is composited; the rows are simply drawn over twice.
 *
 * The reflection is the reason the object and the floor look like they are in
 * the same world rather than one pasted on the other, and it costs one extra
 * span pass with the mesh's y row negated.
 */

#include "beam.h"
#include "arena.h"
#include "rgb565.h"
#include "tables.h"
#include "affine.h"
#include "s3d.h"
#include "mesh.h"
#include "song.h"

#include <math.h>
#include <string.h>

static mesh_t s_sphere, s_torus, s_knot, s_gem;
/* A coarse stand-in used ONLY for reflections: dim, distorted by the ripple of
 * the floor and half the size on screen, so nobody counts its facets -- and it
 * halves the boundaries the busiest rows have to hold. */
static mesh_t s_knot_lo, s_torus_lo, s_sphere_lo;

/* Which mesh stands in for which when reflected. */
static const mesh_t *refl_of(const mesh_t *m)
{
    if (m == &s_knot)   return &s_knot_lo;
    if (m == &s_torus)  return &s_torus_lo;
    if (m == &s_sphere) return &s_sphere_lo;
    return m;                                  /* the gem is already 36 faces */
}

static void plane_enter(void)
{
    affine_texture_generate(AFFINE_TEX_FLOOR);
    mesh_pool_reset();
    /* Triangle counts are chosen by how many the widest row crosses, not by
     * how smooth the silhouette looks: each crossing is one boundary in the
     * span list (s3d.h). A tube of 8 is crossed 16 times. */
    mesh_icosphere(&s_sphere, 2);                  /* 320 tris */
    mesh_torus(&s_torus, 20, 7, 1.0f, 0.42f);      /* 280 */
    mesh_knot(&s_knot, 2, 3, 36, 5, 1.30f, 0.34f); /* 360 */
    mesh_knot(&s_knot_lo, 2, 3, 20, 4, 1.30f, 0.34f);  /* 160, reflections */
    mesh_torus(&s_torus_lo, 14, 5, 1.0f, 0.42f);       /* 140, reflections */
    mesh_icosphere(&s_sphere_lo, 1);                   /*  80, reflections */
    mesh_gem(&s_gem, 9);                           /*  36 */
    s3d_init();
}

/* Shared by the plane and the finale: place, light and draw one object over
 * the floor, with its reflection. `lift` is the beat bounce in texels. */
void plane_object(uint32_t parity, const mesh_t *m, float spin, float camh,
                  int horizon, float height, float scale, float tz,
                  const s3d_material_t *mat, const mesh_t *refl)
{
    s3d_view_t view = { 320.0f, 320, horizon, 0, PV_H };

    s3d_xform_t xf;
    s3d_rot(xf.m, spin * 0.9f, spin * 0.7f, spin * 0.35f);
    xf.scale = scale;
    xf.tx = 0.0f;
    xf.tz = tz;
    xf.ty = camh - height;

    if (refl) {
        s3d_xform_t rf = xf;
        rf.ty = camh + height;
        rf.m[1][0] = -rf.m[1][0]; rf.m[1][1] = -rf.m[1][1]; rf.m[1][2] = -rf.m[1][2];
        s3d_material_t rmat = *mat;
        rmat.dim = 88; rmat.spec = 70;
        s3d_view_t rview = view;
        rview.y_min = horizon + 1 > 0 ? horizon + 1 : 0;
        s3d_object(parity, refl, &rf, &rmat, &rview);
    }
    s3d_object(parity, m, &xf, mat, &view);
}

static void plane_frame(uint32_t f, uint32_t local)
{
    const float t = (float)local / 60.0f;
    const uint32_t par = f & 1;

    /* The horizon sweeps down from far above: a rotozoom becoming a floor.
     * One parameter, and the two effects are the same kernel (affine.c). */
    float tilt = t < 5.0f ? t / 5.0f : 1.0f;
    tilt = tilt * tilt * (3.0f - 2.0f * tilt);
    const int horizon = (int)(-4000.0f + tilt * (4000.0f + 150.0f));

    affine_cam_t cam;
    cam.horizon  = horizon;
    /* Height is chosen so the centre row keeps sampling ~260 texels deep as
     * the horizon moves; otherwise the texture explodes during the sweep. */
    cam.height   = 260.0f * (240.0f - (float)horizon) / 320.0f;
    if (tilt >= 1.0f) cam.height = 72.0f;
    cam.angle    = t * 0.22f;
    cam.x        = 128.0f + 95.0f * t;
    cam.y        = 128.0f + 60.0f * sinf(t * 0.4f);
    cam.fog_near = tilt >= 1.0f ? 420.0f : 0.0f;
    cam.fog_far  = 1700.0f;
    affine_rows(&cam, par);

    uint16_t sky[PV_H];
    affine_sky_dusk(sky, horizon, 0);
    affine_sky(sky, par);

    s3d_begin(par);
    if (tilt < 1.0f) return;                        /* no object until the floor is level */

    const float tt = t - 5.0f;
    const mesh_t *m = &s_torus;
    if (tt > 7.0f)  m = &s_knot;
    if (tt > 14.0f) m = &s_sphere;
    if (tt > 21.0f) m = &s_gem;

    /* the kick lifts it */
    const uint32_t step = pv_step_of_frame(f);
    const float since = (float)(f % PV_FPB) / (float)PV_FPB;
    const float bounce = (song_drums(step) & DR_KICK) ? (1.0f - since) * (1.0f - since) * 14.0f : 0.0f;

    s3d_material_t mat = { 0.45f, -0.70f, -0.55f, 235, 155, 65, 210, 55, 256 };
    const float ll = sqrtf(mat.lx * mat.lx + mat.ly * mat.ly + mat.lz * mat.lz);
    mat.lx /= ll; mat.ly /= ll; mat.lz /= ll;

    plane_object(par, m, tt, cam.height, horizon,
                 105.0f + 18.0f * sinf(tt * 0.8f) + bounce,
                 150.0f, 900.0f, &mat, refl_of(m));
}

void PV_HOT(plane_line)(uint32_t f, uint16_t *px, int y)
{
    affine_line_p(f, px, y);
    s3d_line(f, px, y);
}

void plane_setup1(void) { qs_texmap_setup_interp0(); }

const scene_t fx_plane = { "plane", plane_enter, plane_frame, plane_line, plane_setup1, NULL };

/* ------------------------------------------------------------- finale ----
 *
 * The same floor and the same span renderer, flown hard: a wider grid, three
 * objects instead of one, and a camera low enough that they pass overhead.
 * Reusing the scene rather than writing a second one is the point -- the
 * difference between the two is a parameter block, which is the lesson demo 16
 * taught this group about morphs and which applies just as well to cuts.
 */

static void finale_enter(void)
{
    affine_texture_generate(AFFINE_TEX_GRID);
    mesh_pool_reset();
    mesh_icosphere(&s_sphere, 1);                  /* 80 tris: three at once  */
    mesh_gem(&s_gem, 8);
    mesh_box(&s_torus, 0.8f, 0.8f, 0.8f);          /* reuse the slot as a cube */
    /* The finale carries three objects and a reflection at once, and every
     * triangle a row crosses is one more boundary in that row's list. Five
     * segments round the tube is the coarsest that still reads as round at
     * this size, and it is what keeps the busiest frame inside the budget the
     * audit enforces. */
    mesh_knot(&s_knot, 2, 3, 26, 5, 1.30f, 0.34f); /* 260 */
    mesh_knot(&s_knot_lo, 2, 3, 16, 4, 1.30f, 0.34f);  /* 128, reflections */
    mesh_torus(&s_torus_lo, 12, 4, 1.0f, 0.42f);       /*  96 */
    mesh_icosphere(&s_sphere_lo, 1);
    s3d_init();
}

static void finale_frame(uint32_t f, uint32_t local)
{
    const float t = (float)local / 60.0f;
    const uint32_t par = f & 1;
    const int horizon = 168;

    affine_cam_t cam;
    cam.horizon  = horizon;
    cam.height   = 54.0f;
    cam.angle    = 0.35f * sinf(t * 0.30f);        /* banking, not spinning */
    cam.x        = 128.0f + 300.0f * t;            /* fast */
    cam.y        = 128.0f + 40.0f * sinf(t * 0.5f);
    cam.fog_near = 500.0f;
    cam.fog_far  = 2100.0f;
    affine_rows(&cam, par);

    uint16_t sky[PV_H];
    affine_sky_dusk(sky, horizon, 1);
    affine_sky(sky, par);

    s3d_begin(par);

    const uint32_t step = pv_step_of_frame(f);
    const float since = (float)(f % PV_FPB) / (float)PV_FPB;
    const float bounce = (song_drums(step) & DR_KICK) ? (1.0f - since) * (1.0f - since) * 22.0f : 0.0f;

    s3d_material_t mat = { 0.40f, -0.72f, -0.57f, 250, 170, 80, 220, 50, 256 };
    const float ll = sqrtf(mat.lx * mat.lx + mat.ly * mat.ly + mat.lz * mat.lz);
    mat.lx /= ll; mat.ly /= ll; mat.lz /= ll;

    /* the knot leads, flanked by two smaller solids that swing around it */
    plane_object(par, &s_knot, t * 1.1f, cam.height, horizon,
                 96.0f + 14.0f * sinf(t * 0.9f) + bounce, 130.0f, 780.0f, &mat, &s_knot_lo);

    s3d_material_t cold = mat;
    cold.r = 90; cold.g = 190; cold.b = 235;
    for (int i = 0; i < 2; i++) {
        const float ph = t * 0.8f + (float)i * 3.14159f;
        s3d_xform_t xf;
        s3d_rot(xf.m, t * 1.6f + i, t * 1.1f, t * 0.5f);
        xf.scale = 40.0f;
        xf.tx = 260.0f * sinf(ph);
        xf.tz = 780.0f + 220.0f * cosf(ph);
        xf.ty = cam.height - (70.0f + 30.0f * sinf(t * 1.3f + i));
        s3d_view_t view = { 320.0f, 320, horizon, 0, PV_H };
        s3d_object(par, i ? &s_gem : &s_torus, &xf, &cold, &view);
    }
}

const scene_t fx_finale = { "finale", finale_enter, finale_frame, plane_line, plane_setup1, NULL };
