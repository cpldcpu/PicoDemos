/* Scene 0 — Title (0:00–0:18, MODE_HIRES 320x240 truecolor).
 *
 * The deep-field star panorama fades up from black; "SINGULARITY" rises
 * under it in cold white with a soft cosmic-blue glow (anti-aliased, not
 * blocky). A slow brightness shimmer keeps the field alive.
 *
 * The backdrop is the 8bpp-packed panorama expanded through a per-frame
 * faded palette directly into the RGB565 hires buffer (no full-res RGB565
 * copy needed in flash or scratch).
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "fx_common.h"
#include <stdint.h>
#include <math.h>

#define SCENE_LEN_MS   18000
#define FADE_IN_MS      3000
#define TEXT_AT         2500
#define TEXT_IN_MS      2500
#define FADE_OUT_MS     3000
#define FADE_OUT_AT    (SCENE_LEN_MS - FADE_OUT_MS)
#define W VGA_HIRES_W
#define H VGA_HIRES_H

static uint8_t tr[224], tg[224], tb[224];
static const uint8_t *bg;

static const char TITLE[] = "SINGULARITY";
#define TSCALE 3

static inline int ramp_in(uint32_t t, uint32_t s, uint32_t d){ if(t<s)return 0; if(t>=s+d)return 256; return (int)((t-s)*256/d);}
static inline int ramp_out(uint32_t t, uint32_t s, uint32_t d){ if(t<s)return 256; if(t>=s+d)return 0; return 256-(int)((t-s)*256/d);}

static void title_init(void)
{
    for (int i = 0; i < 224; i++) {
        uint16_t p = asset_title_bg_pal[i];
        tr[i] = (uint8_t)rgb565_r8(p);
        tg[i] = (uint8_t)rgb565_g8(p);
        tb[i] = (uint8_t)rgb565_b8(p);
    }
    bg = asset_title_bg_data;
}

static void title_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    int bg_a, text_a;
    if (t_into < FADE_OUT_AT) {
        bg_a   = ramp_in(t_into, 0, FADE_IN_MS);
        text_a = ramp_in(t_into, TEXT_AT, TEXT_IN_MS);
    } else {
        int o = ramp_out(t_into, FADE_OUT_AT, FADE_OUT_MS);
        bg_a = o; text_a = o;
    }
    float sh = 1.0f + 0.06f * sinf(t_into * 0.0016f);
    int bga = (int)(bg_a * sh);

    /* Per-frame faded RGB565 palette → blit. */
    uint16_t pal[224];
    for (int i = 0; i < 224; i++)
        pal[i] = rgb565_pack((tr[i]*bga)>>8, (tg[i]*bga)>>8, (tb[i]*bga)>>8);

    uint16_t *fb = vga_hires_back_buffer();
    for (int i = 0; i < W*H; i++) fb[i] = pal[bg[i]];

    if (text_a > 0) {
        int x0 = (W - fx_text160_w(TITLE, TSCALE)) / 2;
        fx_text_glow(fb, TITLE, x0, 150, TSCALE,
                     235, 240, 255,      /* cold-white fill */
                      70, 110, 255,      /* cosmic-blue glow */
                     text_a);
    }
}

static void title_done(void) {}

const effect_t fx_title_real = {
    .name = "title", .mode = MODE_HIRES,
    .init = title_init, .frame = title_frame, .done = title_done,
};
