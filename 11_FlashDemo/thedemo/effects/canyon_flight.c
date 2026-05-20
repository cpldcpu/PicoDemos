/* VOLTAGE demoscene - Scene 6: Cyber-Canyon Flight (New Climax Scene)
 *
 * Mode: MODE_320 (320x240 palettized, 8bpp chunky)
 * Visuals: A high-speed, high-octane 3D perspective flight through a wireframe 
 *          canyon. Glowing electric-amber grid lines infinitely scroll and
 *          bend as mountains rise on the left and right, fading seamlessly
 *          into the horizon via distance fog.
 */

#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define W 320
#define H 240
#define GRID_COLS 13
#define GRID_ROWS 15

/* Fast Bresenham line drawing for fb320 palettized buffer */
static void draw_line_320(uint8_t *fb, int x0, int y0, int x1, int y1, uint8_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        if (x0 >= 0 && x0 < W && y0 >= 0 && y0 < H) {
            fb[y0 * W + x0] = color;
        }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fx_canyon_flight_init(void)
{
    /* Index 0: Deep horizon background void */
    vga_320_palette_set(0, 10, 8, 20);

    /* Indices 1..32: Smooth golden-amber depth-mapped fog color ramp
     * 1 (Far away): very close to the deep horizon background void
     * 32 (Closest to camera): bright glowing white-hot golden yellow */
    for (int i = 0; i < 32; i++) {
        float t = i / 31.0f;
        uint8_t r = (uint8_t)(10 * (1.0f - t) + 255 * t);
        uint8_t g = (uint8_t)(8  * (1.0f - t) + 200 * t);
        uint8_t b = (uint8_t)(20 * (1.0f - t) + 110 * t);
        vga_320_palette_set(1 + i, r, g, b);
    }

    /* Index 255: Pure White-hot flash */
    vga_320_palette_set(255, 255, 255, 255);
}

void fx_canyon_flight_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();
    float t_sec = t_ms * 0.001f;
    uint32_t scene_duration = 23000;

    /* Fill background with deep horizon void color index (0) */
    memset(fb, 0, W * H);

    /* Beat trigger pulse for strobe highlights */
    uint32_t beat_interval = 461; /* High speed double-beat */
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.008f);

    /* High speed flight parameters */
    float cam_x = sinf(t_sec * 2.0f) * 0.35f;
    float cam_y = 0.5f + cosf(t_sec * 1.5f) * 0.15f;
    float cam_z = 0.0f;

    /* Pitch/yaw swinging camera */
    float look_x = sinf(t_sec * 2.0f + 0.3f) * 0.2f;
    float look_y = -0.2f + sinf(t_sec * 3.0f) * 0.1f;

    /* Project grid nodes */
    int sx[GRID_COLS][GRID_ROWS];
    int sy[GRID_COLS][GRID_ROWS];
    float rz[GRID_COLS][GRID_ROWS];
    uint8_t valid[GRID_COLS][GRID_ROWS];

    memset(valid, 0, sizeof(valid));

    /* Scroll grid forward infinitely */
    float grid_scroll = fmodf(t_sec * 6.5f, 1.2f);

    for (int c = 0; c < GRID_COLS; c++) {
        float x = (c - (GRID_COLS / 2)) * 1.2f;

        for (int r = 0; r < GRID_ROWS; r++) {
            /* Scroll depth */
            float z = (GRID_ROWS - 1 - r) * 1.2f - grid_scroll;
            
            /* Valley topography: flat canyon floor in center, tall peaks on borders */
            float abs_x = fabsf(x);
            float border = (abs_x > 1.2f) ? (abs_x - 1.2f) * 1.3f : 0.0f;
            float peaks = border * border * 0.8f;

            /* Dynamic landscape wave motion */
            float wave = sinf(x * 0.8f + t_sec * 2.5f) * cosf((z + t_sec * 6.5f) * 0.4f) * 0.35f;
            float y = -0.9f + peaks + wave;

            /* Camera-relative coords */
            float rx = x - cam_x;
            float ry = y - cam_y;
            float depth = z - cam_z;

            if (depth <= 0.08f) continue;

            /* Perspective projection (Doubled focal multiplier from 85.0 to 170.0 to match W=320) */
            float sz_inv = 1.0f / depth;
            sx[c][r] = W / 2 + (int)((rx - look_x * depth) * 170.0f * sz_inv);
            sy[c][r] = H / 2 - (int)((ry - look_y * depth) * 170.0f * sz_inv);
            rz[c][r] = depth;
            valid[c][r] = 1;
        }
    }

    /* Draw 3D Grid Lines! */
    for (int r = 0; r < GRID_ROWS; r++) {
        for (int c = 0; c < GRID_COLS; c++) {
            if (!valid[c][r]) continue;

            /* 1. Longitudinal lines (running toward camera) */
            if (r < GRID_ROWS - 1 && valid[c][r + 1]) {
                float z_avg = (rz[c][r] + rz[c][r + 1]) * 0.5f;
                float fog = z_avg / 14.0f; /* Seamless dissolve distance */
                if (fog > 1.0f) fog = 1.0f;
                if (fog < 0.0f) fog = 0.0f;

                /* Incorporate beat-sync pulse directly into color depth */
                float intensity = (1.0f - fog) * (0.8f + beat_pulse * 0.2f);
                int color_idx = (int)(intensity * 31.0f) + 1;
                if (color_idx > 32) color_idx = 32;
                if (color_idx < 1)  color_idx = 1;

                draw_line_320(fb, sx[c][r], sy[c][r], sx[c][r + 1], sy[c][r + 1], (uint8_t)color_idx);
            }

            /* 2. Transverse lines (horizontal ridge paths) */
            if (c < GRID_COLS - 1 && valid[c + 1][r]) {
                float z_avg = (rz[c][r] + rz[c + 1][r]) * 0.5f;
                float fog = z_avg / 14.0f;
                if (fog > 1.0f) fog = 1.0f;
                if (fog < 0.0f) fog = 0.0f;

                float intensity = (1.0f - fog) * (0.7f + beat_pulse * 0.3f);
                int color_idx = (int)(intensity * 31.0f) + 1;
                if (color_idx > 32) color_idx = 32;
                if (color_idx < 1)  color_idx = 1;

                draw_line_320(fb, sx[c][r], sy[c][r], sx[c + 1][r], sy[c + 1][r], (uint8_t)color_idx);
            }
        }
    }

    /* Dynamic fade-in/out transitions */
    float fade = 1.0f;
    uint32_t fade_duration = 1500;

    if (t_ms < fade_duration) {
        fade = t_ms / (float)fade_duration;
        /* Glitch static noise fade-in from CRT collapse */
        int threshold = (int)(256.0f * (1.0f - fade));
        uint32_t lcg = 0xACE1;
        for (int y = 0; y < H; y++) {
            int row_offset = y * W;
            for (int x = 0; x < W; x++) {
                lcg = lcg * 1103515245U + 12345U;
                uint8_t rand_val = (uint8_t)((lcg >> 16) & 0xFF);
                if (rand_val < threshold) {
                    fb[row_offset + x] = ((lcg & 0x100) == 0) ? 255 : 0;
                }
            }
        }
    } else if (t_ms > (scene_duration - fade_duration)) {
        fade = (scene_duration - t_ms) / (float)fade_duration;
        if (fade < 0.0f) fade = 0.0f;

        /* Scrambling Glitch Analog Static Wipe to cut instantly to black for the silent drop! */
        int threshold = (int)(256.0f * (1.0f - fade));
        uint32_t lcg = 0xACE1;
        for (int y = 0; y < H; y++) {
            int row_offset = y * W;
            for (int x = 0; x < W; x++) {
                lcg = lcg * 1103515245U + 12345U;
                uint8_t rand_val = (uint8_t)((lcg >> 16) & 0xFF);
                if (rand_val < threshold) {
                    fb[row_offset + x] = ((lcg & 0x100) == 0) ? 255 : 0;
                }
            }
        }
    }
}

void fx_canyon_flight_done(void)
{
}

const effect_t fx_canyon_flight = {
    "Canyon Flight",
    MODE_320,
    fx_canyon_flight_init,
    fx_canyon_flight_frame,
    fx_canyon_flight_done
};
