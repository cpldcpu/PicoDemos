/* Shared per-scene scratch memory for the VOLTAGE demo.
 *
 * To avoid blowing the 520 KB SRAM limit on the RP2350, heavy per-scene
 * buffers share the same physical memory space via this global union.
 * Only one scene is active at a time, meaning each scene must treat its
 * union member as uninitialised on init() and populate it from scratch.
 */

#ifndef VOLTAGE_SCENE_SCRATCH_H
#define VOLTAGE_SCENE_SCRATCH_H

#include <stdint.h>
#include "vga.h"

#define SCRATCH_TUNNEL_W   160
#define SCRATCH_TUNNEL_H   120

union scene_scratch_u {
    /* 320x240 background buffer (76,800 bytes) used for backdrops,
     * scrolling text buffers, and rotozoom caches. Reused across scenes. */
    uint8_t bg_cache[VGA_320_W * VGA_320_H];

    /* Scene 2: Plasma Core (Fluid & Particle simulation) */
    struct {
        uint8_t dye[VGA_160_W * VGA_160_H];       /* Active charge density field */
        uint8_t prev_dye[VGA_160_W * VGA_160_H];  /* Back advection target buffer */
        
        /* 1024 dynamic high-voltage energy particles */
        uint16_t px[1024];
        uint16_t py[1024];
        uint8_t pcol[1024];
    } plasma;

    /* Scene 3: Ray-Volt (Raymarching Specular Highlights) */
    struct {
        uint8_t shade_cache[VGA_160_W * VGA_160_H]; /* Raymarching shadow/ao cache */
    } ray;

    /* Scene 4: Vector Strike (3D Wireframe + 8bpp Scroller) */
    struct {
        float x[128];
        float y[128];
        float z[128];
        uint16_t lines_start[256];
        uint16_t lines_end[256];
        uint8_t text_buffer[80 * 240];  /* Scroll overlay text frame cache */
    } vector;

    /* Scene 5: Spark Generator (Lightning Polar Tunnel) */
    struct {
        uint8_t angle[SCRATCH_TUNNEL_W * SCRATCH_TUNNEL_H]; /* Angle map */
        uint8_t dist[SCRATCH_TUNNEL_W * SCRATCH_TUNNEL_H];  /* Distance/depth map */
    } tunnel;

    /* Scene 6: Julia Shockwave (Julia Fractal morph) */
    struct {
        uint8_t iter[VGA_160_W * VGA_160_H]; /* Fractal iteration counts */
    } julia;
};

extern union scene_scratch_u g_scratch;

#endif
