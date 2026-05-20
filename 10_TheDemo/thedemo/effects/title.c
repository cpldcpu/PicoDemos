/* Title scene — 0:00–0:20 (MODE_320).
 *
 * Three layered elements with their own appearance schedules:
 *
 *   1. Backdrop (slots 0..223): the AI-generated title_bg. Brightness
 *      faded in via palette scaling so the bitmap "comes up" from black.
 *
 *   2. Logo (slots 224..247 = vertical-rainbow gradient): the 160×120
 *      coral-frond mask, stamped 1× scale centred above the text. To
 *      avoid hiding the backdrop with a solid black silhouette while
 *      the logo is "off", we *gate stamping with an ordered (Bayer 4×4)
 *      dither* — at logo_visibility=0 nothing stamps, at 256 every
 *      mask pixel stamps. The reveal looks like a CRT building up the
 *      image, and the backdrop is fully visible through unstamped
 *      pixels.
 *
 *   3. Title text (slot 248): shared 8×8 bitmap font, 3× scale,
 *      centred under the logo. Fades in via slot brightness.
 *
 * Timing (ms-into-scene):
 *
 *      0 ── 2000 ── 3500 ── 5000 ────────── 16500 ── 20000
 *      │bg in│logo dither│ text in │  hold  │  fade out  │
 *
 * The fade-out at the end scales bg + logo + text slots together to
 * black for a clean cut into the voxel scene at 0:20.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "../font8x8.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define SCENE_LEN_MS       36000

#define BG_FADE_IN_AT          0
#define BG_FADE_IN_MS       2000

#define LOGO_FADE_AT        2000
#define LOGO_FADE_IN_MS     1500

#define TEXT_FADE_AT        3500
#define TEXT_FADE_IN_MS     1500

#define FADE_OUT_MS         3500
#define FADE_OUT_AT        (SCENE_LEN_MS - FADE_OUT_MS)

/* Palette layout in the 32 reserved slots:
 *   224..247 (24 slots) — logo vertical-rainbow gradient
 *   248..254 (7 slots)  — text body, per-row iridescent gradient
 *   255                 — text drop-shadow */
#define LOGO_BASE         224
#define LOGO_COUNT         24
#define TEXT_BODY_BASE    248
#define TEXT_BODY_COUNT     7
#define TEXT_SHADOW_SLOT  255

/* Logo placement: native 160×120 size, centred above the text strip. */
#define LOGO_X0          ((VGA_320_W - ASSET_LOGO_MASK_W) / 2)   /* 80 */
#define LOGO_Y0           20                                     /* leave 100px under for text */

/* Cached backdrop palette (RGB, 0..255) so per-frame fade can scale
 * without re-unpacking RGB565. */
static uint8_t target_r[224], target_g[224], target_b[224];

/* Backdrop bitmap lives in shared scene scratch — only one scene is
 * active at a time so the same 76 KB is reused by greetz and rotozoom
 * too. See scene_scratch.h. */
#include "../scene_scratch.h"
#define bg_cache (g_scratch.bg_cache)

/* --- ramps ---------------------------------------------------------- */
static inline int ramp_in(uint32_t t, uint32_t start, uint32_t dur)
{
    if (t <  start)       return 0;
    if (t >= start + dur) return 256;
    return (int)((t - start) * 256 / dur);
}
static inline int ramp_out(uint32_t t, uint32_t start, uint32_t dur)
{
    if (t <  start)       return 256;
    if (t >= start + dur) return 0;
    return 256 - (int)((t - start) * 256 / dur);
}

/* --- bayer 4x4 dither ----------------------------------------------- */
static const uint8_t bayer4[16] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5,
};

/* Title text — drawn via the shared 8×8 font in font8x8.h. Chunky 4×
 * scale + iridescent vertical gradient + drop shadow. */
static const char TITLE_STR[] = "SLOP";
#define TITLE_CHARS  ((int)sizeof(TITLE_STR) - 1)
#define TEXT_SCALE   4
#define TEXT_CHAR_W  (8 * TEXT_SCALE)          /* 32 px wide per glyph at 4× */
#define TEXT_GAP     TEXT_SCALE                /* 1-base-pixel space between letters */
#define TEXT_STRIDE  (TEXT_CHAR_W + TEXT_GAP)  /* 36 px per char */
#define TEXT_W       (TITLE_CHARS * TEXT_CHAR_W + (TITLE_CHARS - 1) * TEXT_GAP)
#define TEXT_H       (8 * TEXT_SCALE)
#define TEXT_X0      ((VGA_320_W - TEXT_W) / 2)
#define TEXT_Y0      168
#define SHADOW_DX    3
#define SHADOW_DY    3

/* --- palette updates ------------------------------------------------ */

static void apply_bg_palette(int alpha_q8)
{
    if (alpha_q8 < 0)   alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;
    for (int i = 0; i < 224; i++) {
        uint8_t r = (uint8_t)((target_r[i] * alpha_q8) >> 8);
        uint8_t g = (uint8_t)((target_g[i] * alpha_q8) >> 8);
        uint8_t b = (uint8_t)((target_b[i] * alpha_q8) >> 8);
        vga_320_palette_set(i, r, g, b);
    }
}

static void apply_logo_palette(uint32_t t_ms, int alpha_q8)
{
    if (alpha_q8 < 0)   alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;
    static const uint8_t anchors[3][3] = {
        { 220,  50, 160 },   /* magenta */
        { 240, 140,  40 },   /* orange  */
        {  40, 210, 200 },   /* teal    */
    };
    int phase = (int)((t_ms / 35) % LOGO_COUNT);
    for (int i = 0; i < LOGO_COUNT; i++) {
        int k   = (i + phase) % LOGO_COUNT;
        int seg = (k * 3) / LOGO_COUNT;
        int u   = (k * 3) % LOGO_COUNT;
        const uint8_t *c0 = anchors[seg];
        const uint8_t *c1 = anchors[(seg + 1) % 3];
        int r = c0[0] + (c1[0] - c0[0]) * u / LOGO_COUNT;
        int g = c0[1] + (c1[1] - c0[1]) * u / LOGO_COUNT;
        int b = c0[2] + (c1[2] - c0[2]) * u / LOGO_COUNT;
        r = (r * alpha_q8) >> 8;
        g = (g * alpha_q8) >> 8;
        b = (b * alpha_q8) >> 8;
        vga_320_palette_set(LOGO_BASE + i, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    }
}

static void apply_text_palette(uint32_t t_ms, int alpha_q8)
{
    if (alpha_q8 < 0)   alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;

    /* Iridescent vertical gradient cycling through the same three
     * anchor colours the logo uses (magenta → orange → teal), shifted
     * over time so the rainbow drifts upward through the letters. */
    static const uint8_t anchors[3][3] = {
        { 220,  50, 160 },
        { 240, 140,  40 },
        {  40, 210, 200 },
    };
    int phase = (int)((t_ms / 50) % (TEXT_BODY_COUNT * 3));
    for (int i = 0; i < TEXT_BODY_COUNT; i++) {
        int k   = (i * 3 + phase) % (TEXT_BODY_COUNT * 3);
        int seg = k / TEXT_BODY_COUNT;
        int u   = k % TEXT_BODY_COUNT;
        const uint8_t *c0 = anchors[seg];
        const uint8_t *c1 = anchors[(seg + 1) % 3];
        int r = c0[0] + (c1[0] - c0[0]) * u / TEXT_BODY_COUNT;
        int g = c0[1] + (c1[1] - c0[1]) * u / TEXT_BODY_COUNT;
        int b = c0[2] + (c1[2] - c0[2]) * u / TEXT_BODY_COUNT;
        r = (r * alpha_q8) >> 8;
        g = (g * alpha_q8) >> 8;
        b = (b * alpha_q8) >> 8;
        vga_320_palette_set(TEXT_BODY_BASE + i, (uint8_t)r, (uint8_t)g, (uint8_t)b);
    }

    /* Deep-plum drop-shadow — fades with the body. */
    vga_320_palette_set(TEXT_SHADOW_SLOT,
        (uint8_t)((30  * alpha_q8) >> 8),
        (uint8_t)(( 5  * alpha_q8) >> 8),
        (uint8_t)((25  * alpha_q8) >> 8));
}

/* --- 1bpp mask access ----------------------------------------------- */
static inline int mask_bit(int x, int y)
{
    if ((unsigned)x >= ASSET_LOGO_MASK_W || (unsigned)y >= ASSET_LOGO_MASK_H) return 0;
    int byte_idx = y * (ASSET_LOGO_MASK_W / 8) + (x >> 3);
    return (asset_logo_mask_data[byte_idx] >> (7 - (x & 7))) & 1;
}

/* Stamp logo with Bayer-4 dither gate. visibility_q8 = 0..256 controls
 * how many of the mask's pixels actually stamp. Vertical rainbow uses
 * one of the 24 logo slots per mask row. */
static void stamp_logo(uint8_t *fb, int visibility_q8)
{
    if (visibility_q8 <= 0) return;
    for (int sy = 0; sy < ASSET_LOGO_MASK_H; sy++) {
        int dy = LOGO_Y0 + sy;
        if ((unsigned)dy >= VGA_320_H) continue;
        uint8_t *row = fb + dy * VGA_320_W;
        uint8_t color = (uint8_t)(LOGO_BASE + (sy * LOGO_COUNT / ASSET_LOGO_MASK_H));
        for (int sx = 0; sx < ASSET_LOGO_MASK_W; sx++) {
            if (!mask_bit(sx, sy)) continue;
            int dx = LOGO_X0 + sx;
            if ((unsigned)dx >= VGA_320_W) continue;
            int threshold = bayer4[(sy & 3) * 4 + (sx & 3)] * 16;  /* 0..240 */
            if (threshold >= visibility_q8) continue;
            row[dx] = color;
        }
    }
}

/* Stamp one pass of the title chars with a given (dx, dy) offset and
 * colour-picking strategy. row_colour < 0 means "use TEXT_SHADOW_SLOT"
 * (solid fill); otherwise the slot is picked per font row from the
 * body gradient. */
static void stamp_text_pass(uint8_t *fb, int dx_off, int dy_off, int use_shadow)
{
    for (int ci = 0; ci < TITLE_CHARS; ci++) {
        const uint8_t *g = font8x8_glyph(TITLE_STR[ci]);
        int cx0 = TEXT_X0 + ci * TEXT_STRIDE + dx_off;
        for (int row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            int dy0 = TEXT_Y0 + row * TEXT_SCALE + dy_off;
            uint8_t color = use_shadow
                ? TEXT_SHADOW_SLOT
                : (uint8_t)(TEXT_BODY_BASE + (row * TEXT_BODY_COUNT) / 8);
            for (int col = 0; col < 8; col++) {
                if (!((bits >> (7 - col)) & 1)) continue;
                int dx0 = cx0 + col * TEXT_SCALE;
                for (int dy = dy0; dy < dy0 + TEXT_SCALE; dy++) {
                    if ((unsigned)dy >= VGA_320_H) continue;
                    uint8_t *p = fb + dy * VGA_320_W + dx0;
                    for (int k = 0; k < TEXT_SCALE; k++) {
                        if ((unsigned)(dx0 + k) >= VGA_320_W) continue;
                        p[k] = color;
                    }
                }
            }
        }
    }
}

/* Two-pass stamp: drop-shadow first at (+SHADOW_DX, +SHADOW_DY) in the
 * dim plum slot, then the body on top with per-row iridescent gradient.
 * Both passes only render when the text palette is non-zero — fades are
 * handled by ramping the palette, not by gating the stamp itself. */
static void stamp_text(uint8_t *fb)
{
    stamp_text_pass(fb, SHADOW_DX, SHADOW_DY, /*use_shadow=*/1);
    stamp_text_pass(fb, 0,         0,         /*use_shadow=*/0);
}

/* --- effect lifecycle ----------------------------------------------- */

static void title_init(void)
{
    for (int i = 0; i < 224; i++) {
        uint16_t p = asset_title_bg_pal[i];
        target_r[i] = (uint8_t)rgb565_r8(p);
        target_g[i] = (uint8_t)rgb565_g8(p);
        target_b[i] = (uint8_t)rgb565_b8(p);
    }
    /* Pull the backdrop bitmap into SRAM once. See bg_cache comment for
     * the motivating glitch. */
    memcpy(bg_cache, asset_title_bg_data, sizeof bg_cache);
}

static void title_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;

    /* Per-element alphas. During fade-out everything scales together. */
    int bg_alpha, logo_dither_alpha, logo_palette_alpha, text_alpha;

    if (t_into < FADE_OUT_AT) {
        bg_alpha           = ramp_in(t_into, BG_FADE_IN_AT, BG_FADE_IN_MS);
        logo_dither_alpha  = ramp_in(t_into, LOGO_FADE_AT,  LOGO_FADE_IN_MS);
        logo_palette_alpha = 256;
        text_alpha         = ramp_in(t_into, TEXT_FADE_AT,  TEXT_FADE_IN_MS);
    } else {
        int out = ramp_out(t_into, FADE_OUT_AT, FADE_OUT_MS);
        bg_alpha           = out;
        logo_dither_alpha  = 256;   /* leave full visibility — let palette do the fade */
        logo_palette_alpha = out;
        text_alpha         = out;
    }

    apply_bg_palette(bg_alpha);
    apply_logo_palette(t_into, logo_palette_alpha);
    apply_text_palette(t_into, text_alpha);

    /* Backdrop blit (from SRAM cache, not flash — see bg_cache).
     * Then logo dither stamp, then text on top. */
    uint8_t *fb = vga_320_back_buffer();
    memcpy(fb, bg_cache, VGA_320_W * VGA_320_H);
    stamp_logo(fb, logo_dither_alpha);

    /* Text gate: skip stamping entirely while slot 248 is too dark to
     * read — otherwise we'd be painting near-black pixels over the
     * backdrop and producing a "ghost silhouette" of the text well
     * before the fade-in starts. Threshold matches the equivalent dim
     * warm-white the slot holds at alpha=32 (~12 %). */
    if (text_alpha > 32) stamp_text(fb);
}

static void title_done(void) { /* palette overwritten by next scene */ }

const effect_t fx_title_real = {
    .name  = "title",
    .mode  = MODE_320,
    .init  = title_init,
    .frame = title_frame,
    .done  = title_done,
};
