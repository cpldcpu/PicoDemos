#include "../scene.h"
#include "../vga.h"
#include "../rgb565.h"
#include <math.h>
#include <string.h>

#define W 160
#define H 120

// 16 hand-picked classic Amstrad CPC colors in RGB888 format
static const uint8_t cpc_colors_rgb[16][3] = {
    {0, 0, 0},       // 0: Black
    {0, 0, 128},     // 1: Blue
    {0, 0, 255},     // 2: Bright Blue
    {128, 0, 0},     // 3: Red
    {128, 0, 128},   // 4: Mauve
    {0, 128, 128},   // 5: Dark Cyan
    {0, 128, 255},   // 6: Sky Blue
    {255, 0, 128},   // 7: Purple
    {255, 0, 255},   // 8: Magenta
    {0, 128, 0},     // 9: Green
    {0, 255, 255},   // 10: Bright Cyan
    {128, 255, 0},   // 11: Lime Green
    {255, 255, 0},   // 12: Yellow
    {255, 255, 255}, // 13: White
    {0, 255, 0},     // 14: Bright Green
    {255, 128, 0}    // 15: Orange
};

static uint16_t cpc_colors_565[16];

// Bayer 4x4 dither matrix
static const uint8_t bayer_matrix[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

void fx_plasma_chip_init(void)
{
    // Pre-pack the CPC 16-color palette
    for (int i = 0; i < 16; i++) {
        cpc_colors_565[i] = rgb565_pack(cpc_colors_rgb[i][0], cpc_colors_rgb[i][1], cpc_colors_rgb[i][2]);
    }
}

// Distance or color mapping helper
static int find_closest_cpc_color(int r, int g, int b)
{
    int best_idx = 0;
    int best_dist = 0x7FFFFFFF;
    for (int i = 0; i < 16; i++) {
        int dr = r - cpc_colors_rgb[i][0];
        int dg = g - cpc_colors_rgb[i][1];
        int db = b - cpc_colors_rgb[i][2];
        int dist = dr*dr + dg*dg + db*db;
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }
    return best_idx;
}

void fx_plasma_chip_frame(uint32_t t_ms, uint32_t t_global)
{
    uint16_t *fb = vga_160_back_buffer();
    float t = t_ms * 0.001f;

    // Compute transition fade factor (linear 1000ms fade-in, 1000ms fade-out)
    float global_fade = 1.0f;
    uint32_t duration = 21246; // 38289 - 17043
    if (t_ms < 1000) {
        global_fade = t_ms / 1000.0f;
    } else if (t_ms > duration - 1000) {
        global_fade = (float)(duration - t_ms) / 1000.0f;
        if (global_fade < 0.0f) global_fade = 0.0f;
    }

    // Constrain transitions: starts at 1.0 (CPC ordered dither), transitions to 0.0 (Truecolor continuous)
    // 0:00 to 0:10 (0 to 10000ms): fully CPC. 0:10 to 0:20 (10000ms to 20000ms): fade constraint to truecolor.
    float cpc_bias = 1.0f;
    if (t_ms > 8000) {
        cpc_bias = 1.0f - ((t_ms - 8000) / 10000.0f);
        if (cpc_bias < 0.0f) cpc_bias = 0.0f;
    }

    // Sinusoidal plasma calculation
    // Speed is beat-dependent
    float cx1 = sinf(t * 1.3f) * 20.0f;
    float cy1 = cosf(t * 0.9f) * 15.0f;
    float cx2 = cosf(t * 1.7f) * 10.0f;
    float cy2 = sinf(t * 1.1f) * 12.0f;

    for (int y = 0; y < H; y++) {
        float fy = y - H / 2.0f;
        for (int x = 0; x < W; x++) {
            float fx = x - W / 2.0f;

            // Multiple overlapping sine waves
            float v1 = sinf((fx + cx1) * 0.08f + (fy + cy1) * 0.06f);
            float v2 = sinf(sqrtf((fx - cx2) * (fx - cx2) + (fy - cy2) * (fy - cy2)) * 0.12f - t * 2.0f);
            float v3 = sinf(fx * 0.05f - t * 1.5f) * cosf(fy * 0.07f + t * 0.8f);

            float val = (v1 + v2 + v3) / 3.0f; // Range [-1, 1]
            float norm_val = (val + 1.0f) * 0.5f; // Range [0, 1]

            // Convert raw continuous value to continuous truecolor RGB
            // Warm-to-cool palette mapping (Pink -> Cyan -> Yellow -> Magenta)
            float r_raw = sinf(norm_val * 2.0f * 3.14159f + 0.0f) * 127.0f + 128.0f;
            float g_raw = sinf(norm_val * 2.0f * 3.14159f + 2.094f) * 127.0f + 128.0f;
            float b_raw = sinf(norm_val * 2.0f * 3.14159f + 4.188f) * 127.0f + 128.0f;

            // Ordered dithering for CPC constraints
            float dither_val = (bayer_matrix[y & 3][x & 3] - 7.5f) / 15.0f * 32.0f; // range -16..16

            int r_dith = (int)(r_raw + dither_val);
            int g_dith = (int)(g_raw + dither_val);
            int b_dith = (int)(b_raw + dither_val);

            if (r_dith < 0) r_dith = 0; else if (r_dith > 255) r_dith = 255;
            if (g_dith < 0) g_dith = 0; else if (g_dith > 255) g_dith = 255;
            if (b_dith < 0) b_dith = 0; else if (b_dith > 255) b_dith = 255;

            // Closest CPC color
            int cpc_idx = find_closest_cpc_color(r_dith, g_dith, b_dith);

            // Compute faded color in RGB space
            int r_blend, g_blend, b_blend;
            if (cpc_bias >= 1.0f) {
                r_blend = (int)(cpc_colors_rgb[cpc_idx][0] * global_fade);
                g_blend = (int)(cpc_colors_rgb[cpc_idx][1] * global_fade);
                b_blend = (int)(cpc_colors_rgb[cpc_idx][2] * global_fade);
            } else if (cpc_bias <= 0.0f) {
                r_blend = (int)(r_raw * global_fade);
                g_blend = (int)(g_raw * global_fade);
                b_blend = (int)(b_raw * global_fade);
            } else {
                r_blend = (int)((cpc_colors_rgb[cpc_idx][0] * cpc_bias + r_raw * (1.0f - cpc_bias)) * global_fade);
                g_blend = (int)((cpc_colors_rgb[cpc_idx][1] * cpc_bias + g_raw * (1.0f - cpc_bias)) * global_fade);
                b_blend = (int)((cpc_colors_rgb[cpc_idx][2] * cpc_bias + b_raw * (1.0f - cpc_bias)) * global_fade);
            }
            fb[y * W + x] = rgb565_pack(r_blend, g_blend, b_blend);
        }
    }
}

void fx_plasma_chip_done(void)
{
}

const effect_t fx_plasma_chip = {
    "Plasma Chip",
    MODE_160,
    fx_plasma_chip_init,
    fx_plasma_chip_frame,
    fx_plasma_chip_done
};
