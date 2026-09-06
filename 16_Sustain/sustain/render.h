/* SUSTAIN — the one renderer.
 *
 * There is exactly one of these for the whole demo. It walks view rays through
 * the blended world (world.h) and never learns which field it is drawing, how
 * many fields are live, or that a morph is happening at all. That ignorance is
 * deliberate: a renderer that cannot see a boundary cannot draw one.
 */

#ifndef SUSTAIN_RENDER_H
#define SUSTAIN_RENDER_H

#include <stdint.h>
#include "world.h"

/* Paint one 320x240 RGB565 frame from the camera. */
void render_world(uint16_t *fb, const camera_t *cam, float t);

/* On-device timing breakdown, in microseconds, accumulated since the last
 * read. Guessing at where the frame goes has been wrong three times running;
 * this measures it on the target that actually matters. Cleared by the
 * reader. */
typedef struct {
    uint32_t sky_us;      /* building the azimuth sky cache      */
    uint32_t march_us;    /* the ray walk itself                 */
    uint32_t post_us;     /* motes + overlay                     */
    uint32_t frames;
} render_prof_t;

void render_prof_take(render_prof_t *out);

#endif
