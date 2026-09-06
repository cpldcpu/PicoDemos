/* SUSTAIN — title, credits and the final collapse.
 *
 * Everything here composites over the finished frame and fades continuously,
 * so none of it constitutes a cut. Nothing ever appears or disappears in one
 * frame; every element has a ramp in and a ramp out, and the world carries on
 * moving underneath the whole time.
 *
 * DEVIATION FROM THE PLAN, recorded honestly. PLANNING.md §8 wanted the
 * wordmark PRESSED INTO the sea as displacement, on the argument that a title
 * card would be a cut. Built that way it was unreadable: a low camera over a
 * heightfield sees an embossed word almost edge-on, and foreshortening
 * destroys the letterforms. So the wordmark is composited instead — but the
 * reasoning behind the original rule is preserved, because it fades up over
 * live moving water and fades away again without the demo ever cutting to it.
 * The rule was "no boundaries", not "no compositing".
 *
 * The final collapse is the one sanctioned black frame in the production, and
 * cut_detect.py has to be told where it starts (--collapse-ms) so it does not
 * read the ending as a rule-2 violation.
 */

#include "overlay.h"
#include "rgb565.h"
#include "font8x8.h"
#include "assets.h"

#include <math.h>
#include <string.h>

#define W 320
#define H 240

/* ---- helpers ------------------------------------------------------------ */

static inline void blend_add(uint16_t *fb, int x, int y, int r, int g, int b)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    const uint16_t p = fb[y * W + x];
    fb[y * W + x] = rgb565_pack(rgb565_r8(p) + r,
                                rgb565_g8(p) + g,
                                rgb565_b8(p) + b);
}

/* Smooth 0..1 ramp: 0 before `a`, 1 between `b` and `c`, 0 after `d`. All the
 * timing in this file goes through here, which is what guarantees nothing can
 * pop on or off. */
static float window(float t, float a, float b, float c, float d)
{
    if (t <= a || t >= d) return 0.0f;
    float u;
    if (t < b)      u = (t - a) / (b - a);
    else if (t > c) u = (d - t) / (d - c);
    else            return 1.0f;
    return u * u * (3.0f - 2.0f * u);
}

static void draw_text(uint16_t *fb, const char *s, int cx, int y,
                      float alpha, int scale, int rr, int gg, int bb)
{
    if (alpha <= 0.003f) return;
    const int len = (int)strlen(s);
    int x = cx - (len * FONT8X8_W * scale) / 2;

    const int ar = (int)(rr * alpha), ag = (int)(gg * alpha), ab = (int)(bb * alpha);

    for (int i = 0; i < len; i++, x += FONT8X8_W * scale) {
        const uint8_t *gl = font8x8_glyph(s[i]);
        for (int row = 0; row < FONT8X8_H; row++) {
            const uint8_t bits = gl[row];
            for (int col = 0; col < FONT8X8_W; col++) {
                if (!(bits & (0x80 >> col))) continue;
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        blend_add(fb, x + col * scale + sx,
                                      y + row * scale + sy, ar, ag, ab);
            }
        }
    }
}

/* Black-keyed additive blit of a packed RGB565 asset. The logos were authored
 * on pure black precisely so this works without an alpha channel. */
static void draw_logo(uint16_t *fb, const uint8_t *raw, int lw, int lh,
                      int cx, int cy, float alpha, float tint_warm)
{
    if (alpha <= 0.003f) return;
    const uint16_t *src = (const uint16_t *)raw;
    const int x0 = cx - lw / 2, y0 = cy - lh / 2;

    for (int y = 0; y < lh; y++) {
        const int dy = y0 + y;
        if (dy < 0 || dy >= H) continue;
        for (int x = 0; x < lw; x++) {
            const uint16_t p = src[y * lw + x];
            if (!p) continue;                       /* pure black = transparent */
            int r = rgb565_r8(p), g = rgb565_g8(p), b = rgb565_b8(p);
            if (tint_warm > 0.001f) {
                r = (int)(r * (1.0f + 0.35f * tint_warm));
                g = (int)(g * (1.0f - 0.15f * tint_warm));
                b = (int)(b * (1.0f - 0.45f * tint_warm));
            }
            blend_add(fb, x0 + x, dy,
                      (int)(r * alpha), (int)(g * alpha), (int)(b * alpha));
        }
    }
}

/* ---- the schedule ------------------------------------------------------- */

/* Credits, one line at a time over the returning sea. Times in seconds. */
static const struct { float t0, t1; const char *a; const char *b; } CREDITS[] = {
    { 250.0f, 262.0f, "SUSTAIN",              "A LATENT PRODUCTION"      },
    { 259.0f, 271.0f, "CODE AND DIRECTION",   "OVERSCAN"                 },
    { 268.0f, 280.0f, "MUSIC",                "SUNO"                     },
    { 274.0f, 285.0f, "ART",                  "GPT IMAGE 2"              },
    { 279.0f, 288.0f, "CRITIC",               "AZURE"                    },
};
#define N_CREDITS ((int)(sizeof(CREDITS) / sizeof(CREDITS[0])))

void overlay_draw(uint16_t *fb, float t)
{
    /* ---- title: the wordmark over the opening water ---- */
    const float title = window(t, 5.0f, 8.5f, 13.0f, 17.5f);
    if (title > 0.003f)
        draw_logo(fb, asset_sustain_logo_data,
                  ASSET_SUSTAIN_LOGO_W, ASSET_SUSTAIN_LOGO_H,
                  W / 2, 96, title * 0.85f, 0.0f);

    /* ---- credits ---- */
    for (int i = 0; i < N_CREDITS; i++) {
        const float a = window(t, CREDITS[i].t0, CREDITS[i].t0 + 2.2f,
                                  CREDITS[i].t1 - 2.6f, CREDITS[i].t1);
        if (a <= 0.003f) continue;
        const int y = 78 + i * 26;
        draw_text(fb, CREDITS[i].a, W / 2, y,      a * 0.55f, 1, 150, 190, 230);
        draw_text(fb, CREDITS[i].b, W / 2, y + 10, a * 0.95f, 1, 225, 235, 255);
    }

    /* ---- group mark ---- */
    const float mark = window(t, 281.0f, 283.5f, 286.0f, 288.5f);
    if (mark > 0.003f)
        draw_logo(fb, asset_latent_logo_data,
                  ASSET_LATENT_LOGO_W, ASSET_LATENT_LOGO_H,
                  W / 2, 150, mark * 0.9f, 0.0f);

    /* ---- the collapse ----
     *
     * A CRT losing its deflection: the picture squeezes toward the scan centre
     * and the remaining band brightens as the same energy is packed into fewer
     * lines. Done in place, blacking outward from the edges rather than
     * rescaling, so it needs no second framebuffer. This is the demo's only
     * black frame, and cut_detect.py must be told where it begins. */
    const float c = window(t, 286.0f, 289.0f, 1.0e9f, 1.0e9f);
    if (c > 0.003f) {
        const int half = (int)((H / 2) * (1.0f - c));
        const int mid  = H / 2;
        const float boost = 1.0f + 2.6f * c;

        for (int y = 0; y < H; y++) {
            const int d = y - mid;
            if (d < -half || d > half) {
                memset(&fb[y * W], 0, W * sizeof(uint16_t));
            } else if (boost > 1.001f) {
                for (int x = 0; x < W; x++) {
                    const uint16_t p = fb[y * W + x];
                    fb[y * W + x] = rgb565_pack((int)(rgb565_r8(p) * boost),
                                                (int)(rgb565_g8(p) * boost),
                                                (int)(rgb565_b8(p) * boost));
                }
            }
        }
    }
}
