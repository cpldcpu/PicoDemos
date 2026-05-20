/* Shared per-scene scratch memory.
 *
 * Only one scene is active at a time, so heavy per-scene buffers (76 KB
 * backdrop caches, 4× 19 KB Gray-Scott fields, etc.) all sit in the
 * same physical bytes via a union. Saves on the order of 400 KB of
 * BSS vs giving each scene its own static.
 *
 * Each scene's init() should treat its slot as uninitialised and write
 * what it needs (re-cache backdrop, re-seed RD, recompute tunnel
 * tables, …). Cross-scene state is not preserved through this storage.
 */

#ifndef THEDEMO_SCENE_SCRATCH_H
#define THEDEMO_SCENE_SCRATCH_H

#include <stdint.h>
#include "vga.h"

/* Tunnel uses half-resolution lookup tables (rendered with 2× pixel
 * doubling). 160×120 uint8 per table is comfortably the largest single
 * member, sharing the same bytes as a full 320×240 bg cache. */
#define SCRATCH_TUNNEL_W   160
#define SCRATCH_TUNNEL_H   120

union scene_scratch_u {
    /* MODE_320 backdrop cache, reused by title, greetz, rotozoom. */
    uint8_t bg_cache[VGA_320_W * VGA_320_H];     /* 76 800 */

    /* MODE_160 dye field — fluid sim. Two ping-pong buffers. */
    struct {
        uint8_t a[VGA_160_W * VGA_160_H];        /* 19 200 */
        uint8_t b[VGA_160_W * VGA_160_H];        /* 19 200 */
    } fluid;

    /* MODE_160 Gray-Scott — u, v with ping-pong each. */
    struct {
        uint8_t u0[VGA_160_W * VGA_160_H];
        uint8_t u1[VGA_160_W * VGA_160_H];
        uint8_t v0[VGA_160_W * VGA_160_H];
        uint8_t v1[VGA_160_W * VGA_160_H];
    } rd;                                         /* 76 800 */

    /* Tunnel per-pixel (u, v) tables, half-resolution uint8. */
    struct {
        uint8_t u_table[SCRATCH_TUNNEL_W * SCRATCH_TUNNEL_H];   /* 19 200 */
        uint8_t v_table[SCRATCH_TUNNEL_W * SCRATCH_TUNNEL_H];   /* 19 200 */
    } tunnel;

};

extern union scene_scratch_u g_scratch;

#endif
