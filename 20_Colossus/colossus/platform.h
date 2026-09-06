/* platform.h -- Overscan's side of demo.h.
 *
 * Everything the RP2350 platform offers the demo and the telemetry. The
 * renderer never sees this file; main.c, video.c and audio_pwm.c are its only
 * users. Host builds do not compile any of it.
 *
 * The division of labour, which is VESPER's:
 *
 *   core 0   demo_render() into the back page, then video_present(), which
 *            publishes the page and blocks until core 1 has latched it at a
 *            scanline zero. Also the telemetry printing.
 *   core 1   scanout: one 320-pixel row doubled into a 640-pixel scanline
 *            buffer, and audio_pump() in the gap after handing the buffer
 *            back. Nothing else. It must never touch flash.
 *
 * The clock is the DMA's sample counter (audio_position()); the picture asks
 * what sample is playing and draws that moment. Nothing counts frames.
 */

#ifndef CV_PLATFORM_H
#define CV_PLATFORM_H

#include <stdint.h>
#include "colossus.h"

/* ------------------------------------------------------------------ video -- */

void      video_init(void);        /* launch core 1; returns once it is live  */
uint16_t *video_back(void);        /* the page core 0 may draw into           */
void      video_present(void);     /* publish it and wait for the latch       */

/* Telemetry, sampled by core 0. The cycle counters are core 1's free-running
 * SysTick and wrap; take differences. */
typedef struct {
    uint32_t vsyncs;        /* scanline-zero events since scanout began       */
    uint32_t lines;         /* scanline buffers generated                     */
    uint32_t copy_cycles;   /* summed cost of the doubling copy (wraps)       */
    uint32_t copy_worst;    /* worst single line, cycles                      */
    uint32_t pumps;         /* audio_pump() calls from core 1                 */
    uint32_t pump_cycles;   /* summed cost of them (wraps)                    */
    uint32_t pump_worst;    /* worst single pump, cycles                      */
} video_prof_t;
void      video_prof(video_prof_t *out);

/* How many refreshes the page before the current one was shown for. 1 is on
 * time at 60 Hz, 2 is 30 Hz, 3 or more is under the floor. */
uint32_t  video_last_hold(void);
uint32_t  video_scanout_lines_per_frame(void);   /* 240 or 480, see video.c  */

/* ------------------------------------------------------------------ audio -- */

void      audio_init(void);        /* PWM, DMA, ring prefilled; DMA not going */
void      audio_start(void);       /* sample 0 leaves now                     */
void      audio_pump(void);        /* core 1, between scanlines               */
uint32_t  audio_position(void);    /* stereo frames the DAC has played        */
unsigned  audio_min_fill(void);    /* shallowest the ring has been, frames    */
uint32_t  audio_underruns(void);   /* times the DAC outran the synth          */

/* -------------------------------------------------------------------- heap -- */

uint32_t  heap_region(void);       /* __StackLimit - __end__, from the linker */
uint32_t  heap_free(void);         /* unallocated tail, without allocating    */

#endif
