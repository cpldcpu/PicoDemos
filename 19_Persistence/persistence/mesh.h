/* Procedural meshes, generated into the arena's MESH region on enter().
 * Radius about 1; triangles wound so that the outward face is counter-
 * clockwise on screen after the y-down projection (see the notes in mesh.c --
 * an inverted winding does not look like an error, it looks like a torn mesh). */

#ifndef PV_MESH_H
#define PV_MESH_H

#include <stdint.h>

typedef struct {
    int       nv, nt;
    float    *v;      /* nv * 3 */
    uint16_t *t;      /* nt * 3 */
} mesh_t;

void     mesh_pool_reset(void);
uint32_t mesh_pool_used(void);

int  mesh_icosphere(mesh_t *m, int subdiv);                 /* 0: 20 tris, 1: 80, 2: 320 */
int  mesh_torus(mesh_t *m, int nu, int nv, float R, float r);
int  mesh_knot(mesh_t *m, int p, int q, int segs, int tube, float R, float r);
int  mesh_gem(mesh_t *m, int sides);                        /* a cut crystal */
int  mesh_box(mesh_t *m, float hx, float hy, float hz);

#endif
