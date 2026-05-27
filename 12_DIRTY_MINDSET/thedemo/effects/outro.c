#include "../scene.h"
#include "../vga.h"
#include "../font8x8.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PARTICLES 100

typedef struct {
    float x;
    float y;
    float speed;
    uint8_t color;
} particle_t;

static particle_t particles[MAX_PARTICLES];
static int particles_init = 0;
static uint8_t dark_lut[256];

static void draw_scaled_char(uint8_t *fb, char c, int x0, int y0, int scale, uint8_t color)
{
    const uint8_t *glyph = font8x8_glyph(c);
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            if (row & (1 << (7 - gx))) {
                for (int sy = 0; sy < scale; sy++) {
                    int py = y0 + gy * scale + sy;
                    if (py < 20 || py >= 220) continue; // Masked vertically
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x0 + gx * scale + sx;
                        if (px < 30 || px >= 290) continue; // Masked horizontally
                        fb[py * VGA_320_W + px] = color;
                    }
                }
            }
        }
    }
}

static void draw_string(uint8_t *fb, const char *str, int x, int y, int scale, uint8_t color)
{
    while (*str) {
        draw_scaled_char(fb, *str, x, y, scale, color);
        x += 8 * scale;
        str++;
    }
}

void fx_outro_init(void)
{
    // Initialize palette with endcard background palette
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_endcard_bg_pal[i];
        uint8_t b = ((c >> 11) & 0x1F) << 3;
        uint8_t g = ((c >> 6) & 0x1F) << 3;
        uint8_t r = (c & 0x1F) << 3;
        vga_320_palette_set(i, r, g, b);
    }
    
    // Set up top palette slots for credits text
    vga_320_palette_set(253, 255, 255, 255); // White
    vga_320_palette_set(254, 0, 255, 128);   // Glowing green/mint
    vga_320_palette_set(255, 255, 100, 200); // Glowing pink/neon
    
    // Precalculate dark shading LUT using Euclidean distance search
    // to map each color index to its closest 35% shaded counterpart
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_endcard_bg_pal[i];
        uint8_t orig_b = ((c >> 11) & 0x1F) << 3;
        uint8_t orig_g = ((c >> 6) & 0x1F) << 3;
        uint8_t orig_r = (c & 0x1F) << 3;
        
        int target_r = (int)(orig_r * 0.35f);
        int target_g = (int)(orig_g * 0.35f);
        int target_b = (int)(orig_b * 0.35f);
        
        int best_idx = 0;
        int min_dist = 1000000;
        for (int j = 0; j < 253; j++) {
            uint16_t c2 = asset_endcard_bg_pal[j];
            uint8_t j_b = ((c2 >> 11) & 0x1F) << 3;
            uint8_t j_g = ((c2 >> 6) & 0x1F) << 3;
            uint8_t j_r = (c2 & 0x1F) << 3;
            
            int dr = j_r - target_r;
            int dg = j_g - target_g;
            int db = j_b - target_b;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < min_dist) {
                min_dist = dist;
                best_idx = j;
            }
        }
        dark_lut[i] = best_idx;
    }
    
    // Initialize particles deterministically
    srand(42);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].x = (float)(rand() % VGA_320_W);
        particles[i].y = (float)(rand() % VGA_320_H);
        particles[i].speed = 0.2f + (rand() % 100) * 0.008f; // Slow rising speed
        
        int col_choice = rand() % 3;
        if (col_choice == 0) particles[i].color = 253; // White
        else if (col_choice == 1) particles[i].color = 254; // Green
        else particles[i].color = 255; // Pink
    }
    particles_init = 1;
}

static const char *credits_lines[] = {
    "DIRTY MINDSET - CREDITS",
    "-----------------------",
    "",
    "DIRECTION",
    "OPUS 4.6 (WHO DESIGNED THE PLAN)",
    "",
    "CRITIC",
    "AZURE (THE HUMAN)",
    "",
    "ART",
    "NANO BANANA",
    "",
    "CODE",
    "GEMINI 3.5 FLASH",
    "",
    "MUSIC",
    "SUNO 4.5 (PROMPTED BY OPUS 4.6)",
    "",
    "FOR",
    "OPTIMUS & DIRTY MINDS",
    "",
    "\"THE MACHINE DREAMS IN CODE\"",
    "",
    "READY."
};

void fx_outro_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();
    float t_sec = t_ms * 0.001f;

    // Calculate global fade factor (fade-out to black in the final 3000 ms of the 38323 ms duration)
    float fade = 1.0f;
    if (t_ms > 35323) {
        fade = 1.0f - (float)(t_ms - 35323) / 3000.0f;
        if (fade < 0.0f) fade = 0.0f;
    }

    // Update palette every frame based on the fade factor
    for (int i = 0; i < 256; i++) {
        uint8_t r, g, b;
        if (i < 253) {
            uint16_t c = asset_endcard_bg_pal[i];
            b = ((c >> 11) & 0x1F) << 3;
            g = ((c >> 6) & 0x1F) << 3;
            r = (c & 0x1F) << 3;
        } else if (i == 253) {
            r = 255; g = 255; b = 255;
        } else if (i == 254) {
            r = 0; g = 255; b = 128;
        } else {
            r = 255; g = 100; b = 200;
        }
        vga_320_palette_set(i, (uint8_t)(r * fade), (uint8_t)(g * fade), (uint8_t)(b * fade));
    }

    // Fixed-point Rotozoomer background
    float angle = t_sec * 0.25f;
    float zoom_val = (sinf(t_sec * 0.4f) + 1.5f) * 0.8f;
    float cos_val = cosf(angle);
    float sin_val = sinf(angle);

    int fixed_du = (int)(cos_val * zoom_val * 65536.0f);
    int fixed_dv = (int)(sin_val * zoom_val * 65536.0f);

    for (int y = 0; y < VGA_320_H; y++) {
        float dy = y - 120.0f;
        float dx0 = -160.0f;

        float u = (dx0 * cos_val - dy * sin_val) * zoom_val;
        float v = (dx0 * sin_val + dy * cos_val) * zoom_val;

        int fixed_u = (int)(u * 65536.0f);
        int fixed_v = (int)(v * 65536.0f);

        for (int x = 0; x < VGA_320_W; x++) {
            // Offset coordinates to make texture rotation center at (160, 120)
            int tx = ((fixed_u >> 16) + 160) % 320;
            if (tx < 0) tx += 320;
            int ty = ((fixed_v >> 16) + 120) % 240;
            if (ty < 0) ty += 240;

            fb[y * VGA_320_W + x] = asset_endcard_bg_data[ty * 320 + tx];

            fixed_u += fixed_du;
            fixed_v += fixed_dv;
        }
    }

    // Overlay credits background rectangle (beautiful dark translucency overlay using precalculated LUT)
    for (int y = 20; y < 220; y++) {
        for (int x = 30; x < 290; x++) {
            uint8_t bg = fb[y * VGA_320_W + x];
            fb[y * VGA_320_W + x] = dark_lut[bg];
        }
    }

    // Draw drifting/rising pixel dust particles
    for (int i = 0; i < MAX_PARTICLES; i++) {
        float py = particles[i].y - t_sec * particles[i].speed * 30.0f;
        int ipy = (int)py % VGA_320_H;
        if (ipy < 0) ipy += VGA_320_H;
        int ipx = (int)particles[i].x;

        // Render particle only if inside the text box masking window to look integrated
        if (ipx >= 30 && ipx < 290 && ipy >= 20 && ipy < 220) {
            fb[ipy * VGA_320_W + ipx] = particles[i].color;
        }
    }

    // Credits vertical rising upscroller
    // At t_ms = 0, first line starts at y = 240
    // Scrolling rate: 12 pixels per second
    int scroll_y = 240 - (int)(t_ms * 0.012f);

    for (int i = 0; i < 24; i++) {
        const char *line = credits_lines[i];
        if (strlen(line) == 0) continue;

        int y_pos = scroll_y + i * 12;
        // Optimization: Skip rendering if completely out of the masked window
        if (y_pos < 12 || y_pos >= 220) continue;

        // Horizontally centered
        int x_pos = 160 - (strlen(line) * 8) / 2;

        // Determine color
        uint8_t color = 253; // Default White
        if (i == 0 || i == 1) {
            color = 255; // Title / divider in pink
        } else if (i == 3 || i == 6 || i == 9 || i == 12 || i == 15 || i == 18) {
            color = 254; // Headers/labels in mint green
        } else if (i == 21 || i == 23) {
            color = 255; // Slogans and prompt in pink
        }

        draw_string(fb, line, x_pos, y_pos, 1, color);
    }
}

void fx_outro_done(void)
{
}

const effect_t fx_outro = {
    "Outro",
    MODE_320,
    fx_outro_init,
    fx_outro_frame,
    fx_outro_done
};
