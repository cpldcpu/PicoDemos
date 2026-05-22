#include "../scene.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../font8x8.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define W160 160
#define H120 120

// Character rendering helper for 8bpp scroller
static void draw_scroller_char(uint8_t *fb, char c, int x0, int y0, uint8_t color)
{
    const uint8_t *glyph = font8x8_glyph(c);
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            if (row & (1 << (7 - gx))) {
                // Scale 2x
                for (int sy = 0; sy < 2; sy++) {
                    int py = y0 + gy * 2 + sy;
                    if (py < 0 || py >= VGA_320_H) continue;
                    for (int sx = 0; sx < 2; sx++) {
                        int px = x0 + gx * 2 + sx;
                        if (px < 0 || px >= VGA_320_W) continue;
                        fb[py * VGA_320_W + px] = color;
                    }
                }
            }
        }
    }
}

void fx_greetings_init(void)
{
    // Configure raster split-screen timing boundary: row 160 initially
    vga_set_mode(MODE_SPLIT_160_OVER_320);
    vga_set_split_row(160);
}

void fx_greetings_frame(uint32_t t_ms, uint32_t t_global)
{
    uint16_t *fb160 = vga_160_back_buffer();
    uint8_t *fb320 = vga_320_back_buffer();
    float t_sec = t_ms * 0.001f;

    // Calculate transition sweep down and palette fade
    int current_split = 160;
    float fade = 1.0f;
    uint32_t scene_duration = 27887; // 143197 - 115310
    
    if (t_ms >= scene_duration - 1500) {
        float f = (t_ms - (scene_duration - 1500)) / 1500.0f; // 0.0 to 1.0
        current_split = 160 + (int)(f * 80.0f);
        if (current_split > 240) current_split = 240;
        fade = 1.0f - f;
    }
    
    // Update raster split boundary
    vga_set_split_row(current_split);

    // Apply palettized fade-out on the 320x240 section
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_greetz_bg_pal[i];
        uint8_t b = (uint8_t)((((c >> 11) & 0x1F) << 3) * fade);
        uint8_t g = (uint8_t)((((c >> 6)  & 0x1F) << 3) * fade);
        uint8_t r = (uint8_t)(((c        & 0x1F) << 3) * fade);
        vga_320_palette_set(i, r, g, b);
    }
    // Index 255: Neon scroller text fades to black
    vga_320_palette_set(255, (uint8_t)(230 * fade), (uint8_t)(255 * fade), (uint8_t)(255 * fade));

    // 1. RENDER 2D METABALLS ON TOP PART (fb160)
    uint16_t bg_color = rgb565_pack(8, 4, 20);
    int metaballs_limit = current_split / 2; // Up to 120 lines maximum
    
    for (int y = 0; y < metaballs_limit; y++) {
        for (int x = 0; x < W160; x++) {
            fb160[y * W160 + x] = bg_color;
        }
    }

    // Positions of 3 metaballs
    float m1x = W160 / 2.0f + sinf(t_sec * 2.3f) * 45.0f;
    float m1y = 40.0f + cosf(t_sec * 1.7f) * 25.0f;
    float m2x = W160 / 2.0f + cosf(t_sec * 1.9f) * 35.0f;
    float m2y = 40.0f + sinf(t_sec * 2.5f) * 20.0f;
    float m3x = W160 / 2.0f + sinf(t_sec * 1.1f) * 55.0f;
    float m3y = 40.0f + cosf(t_sec * 1.3f) * 15.0f;

    for (int y = 0; y < metaballs_limit; y++) {
        float fy = y;
        for (int x = 0; x < W160; x++) {
            float fx = x;

            float d1 = (fx - m1x)*(fx - m1x) + (fy - m1y)*(fy - m1y) + 40.0f;
            float d2 = (fx - m2x)*(fx - m2x) + (fy - m2y)*(fy - m2y) + 40.0f;
            float d3 = (fx - m3x)*(fx - m3x) + (fy - m3y)*(fy - m3y) + 40.0f;

            float intensity = (2000.0f / d1) + (2000.0f / d2) + (2000.0f / d3);

            if (intensity > 1.0f) {
                int r = (int)(255 * (intensity - 1.0f));
                int g = (int)(180 * (intensity - 1.0f));
                int b = (int)(220 * (intensity - 1.0f));

                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;

                // Glowing outline
                if (intensity > 1.0f && intensity < 1.15f) {
                    fb160[y * W160 + x] = rgb565_pack(0, 220, 255);
                } else {
                    fb160[y * W160 + x] = rgb565_pack(r, g, b);
                }
            }
        }
    }

    // 2. DRAW GREETINGS BACKGROUND ON BOTTOM PART (fb320)
    memcpy(fb320, asset_greetz_bg_data, ASSET_GREETZ_BG_SIZE);

    // 3. DRAW HORIZONTAL SCROLLING TEXT
    const char *greet_msg = "   ... DIRTY MINDSET ... DEDICATED TO OPTIMUS OF DIRTY MINDS ... GREETINGS TO ALL CODELVERS: DIRTY MINDS ... NASTY BUGS ... ANUBIS ... DETOUR ... KEFRENS ... LOGON SYSTEM ... DUAL ARM CORTEX-M33 PUSHED TO THE LIMIT ... THE MACHINE DREAMS IN CODE ...   ";
    int msg_len = strlen(greet_msg);
    int char_w = 8 * 2;
    int text_scrolled = (int)(t_ms * 0.08f);
    int char_offset = text_scrolled / char_w;
    int pixel_shift = text_scrolled % char_w;

    int draw_y = 190;
    for (int col = 0; col < (VGA_320_W / char_w) + 2; col++) {
        int char_idx = (char_offset + col) % msg_len;
        char c = greet_msg[char_idx];
        int draw_x = col * char_w - pixel_shift;
        draw_scroller_char(fb320, c, draw_x, draw_y, 255);
    }
}

void fx_greetings_done(void)
{
}

const effect_t fx_greetings = {
    "Greetings",
    MODE_SPLIT_160_OVER_320,
    fx_greetings_init,
    fx_greetings_frame,
    fx_greetings_done
};
