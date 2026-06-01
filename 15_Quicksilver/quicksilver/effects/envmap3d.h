/* envmap3d.h — Gouraud-normal environment-mapped (matcap) chrome renderer.
 *
 * QUICKSILVER's centerpiece: high-poly solids reflecting a chrome sphere-map.
 * Per vertex we rotate the normal into view space and map its (x,y) to the
 * matcap UV; those UVs are interpolated across each triangle in screen space
 * and, per pixel, the RP2350 interpolator turns (u,v) into the matcap texel
 * offset (+ bilinear) in one read — the same affine-address trick as the
 * rotozoom/Mode-7 scenes. Builds identically on host (emulator) and device.
 */

#ifndef QS_ENVMAP3D_H
#define QS_ENVMAP3D_H

#include <stdint.h>
#include "../assets/_packed/meshes.h"   /* qs_mvec + the baked objects */

typedef struct {
    const qs_mvec  *v;     /* vertex positions (unit-ish)  */
    const qs_mvec  *n;     /* per-vertex normals           */
    const uint16_t *tri;   /* nt*3 indices                 */
    int nv, nt;
} qs_mesh;

typedef struct {
    float yaw, pitch, roll;   /* object orientation (radians)         */
    float scale;              /* uniform scale                        */
    float ox, oy, oz;         /* translate; oz = distance from camera */
    float focal;              /* ~260 for a natural 320-wide FOV      */
    const uint8_t *env;       /* matcap texture, RGB565 PIO-native    */
    int   envW, envH;         /* power-of-two dims (256)              */
    int   log2bpp, log2w, log2h;
    int   bilinear;           /* 1 = bilinear matcap (smooth, 4 taps), 0 = point */
} qs_env_params;

void qs_env_default(qs_env_params *p);

/* Render `m` into vga_hires_back_buffer() with matcap chrome shading. Does NOT
 * clear the framebuffer (draw a backdrop first). Uses interp0. */
void qs_envmap_render(const qs_mesh *m, const qs_env_params *p);

#endif
