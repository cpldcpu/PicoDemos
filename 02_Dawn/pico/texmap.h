/* Texture-mapped polygon rasterizer with inline environment map sampling.
 *
 * Original env map (dawn_final.s:917-959) is a 256×256 = 64 KB precomputed
 * texture. The 68k stores it because random texel access on a 14 MHz CPU
 * with slow div is otherwise unaffordable. On RP2040 the env map formula
 * is trivial (one mul, one shift, optional XOR for the checker variant),
 * so we sample it inline in the inner loop and save the 64 KB.
 *
 * Original rasterizer (dawn_final.s:1163-1351): scanline edge setup +
 * perspective-correct inner loop. We follow the same span-buffer pattern
 * (left/right X plus UV at each scanline) but write straight to the chunky
 * buffer.
 */

#ifndef TEXMAP_H
#define TEXMAP_H

#include "dawn.h"
#include "vector3d.h"

typedef enum {
    ENV_SPHERE,    /* radial gradient — main torus scenes */
    ENV_CHECKER,   /* checkered sphere — TORUS_3 / TORUS_BLUR scenes */
} env_mode_t;

extern env_mode_t env_mode;

/* When false: writes texel directly (REPLACE).
 * When true:  writes only if texel > existing (MAX blend, used for blur
 *             accumulation — see web_port textureMap.ts:242-248). */
extern bool tex_blur_mode;

/* Light offset (U, V) added to each env-map sample. Animated by the
 * sequencer for TORUS_2's swirling-highlight effect. */
extern int tex_light_u_offset;
extern int tex_light_v_offset;

void texmap_draw_polygon(const screen_pt_t *pts,
                         const uint8_t *us,
                         const uint8_t *vs,
                         int n);

#endif
