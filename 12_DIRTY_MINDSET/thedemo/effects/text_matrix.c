#include "../scene.h"
#include "../vga.h"
#include "../font8x8.h"
#include "../scene_scratch.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define COLS 40
#define ROWS 30

static const char *matrix_messages[] = {
    "AI TOOLING IN THE DEMOSCENE",
    "I DON'T BELIEVE IT",
    "THE MACHINE DREAMS IN SILICON",
    "DEAR OPTIMUS",
    "DIRTY MINDS FOREVER",
    "10 PRINT CHR$(205.5+RND(1));:GOTO 10",
    "POKE 53280,0",
    "THIS CODE IS REAL",
    "ALL YOUR BASE ARE BELONG TO US",
    "HELLO OPTIMUS",
    "MODE 0: BORDER 1: INK 0,1",
    "MEMORY &3FFF: LOAD \"DM\",&4000",
    "CALL &4000: REM RUN DEMO",
    "GEMINI FLASH SAYS HI",
    "OUTSTANDING RP2350 POWER",
    "THE DEBATE CONTINUES..."
};

// Custom drawing for matrix characters
static void draw_matrix_char(uint8_t *fb, char c, int x0, int y0, uint8_t color)
{
    const uint8_t *glyph = font8x8_glyph(c);
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        int py = y0 + gy;
        if (py < 0 || py >= VGA_320_H) continue;
        for (int gx = 0; gx < 8; gx++) {
            if (row & (1 << (7 - gx))) {
                int px = x0 + gx;
                if (px < 0 || px >= VGA_320_W) continue;
                fb[py * VGA_320_W + px] = color;
            }
        }
    }
}

void fx_text_matrix_init(void)
{
    // Build a nice retro-green/cyan neon palette for matrix rain
    vga_320_palette_set(0, 0, 4, 12); // Very dark blue/black background
    
    // Green to cyan gradient (1 to 15)
    for (int i = 0; i < 15; i++) {
        float t = i / 14.0f;
        uint8_t r = 0;
        uint8_t g = (uint8_t)(40 + 215 * t);
        uint8_t b = (uint8_t)(10 + 120 * t);
        vga_320_palette_set(1 + i, r, g, b);
    }
    
    // Index 16: Neon Cyan (head color)
    vga_320_palette_set(16, 180, 255, 255);
    // Index 17: Pure white (bright flash head)
    vga_320_palette_set(17, 255, 255, 255);
    
    // Initialize columns
    for (int i = 0; i < COLS; i++) {
        g_scratch.matrix.column_y[i] = (int16_t)(rand() % 240);
        g_scratch.matrix.column_speed[i] = (rand() % 3) + 2;
        g_scratch.matrix.text_offsets[i] = rand() % 16;
    }
}

void fx_text_matrix_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();
    
    // Determine fade / glitch progress
    float global_fade = 1.0f;
    uint32_t duration = 10867; // 49156 - 38289
    int is_glitching = 0;
    float glitch_progress = 0.0f;
    
    if (t_ms < 1000) {
        global_fade = t_ms / 1000.0f;
    } else if (t_ms >= duration - 1000) {
        glitch_progress = (t_ms - (duration - 1000)) / 1000.0f;
        global_fade = 1.0f - glitch_progress;
        is_glitching = 1;
    }
    
    // Update palette dynamically based on the fade factor
    vga_320_palette_set(0, 0, (uint8_t)(4 * global_fade), (uint8_t)(12 * global_fade));
    for (int i = 0; i < 15; i++) {
        float t_val = i / 14.0f;
        uint8_t r = 0;
        uint8_t g = (uint8_t)((40 + 215 * t_val) * global_fade);
        uint8_t b = (uint8_t)((10 + 120 * t_val) * global_fade);
        vga_320_palette_set(1 + i, r, g, b);
    }
    vga_320_palette_set(16, (uint8_t)(180 * global_fade), (uint8_t)(255 * global_fade), (uint8_t)(255 * global_fade));
    vga_320_palette_set(17, (uint8_t)(255 * global_fade), (uint8_t)(255 * global_fade), (uint8_t)(255 * global_fade));

    // Clear screen to background color 0
    memset(fb, 0, VGA_320_W * VGA_320_H);
    
    // Render columns
    for (int col = 0; col < COLS; col++) {
        // Update column position using signed arithmetic
        int16_t y_pos = g_scratch.matrix.column_y[col];
        y_pos += g_scratch.matrix.column_speed[col];
        if (y_pos >= VGA_320_H) {
            y_pos = -8; // Signed -8 does not wrap to 248 anymore
            g_scratch.matrix.column_speed[col] = (rand() % 3) + 2;
            g_scratch.matrix.text_offsets[col] = rand() % 16;
        }
        g_scratch.matrix.column_y[col] = y_pos;
        
        int text_idx = g_scratch.matrix.text_offsets[col];
        const char *msg = matrix_messages[text_idx];
        int msg_len = strlen(msg);
        
        // The column head character is at (col * 8, y_pos)
        // We draw trailing characters upwards
        for (int r = 0; r < ROWS; r++) {
            int char_y = r * 8;
            if (char_y > y_pos) continue; // ahead of the rain
            
            int dist_from_head = (y_pos - char_y) / 8;
            if (dist_from_head > 18) continue; // out of trail length
            
            // Pick character from message
            char c = msg[(r + col) % msg_len];
            
            // Set trail brightness based on distance from head
            uint8_t color = 0;
            if (dist_from_head == 0) {
                color = 17; // Bright White head
            } else if (dist_from_head == 1) {
                color = 16; // Neon Cyan
            } else {
                // Fade from cyan-green to dark blue
                int strength = 15 - dist_from_head;
                if (strength < 1) strength = 1;
                color = strength;
            }
            
            draw_matrix_char(fb, c, col * 8, char_y, color);
        }
    }

    // Apply digital glitch scanline jitter post-processing
    if (is_glitching) {
        uint8_t temp_row[VGA_320_W];
        for (int y = 0; y < VGA_320_H; y++) {
            // Check for row blackout dropouts
            float blackout_prob = glitch_progress * 0.4f;
            float rand_val = (float)(rand() % 1000) / 1000.0f;
            if (rand_val < blackout_prob) {
                memset(&fb[y * VGA_320_W], 0, VGA_320_W);
                continue;
            }
            
            int offset = 0;
            float tear_val = (float)(rand() % 1000) / 1000.0f;
            if (tear_val < 0.15f * glitch_progress + 0.05f) {
                offset = (int)(sinf(y * 0.2f + t_ms * 0.05f) * 30.0f * glitch_progress);
            } else if (tear_val < 0.5f) {
                offset = (rand() % 5) - 2;
            }
            
            if (offset != 0) {
                memcpy(temp_row, &fb[y * VGA_320_W], VGA_320_W);
                for (int x = 0; x < VGA_320_W; x++) {
                    int src_x = (x + offset) % VGA_320_W;
                    if (src_x < 0) src_x += VGA_320_W;
                    fb[y * VGA_320_W + x] = temp_row[src_x];
                }
            }
            
            // Introduce occasional bright white dropout lines
            float line_prob = (float)(rand() % 1000) / 1000.0f;
            if (line_prob < 0.02f * glitch_progress) {
                memset(&fb[y * VGA_320_W], 17, VGA_320_W);
            }
        }
    }
}

void fx_text_matrix_done(void)
{
}

const effect_t fx_text_matrix = {
    "Text Matrix",
    MODE_320,
    fx_text_matrix_init,
    fx_text_matrix_frame,
    fx_text_matrix_done
};
