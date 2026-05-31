/* Shared per-scene scratch memory for ORIGAMI.
 *
 * Only one scene is active at a time, so the heavy per-scene buffers all
 * alias the same physical bytes via a union. The largest member is the
 * 76 800-byte MODE_320 backdrop cache, so the animated-geometry members
 * below (Miura grid verts, confetti quads + state) cost ZERO extra BSS.
 *
 * The poly3d engine keeps its OWN transform/sort working set in file-static
 * BSS inside poly3d.c (~30 KB) — independent of this union — so a scene can
 * hold an animated model HERE and still call p3_render() which writes there.
 *
 * Each scene's init() must treat its slot as uninitialised and seed it.
 */

#ifndef ORIGAMI_SCENE_SCRATCH_H
#define ORIGAMI_SCENE_SCRATCH_H

#include <stdint.h>
#include "vga.h"
#include "effects/poly3d.h"

/* Miura-ori field (scene 4): NX*NY parallelogram cells on an (NX+1)*(NY+1)
 * vertex grid, vertices animated each frame into a travelling fold wave. */
#define MIURA_NX     18
#define MIURA_NY     11
#define MIURA_NVERT  ((MIURA_NX+1)*(MIURA_NY+1))   /* 228 */
#define MIURA_NFACE  (MIURA_NX*MIURA_NY)           /* 198 */

/* Confetti (scene 5): each is a free-floating quad (4 world verts). */
#define CONFETTI_N   150

union scene_scratch_u {
    /* MODE_320 backdrop cache — blit a packed 8bpp backdrop here once in
     * init(), then memcpy per frame (flash->SRAM). Keeps the union at 76800. */
    uint8_t bg_cache[VGA_320_W * VGA_320_H];          /* 76 800 */

    /* Scene 4 — Miura-ori wave: animated world-space vertex grid. */
    struct {
        p3_vec3 v[MIURA_NVERT];                       /* 2 736 */
    } miura;

    /* Scene 5 — confetti credits: animated quads + per-piece physics. */
    struct {
        p3_vec3 v[CONFETTI_N * 4];                    /* 7 200 */
        p3_face f[CONFETTI_N];                        /* 1 800 */
        struct {
            float px, py, pz;        /* position           */
            float vx, vy, vz;        /* velocity           */
            float ax, ay, az;        /* unit spin axis     */
            float ang, angv;         /* spin angle + rate  */
            float half;              /* half-size          */
            uint8_t mat;             /* paper material     */
        } c[CONFETTI_N];                              /* ~8 400 */
    } confetti;
};

extern union scene_scratch_u g_scratch;

#endif
