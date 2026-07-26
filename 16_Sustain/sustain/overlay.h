/* Title, credits and final collapse. See overlay.c for why compositing these
 * does not violate the no-cut rule. */
#ifndef SUSTAIN_OVERLAY_H
#define SUSTAIN_OVERLAY_H
#include <stdint.h>
void overlay_draw(uint16_t *fb, float t);   /* t in seconds */
#endif
