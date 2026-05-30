/* Scene 7 — Rebirth + endcard (4:37–5:30, MODE_HIRES 320x240 truecolor).
 *
 * Out of the singularity flash a newborn universe fades up (white →
 * colour), the "SINGULARITY" title reprises with a soft glow, and the
 * credits scroll up through the dark lower third over the finale.
 *
 * Backdrop is the 8bpp endcard expanded through a per-frame faded palette
 * (white-out + scene fade) directly into the RGB565 hires buffer.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "fx_common.h"
#include <stdint.h>

#define SCENE_LEN_MS  53000
#define WHITE_MS       3500
#define TITLE_AT       2500
#define TITLE_IN       2500
#define W VGA_HIRES_W
#define H VGA_HIRES_H

static uint8_t tr[224], tg[224], tb[224];
static const uint8_t *bg;

static const char *CREDITS[] = {
    "SINGULARITY", "",
    "a relativistic journey", "for the RP2350", "",
    "direction + code", "CLAUDE OPUS 4.8", "",
    "real-time effects", "curl-noise nebula", "relativistic starfield",
    "doppler-beamed mode-7 disk", "schwarzschild lensing", "",
    "music", "SUNO 4.5  -  GRAVITON CHOIR", "",
    "greetings to the scene", "", "2026",
};
#define NCREDITS ((int)(sizeof(CREDITS)/sizeof(CREDITS[0])))

static inline int ramp_in(uint32_t t,uint32_t s,uint32_t d){if(t<s)return 0;if(t>=s+d)return 256;return (int)((t-s)*256/d);}

static void rebirth_init(void)
{
    for (int i = 0; i < 224; i++) {
        uint16_t p = asset_endcard_bg_pal[i];
        tr[i]=(uint8_t)rgb565_r8(p); tg[i]=(uint8_t)rgb565_g8(p); tb[i]=(uint8_t)rgb565_b8(p);
    }
    bg = asset_endcard_bg_data;
}

static void rebirth_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    int white_a = 256 - ramp_in(t_into, 0, WHITE_MS);
    int endf = (t_into > SCENE_LEN_MS-2500) ? 256-(int)((t_into-(SCENE_LEN_MS-2500))*256/2500) : 256;

    uint16_t pal[224];
    for (int i = 0; i < 224; i++) {
        int r = tr[i] + (((255-tr[i])*white_a)>>8);
        int g = tg[i] + (((255-tg[i])*white_a)>>8);
        int b = tb[i] + (((255-tb[i])*white_a)>>8);
        pal[i] = rgb565_pack((r*endf)>>8, (g*endf)>>8, (b*endf)>>8);
    }

    uint16_t *fb = vga_hires_back_buffer();
    for (int i = 0; i < W*H; i++) fb[i] = pal[bg[i]];

    int title_a = (ramp_in(t_into, TITLE_AT, TITLE_IN) * endf) >> 8;
    if (title_a > 0) {
        const char *T = "SINGULARITY";
        int x0 = (W - fx_text160_w(T, 3)) / 2;
        fx_text_glow(fb, T, x0, 28, 3, 235,240,255, 70,110,255, title_a);
    }

    /* Credits scroll upward in the lower third, starting ~6 s in. */
    if (t_into > 6000 && title_a > 0) {
        /* Float scroll → smooth sub-pixel glide (the renderer vertically
         * anti-aliases), no integer-step jerk. */
        float scroll = (t_into - 6000) * 0.030f;     /* ~30 px/s */
        float y = (float)H + 12.0f - scroll;
        for (int i = 0; i < NCREDITS; i++, y += 18.0f) {
            if (CREDITS[i][0] == 0) continue;
            if (y > H || y < 100.0f) continue;        /* over the dark lower band */
            /* Fade out over the top ~40 px instead of popping off abruptly. */
            int la = title_a;
            if (y < 140.0f) { la = (int)(title_a * (y - 100.0f) / 40.0f); if (la <= 0) continue; }
            int x0 = (W - fx_text160_w(CREDITS[i], 1)) / 2;
            fx_text_scroll(fb, CREDITS[i], x0, y, 1, 230,236,255, la);
        }
    }
}

static void rebirth_done(void) {}

const effect_t fx_rebirth_real = {
    .name = "rebirth", .mode = MODE_HIRES,
    .init = rebirth_init, .frame = rebirth_frame, .done = rebirth_done,
};
