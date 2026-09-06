/* The affine kernel: shared by the plane, the finale and the raster split. */

#ifndef PV_AFFINE_H
#define PV_AFFINE_H

#include <stdint.h>

enum { AFFINE_TEX_FLOOR = 0, AFFINE_TEX_GRID = 1 };

typedef struct {
    float x, y;          /* camera position on the plane, texels                */
    float angle;         /* heading, radians                                    */
    float height;        /* camera height, texels                               */
    int   horizon;       /* screen row of the horizon; very negative = rotozoom */
    float fog_near, fog_far;   /* depth in texels; fog_near <= 0 disables       */
} affine_cam_t;

void affine_texture_generate(int style);                     /* core 0, into the arena */
void affine_rows(const affine_cam_t *cam, uint32_t parity);   /* core 0, per frame      */
void affine_sky(const uint16_t *sky, uint32_t parity);        /* core 0, per frame      */
void affine_sky_dusk(uint16_t *sky, int horizon, int warm);   /* the standard gradient  */
void affine_line_p(uint32_t parity, uint16_t *px, int y);     /* core 1                 */
void qs_texmap_setup_interp0(void);                           /* on the drawing core    */

#endif
