/* race.h — beam-raced full-VGA (640x480) rotozoom scanline generator.
 *
 * Generates one 640-wide RGB565 scanline at a time directly from the chrome
 * texture via the RP2350 interpolator in POP self-stepping mode — NO frame
 * buffer (a 640x480 truecolor framebuffer is 614 KB > 520 KB SRAM, so beam
 * racing is the only way to do full VGA on this chip). One pop_full per pixel
 * returns the texel offset and advances u,v.
 *
 * Beam-racing discipline (critical to hit the per-line deadline at 640x480):
 *  - the texture is copied to SRAM in qs_race_setup() (XIP-flash random reads
 *    would miss the cache and overrun the line);
 *  - the generator is placed in SRAM (__not_in_flash_func on device);
 *  - NO transcendentals on the hot path — the caller passes the pre-rotated
 *    affine basis (ca0,sa0) and an optional per-row flex table; core 1 only
 *    does a few mul/adds plus the POP loop.
 */

#ifndef QS_RACE_H
#define QS_RACE_H

#include <stdint.h>

typedef struct {
    float        ca0, sa0;   /* cos(ang)*zoom, sin(ang)*zoom (pre-flex)   */
    float        cu, cv;     /* texture-space centre                       */
    const float *flex;       /* optional per-row scale (>= H), or NULL=1.0 */
} qs_race_params;

/* Configure interp0 for the roto texture and copy it to SRAM (once). Call on
 * the core that will run qs_race_scanline (i.e. core 1 on device). */
void qs_race_setup(void);

/* Fill dst[0..W) for display row y of a WxH screen. */
void qs_race_scanline(uint16_t *dst, int y, int W, int H, const qs_race_params *p);

#endif
