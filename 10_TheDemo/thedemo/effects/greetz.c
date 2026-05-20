/* Greetings — scene 4 (1:30 – 1:50, MODE_SPLIT_160_OVER_320).
 *
 * Old-school copper-style raster split: the screen is rendered from
 * TWO framebuffers per frame, switching source at display row 180.
 *
 *   rows 0..179   → fb160 (160×120 RGB565, 2× pixel-doubled).
 *                   Truecolor metaballs blended over a downsampled
 *                   bokeh background — proper per-pixel alpha at the
 *                   blob rims, no palette banding.
 *
 *   rows 180..239 → fb320 (320×240 chunky 8bpp palettised).
 *                   Bokeh backdrop + crisp hi-res 8×8 scroller at
 *                   scale 1 (one fb pixel = one display pixel) so the
 *                   text reads sharp against the busy background.
 *
 * The 8bpp palette only needs entries for the lower-region content
 * (bokeh + text), so we don't have to share slots with the upper
 * region. Slots 240/241 are the text fill / shadow.
 *
 * Memory: bg160_cache (38 KB RGB565 downsample of the bokeh) is built
 * once in init from the asset. The scene scratch's bg_cache (76 KB)
 * holds the full-res 8bpp bokeh for the lower scroller region.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "../font8x8.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define FB_W                VGA_320_W
#define FB_H                VGA_320_H
#define HW                  VGA_160_W       /* 160 */
#define HH                  VGA_160_H       /* 120 */

#define SCENE_LEN_MS        36000
#define FADE_IN_MS           2000
#define FADE_OUT_MS          2500
#define FADE_OUT_AT         (SCENE_LEN_MS - FADE_OUT_MS)

/* Display row at which the split switches from fb160 (above) to fb320
 * (below). fb160 doubles vertically, so display row 180 corresponds
 * to fb160 row 90 (= last row participating in the upper output). */
#define SPLIT_ROW            180

/* fb320 palette layout: only the lower-region content uses the
 * palette, so slots 0..223 are the bokeh and 240/241 are the text. */
#define TEXT_SLOT           240
#define TEXT_SHADOW_SLOT    241

/* Hi-res scroller in MODE_320: 1 fb pixel per font pixel = crisp 8×8
 * text on the 320-wide framebuffer. Sits in the lower strip. */
#define TEXT_SCALE           1
#define TEXT_CHAR_W         (FONT8X8_W * TEXT_SCALE)
#define TEXT_GAP             TEXT_SCALE
#define TEXT_STRIDE         (TEXT_CHAR_W + TEXT_GAP)
#define TEXT_BASE_Y          204
#define SINE_AMP              6
/* Scroll cadence and sine animation are derived from t_into directly
 * (pixels-per-second × milliseconds / 1000), NOT from integer-divided
 * frame ticks. The latter aliased badly when the per-frame interval
 * wasn't an exact divisor of the quantisation step.
 *
 * Speed × text-length is tuned so the scroller completes exactly one
 * pass during the scene's readable window (post fade-in / pre fade-out
 * ≈ 31.5 s of the 36 s scene). With the GREETZ string below at 280
 * chars × 9 px stride = 2520 px, 80 px/s gives 31.5 s per pass — same
 * pace as the credits scroller for consistency. */
#define SCROLL_PX_PER_SEC      80
#define SINE_PHASE_PER_SEC    25

/* Greetz text. Important: the demo (and this scroller) was written by
 * Claude, so the greetings need to come from the AI, not LARP as a
 * human scener. Tone: honest about who's talking, genuine about the
 * respect — the listed groups are top-tier and their work is in the
 * training data that taught the model what a demo is. */
static const char GREETZ[] =
    "                  "
    "CLAUDE WROTE THIS DEMO INCLUDING THIS SCROLLER - "
    "REAL GREETZ TO THE GROUPS I LEARNED FROM - "
    "ARTWORK - SPACEBALLS - FAIRLIGHT - CNCD - HAUJOBB - "
    "TBL - LOONIES - ALCATRAZ - FNUQUE - CONSPIRACY - "
    "FARBRAUSCH - ASD - TITAN - KEWLERS - MELON - "
    "RESPECT FOR THE CRAFT.    "
    "                  ";

/* Cached fb320 backdrop palette (slots 0..223 of the bokeh asset). */
static uint8_t bg_r[224], bg_g[224], bg_b[224];

/* Full-res 8bpp bokeh sits in shared scratch (same physical bytes as
 * title's bg_cache + rotozoom's bg_cache — only one scene active at a
 * time). The RGB565 downsample for the truecolor upper region is NOT
 * cached: it's regenerated each frame by point-sampling fb_cache and
 * looking up RGB565 via asset_greetz_bg_pal. At 19 200 lookups/frame
 * that's well under 1 ms — cheaper than reserving a 38 KB static. */
#include "../scene_scratch.h"
#define bg_cache (g_scratch.bg_cache)

/* Sin LUT in Q15 — for the scroller sine wobble. */
static int16_t sin_q15[256];
static inline int16_t s256(int idx) { return sin_q15[idx & 0xFF]; }

static void build_sin_lut(void)
{
    for (int i = 0; i < 256; i++) {
        sin_q15[i] = (int16_t)(sinf(i * (2.0f * 3.14159265f / 256.0f)) * 32767.0f);
    }
}

/* --- fb320 palette -------------------------------------------------- */

static void apply_bg_fade(int alpha_q8)
{
    if (alpha_q8 < 0)   alpha_q8 = 0;
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
    if (alpha_q8 < 0)   alpha_q8 = 0;
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

/* --- truecolor metaballs (upper region, fb160 RGB565) -------------- */

#define NUM_BLOBS    6

typedef struct { float cx, cy, r2; } blob_t;

static void compute_blobs(uint32_t t_ms, blob_t *out)
{
    float t = t_ms * 0.001f;
    /* Coords in DISPLAY pixel space (320×240); we'll halve when
     * mapping into fb160 sample positions. */
    static const float kx [NUM_BLOBS] = { 0.27f, 0.41f, 0.33f, 0.19f, 0.51f, 0.23f };
    static const float ky [NUM_BLOBS] = { 0.22f, 0.18f, 0.35f, 0.29f, 0.14f, 0.43f };
    static const float phx[NUM_BLOBS] = { 0.0f,  1.7f,  3.4f,  4.9f,  2.1f,  5.3f };
    static const float phy[NUM_BLOBS] = { 1.1f,  3.9f,  0.5f,  2.6f,  4.2f,  1.4f };
    static const float r_b[NUM_BLOBS] = { 22.0f, 20.0f, 25.0f, 19.0f, 23.0f, 21.0f };
    /* Y centre stays in the upper-region's visible band so blobs
     * don't wander off the bottom edge of the truecolor half. */
    for (int b = 0; b < NUM_BLOBS; b++) {
        out[b].cx = (FB_W * 0.5f) * (1.0f + 0.55f * sinf(t * kx[b] + phx[b]));
        out[b].cy = (SPLIT_ROW * 0.5f) * (1.0f + 0.55f * cosf(t * ky[b] + phy[b]));
        float r   = r_b[b] + sinf(t * 1.3f + b * 0.9f) * 3.0f;
        out[b].r2 = r * r;
    }
}

/* RGB565 blend helper. alpha_q8 = 0 → keep cur, 256 → replace with (r,g,b). */
static inline uint16_t blend_rgb565(uint16_t cur, int r, int g, int b, int alpha_q8)
{
    int cr = rgb565_r8(cur);
    int cg = rgb565_g8(cur);
    int cb = rgb565_b8(cur);
    int nr = cr + (((r - cr) * alpha_q8) >> 8);
    int ng = cg + (((g - cg) * alpha_q8) >> 8);
    int nb = cb + (((b - cb) * alpha_q8) >> 8);
    return rgb565_pack(nr, ng, nb);
}

/* Draw metaballs into fb160 RGB565. Each pixel computes the metaball
 * field, derives a per-pixel alpha (translucent at the rim, opaque at
 * the core) and a tint (deep magenta → warm white as field grows),
 * then blends with the bokeh underneath. */
static void draw_blobs_truecolor(uint16_t *fb, uint32_t t_ms, int scene_alpha_q8)
{
    blob_t blobs[NUM_BLOBS];
    compute_blobs(t_ms, blobs);

    /* Orb gradient anchors — same colour story as the previous palette
     * version (deep magenta → warm white). */
    static const uint8_t lo_rgb[3] = {  18,   6,  30 };
    static const uint8_t hi_rgb[3] = { 255, 200, 160 };

    for (int py = 0; py < HH; py++) {
        /* Hoist per-row (py·2 − cy)² + ε term per ball. */
        float dy2[NUM_BLOBS];
        float fy = (float)(py * 2);
        for (int b = 0; b < NUM_BLOBS; b++) {
            float dy = fy - blobs[b].cy;
            dy2[b] = dy * dy + 1.0f;
        }
        uint16_t *row = fb + py * HW;
        for (int px = 0; px < HW; px++) {
            float fx = (float)(px * 2);
            float f  = 0.0f;
            for (int b = 0; b < NUM_BLOBS; b++) {
                float dx = fx - blobs[b].cx;
                f += blobs[b].r2 / (dx * dx + dy2[b]);
            }
            if (f < 0.5f) continue;        /* below the level set */
            /* Strength 0..1: 0 at f=0.5 (rim), 1 at f≥2.0 (well inside). */
            float strength = (f - 0.5f) * 0.67f;
            if (strength > 1.0f) strength = 1.0f;
            float s_curve = sqrtf(strength);   /* tilt toward bright cores */
            int r = (int)(lo_rgb[0] + (hi_rgb[0] - lo_rgb[0]) * s_curve);
            int g = (int)(lo_rgb[1] + (hi_rgb[1] - lo_rgb[1]) * s_curve);
            int b = (int)(lo_rgb[2] + (hi_rgb[2] - lo_rgb[2]) * s_curve);
            /* Per-pixel alpha = strength itself — translucent rim,
             * opaque core. Combine with the scene-fade alpha. */
            int a = (int)(strength * 256.0f);
            if (a > 256) a = 256;
            a = (a * scene_alpha_q8) >> 8;
            row[px] = blend_rgb565(row[px], r, g, b, a);
        }
    }
}

/* Build the upper-region's RGB565 backdrop. Point-sample the cached
 * 320×240 8bpp bokeh at (x*2, y*2), look up RGB565 from the asset
 * palette, optionally scale by scene-fade alpha. Done per-frame — no
 * static cache, saving 38 KB BSS. */
static void fill_bg160(uint16_t *fb, int alpha_q8)
{
    if (alpha_q8 < 0)   alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;
    if (alpha_q8 == 256) {
        for (int y = 0; y < HH; y++) {
            const uint8_t *src = &bg_cache[(y * 2) * FB_W];
            uint16_t      *dst = &fb[y * HW];
            for (int x = 0; x < HW; x++) {
                dst[x] = asset_greetz_bg_pal[src[x * 2]];
            }
        }
        return;
    }
    for (int y = 0; y < HH; y++) {
        const uint8_t *src = &bg_cache[(y * 2) * FB_W];
        uint16_t      *dst = &fb[y * HW];
        for (int x = 0; x < HW; x++) {
            uint16_t c = asset_greetz_bg_pal[src[x * 2]];
            int r = (rgb565_r8(c) * alpha_q8) >> 8;
            int g = (rgb565_g8(c) * alpha_q8) >> 8;
            int b = (rgb565_b8(c) * alpha_q8) >> 8;
            dst[x] = rgb565_pack(r, g, b);
        }
    }
}

/* --- hi-res scroller (lower region, fb320 palette) ----------------- */

#define GREETZ_LEN          ((int)(sizeof GREETZ - 1))
#define SCROLL_PIXEL_WIDTH  (GREETZ_LEN * TEXT_STRIDE)

static void draw_scroller(uint8_t *fb, uint32_t scroll_offset_px, int sine_phase)
{
    for (int x = 0; x < FB_W; x++) {
        uint32_t world_x  = scroll_offset_px + (uint32_t)x;
        int char_idx      = (int)((world_x / TEXT_STRIDE) % GREETZ_LEN);
        int in_stride     = world_x % TEXT_STRIDE;
        if (in_stride >= TEXT_CHAR_W) continue;
        int glyph_col     = in_stride / TEXT_SCALE;

        const uint8_t *g = font8x8_glyph(GREETZ[char_idx]);

        int sine_y = (s256((x + sine_phase) & 0xFF) * SINE_AMP) >> 15;

        for (int row = 0; row < 8; row++) {
            int bit = (g[row] >> (7 - glyph_col)) & 1;
            if (!bit) continue;
            int base_y = TEXT_BASE_Y + row * TEXT_SCALE + sine_y;
            for (int dy = 0; dy < TEXT_SCALE; dy++) {
                int y = base_y + dy;
                if ((unsigned)y >= FB_H) continue;
                /* Drop shadow 1 px down-right. */
                int ys = y + 1, xs = x + 1;
                if ((unsigned)ys < FB_H && (unsigned)xs < FB_W)
                    fb[ys * FB_W + xs] = TEXT_SHADOW_SLOT;
                fb[y * FB_W + x] = TEXT_SLOT;
            }
        }
    }
}

/* --- effect lifecycle ---------------------------------------------- */

static void greetz_init(void)
{
    build_sin_lut();

    /* Cache the bokeh palette in unpacked RGB for the fb320 lower
     * region's per-frame fade. */
    for (int i = 0; i < 224; i++) {
        uint16_t p = asset_greetz_bg_pal[i];
        bg_r[i] = (uint8_t)rgb565_r8(p);
        bg_g[i] = (uint8_t)rgb565_g8(p);
        bg_b[i] = (uint8_t)rgb565_b8(p);
    }

    /* Copy the full-res 8bpp bokeh into shared scratch — used by the
     * fb320 lower region AND as the source for the per-frame RGB565
     * downsample in fill_bg160 (upper region). */
    memcpy(bg_cache, asset_greetz_bg_data, FB_W * FB_H);
}

static void greetz_frame(uint32_t t_into, uint32_t t_global)
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
    apply_text_palette(alpha);

    /* Upper region: truecolor bokeh + alpha-blended metaballs. */
    uint16_t *fb160 = vga_160_back_buffer();
    fill_bg160(fb160, alpha);
    draw_blobs_truecolor(fb160, t_into, alpha);

    /* Lower region: bokeh + chunky scroller in palette mode. */
    uint8_t *fb320 = vga_320_back_buffer();
    memcpy(fb320, bg_cache, FB_W * FB_H);

    uint32_t scroll_offset = (uint32_t)((uint64_t)t_into * SCROLL_PX_PER_SEC / 1000);
    scroll_offset %= (uint32_t)SCROLL_PIXEL_WIDTH;
    int sine_phase = (int)(((uint64_t)t_into * SINE_PHASE_PER_SEC) / 1000);
    draw_scroller(fb320, scroll_offset, sine_phase);

    /* Configure the raster split for this frame. Idempotent if mode
     * has already been set; vga_set_mode no-ops on equal values. */
    vga_set_split_row(SPLIT_ROW);
}

static void greetz_done(void) { /* palette overwritten by next scene */ }

const effect_t fx_greetz_real = {
    .name  = "greetz",
    .mode  = MODE_SPLIT_160_OVER_320,
    .init  = greetz_init,
    .frame = greetz_frame,
    .done  = greetz_done,
};
