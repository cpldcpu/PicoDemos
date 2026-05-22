#include "../scene.h"
#include "../vga.h"
#include "../font8x8.h"
#include "../rgb565.h"
#include "../scene_scratch.h"
#include "assets.h"
#include <string.h>
#include <stdlib.h>

// Character rendering helper with 1x scale
static void cpc_draw_char(uint8_t *fb, char c, int x0, int y0, uint8_t fg, uint8_t bg)
{
    const uint8_t *glyph = font8x8_glyph(c);
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        int py = y0 + gy;
        if (py < 0 || py >= VGA_320_H) continue;
        for (int gx = 0; gx < 8; gx++) {
            int px = x0 + gx;
            if (px < 0 || px >= VGA_320_W) continue;
            fb[py * VGA_320_W + px] = (row & (1 << (7 - gx))) ? fg : bg;
        }
    }
}

// Draw a solid cursor block
static void cpc_draw_block(uint8_t *fb, int x0, int y0, uint8_t fg)
{
    for (int gy = 0; gy < 8; gy++) {
        int py = y0 + gy;
        if (py < 0 || py >= VGA_320_H) continue;
        for (int gx = 0; gx < 8; gx++) {
            int px = x0 + gx;
            if (px < 0 || px >= VGA_320_W) continue;
            fb[py * VGA_320_W + px] = fg;
        }
    }
}

static void cpc_draw_string(uint8_t *fb, const char *str, int x, int y, uint8_t fg, uint8_t bg)
{
    while (*str) {
        cpc_draw_char(fb, *str, x, y, fg, bg);
        x += 8;
        str++;
    }
}

void fx_cpc_boot_init(void)
{
    // Load asset palette
    for (int i = 0; i < 256; i++) {
        uint16_t col = asset_title_bg_pal[i];
        vga_320_palette_set(i, rgb565_r8(col), rgb565_g8(col), rgb565_b8(col));
    }
    // Overwrite the last few entries for CPC Boot overlay window
    vga_320_palette_set(253, 0, 0, 128);     // CPC Deep Blue (window background)
    vga_320_palette_set(254, 255, 255, 0);   // CPC Yellow (text)
    vga_320_palette_set(255, 255, 255, 255); // White (border)
}

// Sequence entries
static const char *lines[] = {
    "Amstrad CPC 6128 - 128K RAM",
    "DIRTY MINDS (c) 2026",
    "",
    "Loading MINDSET.BAS...",
    "Ready",
    "",
    "10 REM * DIRTY MINDSET *",
    "20 REM For Optimus",
    "30 REM \"You don't believe\"",
    "40 REM \"I wrote this?\"",
    "50 RUN"
};

void fx_cpc_boot_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();
    uint8_t *temp_fb = g_scratch.bg_cache;
    
    // Copy background bitmap image
    memcpy(temp_fb, asset_title_bg_data, ASSET_TITLE_BG_SIZE);
    
    // Draw retro CPC-blue terminal window in the center overlaying background
    int win_x0 = 40, win_x1 = 280;
    int win_y0 = 40, win_y1 = 200;
    for (int y = win_y0; y < win_y1; y++) {
        for (int x = win_x0; x < win_x1; x++) {
            if (y < win_y0 + 2 || y >= win_y1 - 2 || x < win_x0 + 2 || x >= win_x1 - 2) {
                temp_fb[y * VGA_320_W + x] = 255; // White border
            } else {
                temp_fb[y * VGA_320_W + x] = 253; // Blue fill
            }
        }
    }
    
    // Animate typing out lines
    int start_x = 48;
    int start_y = 48;
    int chars_per_sec = 30;
    int total_chars_to_show = (t_ms * chars_per_sec) / 1000;
    
    int chars_drawn = 0;
    for (int l = 0; l < 11; l++) {
        const char *line = lines[l];
        int len = strlen(line);
        if (total_chars_to_show >= chars_drawn + len) {
            // Draw full line
            cpc_draw_string(temp_fb, line, start_x, start_y + l * 12, 254, 253); // Yellow text (254)
            chars_drawn += len;
        } else {
            // Draw partial line
            int show = total_chars_to_show - chars_drawn;
            if (show > 0) {
                char temp[64];
                strncpy(temp, line, show);
                temp[show] = '\0';
                cpc_draw_string(temp_fb, temp, start_x, start_y + l * 12, 254, 253);
            }
            // Draw blinking block cursor
            if ((t_ms / 250) % 2) {
                cpc_draw_block(temp_fb, start_x + show * 8, start_y + l * 12, 254);
            }
            break;
        }
    }
    
    // If all lines finished typing, blink cursor at the end of the last line
    if (total_chars_to_show >= 200) {
        if ((t_ms / 250) % 2) {
            cpc_draw_block(temp_fb, start_x + strlen(lines[10]) * 8, start_y + 10 * 12, 254);
        }
    }
    
    // CRT Screen Collapse transition (last 1000 ms of the scene)
    uint32_t scene_duration = 17043;
    if (t_ms >= scene_duration - 1000) {
        uint32_t dt = t_ms - (scene_duration - 1000);
        float f = dt / 1000.0f; // 0.0 to 1.0
        
        // Clear output framebuffer to black
        memset(fb, 0, VGA_320_W * VGA_320_H);
        
        if (f <= 0.5f) {
            // Phase 1: vertical collapse
            float h_scale = 1.0f - (f * 2.0f); // goes 1.0 to 0.0
            int H = (int)(240 * h_scale);
            if (H < 2) H = 2; // Keep at least 2 pixels tall
            
            int y_start = 120 - H / 2;
            for (int y = 0; y < H; y++) {
                int py = y_start + y;
                if (py < 0 || py >= 240) continue;
                int src_y = (y * 240) / H;
                if (src_y < 0) src_y = 0;
                if (src_y >= 240) src_y = 239;
                
                memcpy(&fb[py * VGA_320_W], &temp_fb[src_y * VGA_320_W], VGA_320_W);
            }
        } else {
            // Phase 2: horizontal collapse
            float f_horiz = (f - 0.5f) * 2.0f; // goes 0.0 to 1.0
            float w_scale = 1.0f - f_horiz;
            int W = (int)(320 * w_scale);
            
            int x_start = 160 - W / 2;
            uint8_t line_color = 255; // Bright White
            
            if (W > 0) {
                for (int y = 119; y <= 120; y++) {
                    for (int x = 0; x < W; x++) {
                        int px = x_start + x;
                        if (px >= 0 && px < 320) {
                            fb[y * VGA_320_W + px] = line_color;
                        }
                    }
                }
            }
        }
    } else {
        // Normal display: copy from temp background cache
        memcpy(fb, temp_fb, VGA_320_W * VGA_320_H);
    }
}

void fx_cpc_boot_done(void)
{
}

const effect_t fx_cpc_boot = {
    "CPC Boot",
    MODE_320,
    fx_cpc_boot_init,
    fx_cpc_boot_frame,
    fx_cpc_boot_done
};
