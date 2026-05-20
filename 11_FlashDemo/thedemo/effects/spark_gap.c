/* VOLTAGE demoscene - Scene 1: Spark-Gap (Intro Grid)
 *
 * Mode: MODE_320 (320x240 palettized, 8bpp)
 * Visuals: Title backdrop, 3D scrolling neon-perspective grid with sub-pixel 
 *          antialiased screen-space vertical and horizontal grid lines, 
 *          beat-pulsed giant vector logo with horizontal scanline glitch offsets, 
 *          and spark particles.
 */

#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "../font8x8.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define GRID_START_Y 130
#define NUM_SPARKS   48

typedef struct {
    float x, y;
    float vx, vy;
    int life;
    uint8_t color;
} Spark;

/* Reuse scene scratch for the spark list to save SRAM */
static Spark *sparks = (Spark *)&g_scratch.bg_cache[0];

void fx_spark_gap_init(void)
{
    /* Copy the title background palette to the VGA backend */
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_title_bg_pal[i];
        /* Unpack Pimoroni custom BGR555 layout: Blue in bits 15..11, Green in bits 10..6, Red in bits 4..0 */
        uint8_t b = ((c >> 11) & 0x1F) << 3;
        uint8_t g = ((c >> 6) & 0x1F) << 3;
        uint8_t r = (c & 0x1F) << 3;
        vga_320_palette_set(i, r, g, b);
    }

    /* Set up custom neon palette slots in the top indices (224-255)
     * Slot 224..239: Cyan gradient (dark blue-cyan to electric white)
     * Slot 240..255: Magenta gradient (dark violet to hot pink) */
    for (int i = 0; i < 16; i++) {
        float t = i / 15.0f;
        // Cyan
        uint8_t r1 = (uint8_t)(0   * (1.0f - t) + 200 * t);
        uint8_t g1 = (uint8_t)(150 * (1.0f - t) + 255 * t);
        uint8_t b1 = (uint8_t)(255 * (1.0f - t) + 255 * t);
        vga_320_palette_set(224 + i, r1, g1, b1);

        // Magenta
        uint8_t r2 = (uint8_t)(120 * (1.0f - t) + 255 * t);
        uint8_t g2 = (uint8_t)(0   * (1.0f - t) + 150 * t);
        uint8_t b2 = (uint8_t)(200 * (1.0f - t) + 255 * t);
        vga_320_palette_set(240 + i, r2, g2, b2);
    }

    /* Initialize sparks */
    for (int i = 0; i < NUM_SPARKS; i++) {
        sparks[i].life = 0;
    }
}

static void draw_scaled_char(uint8_t *fb, char c, int x0, int y0, int scale, uint8_t color, int glitch_offset)
{
    const uint8_t *glyph = font8x8_glyph(c);
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            /* Check bits MSB-first to avoid character mirroring */
            if (row & (1 << (7 - gx))) {
                /* Draw a block of scale x scale */
                for (int sy = 0; sy < scale; sy++) {
                    int py = y0 + gy * scale + sy;
                    if (py < 0 || py >= VGA_320_H) continue;
                    
                    /* Apply scanline glitch offset to rows */
                    int px_glitch = glitch_offset;
                    for (int sx = 0; sx < scale; sx++) {
                        int px = x0 + gx * scale + sx + px_glitch;
                        if (px < 0 || px >= VGA_320_W) continue;
                        fb[py * VGA_320_W + px] = color;
                    }
                }
            }
        }
    }
}

static void draw_voltage_logo(uint8_t *fb, int cx, int cy, int scale, uint8_t color, int glitch_factor)
{
    const char *text = "VOLTAGE";
    int len = 7;
    int char_w = 8 * scale;
    int total_w = len * char_w + (len - 1) * 2; /* 2px gap between characters */
    int start_x = cx - total_w / 2;

    for (int i = 0; i < len; i++) {
        int glitch_offset = 0;
        if (glitch_factor > 0 && (rand() % 100) < glitch_factor) {
            glitch_offset = (rand() % 17) - 8;
        }

        /* Draw character outline in dark magenta first for depth */
        int x = start_x + i * (char_w + 2);
        draw_scaled_char(fb, text[i], x - 2, cy - 2, scale, 241, glitch_offset);
        draw_scaled_char(fb, text[i], x + 2, cy - 2, scale, 241, glitch_offset);
        draw_scaled_char(fb, text[i], x - 2, cy + 2, scale, 241, glitch_offset);
        draw_scaled_char(fb, text[i], x + 2, cy + 2, scale, 241, glitch_offset);

        /* Draw main character face */
        draw_scaled_char(fb, text[i], x, cy, scale, color, glitch_offset);
    }
}

void fx_spark_gap_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();

    /* Draw background backdrop */
    memcpy(fb, asset_title_bg_data, ASSET_TITLE_BG_SIZE);

    /* Fade-in / Fade-out logic */
    float fade = 1.0f;
    if (t_ms < 1500) {
        fade = t_ms / 1500.0f;
    } else if (t_ms > 13500) {
        fade = (15000 - t_ms) / 1500.0f;
        if (fade < 0.0f) fade = 0.0f;
    }

    /* Simulate periodic high-voltage pulses synced with beat-drops (approx. every 920 ms at 130 BPM) */
    uint32_t beat_interval = 923; 
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.005f); /* Sharp attack, slow decay */
    
    /* Modify palette dynamically for high-voltage strobe flash on beat-drop */
    if (beat_pulse > 0.05f) {
        float st = beat_pulse * fade;
        /* Flash the backdrop palette elements at runtime */
        for (int i = 0; i < 32; i++) {
            uint16_t c = asset_title_bg_pal[i];
            uint8_t r = ((c >> 11) & 0x1F) << 3;
            uint8_t g = ((c >> 5) & 0x3F) << 2;
            uint8_t b = (c & 0x1F) << 3;

            /* Add strobe flash effect */
            r = (uint8_t)(r * (1.0f - st) + 255 * st);
            g = (uint8_t)(g * (1.0f - st) + 255 * st);
            b = (uint8_t)(b * (1.0f - st) + 255 * st);
            vga_320_palette_set(i, r, g, b);
        }
    }

    /* Render the 3D-Perspective Scrolling Cyber Grid with Screen-Space Antialiasing */
    float scroll = t_ms * 0.006f;
    for (int y = GRID_START_Y; y < VGA_320_H; y++) {
        float dy = y - GRID_START_Y + 1.0f;
        float z = 100.0f / dy;  /* 3D Depth */

        /* Horizontal Grid Lines (projected in perspective space) */
        float hz_spacing = 16.0f;
        float h_val = fmodf(z * 4.0f - scroll, hz_spacing);
        if (h_val < 0.0f) h_val += hz_spacing;
        
        /* Distance to nearest horizontal line center */
        float dist_h = fminf(h_val, hz_spacing - h_val);
        float line_w_h = 0.8f;
        float int_h = 0.0f;
        if (dist_h < line_w_h) {
            int_h = 1.0f - dist_h / line_w_h;
        }

        /* Soft fade-out for vertical lines as they merge into the vanishing center */
        float horizon_fade = (dy - 1.0f) / 10.0f;
        if (horizon_fade > 1.0f) horizon_fade = 1.0f;
        if (horizon_fade < 0.0f) horizon_fade = 0.0f;

        /* Depth Fog index (ranges from 0 in far distance to 15 in foreground) */
        int col_idx = (int)(dy * 0.15f);
        if (col_idx > 15) col_idx = 15;

        /* Vertical Lines Constant-Width Screen-Space Antialiasing */
        float step = 0.25f * dy;

        for (int x = 0; x < VGA_320_W; x++) {
            /* Constant O(1) screen-space nearest vertical line resolution */
            float dist_v = 100.0f;
            if (step > 0.05f) {
                float k_exact = (x - 160.0f) / step;
                int k = (int)roundf(k_exact);
                float nearest_x_center = 160.0f + k * step;
                dist_v = fabsf(x - nearest_x_center);
            }

            float line_w_v = 1.0f; /* 1.0 pixel screen-space radius (2px total width) */
            float int_v = 0.0f;
            if (dist_v < line_w_v) {
                int_v = (1.0f - dist_v / line_w_v) * horizon_fade;
            }

            /* Combine line intensities for master antialiasing */
            float intensity = fmaxf(int_h, int_v);
            if (intensity > 0.05f) {
                /* Scale fog color index by the line edge intensity */
                int brightness = (int)(col_idx * intensity);
                fb[y * VGA_320_W + x] = 224 + brightness;
            }
        }
    }

    /* Draw glowing neon horizon separator line to smooth the transition perfectly */
    memset(&fb[GRID_START_Y * VGA_320_W], 239, VGA_320_W);

    /* Manage and draw sparks */
    for (int i = 0; i < NUM_SPARKS; i++) {
        Spark *s = &sparks[i];
        if (s->life <= 0) {
            /* Spawn new spark at the logo bounding box on beat drop or randomly */
            if (beat_pulse > 0.6f && (rand() % 10) < 3) {
                s->x = 160.0f + (rand() % 180 - 90);
                s->y = 80.0f + (rand() % 30 - 15);
                float angle = (rand() % 360) * (3.14159f / 180.0f);
                float speed = (rand() % 300) / 100.0f + 1.5f;
                s->vx = cosf(angle) * speed;
                s->vy = sinf(angle) * speed + 0.5f; /* Gravity drift down */
                s->life = rand() % 30 + 15;
                s->color = 240 + (rand() % 16); /* Neon magenta/pink */
            }
        } else {
            /* Update particle */
            s->x += s->vx;
            s->y += s->vy;
            s->vy += 0.05f; /* Gravity */
            s->life--;

            /* Draw particle */
            int px = (int)s->x;
            int py = (int)s->y;
            if (px >= 0 && px < VGA_320_W && py >= 0 && py < VGA_320_H) {
                fb[py * VGA_320_W + px] = s->color;
                // Add a tail
                int pprev_x = (int)(s->x - s->vx * 0.7f);
                int pprev_y = (int)(s->y - s->vy * 0.7f);
                if (pprev_x >= 0 && pprev_x < VGA_320_W && pprev_y >= 0 && pprev_y < VGA_320_H) {
                    fb[pprev_y * VGA_320_W + pprev_x] = 224 + (s->life % 16); /* Cyan tail */
                }
            }
        }
    }

    /* Render "VOLTAGE" giant logo */
    int scale = 4;
    /* Pulsate logo scale in sync with beat */
    if (beat_pulse > 0.0f) {
        scale = 4 + (int)(beat_pulse * 0.8f);
    }
    
    /* Calculate glitching factor: high-voltage glitch bursts during beats and randomly */
    int glitch_factor = 2;
    if (beat_pulse > 0.7f) {
        glitch_factor = 25; /* Massive horizontal scanline tearing */
    }

    int logo_y = 70 - (scale - 4) * 4;
    uint8_t logo_col = 239; /* Glowing electric white-cyan at top of gradient */
    if (beat_pulse > 0.8f) {
        logo_col = 255; /* White-hot pink on beat drops */
    }
    
    draw_voltage_logo(fb, 160, logo_y, scale, logo_col, glitch_factor);

    /* Fade-in/out black overlay mask if needed */
    if (fade < 1.0f) {
        int threshold = (int)(256.0f * (1.0f - fade));
        for (int y = 0; y < VGA_320_H; y++) {
            for (int x = 0; x < VGA_320_W; x++) {
                /* Fake dither fade to black */
                if (((x ^ y) & 0xFF) < threshold) {
                    fb[y * VGA_320_W + x] = 0; /* Black */
                }
            }
        }
    }
}

void fx_spark_gap_done(void)
{
    /* Clean up any scene-specific resources */
}

/* Register standard interface struct */
const effect_t fx_spark_gap = {
    "Spark-Gap Grid",
    MODE_320,
    fx_spark_gap_init,
    fx_spark_gap_frame,
    fx_spark_gap_done
};
