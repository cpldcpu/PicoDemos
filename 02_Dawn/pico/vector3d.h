/* 3D rotation and projection — Y then X axis, perspective divide.
 *
 * Original: dawn_final.s:1632-1728 (rotation matrices), :1676-1686
 * (projection), :1693-1727 (normal → UV).
 *
 * RP2040 has no FPU on Cortex-M0+ but is fast enough at int multiplies that
 * we keep the original 16.16-style fixed-point throughout. Angles are in
 * the 0..65535 range and divided by 32 (>> 5) when indexing the sine LUT,
 * matching the assembly's `lsr #5`.
 */

#ifndef VECTOR3D_H
#define VECTOR3D_H

#include "dawn.h"

typedef struct { int16_t x, y, z; } vec3_t;

typedef struct {
    int16_t sx, sy;     /* screen pixel (0..159, 0..127) */
    int16_t depth;      /* z value used for painter sort */
} screen_pt_t;

/* Rotation state — see sequencer.c for animation. */
extern int v3_angle_x;
extern int v3_angle_y;
extern int v3_angle_z;
extern int v3_translation;     /* ro_trans; varies between 800 and 3000 */
extern int v3_zoom;            /* RO_zoom; original constant #1000 */

void v3_rotate_project(const vec3_t *in, screen_pt_t *out);
void v3_normal_to_uv(const vec3_t *normal, int *u_out, int *v_out);

#endif
