/* VOLTAGE demoscene - Scene 2: Plasma Core (Fluid & Particle Sim)
 *
 * Mode: MODE_160 (160x120 direct RGB565 truecolor, pixel-doubled to 320x240)
 * Visuals: Turbulent trigonometric velocity fields, advection fluid dye,
 *          1024 sparks orbiting an unstable wobbly plasma core, and a smooth
 *          high-fidelity additive beat strobe flash overlay.
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
#define NUM_PARTICLES 1024

/* Pre-allocated lookup tables for performance */
static uint16_t colormap[256];
static int8_t sintab[256];
static int8_t costab[256];

/* High-performance hardware-friendly RGB565 additive saturate blend */
static inline uint16_t rgb565_add_saturate(uint16_t c1, uint16_t c2)
{
    int r1 = (c1 >> 11) & 0x1F;
    int g1 = (c1 >> 5) & 0x3F;
    int b1 = c1 & 0x1F;
    
    int r2 = (c2 >> 11) & 0x1F;
    int g2 = (c2 >> 5) & 0x3F;
    int b2 = c2 & 0x1F;
    
    int r = r1 + r2; if (r > 0x1F) r = 0x1F;
    int g = g1 + g2; if (g > 0x3F) g = 0x3F;
    int b = b1 + b2; if (b > 0x1F) b = 0x1F;
    
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void fx_plasma_core_init(void)
{
    /* Build our high-fidelity truecolor fluid colormap.
     * Gradients: Dark Void -> Deep Violet -> Neon Magenta -> Cyan -> White */
    for (int i = 0; i < 256; i++) {
        float t = i / 255.0f;
        int r, g, b;
        if (t < 0.2f) {
            float u = t / 0.2f;
            r = (int)(10 + 30 * u);
            g = (int)(8 + 0 * u);
            b = (int)(20 + 60 * u);
        } else if (t < 0.5f) {
            float u = (t - 0.2f) / 0.3f;
            r = (int)(40 + 215 * u);
            g = (int)(8 + 0 * u);
            b = (int)(80 + 100 * u);
        } else if (t < 0.8f) {
            float u = (t - 0.5f) / 0.3f;
            r = (int)(255 * (1.0f - u) + 0 * u);
            g = (int)(0 * (1.0f - u) + 240 * u);
            b = (int)(180 * (1.0f - u) + 255 * u);
        } else {
            float u = (t - 0.8f) / 0.2f;
            r = (int)(0 * (1.0f - u) + 255 * u);
            g = (int)(240 * (1.0f - u) + 255 * u);
            b = (int)(255 * (1.0f - u) + 255 * u);
        }
        colormap[i] = rgb565_pack(r, g, b);
    }

    /* Precalculate sine/cosine tables for fast trigonometric advection */
    for (int i = 0; i < 256; i++) {
        float rad = (i / 256.0f) * 2.0f * 3.14159f;
        sintab[i] = (int8_t)(sinf(rad) * 64.0f);
        costab[i] = (int8_t)(cosf(rad) * 64.0f);
    }

    /* Initialize dye and particles */
    memset(g_scratch.plasma.dye, 0, sizeof(g_scratch.plasma.dye));
    memset(g_scratch.plasma.prev_dye, 0, sizeof(g_scratch.plasma.prev_dye));

    for (int i = 0; i < NUM_PARTICLES; i++) {
        /* Distribute around screen center */
        float r = (rand() % 40) + 5;
        float a = (rand() % 360) * (3.14159f / 180.0f);
        g_scratch.plasma.px[i] = (uint16_t)((W / 2 + cosf(a) * r) * 16.0f);
        g_scratch.plasma.py[i] = (uint16_t)((H / 2 + sinf(a) * r) * 16.0f);
        
        /* Particle color slot */
        g_scratch.plasma.pcol[i] = (rand() % 2 == 0) ? 180 : 230;
    }
}

void fx_plasma_core_frame(uint32_t t_ms, uint32_t t_global)
{
    uint16_t *fb = vga_160_back_buffer();
    uint8_t *dye = g_scratch.plasma.dye;
    uint8_t *prev_dye = g_scratch.plasma.prev_dye;

    float t_sec = t_ms * 0.001f;

    /* Get beat pulse for lighting core effects */
    uint32_t beat_interval = 923; 
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.006f);

    /* Phase shifts for dynamic velocity field */
    uint8_t t_phase = (uint8_t)(t_ms >> 3);
    uint8_t t_phase2 = (uint8_t)(t_ms >> 4);

    /* 1. BACKWARDS ADVECTION: Solve dye grid propagation
     * Loop through every cell, trace velocity backwards, look up density, 
     * apply diffusion and decay. */
    for (int y = 1; y < H - 1; y++) {
        for (int x = 1; x < W - 1; x++) {
            /* Fast trigonometric velocity field lookup using sintab/costab */
            uint8_t u_idx = (uint8_t)((y << 2) + t_phase);
            uint8_t v_idx = (uint8_t)((x << 2) - t_phase2);

            int vx = (sintab[u_idx] + costab[v_idx]) >> 5;  /* velocity X range -4..4 */
            int vy = (costab[u_idx] - sintab[v_idx]) >> 5;  /* velocity Y range -4..4 */

            /* Trace back */
            int sx = x - vx;
            int sy = y - vy;

            if (sx < 0) sx = 0; else if (sx >= W) sx = W - 1;
            if (sy < 0) sy = 0; else if (sy >= H) sy = H - 1;

            /* Advect & slightly diffuse with neighbor cells */
            int val = prev_dye[sy * W + sx];
            int neighbors = (prev_dye[y * W + (x-1)] + prev_dye[y * W + (x+1)] + 
                             prev_dye[(y-1) * W + x] + prev_dye[(y+1) * W + x]) >> 2;

            /* Blend advection with diffusion and apply global cooling/decay */
            int final_dye = (val * 7 + neighbors * 1) >> 3;
            final_dye = (final_dye * 250) >> 8; /* 97% persistence */

            dye[y * W + x] = (uint8_t)final_dye;
        }
    }

    /* 2. CORE REACTOR ORB
     * Let the core drift dynamically in an unstable figure-8 orbit */
    float core_cx = W / 2.0f + sinf(t_sec * 3.0f) * 15.0f;
    float core_cy = H / 2.0f + cosf(t_sec * 2.1f) * 10.0f;
    
    int r_core = 10 + (int)(beat_pulse * 12.0f);
    int r_max = r_core + 8;
    
    for (int y = (int)(core_cy - r_max); y < (int)(core_cy + r_max); y++) {
        if (y < 0 || y >= H) continue;
        float dy = y - core_cy;
        for (int x = (int)(core_cx - r_max); x < (int)(core_cx + r_max); x++) {
            if (x < 0 || x >= W) continue;
            float dx = x - core_cx;
            float d2 = dx * dx + dy * dy;
            float dist = sqrtf(d2);
            
            /* Modulate core boundary dynamically to shape flaring electric solar spicules */
            float angle = atan2f(dy, dx);
            float spike = sinf(angle * 6.0f + t_sec * 12.0f) * 3.0f * (1.0f + beat_pulse * 1.5f);
            float dynamic_r = r_core + spike;
            
            if (dist < dynamic_r) {
                float intensity = 1.0f - (dist / dynamic_r);
                int charge = dye[y * W + x] + (int)(intensity * 140.0f);
                dye[y * W + x] = (charge > 255) ? 255 : charge;
            }
        }
    }

    /* 3. UPDATE PARTICLES & DRAW TRAILS
     * Particles trace along the flow, pulling dynamically towards the DRIFTING reactor core. */
    float cx = core_cx;
    float cy = core_cy;
    float orbital_strength = 0.8f + beat_pulse * 1.5f;

    for (int i = 0; i < NUM_PARTICLES; i++) {
        float px = g_scratch.plasma.px[i] / 16.0f;
        float py = g_scratch.plasma.py[i] / 16.0f;

        /* Core gravity & orbital spin */
        float dx = px - cx;
        float dy = py - cy;
        float dist = sqrtf(dx * dx + dy * dy) + 0.1f;

        /* Orbital velocity component */
        float ox = -dy / dist * orbital_strength;
        float oy =  dx / dist * orbital_strength;

        /* Pull component */
        float pull = -0.05f * dist; 
        if (dist < 15.0f) pull = 0.1f * (15.0f - dist); /* Repel near core */

        float px_new = px + ox + (dx / dist) * pull + (rand() % 10 - 5) * 0.05f;
        float py_new = py + oy + (dy / dist) * pull + (rand() % 10 - 5) * 0.05f;

        /* Boundary check */
        if (px_new < 1.0f || px_new >= W - 1.0f || py_new < 1.0f || py_new >= H - 1.0f || dist > 60.0f) {
            /* Respawn at core edge */
            float a = (rand() % 360) * (3.14159f / 180.0f);
            float r = (rand() % 8) + 2.0f;
            px_new = cx + cosf(a) * r;
            py_new = cy + sinf(a) * r;
        }

        /* Save particle state in 12.4 fixed-point */
        g_scratch.plasma.px[i] = (uint16_t)(px_new * 16.0f);
        g_scratch.plasma.py[i] = (uint16_t)(py_new * 16.0f);

        /* Inject dye trail along path */
        int ix = (int)px_new;
        int iy = (int)py_new;
        if (ix > 0 && ix < W - 1 && iy > 0 && iy < H - 1) {
            int d = dye[iy * W + ix] + 48;
            dye[iy * W + ix] = (d > 255) ? 255 : d;

            /* Add some dye to immediate neighbors for thicker trail */
            if (dye[iy * W + (ix+1)] < 200) dye[iy * W + (ix+1)] += 16;
            if (dye[(iy+1) * W + ix] < 200) dye[(iy+1) * W + ix] += 16;
        }
    }

    /* Copy current dye to prev_dye for next frame */
    memcpy(prev_dye, dye, W * H);

    /* 4. PRESENT GRID TO FRAMEBUFFER
     * Map dye values (0..255) to the high-voltage colormap. Overlay particle positions. */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            fb[y * W + x] = colormap[dye[y * W + x]];
        }
    }

    /* Draw particle charges as bright hot pixels */
    for (int i = 0; i < NUM_PARTICLES; i += 2) { /* Draw half for speed and contrast */
        int px = g_scratch.plasma.px[i] >> 4;
        int py = g_scratch.plasma.py[i] >> 4;
        if (px >= 0 && px < W && py >= 0 && py < H) {
            uint16_t c = (g_scratch.plasma.pcol[i] == 180) ? 
                         rgb565_pack(0, 255, 255) :   /* Glowing Cyan charge */
                         rgb565_pack(255, 240, 200);  /* Hot White-Yellow charge */
            fb[py * W + px] = c;
        }
    }

    /* 5. SMOOTH HIGH-FIDELITY ADDITIVE STROBE FLASH
     * Replace striped horizontal lines with a gorgeous full-screen additive beat flash! */
    if (beat_pulse > 0.8f) {
        float st = (beat_pulse - 0.8f) / 0.2f * 0.25f; /* Max 25% blend intensity */
        uint16_t flash_col = rgb565_pack((int)(100 * st), (int)(150 * st), (int)(255 * st));
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                fb[y * W + x] = rgb565_add_saturate(fb[y * W + x], flash_col);
            }
        }
    }

    /* Dynamic fade-in/out black filter overlay */
    float fade = 1.0f;
    if (t_ms < 1500) {
        fade = t_ms / 1500.0f;
        /* Fade in from black using dither */
        if (fade < 1.0f) {
            int threshold = (int)(256.0f * (1.0f - fade));
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    if (((x ^ y) & 0xFF) < threshold) {
                        fb[y * W + x] = 0x0842; /* Deep Void background color */
                    }
                }
            }
        }
    } else if (t_ms > 13500) {
        fade = (15000 - t_ms) / 1500.0f;
        if (fade < 0.0f) fade = 0.0f;
        
        /* Thermal Burnout: add increasingly intense white-hot glow! */
        float intensity = 1.0f - fade;
        uint16_t burnout_col = rgb565_pack((int)(255 * intensity), (int)(240 * intensity), (int)(220 * intensity));
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                fb[y * W + x] = rgb565_add_saturate(fb[y * W + x], burnout_col);
            }
        }
    }
}

void fx_plasma_core_done(void)
{
}

const effect_t fx_plasma_core = {
    "Plasma Core",
    MODE_160,
    fx_plasma_core_init,
    fx_plasma_core_frame,
    fx_plasma_core_done
};
