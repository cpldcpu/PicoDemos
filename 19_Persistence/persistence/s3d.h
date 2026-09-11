/* Solid 3D without a framebuffer: span lists.
 *
 * Core 0 transforms a mesh, culls, sorts the faces far-to-near, and scan
 * converts each one into per-row spans {x0, x1, colour}. That list is the
 * only thing that crosses to core 1, which draws the spans of row y in order
 * -- later spans overwrite earlier ones, so painter's order is occlusion. It
 * is the S-buffer idea from the software-rendering era, which was invented
 * for machines that could not afford a z-buffer and turns out to be exactly
 * what a machine that cannot afford a framebuffer needs.
 *
 * Flat shading, one colour per face. Each row keeps only the boundaries that
 * survive occlusion, at most S3D_MAX_SEGS; beyond that the incoming span is
 * dropped for that row and counted (the host audit reports overflows, per
 * scene, and a build that drops any does not ship).
 *
 * How many are needed is set by the MESHES, not by the screen: the list length
 * is the number of visible colour runs across the row, and with flat shading
 * that is one per triangle the row crosses. A torus tubed at eleven segments
 * is crossed twenty-two times, so twenty-four boundaries was under half of
 * what one object needed and the audit counted 279,302 dropped spans -- every
 * one of them a hole. Thirty-two boundaries and coarser tubes fit; the coarser
 * tubes also suit the flat-shaded look better, which is the rare case where
 * the cheap answer is the better-looking one.
 *
 * Lists are double-buffered by frame parity, like everything else.
 */

#ifndef PV_S3D_H
#define PV_S3D_H

#include <stdint.h>
#include "mesh.h"

#define S3D_MAX_SEGS 32

/* A boundary: from x onwards the row shows c, until the next boundary. */
typedef struct { uint16_t x, c; } seg_t;

typedef struct {
    float m[3][3];       /* rotation (rows), applied to the unit mesh       */
    float scale;
    float tx, ty, tz;    /* camera-space position: x right, y down, z forward */
} s3d_xform_t;

typedef struct {
    float lx, ly, lz;            /* light direction (towards the light), unit */
    uint8_t r, g, b;             /* base colour                                */
    uint8_t spec;                /* specular strength 0..255                   */
    uint8_t amb;                 /* ambient 0..255                             */
    int     dim;                 /* 0..256 multiplier for the whole face colour */
} s3d_material_t;

typedef struct {
    float F;                     /* focal length in pixels                     */
    int   cx, cy;                /* principal point (cy is the horizon row)    */
    int   y_min, y_max;          /* rows that may receive spans, [y_min, y_max) */
} s3d_view_t;

void s3d_init(void);                            /* once: point at the arena  */
void s3d_begin(uint32_t parity);                /* clear the list             */
/* Transform + shade + rasterise one object into the current list. */
void s3d_object(uint32_t parity, const mesh_t *m, const s3d_xform_t *xf,
                const s3d_material_t *mat, const s3d_view_t *view);
/* Draw row y's spans over px (core 1). */
void s3d_line(uint32_t parity, uint16_t *px, int y);

/* Rows that were full and had to give up their narrowest run to fit an
 * incoming nearer span. Not a dropped face: the near surface is always kept
 * (see insert_span). Reported and budgeted rather than forbidden. */
uint32_t s3d_merges(void);
/* How many of the 480 rows lost a sliver in the current frame. This is the
 * number the audit budgets, because it is the fraction of the picture that is
 * affected; the raw event count is dominated by whichever single row happens
 * to be busiest. */
uint32_t s3d_rows_merged(void);
uint32_t s3d_last_faces(void);                  /* faces drawn by the last object */

/* Rotation helpers (row-major, m * v). */
void s3d_rot(float m[3][3], float ax, float ay, float az);

#endif
