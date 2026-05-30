/* Scene 5 — Gravitational lensing, the climax (2:43–3:50, MODE_HIRES).
 *
 * The whole scene is a flash-LUT gather: lens_lut[] (baked offline from the
 * Schwarzschild null geodesics) maps each screen pixel to either the event-
 * horizon shadow or a (u,v) into the equirectangular star panorama. The
 * Einstein ring and the black shadow are pure physics.
 *
 * Motion (so it lives instead of sitting still):
 *   - accelerating FALL-IN zoom: we sample an ever-smaller central region of
 *     the LUT, so the shadow grows and the lensed sky sweeps outward — we are
 *     plunging toward the hole (sets up the event-horizon scene next),
 *   - a gentle ROLL: the lensed galaxy arcs spiral around the shadow,
 *   - a smooth sub-texel YAW with bilinear panorama sampling so the
 *     background drifts fluidly (no more 1-texel jerk).
 * A thin, soft pale-blue PHOTON RING marks the shadow edge (the lensed light),
 * replacing the old hard orange circle.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "fx_common.h"
#include "lens_lut.h"
#include <stdint.h>
#include <math.h>

#define SCENE_LEN_MS  67000
#define W             LENS_LUT_W
#define H             LENS_LUT_H
#define PW            LENS_PANO_W
#define PH            LENS_PANO_H

static const uint16_t *pano;

static void lensing_init(void) { pano = (const uint16_t *)asset_star_pano_data; }

static inline int is_shadow(int lx, int ly)
{
    if (lx < 0) lx = 0; else if (lx >= W) lx = W-1;
    if (ly < 0) ly = 0; else if (ly >= H) ly = H-1;
    return lens_lut[ly*W + lx] == LENS_SHADOW;
}

static void lensing_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    int A = fx_scene_alpha(t_into, SCENE_LEN_MS, 3000, 3000);
    float app = t_into / (float)SCENE_LEN_MS;
    float zoom = 1.0f + 1.30f * app * app;            /* accelerating fall-in */
    float inv_zoom = 1.0f / zoom;
    float rot  = t_into * 0.00010f;                   /* gentle roll */
    float cr = cosf(rot) * inv_zoom, sn = sinf(rot) * inv_zoom;
    float yawf = t_into * (PW / 26000.0f);            /* smooth sub-texel yaw */
    float breath = 1.0f + 0.10f * sinf(t_into * 0.0011f);
    int gain = (int)(A * breath);
    float cxf = W * 0.5f, cyf = H * 0.5f;

    uint16_t *fb = vga_hires_back_buffer();
    for (int y = 0; y < H; y++) {
        float dy = y - cyf;
        uint16_t *out = &fb[y*W];
        for (int x = 0; x < W; x++) {
            float dx = x - cxf;
            /* rolled + zoomed sample coordinate into the LUT */
            int lx = (int)(cxf + dx*cr - dy*sn);
            int ly = (int)(cyf + dx*sn + dy*cr);
            /* Outside the LUT → deep-space black (clean vignette, no stretched
             * edge bands when the roll/zoom samples past the border). */
            if ((unsigned)lx >= W || (unsigned)ly >= H) { out[x] = 0; continue; }
            uint16_t e = lens_lut[ly*W + lx];

            if (e == LENS_SHADOW) { out[x] = 0; continue; }

            /* bilinear-in-u panorama sample with the fractional yaw */
            float uf = (float)(e >> 7) + yawf;
            int ui = (int)uf; float fr = uf - ui;
            ui &= (PW - 1); int u1 = (ui + 1) & (PW - 1);
            int v = e & 0x7F; if (v >= PH) v = PH - 1;
            const uint16_t *prow = &pano[v*PW];
            uint16_t c0 = prow[ui], c1 = prow[u1];
            int r = rgb565_r8(c0) + (int)((rgb565_r8(c1) - rgb565_r8(c0)) * fr);
            int g = rgb565_g8(c0) + (int)((rgb565_g8(c1) - rgb565_g8(c0)) * fr);
            int b = rgb565_b8(c0) + (int)((rgb565_b8(c1) - rgb565_b8(c0)) * fr);

            /* soft pale-blue photon ring: brighten just outside the shadow */
            int near = is_shadow(lx-2, ly) + is_shadow(lx+2, ly)
                     + is_shadow(lx, ly-2) + is_shadow(lx, ly+2);
            if (near) {
                int gl = near * 40;
                r += (gl*7)>>3; g += (gl*8)>>3; b += gl;
            }

            r = (fx_clampi(r,0,255) * gain) >> 8;
            g = (fx_clampi(g,0,255) * gain) >> 8;
            b = (fx_clampi(b,0,255) * gain) >> 8;
            out[x] = rgb565_pack(r, g, b);
        }
    }
}

static void lensing_done(void) {}

const effect_t fx_lensing_real = {
    .name = "lensing", .mode = MODE_HIRES,
    .init = lensing_init, .frame = lensing_frame, .done = lensing_done,
};
