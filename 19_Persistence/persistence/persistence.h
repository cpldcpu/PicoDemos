/* PERSISTENCE — the constants everything is built from.
 *
 * There is no framebuffer in this production. The numbers below are the whole
 * contract between the two cores: core 0 prepares per-row tables for frame f,
 * core 1 turns rows into 640 pixels for the beam, and audio is 400 samples per
 * frame so the picture and the sound share one integer clock.
 */

#ifndef PV_PERSISTENCE_H
#define PV_PERSISTENCE_H

#include <stdint.h>

#define PV_W            640
#define PV_H            480
#define PV_FPS          60

#define PV_RATE         24000            /* Hz, stereo                          */
#define PV_SPF          400              /* samples per frame: 24000 / 60       */

#define PV_BPM          144
#define PV_FPB          25               /* frames per beat: 3600 / 144         */
#define PV_SPB          10000            /* samples per beat                    */
#define PV_FPBAR        100              /* frames per bar (4/4)                */
#define PV_STEPS_PER_BAR 16              /* 16ths                               */

#define PV_BARS         90
#define PV_TOTAL_FRAMES (PV_BARS * PV_FPBAR)          /* 9000 = 2:30 */
#define PV_TOTAL_SAMPLES ((uint32_t)PV_TOTAL_FRAMES * PV_SPF)

/* 16th-note step index for a frame: 16 steps per 100 frames. Exact in integer
 * arithmetic (f * 4 / 25), which is the reason for 144 BPM. */
static inline uint32_t pv_step_of_frame(uint32_t f) { return (f * 4u) / 25u; }
static inline uint32_t pv_bar_of_frame(uint32_t f)  { return f / PV_FPBAR; }

/* SRAM placement for the hot path. Anything core 1 executes per line goes in
 * SRAM: a kernel that streams 640 pixels will evict its own code from the XIP
 * cache otherwise (17_Hysteresis/hot.h, 15_Quicksilver's vga640 findings). */
#if defined(HOST_BUILD)
#  define PV_HOT(fn) fn
#  define PV_SRAM_DATA
#else
#  include "pico.h"
#  define PV_HOT(fn) __not_in_flash_func(fn)
#  define PV_SRAM_DATA
#endif

#endif
