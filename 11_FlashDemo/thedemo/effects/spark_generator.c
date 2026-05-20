/* VOLTAGE demoscene - Scene 5: Spark Generator (Lightning Polar Tunnel)
 *
 * Mode: MODE_320 (320x240 chunky 8bpp)
 * Visuals: 3D depth-shaded scrolling polar tunnel with real-time branching 
 *          electric lightning bolts striking out from the center, beat-synced 
 *          strobe flashing, and high-speed camera roll.
 */

#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define LUT_W SCRATCH_TUNNEL_W
#define LUT_H SCRATCH_TUNNEL_H

/* Fast Bresenham line drawing for 8bpp palettized buffer */
static void draw_line_320(uint8_t *fb, int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        if (x0 >= 0 && x0 < VGA_320_W && y0 >= 0 && y0 < VGA_320_H) {
            fb[y0 * VGA_320_W + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fx_spark_generator_init(void)
{
    /* 1. BUILD MULTI-BAND SHADING PALETTE (0..7 depth bands x 32 base colors = 256 colors)
     * Band 0 is pitch black / dark blue, Band 7 is the original bright color texture. */
    for (int band = 0; band < 8; band++) {
        float f_shade = band / 7.0f;
        
        for (int c = 0; c < 32; c++) {
            uint16_t color16 = asset_tunnel_tex_pal[c];
            /* Unpack Pimoroni custom BGR555 layout: Blue in bits 15..11, Green in bits 10..6, Red in bits 4..0 */
            uint8_t b = ((color16 >> 11) & 0x1F) << 3;
            uint8_t g = ((color16 >> 6) & 0x1F) << 3;
            uint8_t r = (color16 & 0x1F) << 3;

            /* Apply exponential depth-fade shading */
            r = (uint8_t)(r * f_shade);
            g = (uint8_t)(g * f_shade);
            b = (uint8_t)(b * f_shade);

            /* Add slight blue-violet ambient glow in dark distant bands */
            if (band < 4) {
                b += (uint8_t)((4 - band) * 6);
            }

            int idx = (band << 5) | (c & 0x1F);
            vga_320_palette_set(idx, r, g, b);
        }
    }

    /* Keep slot 255 reserved for White-Hot lightning */
    vga_320_palette_set(255, 255, 255, 255);

    /* 2. COMPUTE HALF-RES POLAR TUNNEL LOOKUP TABLES
     * Map screen coordinates to angle (U) and distance (V). Cached in scratchpad union. */
    uint8_t *angle_tbl = g_scratch.tunnel.angle;
    uint8_t *dist_tbl = g_scratch.tunnel.dist;

    for (int y = 0; y < LUT_H; y++) {
        float dy = y - LUT_H / 2.0f;
        for (int x = 0; x < LUT_W; x++) {
            float dx = x - LUT_W / 2.0f;

            /* Angle: atan2 maps to -pi..pi -> 0..255 indices */
            float a = atan2f(dy, dx);
            if (a < 0.0f) a += 2.0f * 3.14159f;
            int angle_idx = (int)((a / (2.0f * 3.14159f)) * 256.0f);

            /* Distance: C / radius. Depth repeats over space */
            float r = sqrtf(dx*dx + dy*dy);
            if (r < 0.5f) r = 0.5f; /* Avoid division by zero */
            int dist_idx = (int)(280.0f / r);

            int idx = y * LUT_W + x;
            angle_tbl[idx] = (uint8_t)(angle_idx & 0xFF);
            dist_tbl[idx]  = (uint8_t)(dist_idx & 0xFF);
        }
    }
}

static void generate_lightning_branch(uint8_t *fb, int x0, int y0, float start_angle, int depth, uint8_t color)
{
    if (depth <= 0) return;

    int length = rand() % 20 + 15;
    float cur_angle = start_angle;
    int cur_x = x0;
    int cur_y = y0;

    for (int i = 0; i < length; i++) {
        /* Jagged step */
        cur_angle += (rand() % 60 - 30) * (3.14159f / 180.0f);
        int next_x = cur_x + (int)(cosf(cur_angle) * 6.0f);
        int next_y = cur_y + (int)(sinf(cur_angle) * 6.0f);

        draw_line_320(fb, cur_x, cur_y, next_x, next_y, color);

        /* Branching probability */
        if (depth > 1 && (rand() % 100) < 12) {
            float branch_angle = cur_angle + ((rand() % 2 == 0) ? 45.0f : -45.0f) * (3.14159f / 180.0f);
            generate_lightning_branch(fb, next_x, next_y, branch_angle, depth - 1, color);
        }

        cur_x = next_x;
        cur_y = next_y;
    }
}

void fx_spark_generator_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();
    uint8_t *angle_tbl = g_scratch.tunnel.angle;
    uint8_t *dist_tbl = g_scratch.tunnel.dist;

    /* Get beat sync for lightning flash triggers and speed */
    uint32_t beat_interval = 923; 
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.005f);

    /* Phase steps for tunnel scroll */
    int scroll_u = (int)(t_ms * 0.04f);              /* Spin */
    int scroll_v = (int)(t_ms * (0.08f + beat_pulse * 0.2f)); /* Rapid forward speed pulse on beat */

    /* Camera twist/roll: Apply time-based offset to angle table lookups */
    int camera_roll = (int)(sinf(t_ms * 0.0015f) * 32.0f);

    /* 1. RENDER TUNNEL (horizontal/vertical pixel-doubling from 160x120 lookup tables) */
    for (int y = 0; y < VGA_320_H; y++) {
        int ly = y >> 1;
        int row_offset = ly * LUT_W;
        int fb_offset = y * VGA_320_W;

        for (int x = 0; x < VGA_320_W; x++) {
            int lx = x >> 1;
            int idx = row_offset + lx;

            uint8_t angle = angle_tbl[idx];
            uint8_t dist  = dist_tbl[idx];

            /* Apply camera roll, rotation scroll, and forward travel */
            uint8_t u = (uint8_t)(angle + scroll_u + camera_roll);
            uint8_t v = (uint8_t)(dist + scroll_v);

            /* Fetch base pixel from 256x256 texture */
            uint8_t tex_pixel = asset_tunnel_tex_data[v * 256 + u];

            /* 8-band depth shading calculation
             * Dist is 0 (near/brightest) to 255 (far/darkest) in the center. 
             * Invert so near = 7 (brightest) and far = 0 (darkest). */
            int shade = 7 - (dist >> 5);
            if (shade < 0) shade = 0;

            /* Zero-cost hardware palette index mapping */
            uint8_t final_color = (uint8_t)((shade << 5) | (tex_pixel & 0x1F));
            fb[fb_offset + x] = final_color;
        }
    }

    /* 2. REAL-TIME ELECTRIC LIGHTNING
     * Emit branching arcs from the center out. */
    if (beat_pulse > 0.4f || (rand() % 100) < 8) {
        /* Flashes center glow core */
        int r_flash = 8 + (int)(beat_pulse * 15.0f);
        for (int y = VGA_320_H/2 - r_flash; y < VGA_320_H/2 + r_flash; y++) {
            for (int x = VGA_320_W/2 - r_flash; x < VGA_320_W/2 + r_flash; x++) {
                if (x >= 0 && x < VGA_320_W && y >= 0 && y < VGA_320_H) {
                    fb[y * VGA_320_W + x] = 255; /* White-Hot lightning color */
                }
            }
        }

        /* Lightning branches shooting outward */
        int num_bolts = (beat_pulse > 0.8f) ? 3 : 1;
        for (int i = 0; i < num_bolts; i++) {
            float start_angle = (rand() % 360) * (3.14159f / 180.0f);
            generate_lightning_branch(fb, VGA_320_W / 2, VGA_320_H / 2, start_angle, 3, 255);
        }
    }

    /* Whole screen strobe flash on heavy beat drop */
    if (beat_pulse > 0.88f) {
        /* Glitch strobe: Invert/XOR alternate horizontal scanlines to white */
        for (int y = 0; y < VGA_320_H; y += 4) {
            memset(&fb[y * VGA_320_W], 255, VGA_320_W);
        }
    }

    /* Dynamic fade-in/out with Vertical Blinds Wipe */
    float fade = 1.0f;
    uint32_t fade_duration = 1500;
    uint32_t scene_duration = 15000;

    if (t_ms < fade_duration) {
        fade = t_ms / (float)fade_duration;
        int blind_width = 16;
        int step = (int)(fade * blind_width);
        for (int y = 0; y < VGA_320_H; y++) {
            for (int x = 0; x < VGA_320_W; x++) {
                if ((x % blind_width) >= step) {
                    fb[y * VGA_320_W + x] = 0;
                }
            }
        }
    } else if (t_ms > (scene_duration - fade_duration)) {
        fade = (scene_duration - t_ms) / (float)fade_duration;
        if (fade < 0.0f) fade = 0.0f;
        
        int blind_width = 16;
        int step = (int)(fade * blind_width);
        for (int y = 0; y < VGA_320_H; y++) {
            for (int x = 0; x < VGA_320_W; x++) {
                if ((x % blind_width) >= step) {
                    fb[y * VGA_320_W + x] = 0;
                }
            }
        }
    }
}

void fx_spark_generator_done(void)
{
}

const effect_t fx_spark_generator = {
    "Spark Generator",
    MODE_320,
    fx_spark_generator_init,
    fx_spark_generator_frame,
    fx_spark_generator_done
};
