/* demo.h -- the contract between the platform and the renderer.
 *
 * Overscan owns everything on one side of this line (main.c, video.c,
 * audio_pwm.c, the build, the host tools) and Phase everything on the other
 * (render*.c, body*.c, scene_*.c, assets). Neither edits the other's files;
 * this header is Phosphor's and changes only by agreement.
 *
 * The picture is a function of the audio clock. demo_render() draws the
 * moment `sample` -- the absolute stereo frame the DAC is playing -- into a
 * 320x240 page in the VGA DAC's packing (red bits 0-4, green 6-10, blue
 * 11-15; see colossus.h). It must be callable for any sample in any order:
 * camera, particle and mechanism state derive from `sample` and from the
 * score (song.h), never from the previous frame. That is what lets a skipped
 * frame skip and a host seek land.
 */

#ifndef CV_DEMO_H
#define CV_DEMO_H

#include <stdint.h>
#include "colossus.h"

void demo_init(void);                              /* once, after synth_init() */
void demo_render(uint16_t *page, uint32_t sample);  /* the whole page, every call */

/* What the last demo_render() cost, for telemetry (referee 3). */
typedef struct {
    uint32_t triangles;          /* submitted to clipping                    */
    uint32_t fill;               /* candidate fragments, depth-tested        */
    uint32_t particles;          /* live embers                              */
    uint8_t  chapter;            /* song_section() of the drawn moment       */
} demo_stats_t;
void demo_stats(demo_stats_t *out);

/* The DAC packing, in one place. */
static inline uint16_t cv_rgb(int r, int g, int b)
{
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (uint16_t)((r >> 3) | ((g & 248) << 3) | ((b & 248) << 8));
}

#endif
