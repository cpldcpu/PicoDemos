/* The one sky. See sky_common.h for why it is shared rather than per-family. */

#include "sky_common.h"
#include "../tex.h"
#include "assets.h"

void sustain_sky(float grade, float u, float v, int *r, int *g, int *b)
{
    /* The panorama puts its horizon glow at the image's vertical CENTRE, so
     * screen-horizon (v=0) maps to row H/2 and the top of frame (v=1) to row 0.
     * Mapping v=0 to the bottom row instead painted the glow halfway up the sky
     * and left the true horizon black — which drove the tunnel section under
     * cut_detect.py's rule-2 black-frame threshold. */
    float row = (1.0f - v) * (float)(ASSET_SKY_PANO_H / 2);
    if (row < 0.0f) row = 0.0f;

    tex_rgb_bilin(asset_sky_pano_data,
                  ASSET_SKY_PANO_W - 1, ASSET_SKY_PANO_H - 1,
                  ASSET_SKY_PANO_W,
                  u * (float)ASSET_SKY_PANO_W, row, r, g, b);

    if (grade > 0.001f) {
        const int wr = (int)(*r * 1.35f + 40.0f * grade);
        const int wg = (int)(*g * 0.80f);
        const int wb = (int)(*b * 0.55f);
        *r += (int)((wr - *r) * grade);
        *g += (int)((wg - *g) * grade);
        *b += (int)((wb - *b) * grade);
    }
    if (*r > 255) *r = 255;
    if (*g > 255) *g = 255;
    if (*b > 255) *b = 255;
}
