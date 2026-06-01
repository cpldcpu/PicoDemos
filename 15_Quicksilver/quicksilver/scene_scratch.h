/* Shared per-scene scratch memory for QUICKSILVER.
 *
 * Only one scene is active at a time, so heavy per-scene buffers all alias the
 * same physical bytes via a union (zero extra BSS for the smaller members).
 * The per-frame transient working sets of the renderers (poly3d / envmap3d)
 * live in their own file-static BSS; this union holds animated SCENE geometry
 * and large caches.
 *
 * Each scene's init() must treat its slot as uninitialised. Cross-scene state
 * is NOT preserved here.
 */

#ifndef QS_SCENE_SCRATCH_H
#define QS_SCENE_SCRATCH_H

#include <stdint.h>
#include "vga.h"

/* Max animated vertices a scene may keep here (chrome morph targets). The
 * 76 800-byte bg_cache is the largest member and fixes the union size, so this
 * costs nothing extra up to ~6400 verts. */
#define QS_SCRATCH_MAX_VERTS 2600

union scene_scratch_u {
    /* Generic 320x240 byte scratch — coverage masks, 8bpp caches. 76 800 B. */
    uint8_t bg_cache[VGA_320_W * VGA_320_H];

    /* Animated chrome geometry (morph result written each frame in world
     * space, then handed to envmap3d). 2600 * 12 = 31 200 B. */
    struct { float x, y, z; } morph_verts[QS_SCRATCH_MAX_VERTS];
};

extern union scene_scratch_u g_scratch;

#endif
