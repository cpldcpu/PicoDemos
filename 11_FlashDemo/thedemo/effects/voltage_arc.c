/* VOLTAGE demoscene - Scene 7: Voltage Arc (Rotozoom Outro & Credits)
 *
 * Mode: MODE_320 (320x240 chunky 8bpp)
 * Visuals: 32-bit fixed-point real-time rotozoomer of the endcard background,
 *          vertical scrolling credit scroller, raining electric spark embers, 
 *          and a final visual convergence to a single flashing white energy spark.
 */

#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "../font8x8.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define NUM_EMBERS 32

typedef struct {
    float x, y;
    float speed;
    uint8_t color;
} Ember;

/* Reuse scene scratch for particle embers */
static Ember *embers = (Ember *)&g_scratch.bg_cache[0];

void fx_voltage_arc_init(void)
{
    /* Load the endcard background palette to the VGA backend */
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_endcard_bg_pal[i];
        /* Unpack Pimoroni custom BGR555 layout: Blue in bits 15..11, Green in bits 10..6, Red in bits 4..0 */
        uint8_t b = ((c >> 11) & 0x1F) << 3;
        uint8_t g = ((c >> 6) & 0x1F) << 3;
        uint8_t r = (c & 0x1F) << 3;
        vga_320_palette_set(i, r, g, b);
    }

    /* Top colors are reserved for custom glowing vector scroller and sparks */
    vga_320_palette_set(253, 0, 240, 255);   /* Neon Cyan scroller */
    vga_320_palette_set(254, 255, 0, 180);   /* Neon Magenta scroller */
    vga_320_palette_set(255, 255, 255, 255); /* White-Hot spark */

    /* Initialize ember particles */
    for (int i = 0; i < NUM_EMBERS; i++) {
        embers[i].x = (float)(rand() % VGA_320_W);
        embers[i].y = (float)(rand() % 80 - 80);
        embers[i].speed = (rand() % 200) / 100.0f + 1.2f;
        embers[i].color = (rand() % 2 == 0) ? 253 : 254;
    }
}

static void draw_outro_char(uint8_t *fb, char c, int x0, int y0, uint8_t color)
{
    const uint8_t *glyph = font8x8_glyph(c);
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            if (row & (1 << (7 - gx))) {
                /* Scale characters 2x horizontally and 2x vertically */
                for (int sy = 0; sy < 2; sy++) {
                    int py = y0 + gy * 2 + sy;
                    if (py < 0 || py >= VGA_320_H) continue;
                    for (int sx = 0; sx < 2; sx++) {
                        int px = x0 + gx * 2 + sx;
                        if (px < 0 || px >= VGA_320_W) continue;
                        fb[py * VGA_320_W + px] = color;
                    }
                }
            }
        }
    }
}

static void draw_outro_text_center(uint8_t *fb, const char *text, int cy, uint8_t color)
{
    int len = strlen(text);
    int char_w = 8 * 2;
    int total_w = len * char_w;
    int start_x = VGA_320_W / 2 - total_w / 2;

    for (int i = 0; i < len; i++) {
        draw_outro_char(fb, text[i], start_x + i * char_w, cy, color);
    }
}

void fx_voltage_arc_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();
    float t_sec = t_ms * 0.001f;

    /* Beat pulse */
    uint32_t beat_interval = 923; 
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.005f);

    /* -------------------------------------------------------------
     * 1. 32-BIT FIXED POINT REAL-TIME ROTOZOOMER
     * Fast incremental stepping (DDA) on coordinates (u, v)
     * ------------------------------------------------------------- */
    float rot_angle = sinf(t_sec * 0.3f) * 0.4f; /* Shifting wobble angle */
    
    /* Dynamic scaling: Zooms outward gradually, bounces on beat */
    float scale_base = 1.0f + t_sec * 0.04f;
    float scale = scale_base - beat_pulse * 0.15f;

    /* Convergence ending (collapses to single spot for final spark) */
    if (t_ms > 29000) {
        float collapse_t = (t_ms - 29000) / 3000.0f;
        if (collapse_t > 1.0f) collapse_t = 1.0f;
        scale += collapse_t * 8.0f; /* Zoom out deeply */
        rot_angle += collapse_t * 3.14159f; /* Quick camera spin */
    }

    float cos_a = cosf(rot_angle) * scale;
    float sin_a = sinf(rot_angle) * scale;

    /* 16.16 fixed-point steps */
    int32_t dx_u = (int32_t)(cos_a * 65536.0f);
    int32_t dx_v = (int32_t)(sin_a * 65536.0f);
    int32_t dy_u = (int32_t)(-sin_a * 65536.0f);
    int32_t dy_v = (int32_t)(cos_a * 65536.0f);

    int32_t center_u = (160 << 16);
    int32_t center_v = (120 << 16);

    for (int y = 0; y < VGA_320_H; y++) {
        /* Set initial coordinates at the start of the row */
        int32_t u = center_u + (y - 120) * dy_u - (160 * dx_u);
        int32_t v = center_v + (y - 120) * dy_v - (160 * dx_v);
        int row_offset = y * VGA_320_W;

        for (int x = 0; x < VGA_320_W; x++) {
            /* Map coordinates and wrap safely to 320x240 bounds */
            int32_t tu = (u >> 16) % 320; if (tu < 0) tu += 320;
            int32_t tv = (v >> 16) % 240; if (tv < 0) tv += 240;

            fb[row_offset + x] = asset_endcard_bg_data[tv * 320 + tu];

            u += dx_u;
            v += dx_v;
        }
    }

    /* -------------------------------------------------------------
     * 2. CREDITS VERTICAL SCROLLER
     * Shortened elegantly to fit screen under 20 chars at 2x scale.
     * ------------------------------------------------------------- */
    const char *credits[] = {
        "VOLTAGE",
        "",
        "AN ORIGINAL",
        "RP2350 DEMO",
        "",
        "VISUAL EFFECTS",
        "AND DIRECTION",
        "PAIR-PROGRAMMED BY",
        "ANTIGRAVITY",
        "",
        "ENGINE DESIGN",
        "AND HARNESS",
        "GEMINI 3 FLASH",
        "AND AZURE",
        "",
        "SOUNDTRACK",
        "SUNO & ANTIGRAVITY",
        "",
        "TARGET HARDWARE",
        "WAVESHARE RP2350",
        "PIMORONI VGA BASE",
        "",
        "RELEASED MAY 2026",
        "",
        "THANKS FOR WATCHING!",
        "",
        "ELECTRICITY FADES..."
    };
    int num_lines = sizeof(credits) / sizeof(credits[0]);
    int line_h = 24;

    /* Upward scroll displacement */
    int start_scroll_y = 260 - (int)(t_ms * 0.03f); 

    for (int i = 0; i < num_lines; i++) {
        int cy = start_scroll_y + i * line_h;
        if (cy > -20 && cy < VGA_320_H + 20) {
            uint8_t color = (i % 2 == 0) ? 253 : 254;
            if (i == 0 || i == num_lines - 1) color = 255; /* White-Hot highlight */
            draw_outro_text_center(fb, credits[i], cy, color);
        }
    }

    /* -------------------------------------------------------------
     * 3. RAINING ELECTRICAL SPARK EMBERS
     * ------------------------------------------------------------- */
    for (int i = 0; i < NUM_EMBERS; i++) {
        Ember *e = &embers[i];
        e->y += e->speed;
        
        /* Sway slightly left-to-right */
        e->x += sinf(t_sec * 2.0f + i) * 0.3f;

        if (e->y >= VGA_320_H) {
            e->x = (float)(rand() % VGA_320_W);
            e->y = -10.0f;
            e->speed = (rand() % 200) / 100.0f + 1.2f;
        }

        int px = (int)e->x;
        int py = (int)e->y;
        if (px >= 0 && px < VGA_320_W && py >= 0 && py < VGA_320_H) {
            fb[py * VGA_320_W + px] = e->color;
        }
    }

    /* -------------------------------------------------------------
     * 4. FINAL CONVERGENCE TO SINGULAR WHITE FLASH SPARK (t > 29s)
     * ------------------------------------------------------------- */
    if (t_ms > 29000) {
        float spark_t = (t_ms - 29000) / 3000.0f; /* 0..1 range */
        if (spark_t > 1.0f) spark_t = 1.0f;

        /* Erase background / contents in a shrinking tunnel iris shape */
        int r_iris = (int)((1.0f - spark_t) * 200.0f);
        if (r_iris < 0) r_iris = 0;

        for (int y = 0; y < VGA_320_H; y++) {
            int dy = y - 120;
            for (int x = 0; x < VGA_320_W; x++) {
                int dx = x - 160;
                int d2 = dx*dx + dy*dy;
                if (d2 > r_iris * r_iris) {
                    fb[y * VGA_320_W + x] = 0; /* Clear to pitch black */
                }
            }
        }

        /* Central electrical flash spark */
        if (spark_t < 0.95f) {
            /* Alternating strobe size */
            int r_spark = (t_ms % 100 < 50) ? 3 : 1;
            for (int y = 120 - r_spark; y <= 120 + r_spark; y++) {
                for (int x = 160 - r_spark; x <= 160 + r_spark; x++) {
                    fb[y * VGA_320_W + x] = 255; /* White-Hot energy */
                }
            }
        }
    }

    /* Global fade-out mask */
    float fade = 1.0f;
    if (t_ms > 31700) {
        fade = (33200 - t_ms) / 1500.0f;
        if (fade < 0.0f) fade = 0.0f;
    }

    if (fade < 1.0f) {
        int threshold = (int)(256.0f * (1.0f - fade));
        for (int y = 0; y < VGA_320_H; y++) {
            for (int x = 0; x < VGA_320_W; x++) {
                if (((x ^ y) & 0xFF) < threshold) {
                    fb[y * VGA_320_W + x] = 0;
                }
            }
        }
    }
}

void fx_voltage_arc_done(void)
{
}

const effect_t fx_voltage_arc = {
    "Voltage Arc",
    MODE_320,
    fx_voltage_arc_init,
    fx_voltage_arc_frame,
    fx_voltage_arc_done
};
