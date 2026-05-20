/* Torus generation — parametric, regenerated per scene change.
 *
 * Original (dawn_final.s:735-789) pre-bakes 40 keyframes (~110 KB if
 * stored verbatim on the Pico). We instead store ONE frame at a time and
 * re-emit it from three parameters (inner step, amplitude, phase). The
 * three torus scenes use fixed parameter triples; the FINALE varies
 * amplitude continuously with sin(t).
 */

#ifndef TORUS_H
#define TORUS_H

#include "dawn.h"
#include "vector3d.h"

/* Quad polygon — 4 vertex indices. The original lays out vertices in the
 * order [p1, p4, p3, p2] (winding noted in web_port torus.ts:101). */
typedef struct {
    uint8_t v[4];
} torus_poly_t;

extern vec3_t       torus_verts[TORUS_VERTS];
extern vec3_t       torus_norms[TORUS_VERTS];
extern torus_poly_t torus_polys[TORUS_POLYS];
extern int          torus_poly_count;

/* Build polygon connectivity (the loops in dawn_final.s:82-118). Call
 * once at startup. */
void torus_init_polys(void);

/* Generate one torus frame from morph parameters.
 *   inner_step: how the inner ring's UV phase advances (0..255)
 *   amplitude:  morph bulge magnitude (0..48000)
 *   phase:      phase offset for the secondary modulation (0..255)
 * Reference: web_port torus.ts:136-210, dawn_final.s:735-789. */
void torus_build_frame(int inner_step, int amplitude, int phase);

#endif
