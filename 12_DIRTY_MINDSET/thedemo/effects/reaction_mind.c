#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "../rgb565.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define SIM_W 80
#define SIM_H 60

static float *grid_a;
static float *grid_b;
static float *grid_a_next;
static float *grid_b_next;

static inline float clamp_f(float val, float min, float max)
{
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

void fx_reaction_mind_init(void)
{
    // Direct, type-safe pointer assignment without unsafe casting
    grid_a = g_scratch.rd.grid_a;
    grid_b = g_scratch.rd.grid_b;
    grid_a_next = g_scratch.rd.grid_a_next;
    grid_b_next = g_scratch.rd.grid_b_next;

    // Initialize simulation grids stably
    for (int y = 0; y < SIM_H; y++) {
        for (int x = 0; x < SIM_W; x++) {
            int idx = y * SIM_W + x;
            grid_a[idx] = 1.0f;
            grid_b[idx] = 0.0f;
        }
    }

    // Seed center with organic catalyst pattern (cluster of random blocks)
    srand(123);
    int cx = SIM_W / 2;
    int cy = SIM_H / 2;
    int seed_size = 6;
    for (int y = cy - seed_size; y < cy + seed_size; y++) {
        for (int x = cx - seed_size; x < cx + seed_size; x++) {
            int idx = y * SIM_W + x;
            if (rand() % 100 < 50) {
                grid_a[idx] = 0.50f;
                grid_b[idx] = 0.25f;
            }
        }
    }
}

void fx_reaction_mind_frame(uint32_t t_ms, uint32_t t_global)
{
    uint16_t *fb = vga_160_back_buffer();

    // Classic stable Gray-Scott parameters for beautiful organic labyrinths/filaments
    // using isotropic 9-point discrete Laplacian stencil weights
    float feed = 0.0545f;
    float kill = 0.062f;
    float diff_a = 1.0f;
    float diff_b = 0.5f;
    float dt = 1.0f;

    // Inject B-catalyst on music beats dynamically
    uint32_t beat_interval = 464;
    static uint32_t last_beat_trigger = 99999;
    uint32_t current_beat = t_ms / beat_interval;
    if (t_ms > 0 && current_beat != last_beat_trigger) {
        last_beat_trigger = current_beat;
        
        // Seed at 1-2 random locations on beat pulse
        int seeds = 1 + (rand() % 2);
        for (int s = 0; s < seeds; s++) {
            int rx = 15 + (rand() % (SIM_W - 30));
            int ry = 15 + (rand() % (SIM_H - 30));
            int rsize = 2 + (rand() % 3);
            for (int y = ry - rsize; y <= ry + rsize; y++) {
                for (int x = rx - rsize; x <= rx + rsize; x++) {
                    if (x >= 0 && x < SIM_W && y >= 0 && y < SIM_H) {
                        int idx = y * SIM_W + x;
                        grid_a[idx] = 0.50f;
                        grid_b[idx] = 0.35f;
                    }
                }
            }
        }
    }

    // Run 3 steps of simulation per frame for organic growth
    for (int step = 0; step < 3; step++) {
        // Enforce boundary values
        for (int x = 0; x < SIM_W; x++) {
            grid_a_next[x] = grid_a[x];
            grid_a_next[(SIM_H - 1) * SIM_W + x] = grid_a[(SIM_H - 1) * SIM_W + x];
            grid_b_next[x] = grid_b[x];
            grid_b_next[(SIM_H - 1) * SIM_W + x] = grid_b[(SIM_H - 1) * SIM_W + x];
        }
        for (int y = 0; y < SIM_H; y++) {
            grid_a_next[y * SIM_W] = grid_a[y * SIM_W];
            grid_a_next[y * SIM_W + (SIM_W - 1)] = grid_a[y * SIM_W + (SIM_W - 1)];
            grid_b_next[y * SIM_W] = grid_b[y * SIM_W];
            grid_b_next[y * SIM_W + (SIM_W - 1)] = grid_b[y * SIM_W + (SIM_W - 1)];
        }

        for (int y = 1; y < SIM_H - 1; y++) {
            for (int x = 1; x < SIM_W - 1; x++) {
                int idx = y * SIM_W + x;

                float a = grid_a[idx];
                float b = grid_b[idx];

                // High-fidelity isotropic 9-point discrete Laplacian stencil equation
                // Cardinal neighbors: weight 0.20
                // Diagonal neighbors: weight 0.05
                // Center neighbor: weight -1.00
                float lap_a = 0.20f * (grid_a[idx - 1] + grid_a[idx + 1] + grid_a[idx - SIM_W] + grid_a[idx + SIM_W]) +
                              0.05f * (grid_a[idx - SIM_W - 1] + grid_a[idx - SIM_W + 1] + grid_a[idx + SIM_W - 1] + grid_a[idx + SIM_W + 1]) -
                              a;
                float lap_b = 0.20f * (grid_b[idx - 1] + grid_b[idx + 1] + grid_b[idx - SIM_W] + grid_b[idx + SIM_W]) +
                              0.05f * (grid_b[idx - SIM_W - 1] + grid_b[idx - SIM_W + 1] + grid_b[idx + SIM_W - 1] + grid_b[idx + SIM_W + 1]) -
                              b;

                float abb = a * b * b;

                float a_next = a + (diff_a * lap_a - abb + feed * (1.0f - a)) * dt;
                float b_next = b + (diff_b * lap_b + abb - (kill + feed) * b) * dt;

                grid_a_next[idx] = clamp_f(a_next, 0.0f, 1.0f);
                grid_b_next[idx] = clamp_f(b_next, 0.0f, 1.0f);
            }
        }

        memcpy(grid_a, grid_a_next, SIM_W * SIM_H * sizeof(float));
        memcpy(grid_b, grid_b_next, SIM_W * SIM_H * sizeof(float));
    }

    // Determine fade factor for smooth transitions (duration = 21270 ms)
    float fade = 1.0f;
    uint32_t duration = 21270; // 93576 - 72306
    if (t_ms < 1000) {
        fade = t_ms / 1000.0f;
    } else if (t_ms >= duration - 1000) {
        fade = 1.0f - (float)(t_ms - (duration - 1000)) / 1000.0f;
        if (fade < 0.0f) fade = 0.0f;
    }

    // Render with 2x2 upscale and custom neon gradient mapping
    for (int sy = 0; sy < SIM_H; sy++) {
        for (int sx = 0; sx < SIM_W; sx++) {
            float b_val = grid_b[sy * SIM_W + sx];

            int r, g, b;
            if (b_val < 0.2f) {
                float u = b_val / 0.2f;
                r = (int)((8 + 12 * u) * fade);
                g = (int)((4 + 6 * u) * fade);
                b = (int)((20 + 30 * u) * fade);
            } else if (b_val < 0.5f) {
                float u = (b_val - 0.2f) / 0.3f;
                r = (int)((20 + 235 * u) * fade);
                g = (int)((10 + 30 * u) * fade);
                b = (int)((50 + 110 * u) * fade);
            } else if (b_val < 0.8f) {
                float u = (b_val - 0.5f) / 0.3f;
                r = (int)((255 * (1.0f - u) + 0 * u) * fade);
                g = (int)((40 * (1.0f - u) + 220 * u) * fade);
                b = (int)((160 * (1.0f - u) + 255 * u) * fade);
            } else {
                float u = (b_val - 0.8f) / 0.2f;
                r = (int)((0 * (1.0f - u) + 255 * u) * fade);
                g = (int)((220 * (1.0f - u) + 255 * u) * fade);
                b = (int)((255 * (1.0f - u) + 0 * u) * fade);
            }

            uint16_t color = rgb565_pack(r, g, b);

            int px = sx * 2;
            int py = sy * 2;
            fb[py * VGA_160_W + px] = color;
            fb[py * VGA_160_W + (px + 1)] = color;
            fb[(py + 1) * VGA_160_W + px] = color;
            fb[(py + 1) * VGA_160_W + (px + 1)] = color;
        }
    }
}

void fx_reaction_mind_done(void)
{
}

const effect_t fx_reaction_mind = {
    "Reaction Mind",
    MODE_160,
    fx_reaction_mind_init,
    fx_reaction_mind_frame,
    fx_reaction_mind_done
};
