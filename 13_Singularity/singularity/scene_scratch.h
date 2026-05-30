/* Shared per-scene scratch memory for SINGULARITY.
 *
 * Only one scene is active at a time, so the heavy per-scene buffers all
 * alias the same physical bytes via a union. The largest member is the
 * 76 800-byte MODE_320 backdrop cache (title + rebirth), so adding the
 * smaller MODE_160 scene members below costs ZERO extra BSS.
 *
 * Each scene's init() must treat its slot as uninitialised and seed what
 * it needs. Cross-scene state is NOT preserved through this storage.
 */

#ifndef SINGULARITY_SCENE_SCRATCH_H
#define SINGULARITY_SCENE_SCRATCH_H

#include <stdint.h>
#include "vga.h"

/* Particle counts — sized so each struct stays well under the 76 800 B
 * bg_cache so the union size is unchanged. Bumped for the 320x240 canvas
 * (4× the pixels of the old 160x120). */
#define NEBULA_PARTICLES   6000   /* 6000 * 12 B = 72 000 */
#define STARFIELD_STARS    4700   /* 4700 * 16 B = 75 200 */

union scene_scratch_u {
    /* MODE_320 backdrop cache — title + rebirth blit the packed 8bpp
     * backdrop here once in init(), then memcpy per frame (flash→SRAM,
     * same motivation as 10_TheDemo's title). Keeps the union at 76 800. */
    uint8_t bg_cache[VGA_320_W * VGA_320_H];          /* 76 800 */

    /* Scene 1 — curl-noise nebula. Particle cloud advected by an
     * analytic flow field (no stored grid). ~49 KB. */
    struct {
        struct { float x, y; uint8_t life; } p[NEBULA_PARTICLES];
    } nebula;

    /* Scene 3 — relativistic starfield. Unit-ish direction per star
     * plus an intrinsic magnitude. ~48 KB. */
    struct {
        struct { float x, y, z; uint8_t mag; } s[STARFIELD_STARS];
    } stars;
};

extern union scene_scratch_u g_scratch;

#endif
