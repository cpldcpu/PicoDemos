/* transition.c — see transition.h. Five distinct liquid-chrome transitions.
 * Each covers the outgoing frame toward chrome-white with its own pattern, and
 * the incoming frame is revealed back out of white with the same pattern, so a
 * boundary reads as one coherent move. All passes only blend toward white (no
 * spatial resampling) so they're safe in-place on the freshly-rendered frame. */

#include "transition.h"
#include "../vga.h"
#include "../rgb565.h"

#define FADE_MS 460
#define WR 232
#define WG 240
#define WB 255

static inline int clampu(int v){ return v < 0 ? 0 : v > 256 ? 256 : v; }

/* White coverage 0..256 for pixel (x,y) given progress a (0=clear,256=all white). */
static int cover(int style, int x, int y, int a)
{
    switch (style) {
    default:
    case QS_TR_MELT: {
        int f = 80 + ((x * 7 + ((x * x) >> 5)) % 31);          /* 80..110 uneven */
        int front = a * (VGA_HIRES_H + 24) / 256 * f / 100;     /* flood from top */
        return clampu((front - y) * 24);
    }
    case QS_TR_WIPE: {
        int edge = a * (VGA_HIRES_W + 30) / 256 - 15;
        return clampu((edge - x) * 24);
    }
    case QS_TR_DISSOLVE: {
        int h = ((x * 73) ^ (y * 151) ^ ((x + y) << 2)) & 0xFF; /* cheap hash */
        return clampu((a - h) * 10);
    }
    case QS_TR_IRIS: {
        int dx = x - VGA_HIRES_W / 2, dy = y - VGA_HIRES_H / 2;
        int r = a * 205 / 256;                                  /* grows to corner */
        int d2 = dx * dx + dy * dy;
        return clampu((r * r - d2) >> 6);
    }
    case QS_TR_BLINDS: {
        int band = 12;
        int local = (y % band) * 256 / band;
        return clampu((a - local) * 12);
    }
    }
}

static void apply_style(int style, int a)
{
    if (a <= 0) return;
    uint16_t *fb = vga_hires_back_buffer();
    for (int y = 0; y < VGA_HIRES_H; y++) {
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int c = cover(style, x, y, a);
            if (!c) continue;
            uint16_t p = row[x];
            row[x] = rgb565_pack(rgb565_r8(p) + (((WR - rgb565_r8(p)) * c) >> 8),
                                 rgb565_g8(p) + (((WG - rgb565_g8(p)) * c) >> 8),
                                 rgb565_b8(p) + (((WB - rgb565_b8(p)) * c) >> 8));
        }
    }
}

void qs_transition_apply(uint32_t t_global, uint32_t scene_start, uint32_t scene_end,
                         int suppress_out, int out_style, int in_style)
{
    uint32_t into = t_global - scene_start;
    uint32_t left = (scene_end > t_global) ? (scene_end - t_global) : 0;

    if (into < FADE_MS) {                       /* incoming: reveal out of white */
        apply_style(in_style, (int)(256 - into * 256 / FADE_MS));
    } else if (!suppress_out && left < FADE_MS) { /* outgoing: cover to white */
        apply_style(out_style, (int)(256 - left * 256 / FADE_MS));
    }
}
