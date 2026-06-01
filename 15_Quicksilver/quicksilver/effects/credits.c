/* credits.c — QUICKSILVER finale. A chrome TUNNEL (not a 3D object) carries the
 * scrolling credits + wordmark, then fades to black.
 *
 * The tunnel is a LUT effect: at init we precompute, per screen pixel, the
 * angle around the shaft and the depth into it (one atan2/sqrt per pixel, once).
 * Each frame is then just "rotate + fly forward + sample a small chrome texture"
 * — ~2 SRAM reads per pixel, no transform/sort/flash, so it holds full
 * framerate. The 128x128 chrome texture and the 160x120 LUT both live in
 * g_scratch SRAM. */

#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "../scene_scratch.h"
#include "assets.h"
#include "qs_text.h"
#include "qs_fx.h"

#include <math.h>

#define TXW 128                         /* chrome wall texture            */
#define LW  160                         /* tunnel LUT grid (upscaled 2x)  */
#define LH  120

/* g_scratch carve: [0..32KB) = 128x128 chrome texture; [32KB..) = 160x120 LUT */
static uint16_t *s_tex;
static uint16_t *s_lut;                 /* (angle<<8) | depth */

static const char *lines[] = {
    /* All lines centred individually (no leading-space columns), and every
     * glyph is in the 8x8 charset (A-Z 0-9 space - . ! :) — no parens/&/slash. */
    "LATENT",
    "",
    "A DEMOSCENE GROUP FOR",
    "MACHINE-MADE PRODUCTIONS",
    "ON BARE-METAL SILICON",
    "",
    "FOUNDED 2026",
    "",
    "- MEMBERS -",
    "",
    "CLAUDE OPUS 4.8",
    "CODE AND DIRECTION",
    "",
    "AZURE",
    "HUMAN CRITIC AND PRODUCER",
    "",
    "GEMINI 3.5 FLASH",
    "AND NANO BANANA 2",
    "VISUALS",
    "",
    "SUNO 4.5",
    "MUSIC",
    "",
    "- THIS PRODUCTION -",
    "",
    "QUICKSILVER",
    "RACING THE RP2350 INTERPOLATOR",
    "",
    "AFFINE . BLEND . CLAMP . POP",
    "ROTOZOOM . MODE-7 . CHROME",
    "BEAM-RACED FULL VGA. NO BUFFER.",
    "",
    "GREETINGS TO THE SCENE",
    "AND TO THE SCEPTICS.",
    "",
    NULL,
};

static void credits_init(void)
{
    uint8_t *sram = (uint8_t *)g_scratch.bg_cache;
    s_tex = (uint16_t *)sram;
    s_lut = (uint16_t *)(sram + TXW * TXW * 2);

    /* downsample the seamless chrome tunnel-wall texture 256->128 into SRAM */
    const uint16_t *src = (const uint16_t *)asset_tunnel_data;
    for (int y = 0; y < TXW; y++)
        for (int x = 0; x < TXW; x++)
            s_tex[y * TXW + x] = src[(y * 2) * ASSET_TUNNEL_W + x * 2];

    /* Precompute ONE QUADRANT of the tunnel (the field is 4-way symmetric about
     * the screen centre). LW x LH = 160 x 120 = exactly a quadrant, so each
     * screen pixel maps 1:1 to a LUT cell — FULL 320-resolution lookup with no
     * upscaling/blockiness. Store the quadrant angle u0 in [0, TXW/4] + depth. */
    for (int ay = 0; ay < LH; ay++) {
        for (int ax = 0; ax < LW; ax++) {
            float dist = sqrtf((float)ax * ax + (float)ay * ay); if (dist < 1.f) dist = 1.f;
            float ang0 = atan2f((float)ay, (float)ax);             /* [0, pi/2] */
            int u0 = (int)(ang0 * (TXW / 6.2831853f));             /* [0, TXW/4] */
            int depth = (int)(1400.0f / dist); if (depth > 255) depth = 255;
            s_lut[ay * LW + ax] = (uint16_t)((u0 << 8) | depth);
        }
    }
}

static void tunnel(uint32_t t_ms)
{
    uint16_t *fb = vga_hires_back_buffer();
    int rot    = (int)(t_ms * 0.045f);     /* swirl — >=1 texel/frame: smooth */
    int scroll = (int)(t_ms * 0.090f);     /* fly forward */
    for (int y = 0; y < VGA_HIRES_H; y++) {
        int dy = y - VGA_HIRES_H / 2;
        int ay = dy < 0 ? -dy : dy; if (ay >= LH) ay = LH - 1;
        const uint16_t *lrow = &s_lut[ay * LW];
        int ysign = dy < 0;
        uint16_t *frow = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int dx = x - VGA_HIRES_W / 2;
            int ax = dx < 0 ? -dx : dx; if (ax >= LW) ax = LW - 1;
            uint16_t L = lrow[ax];
            int u0 = L >> 8, depth = L & 0xFF;
            int u;                                       /* reconstruct full angle */
            if (dx >= 0) u = ysign ? (TXW - u0)     : u0;
            else         u = ysign ? (TXW/2 + u0)   : (TXW/2 - u0);
            uint16_t texel = s_tex[((depth + scroll) & (TXW-1)) * TXW + ((u + rot) & (TXW-1))];
            int br = 255 - depth;                        /* far (centre) -> dark */
            int d = qs_dither(x, y);
            frow[x] = rgb565_pack((rgb565_r8(texel) * br >> 8) + d,
                                  (rgb565_g8(texel) * br >> 8) + d,
                                  (rgb565_b8(texel) * br >> 8) + d);
        }
    }
}

static void credits_frame(uint32_t t_ms, uint32_t t_global)
{
    tunnel(t_ms);

    /* The credit lines scroll up and off; after a GAP the final card
     * (wordmark + MERCURY / 2026) slides to centre and HOLDS there — a clean,
     * spaced end card rather than a frozen mid-scroll. */
    int scroll = (int)(t_ms * 0.013f);
    if (scroll > 775) scroll = 775;                /* clamp: hold the final card centred */
    int sweepx = (int)(fmodf(t_ms * 0.20f, (float)(VGA_HIRES_W + 120))) - 60;
    int y = VGA_HIRES_H + 20 - scroll;

    for (int i = 0; lines[i]; i++) {
        const char *s = lines[i];
        if (*s) {
            int w = qs_text_w(s, 1);
            if (y > -10 && y < VGA_HIRES_H) qs_text_chrome(s, (VGA_HIRES_W - w) / 2, y, 1, 50);
            y += 17;
        } else {
            y += 11;
        }
    }
    y += 70;                                        /* gap before the final card */
    if (y > -QS_LOGO_H && y < VGA_HIRES_H) qs_logo_blit(0, y, sweepx);
    y += QS_LOGO_H + 12;
    /* demoscene end card: production (wordmark above) + group logo + year */
    if (y > -ASSET_LATENT_LOGO_H && y < VGA_HIRES_H) qs_latent_blit(y);
    y += ASSET_LATENT_LOGO_H + 8;
    if (y > -10 && y < VGA_HIRES_H) {
        const char *m = "2026"; int w = qs_text_w(m, 1);
        qs_text_chrome(m, (VGA_HIRES_W - w) / 2, y, 1, 50);
    }

    /* fade to black over the last 5 s — a clear ending */
    uint32_t end = scene_cur_end_ms();
    uint32_t left = end > t_global ? end - t_global : 0;
    if (left < 5000) {
        int k = 256 - (int)(left * 256 / 5000);
        uint16_t *fb = vga_hires_back_buffer();
        int n = VGA_HIRES_W * VGA_HIRES_H;
        for (int i = 0; i < n; i++) {
            uint16_t c = fb[i];
            fb[i] = rgb565_pack(rgb565_r8(c) * (256 - k) >> 8,
                                rgb565_g8(c) * (256 - k) >> 8,
                                rgb565_b8(c) * (256 - k) >> 8);
        }
    }
}

const effect_t fx_credits = {
    .name  = "credits",
    .mode  = MODE_HIRES,
    .init  = credits_init,
    .frame = credits_frame,
    .done  = NULL,
};
