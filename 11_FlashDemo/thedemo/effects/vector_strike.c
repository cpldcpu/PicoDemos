/* VOLTAGE demoscene - Scene 4: Vector Strike (High-Res 3D Wireframe & Floating Scroller)
 *
 * Mode: MODE_320 (320x240 palettized, 8bpp)
 * Visuals: Full-screen rotating 3D morphing wireframe vectors drawn in high resolution,
 *          scrolling greetings text floating over a retro sci-fi scanline band, and
 *          a premium electric bokeh backdrop.
 */

#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "../font8x8.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define NUM_VERTICES 32
#define NUM_EDGES    60

/* Fast Bresenham line drawing for fb320 palettized buffer */
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

/* 3D vector model edge definition */
typedef struct {
    uint8_t u, v;
} Edge;

static Edge star_edges[NUM_EDGES];
static int star_edges_init = 0;

void fx_vector_strike_init(void)
{
    /* Initialize the 3D star edge connectivity (once) */
    if (!star_edges_init) {
        int edge_idx = 0;
        /* Outer points to inner center points */
        for (int i = 0; i < 8; i++) {
            /* Outer vertices: 0..7
             * Mid vertices: 8..15
             * Inner vertices: 16..31 */
            star_edges[edge_idx++] = (Edge){ (uint8_t)i, (uint8_t)((i + 1) % 8) };
            star_edges[edge_idx++] = (Edge){ (uint8_t)i, (uint8_t)(8 + i) };
            star_edges[edge_idx++] = (Edge){ (uint8_t)(8 + i), (uint8_t)(16 + i) };
            star_edges[edge_idx++] = (Edge){ (uint8_t)(8 + i), (uint8_t)(16 + ((i + 1) % 8)) };
            star_edges[edge_idx++] = (Edge){ (uint8_t)(16 + i), (uint8_t)(24 + i) };
            star_edges[edge_idx++] = (Edge){ (uint8_t)(24 + i), (uint8_t)(16 + ((i + 3) % 8)) };
        }
        /* Extra structural wireframe edges to pad the model */
        for (int i = 0; i < 12; i++) {
            star_edges[edge_idx++] = (Edge){ (uint8_t)(16 + i % 8), (uint8_t)(24 + (i + 4) % 8) };
        }
        star_edges_init = 1;
    }

    /* Set up custom neon colors and greeting backdrop palette */
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_greetz_bg_pal[i];
        /* Unpack Pimoroni custom BGR555 layout: Blue in bits 15..11, Green in bits 10..6, Red in bits 4..0 */
        uint8_t b = ((c >> 11) & 0x1F) << 3;
        uint8_t g = ((c >> 6) & 0x1F) << 3;
        uint8_t r = (c & 0x1F) << 3;
        vga_320_palette_set(i, r, g, b);
    }

    /* Assign palette slots for glowing vector lines in the upper reserved area (224..253)
     * Slot 224..238: Neon Cyan-electric gradient (15 colors)
     * Slot 239..253: Neon Pink-electric gradient (15 colors) */
    for (int i = 0; i < 15; i++) {
        float t = i / 14.0f;
        // Cyan-electric
        uint8_t r = (uint8_t)(0   * (1.0f - t) + 180 * t);
        uint8_t g = (uint8_t)(150 * (1.0f - t) + 255 * t);
        uint8_t b = (uint8_t)(255 * (1.0f - t) + 255 * t);
        vga_320_palette_set(224 + i, r, g, b);

        // Pink-electric
        uint8_t r2 = (uint8_t)(180 * (1.0f - t) + 255 * t);
        uint8_t g2 = (uint8_t)(0);
        uint8_t b2 = (uint8_t)(120 * (1.0f - t) + 255 * t);
        vga_320_palette_set(239 + i, r2, g2, b2);
    }

    /* Index 254 and 255: Pure White-hot flash for scroller text and glowing accents */
    vga_320_palette_set(254, 255, 255, 255);
    vga_320_palette_set(255, 255, 255, 255);
}

static void draw_scroller_char(uint8_t *fb, char c, int x0, int y0, uint8_t color)
{
    const uint8_t *glyph = font8x8_glyph(c);
    for (int gy = 0; gy < 8; gy++) {
        uint8_t row = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            /* MSB-first bits to prevent mirroring */
            if (row & (1 << (7 - gx))) {
                /* Scale 2x horizontally and 3x vertically for premium retro presence */
                for (int sy = 0; sy < 3; sy++) {
                    int py = y0 + gy * 3 + sy;
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

void fx_vector_strike_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb320 = vga_320_back_buffer();
    float t_sec = t_ms * 0.001f;

    /* Get beat sync for morphing */
    uint32_t beat_interval = 923; 
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.005f);

    /* Dynamic fade tracking */

    /* 1. Clear with electric greetz backdrop */
    memcpy(fb320, asset_greetz_bg_data, ASSET_GREETZ_BG_SIZE);

    /* 2. Compute 3D rotations */
    float rx_ang = t_sec * 0.9f;
    float ry_ang = t_sec * 1.3f;
    float rz_ang = t_sec * 0.5f;

    float cos_x = cosf(rx_ang), sin_x = sinf(rx_ang);
    float cos_y = cosf(ry_ang), sin_y = sinf(ry_ang);
    float cos_z = cosf(rz_ang), sin_z = sinf(rz_ang);

    /* Project morphing vertices */
    int screen_x[NUM_VERTICES];
    int screen_y[NUM_VERTICES];

    float morph_t = sinf(t_sec * 1.5f) * 0.5f + 0.5f; /* 0..1 morph wave */
    morph_t += beat_pulse * 0.3f;
    if (morph_t > 1.0f) morph_t = 1.0f;

    for (int i = 0; i < NUM_VERTICES; i++) {
        /* Sphere coordinates mixed with star points */
        float u = (i % 8) / 8.0f * 2.0f * 3.14159f;
        float v = (i / 8) / 4.0f * 3.14159f;

        /* Shape A: Rotating double-torus */
        float radius_a = 1.0f;
        if (i < 8) radius_a = 1.3f;
        else if (i < 16) radius_a = 0.6f;
        else radius_a = 0.9f;

        float x_a = cosf(u) * sinf(v) * radius_a;
        float y_a = sinf(u) * sinf(v) * radius_a;
        float z_a = cosf(v) * radius_a;

        /* Shape B: Perfect octahedron */
        float x_b = (i % 2 == 0) ? 1.0f : -1.0f;
        float y_b = ((i / 2) % 2 == 0) ? 1.0f : -1.0f;
        float z_b = ((i / 4) % 2 == 0) ? 1.0f : -1.0f;
        float len_b = sqrtf(x_b*x_b + y_b*y_b + z_b*z_b);
        x_b /= len_b; y_b /= len_b; z_b /= len_b;

        /* Interpolated morphing shape */
        float x3 = x_a * (1.0f - morph_t) + x_b * morph_t;
        float y3 = y_a * (1.0f - morph_t) + y_b * morph_t;
        float z3 = z_a * (1.0f - morph_t) + z_b * morph_t;

        /* 3D Rotations */
        // Y-axis
        float x_rot = x3 * cos_y - z3 * sin_y;
        float z_rot = x3 * sin_y + z3 * cos_y;
        float y_rot = y3;

        // X-axis
        float y_rot2 = y_rot * cos_x - z_rot * sin_x;
        float z_rot2 = y_rot * sin_x + z_rot * cos_x;

        // Z-axis
        float x_final = x_rot * cos_z - y_rot2 * sin_z;
        float y_final = x_rot * sin_z + y_rot2 * cos_z;
        float z_final = z_rot2 + 2.5f; /* Translate back */

        /* High-resolution 320x240 perspective projection */
        screen_x[i] = (int)(VGA_320_W / 2 + (x_final * 180.0f) / z_final);
        screen_y[i] = (int)(VGA_320_H / 2 + (y_final * 180.0f) / z_final);
    }

    /* Draw wireframe lines in full-screen high resolution */
    for (int i = 0; i < NUM_EDGES; i++) {
        int u = star_edges[i].u;
        int v = star_edges[i].v;

        /* Alternate colors between glowing cyan (224-238) and electric pink (239-253) on beat */
        int col_idx = (int)(14.0f * (sinf(t_sec * 4.0f + i * 0.1f) * 0.5f + 0.5f));
        uint8_t line_color = 224 + col_idx; /* Glowing cyan */
        if (beat_pulse > 0.4f && (i % 3 == 0)) {
            line_color = 239 + col_idx; /* Pulsate pink edge */
        }
        
        draw_line_320(fb320, screen_x[u], screen_y[u], screen_x[v], screen_y[v], line_color);
    }

    /* 3. Draw a premium sci-fi horizontal scanline band for scroller readability */
    for (int y = 195; y < 235; y++) {
        if (y % 2 == 0) {
            memset(&fb320[y * VGA_320_W], 0, VGA_320_W); /* Solid black scanline */
        }
    }

    /* 4. Render scrolling greeting text floating over the scanline band */
    const char *scroll_msg = "    *** FLASHPOINT DEMO ***    VOLTAGE RUNS ON DUAL ARM CORTEX-M33 AT 300MHZ!    GREETINGS TO ALL CREATIVE CODELVERS: FARBRAUSCH - CONSPIRACY - SHENANIGANS - ALIVE - RECKONING - AZURE!    PAIR-PROGRAMMED BY ANTIGRAVITY POWERED BY GEMINI 3 FLASH!   PRESS SPACE TO ADVANCE    ";
    int msg_len = strlen(scroll_msg);
    int char_w = 8 * 2; /* 2x scale width */
    int text_scrolled = (int)(t_ms * 0.08f); /* scroll speed pixels */
    int char_offset = text_scrolled / char_w;
    int pixel_shift = text_scrolled % char_w;

    int draw_y = 202;
    for (int draw_col = 0; draw_col < (VGA_320_W / char_w) + 2; draw_col++) {
        int char_idx = (char_offset + draw_col) % msg_len;
        char c = scroll_msg[char_idx];
        int draw_x = draw_col * char_w - pixel_shift;
        
        /* Highlight character with electric white face */
        draw_scroller_char(fb320, c, draw_x, draw_y, 255);
    }

    /* Dynamic fade-in/out transitions */
    uint32_t fade_duration = 1500;
    uint32_t scene_duration = 25000;
    
    if (t_ms < fade_duration) {
        /* Fade-In: Horizontal scanline blind wipe */
        float fade = t_ms / (float)fade_duration;
        int blind_height = 12;
        int step = (int)(fade * blind_height);
        for (int y = 0; y < VGA_320_H; y++) {
            if ((y % blind_height) >= step) {
                memset(&fb320[y * VGA_320_W], 0, VGA_320_W);
            }
        }
    } else if (t_ms > (scene_duration - fade_duration)) {
        /* Fade-Out: Legendary CRT Screen Shut Down Glitch! */
        float f_crt = (scene_duration - t_ms) / (float)fade_duration;
        if (f_crt < 0.0f) f_crt = 0.0f;

        if (f_crt > 0.2f) {
            /* Vertical collapse */
            float norm_h = (f_crt - 0.2f) / 0.8f; /* 1.0 -> 0.0 */
            int half_h = (int)(VGA_320_H * 0.5f * norm_h);
            int cy = VGA_320_H / 2;

            for (int y = 0; y < VGA_320_H; y++) {
                int dy = abs(y - cy);
                if (dy > half_h) {
                    memset(&fb320[y * VGA_320_W], 0, VGA_320_W);
                } else if (dy == half_h && half_h > 2) {
                    /* Glowing white phosphor collapsing scanline */
                    memset(&fb320[y * VGA_320_W], 255, VGA_320_W);
                }
            }
        } else {
            /* Screen collapsed to a thin center line, now horizontal shrink! */
            float norm_w = f_crt / 0.2f; /* 1.0 -> 0.0 */
            int half_w = (int)(VGA_320_W * 0.5f * norm_w);
            int cx = VGA_320_W / 2;
            int cy = VGA_320_H / 2;

            /* Clear everything except the phosphor dot line */
            for (int y = 0; y < VGA_320_H; y++) {
                if (y != cy) {
                    memset(&fb320[y * VGA_320_W], 0, VGA_320_W);
                } else {
                    for (int x = 0; x < VGA_320_W; x++) {
                        int dx = abs(x - cx);
                        if (dx > half_w) {
                            fb320[y * VGA_320_W + x] = 0;
                        } else {
                            fb320[y * VGA_320_W + x] = 255; /* Glowing white phosphor dot */
                        }
                    }
                }
            }
        }
    }
}

void fx_vector_strike_done(void)
{
}

const effect_t fx_vector_strike = {
    "Vector Strike",
    MODE_320, /* Upgraded to high-res full-screen */
    fx_vector_strike_init,
    fx_vector_strike_frame,
    fx_vector_strike_done
};
