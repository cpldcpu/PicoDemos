#include "../scene.h"
#include "../vga.h"
#include "../rgb565.h"
#include <math.h>
#include <string.h>

#define W 160
#define H 120

void fx_fractal_zoom_init(void)
{
}

void fx_fractal_zoom_frame(uint32_t t_ms, uint32_t t_global)
{
    uint16_t *fb = vga_160_back_buffer();
    float t_sec = t_ms * 0.001f;

    // Beat sync: 464 ms beats (128 BPM)
    uint32_t beat_interval = 464;
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.006f);

    // Zoom factor: exponentially grows over time
    // In the final 1500 ms, we accelerate the zoom exponentially to dive deep, and fade to black.
    float t_zoom = t_sec;
    float dive_fade = 1.0f;
    uint32_t scene_duration = 21734; // 115310 - 93576
    
    if (t_ms >= scene_duration - 1500) {
        float f = (t_ms - (scene_duration - 1500)) / 1500.0f; // 0.0 to 1.0
        t_zoom = t_sec + f * 12.0f; // Rapidly accelerate zoom
        dive_fade = 1.0f - f;       // Fade out to black
    }

    float zoom_exponent = 1.0f + t_zoom * 0.45f + beat_pulse * 0.12f;
    float zoom = powf(2.0f, zoom_exponent);

    // Target coordinate (Seahorse Valley boundary)
    float target_x = -0.743643887f;
    float target_y = 0.131825904f;

    // View boundaries
    float w_span = 3.0f / zoom;
    float h_span = (3.0f * (float)H / (float)W) / zoom;

    float min_x = target_x - w_span * 0.5f;
    float min_y = target_y - h_span * 0.5f;

    // Max iterations grows slightly with zoom to keep details sharp
    int max_iter = 32 + (int)(t_zoom * 1.5f);
    if (max_iter > 70) max_iter = 70; // Keep performance safe

    for (int y = 0; y < H; y++) {
        float c_im = min_y + ((float)y / (float)H) * h_span;
        for (int x = 0; x < W; x++) {
            float c_re = min_x + ((float)x / (float)W) * w_span;

            float z_re = 0.0f;
            float z_im = 0.0f;

            int iter = 0;
            float z_re2 = 0.0f;
            float z_im2 = 0.0f;

            while (iter < max_iter && (z_re2 + z_im2) < 4.0f) {
                z_im = 2.0f * z_re * z_im + c_im;
                z_re = z_re2 - z_im2 + c_re;
                z_re2 = z_re * z_re;
                z_im2 = z_im * z_im;
                iter++;
            }

            uint16_t color;
            if (iter < max_iter) {
                // Smooth coloring: i - log(log(|z|)) / log(2)
                float log_zn = logf(z_re2 + z_im2) * 0.5f;
                float nu = logf(log_zn / 0.69314718f) / 0.69314718f;
                float smooth_i = iter + 1 - nu;

                // Color palette lookup based on smooth_i
                float c_val = smooth_i * 0.15f + t_sec * 0.1f;
                
                // Neon synthwave colors: Pink -> Violet -> Cyan -> Yellow
                int r = (int)(sinf(c_val * 2.0f * 3.14159f + 0.0f) * 127.0f + 128.0f);
                int g = (int)(sinf(c_val * 2.0f * 3.14159f + 2.094f) * 100.0f + 100.0f);
                int b = (int)(sinf(c_val * 2.0f * 3.14159f + 4.188f) * 127.0f + 128.0f);

                // Apply transition fade
                r = (int)(r * dive_fade);
                g = (int)(g * dive_fade);
                b = (int)(b * dive_fade);

                color = rgb565_pack(r, g, b);
            } else {
                // Inside Mandelbrot set: deep dark void
                color = rgb565_pack((int)(8 * dive_fade), (int)(4 * dive_fade), (int)(20 * dive_fade));
            }

            fb[y * W + x] = color;
        }
    }
}

void fx_fractal_zoom_done(void)
{
}

const effect_t fx_fractal_zoom = {
    "Fractal Zoom",
    MODE_160,
    fx_fractal_zoom_init,
    fx_fractal_zoom_frame,
    fx_fractal_zoom_done
};
