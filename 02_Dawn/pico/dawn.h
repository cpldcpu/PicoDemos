/* Dawn — RP2040 port
 *
 * Top-level shared definitions for the Dawn 4K intro port.
 * Reference: ../web_port/web/src/ (TypeScript transliteration) and
 *            ../web_port/original_source/dawn_final.s (68020 assembly).
 *
 * Engine native resolution stays at 160x128 chunky (6-bit palette indices),
 * matching the original. Display backend (VGA via pico_scanvideo) doubles
 * horizontally to 320 and crops 4 rows top/bottom for 320x240@60.
 */

#ifndef DAWN_H
#define DAWN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SCREEN_W   160
#define SCREEN_H   128
#define SCREEN_BYTES (SCREEN_W * SCREEN_H)

/* Display crop: render rows VPORT_TOP..VPORT_TOP+VPORT_H-1 of the chunky
 * buffer. 120 visible rows × 2 = 240 VGA scanlines. */
#define VPORT_TOP  4
#define VPORT_H    120

/* Palette is 64 entries (6 bits per pixel index). */
#define PAL_SIZE   64

/* Torus geometry (CIRC_SEG outer ring × ROUN_SEG round segments). Matches
 * the original constants (circseg/rounseg in dawn_final.s). */
#define CIRC_SEG       32
#define ROUN_SEG       8
#define TORUS_VERTS    (CIRC_SEG * ROUN_SEG)
#define TORUS_POLYS    (CIRC_SEG * ROUN_SEG)

/* Voxel maps at the original 256×256 (128 KB total across heightmap +
 * colormap). The 128×128 version we briefly used scaled `dsq` by 1/4
 * which cut feature density to 1/4 of the asm AND let the camera's
 * 17..241 integer-X traversal wrap the map mid-frame. 256 fits the RP2040
 * SRAM (≈190 KB total used) and matches the asm exactly. Address wraps
 * with VOXEL_MASK so the ray walk doesn't need a clip per step. */
#define VOXEL_DIM   256
#define VOXEL_MASK  (VOXEL_DIM - 1)

/* Math */
#define SIN_TAB_LEN  1024
#define SIN_TAB_MASK (SIN_TAB_LEN - 1)

#endif
