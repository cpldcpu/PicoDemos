/* Rotozoom + credits + fade — scene 8 (3:40 – 4:06, MODE_320).
 *
 * Three layered elements:
 *
 *   1. Slow rotozoom of the endcard_bg image: per-pixel affine
 *      transform sampled from the 320×240 packed bitmap (which is
 *      pre-tiled by texture-mod so it wraps cleanly). Both the
 *      rotation angle and the scale "breathe" over the scene.
 *
 *   2. Static logo silhouette using `asset_logo_mask` stamped 2× scale
 *      centred, painted with the same warm-white the title text uses,
 *      so it ties the closing back to the opening.
 *
 *   3. Scrolling credits along the bottom of the screen — sine-less,
 *      straight horizontal scroll, slow. Reuses font8x8.h.
 *
 *   4. Fade to black at the end so the demo ends on a clean cut.
 *
 * Palette layout (endcard_bg packed with reserved=32):
 *
 *   0..223    backdrop colours (per-frame fade-scalable)
 *   224..247  logo bumpmap shading ramp (24 iridescent shades)
 *   248       text fill
 *   249       text shadow
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "../font8x8.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define FB_W              VGA_320_W
#define FB_H              VGA_320_H

/* Scene length tracks the timeline allocation (220 s .. 275 s = 55 s).
 * The QOA song ends at ~246 s so the last 29 s play with the audio at
 * silence — gives the credits scroller time to complete a full pass at
 * a readable cadence. */
#define SCENE_LEN_MS      55000
#define FADE_IN_MS         1500
#define FADE_OUT_MS        4000
#define FADE_OUT_AT       (SCENE_LEN_MS - FADE_OUT_MS)

#define TEX_W              ASSET_ENDCARD_BG_W       /* 320 */
#define TEX_H              ASSET_ENDCARD_BG_H       /* 240 */

#define TEXT_SLOT         248
#define TEXT_SHADOW_SLOT  249

/* Logo: rendered as a 2D bumpmap over the rotozoomed backdrop. The
 * heightmap is the logo silhouette mask, multi-pass-blurred at init
 * into a smooth height field. Per-frame shading dots the height
 * gradient against a roving light direction so the logo appears
 * embossed and lit, with the light sweeping over its bumps. */
#define LOGO_W             ASSET_LOGO_MASK_W                  /* 160 */
#define LOGO_H             ASSET_LOGO_MASK_H                  /* 120 */
#define LOGO_BASE_X       ((VGA_320_W - LOGO_W) / 2)           /* 80 */
#define LOGO_BASE_Y       ((VGA_320_H - LOGO_H) / 2)           /* 60 */

#define BUMP_BASE          224
#define BUMP_COUNT          24

#define CREDITS_SCALE      1
#define CREDITS_CHAR_W    (FONT8X8_W * CREDITS_SCALE)
#define CREDITS_GAP        1
#define CREDITS_STRIDE    (CREDITS_CHAR_W + CREDITS_GAP)
#define CREDITS_Y         220
/* Continuous-time scroll cadence — derived from t_into directly rather
 * than ticked per-frame, so it stays smooth regardless of frame jitter
 * (vblank misses, varying decode cost, etc.). 80 px/s is brisk-but-
 * readable; with the credits text below that works out to ~55 s for a
 * full single pass, matching the extended timeline allocation. */
#define CREDITS_SCROLL_PX_PER_SEC 80

/* Forty leading spaces (= ~one screen-width at scale 1) so the
 * scroller enters cleanly from the right edge instead of being
 * mid-text on the first frame. */
static const char CREDITS[] =
    "                                        "
    "SLOP - DESIGNED AND CODED BY CLAUDE OPUS 4.7 WITH SOME NUDGING BY AZURE - "
    "MUSIC: SUNO 4.5 - IMAGES: NANO BANANA PRO AND GPT IMAGE 2 - "
    "THE NAME OWNS THE JOKE: A 2026 DEMO WHERE THE CODE IS AI TOO - "
    "AZURE POINTED AT WHAT NEEDED FIXING - CLAUDE FIXED IT - "
    "DEBUGGED ENTIRELY VIA SDL SCREENSHOTS AND PRINTF - "
    "RUNNING ON A RASPBERRY PI PICO 2 - 520KB SRAM AND VIBES - "
    "GREETZ TO THE HAND-CODERS - THIS DEMO STANDS ON YOUR SHOULDERS - "
    "THANKS FOR WATCHING.                                        ";

/* Sin/cos LUT for the rotozoom matrix (Q15). Cheap, integer-only. */
static int16_t sin_q15[256];
static inline int16_t s256(int idx) { return sin_q15[idx & 0xFF]; }
static inline int16_t c256(int idx) { return sin_q15[(idx + 64) & 0xFF]; }

static uint8_t bg_r[224], bg_g[224], bg_b[224];

/* Shared scratch — same physical bytes as title/greetz bg_cache. */
#include "../scene_scratch.h"
#define bg_cache (g_scratch.bg_cache)

/* Logo bumpmap heightmap, built once at init. 160×120 = 19,200 B. */
static uint8_t bump_h[LOGO_W * LOGO_H];

static void build_sin_lut(void)
{
    for (int i = 0; i < 256; i++) {
        sin_q15[i] = (int16_t)(sinf(i * (2.0f * 3.14159265f / 256.0f)) * 32767.0f);
    }
}

/* --- palette ------------------------------------------------------ */

static void apply_bg_fade(int alpha_q8)
{
    if (alpha_q8 < 0) alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;
    for (int i = 0; i < 224; i++) {
        uint8_t r = (uint8_t)((bg_r[i] * alpha_q8) >> 8);
        uint8_t g = (uint8_t)((bg_g[i] * alpha_q8) >> 8);
        uint8_t b = (uint8_t)((bg_b[i] * alpha_q8) >> 8);
        vga_320_palette_set(i, r, g, b);
    }
}

static void apply_text_palette(int alpha_q8)
{
    if (alpha_q8 < 0) alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;
    vga_320_palette_set(TEXT_SLOT,
        (uint8_t)((255 * alpha_q8) >> 8),
        (uint8_t)((240 * alpha_q8) >> 8),
        (uint8_t)((215 * alpha_q8) >> 8));
    vga_320_palette_set(TEXT_SHADOW_SLOT,
        (uint8_t)((10  * alpha_q8) >> 8),
        (uint8_t)((4   * alpha_q8) >> 8),
        (uint8_t)((22  * alpha_q8) >> 8));
}

/* 24-shade linear ramp for the bumpmap. Earlier this was a 3-anchor
 * iridescent rainbow (magenta → orange → teal) that visually fought
 * with the rotozoomed bokeh — the bokeh is muted dark-blue with warm
 * magenta accents, so a fully-saturated rainbow logo on top read as a
 * foreign decal. Now: a single dark-cool-shadow → warm-cream-highlight
 * ramp that picks up tones already present in the bokeh, so the
 * embossed logo reads as part of the same scene. */
static void apply_bump_palette(int alpha_q8)
{
    static const uint8_t lo[3] = {  20,  28,  44 };   /* deep slate shadow */
    static const uint8_t hi[3] = { 235, 215, 180 };   /* warm bone highlight */
    if (alpha_q8 < 0)   alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;
    for (int i = 0; i < BUMP_COUNT; i++) {
        int r = lo[0] + (hi[0] - lo[0]) * i / (BUMP_COUNT - 1);
        int g = lo[1] + (hi[1] - lo[1]) * i / (BUMP_COUNT - 1);
        int b = lo[2] + (hi[2] - lo[2]) * i / (BUMP_COUNT - 1);
        r = (r * alpha_q8) >> 8;
        g = (g * alpha_q8) >> 8;
        b = (b * alpha_q8) >> 8;
        vga_320_palette_set(BUMP_BASE + i, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
}

/* --- rotozoom ------------------------------------------------------ */

/* Sample the cached endcard_bg with an affine transform centred on the
 * screen middle. Source coords wrap mod 320/240 so the texture tiles
 * — looks like the image swimming behind the closing logo.
 *
 * Previous attempts:
 *   - Modulo per pixel: ~30 cy on M33's software divide. Too slow.
 *   - Big bias + while-loop subtract: 4–5 iterations per pixel, also slow.
 *   - 256×128 power-of-2 crop with AND wrap: fast but the crop didn't
 *     tile cleanly across its boundary (the bokeh image has features
 *     that need the full 320×240 view to wrap continuously).
 *
 * Now: modulo ONCE per row (240 mods/frame, negligible) to bring the
 * source coords into the canonical range, then per-pixel compare-and-
 * subtract for the wrap (at most one iteration since |dxx|, |dxy| are
 * small relative to TEX_W*256 / TEX_H*256). */
static void draw_rotozoom(uint8_t *fb, uint32_t t_ms)
{
    /* The previous version computed `ang_idx = (t_ms / 60) & 0xFF` and
     * sampled the sin LUT with that integer index — which only
     * advanced every 60 ms (≈ 3.6 frames at 60 Hz). Each advance was a
     * visible JUMP in the rotation/scale, producing a stuttery feel
     * that LOOKED like a frame-rate problem but was really time
     * quantisation. Same issue with the scale's `t_ms / 40` step.
     *
     * Fix: compute angle and scale in float using sinf/cosf directly.
     * Smooth sub-degree advance every frame. M33 FPU makes this cheap
     * — three trig calls per frame is trivial. */
    float t_sec  = t_ms * 0.001f;
    float angle  = t_sec * (2.0f * 3.14159265f / 15.36f);   /* one rev / 15.36 s */
    float bump   = sinf(t_sec * (2.0f * 3.14159265f / 10.24f));
    /* Scale breathes 0.625× ↔ 1.375× — matches the original
     * (s256 * 96) >> 15 amplitude of ±96 in Q8 = ±0.375 in floats. */
    float scale  = 1.0f + bump * 0.375f;
    int   cos_q15  = (int)(cosf(angle) * 32767.0f);
    int   sin_q15v = (int)(sinf(angle) * 32767.0f);
    int   scale_q8 = (int)(scale * 256.0f);
    int dxx = (scale_q8 * cos_q15) >> 15;
    int dxy = (scale_q8 * sin_q15v) >> 15;
    int dyx = -dxy;
    int dyy = dxx;

    int cx = FB_W / 2, cy = FB_H / 2;
    const int TSX = TEX_W * 256;
    const int TSY = TEX_H * 256;
    for (int y = 0; y < FB_H; y++) {
        int row_src_x = (cx << 8) - dxx * cx - dyx * cy + dyx * y;
        int row_src_y = (cy << 8) - dxy * cx - dyy * cy + dyy * y;
        row_src_x = ((row_src_x % TSX) + TSX) % TSX;
        row_src_y = ((row_src_y % TSY) + TSY) % TSY;

        uint8_t *fb_row = fb + y * FB_W;
        for (int x = 0; x < FB_W; x++) {
            fb_row[x] = bg_cache[(row_src_y >> 8) * TEX_W + (row_src_x >> 8)];
            row_src_x += dxx;
            if      (row_src_x >= TSX) row_src_x -= TSX;
            else if (row_src_x <  0)   row_src_x += TSX;
            row_src_y += dxy;
            if      (row_src_y >= TSY) row_src_y -= TSY;
            else if (row_src_y <  0)   row_src_y += TSY;
        }
    }
}

/* --- logo bumpmap -------------------------------------------------- */

static inline int mask_bit(int x, int y)
{
    if ((unsigned)x >= ASSET_LOGO_MASK_W) return 0;
    if ((unsigned)y >= ASSET_LOGO_MASK_H) return 0;
    int byte_idx = y * (ASSET_LOGO_MASK_W / 8) + (x >> 3);
    return (asset_logo_mask_data[byte_idx] >> (7 - (x & 7))) & 1;
}

/* Build the bumpmap heightmap by blurring the 1-bit logo silhouette
 * several times. Each pixel ends up holding "elevation" from 0 (well
 * outside the logo) to 255 (deep inside). Smooth gradients between
 * these are what the per-frame shader dots against the light. */
static void build_bump(void)
{
    /* Start: 255 inside mask, 0 outside. */
    for (int y = 0; y < LOGO_H; y++) {
        for (int x = 0; x < LOGO_W; x++) {
            bump_h[y * LOGO_W + x] = mask_bit(x, y) ? 255 : 0;
        }
    }
    /* Multi-pass 3×3 box blur — four passes gives a Gaussian-ish hump
     * with usable gradients at the silhouette edges. */
    static uint8_t tmp[LOGO_W * LOGO_H];
    for (int pass = 0; pass < 4; pass++) {
        memcpy(tmp, bump_h, sizeof tmp);
        for (int y = 1; y < LOGO_H - 1; y++) {
            for (int x = 1; x < LOGO_W - 1; x++) {
                int idx = y * LOGO_W + x;
                int sum = tmp[idx - LOGO_W - 1] + tmp[idx - LOGO_W] + tmp[idx - LOGO_W + 1]
                        + tmp[idx          - 1] + tmp[idx]          + tmp[idx          + 1]
                        + tmp[idx + LOGO_W - 1] + tmp[idx + LOGO_W] + tmp[idx + LOGO_W + 1];
                bump_h[idx] = (uint8_t)(sum / 9);
            }
        }
    }
}

/* Draw the logo as a 2D bumpmap over the rotozoomed framebuffer. The
 * light direction orbits slowly so the bumps "catch the light" as it
 * sweeps. Only pixels inside the silhouette's reach get touched —
 * outside the soft edge the rotozoom shows through unchanged. */
static void draw_bump_logo(uint8_t *fb, uint32_t t_ms)
{
    float t = t_ms * 0.001f;
    /* Light direction as Q8 vector (each component in [-256, 256]).
     * Orbiting through angle a; a vertical bias keeps the highlight
     * near the top so the logo feels like it has a sky-direction up. */
    float a = t * 0.6f;
    int   lx = (int)(cosf(a) * 220.0f);
    int   ly = (int)(sinf(a) * 220.0f - 64.0f);
    /* Ambient floor so the logo silhouette is always slightly visible. */
    const int ambient = 8;

    for (int sy = 1; sy < LOGO_H - 1; sy++) {
        int dy = LOGO_BASE_Y + sy;
        if ((unsigned)dy >= FB_H) continue;
        uint8_t *row = fb + dy * FB_W;
        const uint8_t *hr   = bump_h + sy * LOGO_W;
        const uint8_t *hr_n = hr - LOGO_W;
        const uint8_t *hr_s = hr + LOGO_W;
        for (int sx = 1; sx < LOGO_W - 1; sx++) {
            int h = hr[sx];
            if (h <= 8) continue;                  /* outside reach */
            int dx = LOGO_BASE_X + sx;
            if ((unsigned)dx >= FB_W) continue;

            int gx = hr  [sx + 1] - hr  [sx - 1];  /* ~[-255, 255] */
            int gy = hr_s[sx    ] - hr_n[sx    ];

            /* Dot with light (Q8 × ~Q0) — divide back for sane range. */
            int dot = (gx * lx + gy * ly) >> 10;
            /* Add a contribution from height itself so the logo "body"
             * lifts the shade above pure ambient even where the surface
             * is locally flat. */
            int shade = ambient + (h >> 5) + dot;
            if (shade < 0)             shade = 0;
            if (shade >= BUMP_COUNT)   shade = BUMP_COUNT - 1;
            row[dx] = (uint8_t)(BUMP_BASE + shade);
        }
    }
}

/* --- credits scroller --------------------------------------------- */

#define CREDITS_LEN          ((int)(sizeof CREDITS - 1))
#define CREDITS_PIXEL_WIDTH  (CREDITS_LEN * CREDITS_STRIDE)

static void draw_credits(uint8_t *fb, uint32_t scroll_offset_px)
{
    for (int x = 0; x < FB_W; x++) {
        uint32_t world_x  = scroll_offset_px + (uint32_t)x;
        int char_idx      = (int)((world_x / CREDITS_STRIDE) % CREDITS_LEN);
        int in_stride     = world_x % CREDITS_STRIDE;
        if (in_stride >= CREDITS_CHAR_W) continue;
        int glyph_col     = in_stride / CREDITS_SCALE;

        const uint8_t *g = font8x8_glyph(CREDITS[char_idx]);
        for (int row = 0; row < 8; row++) {
            int bit = (g[row] >> (7 - glyph_col)) & 1;
            if (!bit) continue;
            int y = CREDITS_Y + row * CREDITS_SCALE;
            if ((unsigned)y >= FB_H) continue;
            /* drop shadow */
            int ys = y + 1, xs = x + 1;
            if ((unsigned)ys < FB_H && (unsigned)xs < FB_W)
                fb[ys * FB_W + xs] = TEXT_SHADOW_SLOT;
            fb[y * FB_W + x] = TEXT_SLOT;
        }
    }
}

/* --- effect lifecycle --------------------------------------------- */

static void rotozoom_init(void)
{
    build_sin_lut();
    build_bump();
    for (int i = 0; i < 224; i++) {
        uint16_t p = asset_endcard_bg_pal[i];
        bg_r[i] = (uint8_t)rgb565_r8(p);
        bg_g[i] = (uint8_t)rgb565_g8(p);
        bg_b[i] = (uint8_t)rgb565_b8(p);
    }
    memcpy(bg_cache, asset_endcard_bg_data, FB_W * FB_H);
}

static void rotozoom_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;

    int alpha = 256;
    if (t_into < FADE_IN_MS) {
        alpha = (int)(t_into * 256 / FADE_IN_MS);
    } else if (t_into >= FADE_OUT_AT) {
        uint32_t into = t_into - FADE_OUT_AT;
        alpha = (into >= FADE_OUT_MS) ? 0 : 256 - (int)(into * 256 / FADE_OUT_MS);
    }
    apply_bg_fade(alpha);
    apply_bump_palette(alpha);
    apply_text_palette(alpha);

    uint8_t *fb = vga_320_back_buffer();
    draw_rotozoom(fb, t_into);
    draw_bump_logo(fb, t_into);

    uint32_t scroll = (uint32_t)((uint64_t)t_into * CREDITS_SCROLL_PX_PER_SEC / 1000);
    scroll %= (uint32_t)CREDITS_PIXEL_WIDTH;
    draw_credits(fb, scroll);
}

static void rotozoom_done(void) { /* palette overwritten by next scene */ }

const effect_t fx_rotozoom_credits_real = {
    .name  = "rotozoom_credits",
    .mode  = MODE_320,
    .init  = rotozoom_init,
    .frame = rotozoom_frame,
    .done  = rotozoom_done,
};
