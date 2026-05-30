/* Scene 2 — Star ignition (0:50–1:14, MODE_HIRES 320x240).
 *
 * The gathered dust ignites: three seamless granulation tiles are summed
 * at different scales/offsets (a cheap multi-octave noise), pushed through
 * a blackbody ramp, with limb darkening and a hot core. Over the scene we
 * zoom into the surface and the intensity climbs, then a full-frame
 * ignition flare lands on the 1:14 musical hit (end of scene).
 *
 * Perf: limb darkening uses an r²-based falloff (no per-pixel sqrt); the
 * three texture taps are cheap masked lookups.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "fx_common.h"
#include <stdint.h>

#define SCENE_LEN_MS  24000
#define W             VGA_HIRES_W
#define H             VGA_HIRES_H
#define CX            160.0f
#define CY            120.0f

static const uint16_t *tile0, *tile1, *tile2;

static inline int tap(const uint16_t *t, int u, int v)
{
    u &= 127; v &= 127;
    return rgb565_g8(t[v*128 + u]);     /* 0..248 */
}

static void star_init(void)
{
    tile0 = (const uint16_t *)asset_solar0_data;
    tile1 = (const uint16_t *)asset_solar1_data;
    tile2 = (const uint16_t *)asset_solar2_data;
}

static void star_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    int A = fx_scene_alpha(t_into, SCENE_LEN_MS, 2000, 600);
    float t = t_into * 0.001f;

    float zoom = 1.0f - 0.55f * (t_into / (float)SCENE_LEN_MS);
    float inten = 0.55f + 0.45f * (t_into / (float)SCENE_LEN_MS);
    float flare = 0.0f;
    if (t_into > SCENE_LEN_MS - 1800) flare = (t_into - (SCENE_LEN_MS - 1800)) / 1800.0f;

    float ox0 = t * 6.0f,  oy0 = t * 3.0f;
    float ox1 = -t * 9.0f, oy1 = t * 5.0f;
    float ox2 = t * 4.0f,  oy2 = -t * 7.0f;

    uint16_t *fb = vga_hires_back_buffer();
    for (int y = 0; y < H; y++) {
        float fyc = (y - CY);
        float fy  = fyc * zoom * 0.5f;
        for (int x = 0; x < W; x++) {
            float fxc = (x - CX);
            float fx  = fxc * zoom * 0.5f;

            int s = tap(tile0, (int)(fx        + ox0), (int)(fy        + oy0));
            s    += tap(tile1, (int)(fx*2.1f   + ox1), (int)(fy*2.1f   + oy1)) >> 1;
            s    += tap(tile2, (int)(fx*4.3f   + ox2), (int)(fy*4.3f   + oy2)) >> 2;
            int v = (s * 255) / 434;

            /* r²-based limb darkening (no sqrt): bright core, dim edges. */
            float r2 = fxc*fxc + fyc*fyc;
            float limb = 1.12f - r2 * 0.0000125f;
            if (limb < 0.15f) limb = 0.15f;
            v = (int)(v * limb * inten);

            int rr, gg, bb;
            fx_blackbody(v, &rr, &gg, &bb);
            if (flare > 0) {
                rr += (int)((255 - rr) * flare);
                gg += (int)((255 - gg) * flare);
                bb += (int)((255 - bb) * flare);
            }
            fb[y*W + x] = rgb565_pack((rr*A)>>8, (gg*A)>>8, (bb*A)>>8);
        }
    }
}

static void star_done(void) {}

const effect_t fx_star_real = {
    .name = "star", .mode = MODE_HIRES,
    .init = star_init, .frame = star_frame, .done = star_done,
};
