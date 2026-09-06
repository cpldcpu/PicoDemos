/* Every kernel in the production, by name. */

#ifndef PV_SCENES_H
#define PV_SCENES_H

#include "beam.h"

extern const scene_t fx_title;
extern const scene_t fx_copper;
extern const scene_t fx_plasma;
extern const scene_t fx_kefrens;
extern const scene_t fx_twister;
extern const scene_t fx_tunnel;
extern const scene_t fx_plane;
extern const scene_t fx_split;
extern const scene_t fx_finale;
extern const scene_t fx_credits;
extern const scene_t fx_endcard;

/* Cross-kernel hooks used by the raster split (fx_split.c). */
void kefrens_line0(uint32_t f);
void plane_line(uint32_t f, uint16_t *px, int y);
void plane_setup1(void);

#endif
