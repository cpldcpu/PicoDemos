#include "vector3d.h"
#include "mathtab.h"

int v3_angle_x = 0;
int v3_angle_y = 0;
int v3_angle_z = 0;
int v3_translation = 3000;
int v3_zoom = 1000;

/* The assembly's `lsr #5` converts a 16-bit angle to a 0..2047 index into a
 * sine table of length 2048 (the original keeps multiple copies to avoid a
 * mask). We use a single 1024 table and mask — equivalent up to a /2 in
 * granularity, indistinguishable visually. */
static inline int angle_to_sin_idx(int angle)
{
    return (angle >> 6) & SIN_TAB_MASK;   /* 16-bit → 1024 */
}

/* sin_tab values peak at ~32255 (~31.5 K). Treating that as 1.0 in 16.16
 * means a multiply by sin then >> 15 gives a signed-fixed result roughly
 * equivalent to sin * x with sin in [-1, 1]. We use shifts that match the
 * assembly's fixed-point flow (no calibration constants needed). */
static inline int fixmul_sin(int v, int s)
{
    return (v * s) >> 15;
}

void v3_rotate_project(const vec3_t *in, screen_pt_t *out)
{
    const int x0 = in->x;
    const int y0 = in->y;
    const int z0 = in->z;

    const int ay = angle_to_sin_idx(v3_angle_y);
    const int ax = angle_to_sin_idx(v3_angle_x);

    const int sinY = sin_lookup(ay);
    const int cosY = cos_lookup(ay);
    const int sinX = sin_lookup(ax);
    const int cosX = cos_lookup(ax);

    /* Rotate about Y: x' = x*cos - z*sin; z' = x*sin + z*cos. */
    const int x1 = fixmul_sin(x0, cosY) - fixmul_sin(z0, sinY);
    const int z1 = fixmul_sin(x0, sinY) + fixmul_sin(z0, cosY);
    const int y1 = y0;

    /* Rotate about X: y'' = y*cos - z'*sin; z'' = y*sin + z'*cos. */
    const int y2 = fixmul_sin(y1, cosX) - fixmul_sin(z1, sinX);
    const int z2 = fixmul_sin(y1, sinX) + fixmul_sin(z1, cosX);
    const int x2 = x1;

    /* Perspective: screen = (coord * zoom) / (zoom + z + translation), then
     * shift to screen center. The web port noted that with vertices stored
     * at engine scale, zoom=1000 keeps object size in the visible window;
     * we use the same constant. */
    const int denom = v3_zoom + z2 + v3_translation;
    const int d = denom > 0 ? denom : 1;
    const int sx = ((x2 * v3_zoom) / d) + SCREEN_W / 2;
    const int sy = ((y2 * v3_zoom) / d) + SCREEN_H / 2;

    out->sx = (int16_t)sx;
    out->sy = (int16_t)sy;
    out->depth = (int16_t)(z2 + v3_translation);
}

void v3_normal_to_uv(const vec3_t *normal, int *u_out, int *v_out)
{
    /* Direct port of dawn_final.s:1693-1727 (the asm normal-rotation
     * loop). The TS reference (web_port/vector3d.ts:110-144) had one
     * bug we'd been inheriting:
     *
     *   It inverse-rotates normals via `Math.sin(-ay)`. The asm reuses
     *   the SAME d5/d6 sin/cos values as the vertex rotation at lines
     *   1632-1691 — normals rotate the same direction as the object.
     *   That gives world-anchored env-map lighting (highlight stays
     *   where the "light" is in world space, surface slides through);
     *   the TS gave object-anchored (highlight painted on the surface).
     *
     * Bias accounting: the asm stores `high16(rot_nx) + 64` to
     * tm_imag (line 1722) AND at sample time the `light` base pointer
     * is `ppic + 256*64 + 64` (line 1088), so the effective sample x
     * is `64 + u_stored + 64 = 128 + high16(rot_nx)`. We bake both
     * +64s into a single +128 here so env_sample() sees ready-to-look-
     * up coordinates. Brightness peaks at u=128, which happens when
     * `high16(rot_nx) = 0` — viewer-facing normals, the physically
     * correct location for a specular highlight.
     *
     * Our fixmul_sin uses >> 15 with sin/cos peaking at ~32255, which
     * preserves magnitude. The asm's `swap`/high16 pattern is >> 16,
     * giving half the magnitude — so `(nx1 >> 1)` here matches the
     * asm's `high16(rot_nx)` directly. The asm's `asl d2` doubling of
     * rotated_z before the X rotation is implicit in our path: our
     * nz1 already IS the asm's `2*high16(rot_z)`. */
    const int ay = angle_to_sin_idx(v3_angle_y);
    const int ax = angle_to_sin_idx(v3_angle_x);
    const int sinY = sin_lookup(ay);   /* POSITIVE — forward rotation */
    const int cosY = cos_lookup(ay);
    const int sinX = sin_lookup(ax);
    const int cosX = cos_lookup(ax);

    const int nx0 = normal->x;
    const int ny0 = normal->y;
    const int nz0 = normal->z;

    /* Forward Y rotation: x' = x cosY - z sinY,  z' = x sinY + z cosY. */
    const int nx1 = fixmul_sin(nx0, cosY) - fixmul_sin(nz0, sinY);
    const int nz1 = fixmul_sin(nx0, sinY) + fixmul_sin(nz0, cosY);

    /* Forward X rotation, only the y' we need for v. */
    const int ny2 = fixmul_sin(ny0, cosX) - fixmul_sin(nz1, sinX);

    *u_out = ((nx1 >> 1) + 128) & 0xFF;
    *v_out = ((ny2 >> 1) + 128) & 0xFF;
}
