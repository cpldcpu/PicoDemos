/* Mip chains for the material textures. See mip.c for why this replaced the
 * fade-to-mean LOD (short version: fading to a mean colour removed the
 * material, not just the aliasing). */
#ifndef SUSTAIN_MIP_H
#define SUSTAIN_MIP_H

#include <stdint.h>

#define MIP_LEVELS 3          /* 64x64, 32x32, 16x16 below a 128x128 level 0 */

enum { MIP_SURFACE_COLD = 0, MIP_SURFACE_HOT, MIP_WALL_COLD, MIP_WALL_HOT,
       MIP_COUNT };

typedef struct {
    const uint16_t *l0;             /* level 0, straight from flash */
    int             w0;
    const uint16_t *l[MIP_LEVELS];  /* generated levels, in SRAM */
    int             w[MIP_LEVELS];
} mip_t;

void mip_build(int slot, const uint8_t *raw, int w);
void mip_build_all(const uint8_t *surface_cold, const uint8_t *surface_hot,
                   const uint8_t *wall_cold, const uint8_t *wall_hot, int w);

/* texels_per_pixel is the measured footprint; the level is chosen from it and
 * blended with the next, so the LOD transition cannot band. */
void mip_sample(int slot, float u_tex, float v_tex, float texels_per_pixel,
                int *r, int *g, int *b);

#endif
