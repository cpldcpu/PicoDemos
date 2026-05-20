/* Voxel landscape — column-based raycasting heightmap renderer.
 *
 * Original (dawn_final.s:1494-1545 generation, :517-575 render) uses a
 * 256×256 heightmap + 256×256 colormap + 65×256 shade table — ~144 KB. We
 * shrink to 128×128 (32 KB total) and compute the shade table inline.
 * Visually indistinguishable: the height field is just two overlapping
 * sine bumps and downsamples cleanly.
 */

#ifndef VOXEL_H
#define VOXEL_H

#include "dawn.h"

void voxel_init(void);
void voxel_render(int frame_count);

#endif
