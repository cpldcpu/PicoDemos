/* VOLTAGE demoscene - Scene 6: Julia Shockwave (Fractal Zoomer)
 *
 * Mode: MODE_160 (160x120 direct RGB565 truecolor, pixel-doubled to 320x240)
 * Visuals: Real-time morphing Julia Set fractal (z_next = z^2 + c) with dynamic
 *          beat-driven zoom pulses, psychedelic color-palette cycling, and 
 *          vortex shockwaves.
 */

#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "../rgb565.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define W 160
#define H 120
#define MAX_ITER 18

static uint16_t julia_colors[256];

void fx_julia_shockwave_init(void)
{
    /* Initialize dynamic psychedelic color palette
     * Shift through Neon Pink -> Violet -> Electric Blue -> Neon Cyan -> Orange */
    for (int i = 0; i < 256; i++) {
        float t = i / 255.0f;
        int r, g, b;
        if (t < 0.25f) {
            float u = t / 0.25f;
            r = (int)(255 * u);
            g = 0;
            b = (int)(180 + 75 * u);
        } else if (t < 0.5f) {
            float u = (t - 0.25f) / 0.25f;
            r = (int)(255 * (1.0f - u));
            g = (int)(150 * u);
            b = 255;
        } else if (t < 0.75f) {
            float u = (t - 0.5f) / 0.25f;
            r = 0;
            g = (int)(150 * (1.0f - u) + 240 * u);
            b = (int)(255 * (1.0f - u) + 255 * u);
        } else {
            float u = (t - 0.75f) / 0.25f;
            r = (int)(255 * u);
            g = (int)(240 * (1.0f - u) + 120 * u);
            b = (int)(255 * (1.0f - u) + 0 * u);
        }
        julia_colors[i] = rgb565_pack(r, g, b);
    }
}

void fx_julia_shockwave_frame(uint32_t t_ms, uint32_t t_global)
{
    uint16_t *fb = vga_160_back_buffer();
    float t_sec = t_ms * 0.001f;

    /* Get beat sync for zoom pulses and flashing */
    uint32_t beat_interval = 923; 
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.006f);

    /* 1. CALCULATE ORBITING COMPLEX CONSTANT C
     * Orbiting trajectory causes the fractal boundary to morph organically. */
    float c_real = -0.7f + sinf(t_sec * 0.4f) * 0.08f;
    float c_imag = 0.27015f + cosf(t_sec * 0.6f) * 0.05f;

    /* Combine with beat impact */
    c_real += beat_pulse * 0.02f;

    /* 2. DYNAMIC CAMERA ZOOM
     * Spirals inward, pulsing deeply on beat-drops */
    float zoom_base = 0.016f - (t_sec * 0.0001f); /* Smooth zoom in over scene */
    if (zoom_base < 0.008f) zoom_base = 0.008f;
    
    float zoom = zoom_base / (1.0f + beat_pulse * 0.5f); /* Sharp drop zoom */
    float angle = t_sec * 0.15f;
    float cos_a = cosf(angle), sin_a = sinf(angle);

    /* Palette cycling shift */
    int palette_shift = (int)(t_ms * 0.2f);

    /* 3. REAL-TIME JULIA RENDER LOOP
     * Fast float math optimized for M33 FPU */
    for (int y = 0; y < H; y++) {
        float dy = (y - H / 2.0f);
        int row_offset = y * W;

        for (int x = 0; x < W; x++) {
            float dx = (x - W / 2.0f);

            /* Center & rotate coordinates for spiral rotation */
            float rx = (dx * cos_a - dy * sin_a) * zoom;
            float ry = (dx * sin_a + dy * cos_a) * zoom;

            /* Julia iteration check: z_next = z^2 + c */
            float zx = rx;
            float zy = ry;
            int iter = 0;

            for (int i = 0; i < MAX_ITER; i++) {
                float zx2 = zx * zx;
                float zy2 = zy * zy;

                if (zx2 + zy2 > 4.0f) {
                    iter = i;
                    break;
                }

                zy = 2.0f * zx * zy + c_imag;
                zx = zx2 - zy2 + c_real;
                iter = MAX_ITER;
            }

            /* Draw color */
            uint16_t pixel_color;
            if (iter == MAX_ITER) {
                /* Inner body is pitch black */
                pixel_color = 0x0842;
            } else {
                /* Map iterations + depth to palette indices */
                int col_idx = (iter * 14 + palette_shift) & 0xFF;
                pixel_color = julia_colors[col_idx];
                
                /* Overlay beat flash lighting highlights */
                if (beat_pulse > 0.7f && (iter % 3 == 0)) {
                    pixel_color |= 0xFEE0; /* Spark yellow highlight overlay */
                }
            }

            fb[row_offset + x] = pixel_color;
        }
    }

    /* Dynamic fade-in/out black filter overlay */
    float fade = 1.0f;
    uint32_t fade_duration = 1500;
    uint32_t scene_duration = 17000;

    if (t_ms < fade_duration) {
        fade = t_ms / (float)fade_duration;
        int threshold = (int)(256.0f * (1.0f - fade));
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (((x ^ y) & 0xFF) < threshold) {
                    fb[y * W + x] = 0x0842;
                }
            }
        }
    } else if (t_ms > (scene_duration - fade_duration)) {
        fade = (scene_duration - t_ms) / (float)fade_duration;
        if (fade < 0.0f) fade = 0.0f;

        /* Expanding circular shockwave iris wipe! (Optimized with squared-distance checks to avoid costly sqrtf) */
        float max_r = sqrtf(W * W + H * H) * 0.5f;
        float r_shock = (1.0f - fade) * max_r;
        float r_inner = r_shock - 1.5f;
        float r_outer = r_shock + 1.5f;
        float r_inner_sq = (r_inner > 0.0f) ? (r_inner * r_inner) : 0.0f;
        float r_outer_sq = r_outer * r_outer;
        int cx = W / 2, cy = H / 2;
        
        for (int y = 0; y < H; y++) {
            float dy = (float)(y - cy);
            int row_offset = y * W;
            for (int x = 0; x < W; x++) {
                float dx = (float)(x - cx);
                float d_sq = dx * dx + dy * dy;
                if (d_sq < r_inner_sq) {
                    fb[row_offset + x] = 0x0842; /* Deep Void background color */
                } else if (d_sq < r_outer_sq) {
                    fb[row_offset + x] = 0x07FF; /* Glowing Cyan shockwave ring */
                }
            }
        }
    }
}

void fx_julia_shockwave_done(void)
{
}

const effect_t fx_julia_shockwave = {
    "Julia Shockwave",
    MODE_160,
    fx_julia_shockwave_init,
    fx_julia_shockwave_frame,
    fx_julia_shockwave_done
};
