#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "../rgb565.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define NUM_VERTICES 16
#define NUM_EDGES    32

// 3D vector model edge definition
typedef struct {
    uint8_t u, v;
} Edge;

static const Edge logo_edges[NUM_EDGES] = {
    // Outer cube (0..7)
    {0, 1}, {1, 3}, {3, 2}, {2, 0},
    {4, 5}, {5, 7}, {7, 6}, {6, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
    
    // Inner cube (8..15)
    {8, 9}, {9, 11}, {11, 10}, {10, 8},
    {12, 13}, {13, 15}, {15, 14}, {14, 12},
    {8, 12}, {9, 13}, {10, 14}, {11, 15},
    
    // Connections
    {0, 8}, {1, 9}, {2, 10}, {3, 11},
    {4, 12}, {5, 13}, {6, 14}, {7, 15}
};

static inline void draw_pixel_aa(uint8_t *fb, int x, int y, int base, float brightness)
{
    if (x >= 0 && x < VGA_320_W && y >= 0 && y < VGA_320_H) {
        int level = (int)(brightness * 7.99f);
        if (level < 0) level = 0;
        if (level > 7) level = 7;
        fb[y * VGA_320_W + x] = (uint8_t)(base + level);
    }
}

// Helper functions and macros for Xiaolin Wu line drawing
#define ipart_(x) ((int)(x))
#define round_(x) ((int)((x) + 0.5f))
#define fpart_(x) ((x) - (int)(x))
#define rfpart_(x) (1.0f - fpart_(x))

static void draw_line_aa(uint8_t *fb, float x0, float y0, float x1, float y1, int base)
{
    int steep = fabsf(y1 - y0) > fabsf(x1 - x0);
    if (steep) {
        float tmp = x0; x0 = y0; y0 = tmp;
        tmp = x1; x1 = y1; y1 = tmp;
    }
    if (x0 > x1) {
        float tmp = x0; x0 = x1; x1 = tmp;
        tmp = y0; y0 = y1; y1 = tmp;
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dx == 0.0f ? 1.0f : dy / dx;

    // Handle first endpoint
    int xend = round_(x0);
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart_(x0 + 0.5f);
    int xpxl1 = xend;
    int ypxl1 = ipart_(yend);

    if (steep) {
        draw_pixel_aa(fb, ypxl1, xpxl1, base, rfpart_(yend) * xgap);
        draw_pixel_aa(fb, ypxl1 + 1, xpxl1, base, fpart_(yend) * xgap);
    } else {
        draw_pixel_aa(fb, xpxl1, ypxl1, base, rfpart_(yend) * xgap);
        draw_pixel_aa(fb, xpxl1, ypxl1 + 1, base, fpart_(yend) * xgap);
    }
    float intery = yend + gradient;

    // Handle second endpoint
    xend = round_(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart_(x1 + 0.5f);
    int xpxl2 = xend;
    int ypxl2 = ipart_(yend);

    if (steep) {
        draw_pixel_aa(fb, ypxl2, xpxl2, base, rfpart_(yend) * xgap);
        draw_pixel_aa(fb, ypxl2 + 1, xpxl2, base, fpart_(yend) * xgap);
    } else {
        draw_pixel_aa(fb, xpxl2, ypxl2, base, rfpart_(yend) * xgap);
        draw_pixel_aa(fb, xpxl2, ypxl2 + 1, base, fpart_(yend) * xgap);
    }

    // Main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            draw_pixel_aa(fb, ipart_(intery), x, base, rfpart_(intery));
            draw_pixel_aa(fb, ipart_(intery) + 1, x, base, fpart_(intery));
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            draw_pixel_aa(fb, x, ipart_(intery), base, rfpart_(intery));
            draw_pixel_aa(fb, x, ipart_(intery) + 1, base, fpart_(intery));
            intery += gradient;
        }
    }
}

void fx_dirty_logo_init(void)
{
    // Setup initial static palette values (will be updated dynamically during frame execution)
    vga_320_palette_set(0, 8, 4, 20);
    
    // Starfield colors: index 1 to 15 (bright white/cyan down to dark gray-blue)
    for (int i = 0; i < 15; i++) {
        float t = i / 14.0f;
        uint8_t r = (uint8_t)(255 * (1.0f - t) + 16 * t);
        uint8_t g = (uint8_t)(255 * (1.0f - t) + 24 * t);
        uint8_t b = (uint8_t)(255 * (1.0f - t) + 48 * t);
        vga_320_palette_set(1 + i, r, g, b);
    }
    
    // Grid neon colors: index 16 to 31 (Neon Magenta to Purple/Violet)
    for (int i = 0; i < 16; i++) {
        float t = i / 15.0f;
        uint8_t r = (uint8_t)(255 * (1.0f - t) + 80 * t);
        uint8_t g = 0;
        uint8_t b = (uint8_t)(255 * (1.0f - t) + 160 * t);
        vga_320_palette_set(16 + i, r, g, b);
    }

    // Four 8-level gradients for palettized antialiasing
    // Cyan gradient: indices 32..39
    for (int i = 0; i < 8; i++) {
        float t = i / 7.0f;
        uint8_t r = (uint8_t)(8 * (1.0f - t) + 0 * t);
        uint8_t g = (uint8_t)(4 * (1.0f - t) + 255 * t);
        uint8_t b = (uint8_t)(20 * (1.0f - t) + 255 * t);
        vga_320_palette_set(32 + i, r, g, b);
    }
    // Magenta gradient: indices 40..47
    for (int i = 0; i < 8; i++) {
        float t = i / 7.0f;
        uint8_t r = (uint8_t)(8 * (1.0f - t) + 255 * t);
        uint8_t g = (uint8_t)(4 * (1.0f - t) + 0 * t);
        uint8_t b = (uint8_t)(20 * (1.0f - t) + 255 * t);
        vga_320_palette_set(40 + i, r, g, b);
    }
    // Blue gradient: indices 48..55
    for (int i = 0; i < 8; i++) {
        float t = i / 7.0f;
        uint8_t r = (uint8_t)(8 * (1.0f - t) + 0 * t);
        uint8_t g = (uint8_t)(4 * (1.0f - t) + 100 * t);
        uint8_t b = (uint8_t)(20 * (1.0f - t) + 255 * t);
        vga_320_palette_set(48 + i, r, g, b);
    }
    // Purple gradient: indices 56..63
    for (int i = 0; i < 8; i++) {
        float t = i / 7.0f;
        uint8_t r = (uint8_t)(8 * (1.0f - t) + 160 * t);
        uint8_t g = (uint8_t)(4 * (1.0f - t) + 0 * t);
        uint8_t b = (uint8_t)(20 * (1.0f - t) + 255 * t);
        vga_320_palette_set(56 + i, r, g, b);
    }

    // Initialize 3D Starfield particles (indices 32..127 in shared px, py, pz)
    srand(42);
    for (int i = 32; i < 128; i++) {
        g_scratch.logo.px[i] = (float)((rand() % 300) - 150);
        g_scratch.logo.py[i] = (float)((rand() % 160) - 80);
        g_scratch.logo.pz[i] = 0.5f + (float)(rand() % 350) * 0.01f;
    }
}

void fx_dirty_logo_frame(uint32_t t_ms, uint32_t t_global)
{
    uint8_t *fb = vga_320_back_buffer();
    float t_sec = t_ms * 0.001f;

    // Transition timers
    float melt_progress = 0.0f;
    int is_melting = 0;
    float global_fade = 1.0f;
    uint32_t duration = 23150; // 72306 - 49156
    
    if (t_ms >= duration - 1000) {
        melt_progress = (t_ms - (duration - 1000)) / 1000.0f;
        global_fade = 1.0f - melt_progress;
        is_melting = 1;
    }

    // Dynamic palette fading to black during vector melt
    vga_320_palette_set(0, (uint8_t)(8 * global_fade), (uint8_t)(4 * global_fade), (uint8_t)(20 * global_fade));
    for (int i = 0; i < 15; i++) {
        float t_val = i / 14.0f;
        uint8_t r = (uint8_t)((255 * (1.0f - t_val) + 16 * t_val) * global_fade);
        uint8_t g = (uint8_t)((255 * (1.0f - t_val) + 24 * t_val) * global_fade);
        uint8_t b = (uint8_t)((255 * (1.0f - t_val) + 48 * t_val) * global_fade);
        vga_320_palette_set(1 + i, r, g, b);
    }
    for (int i = 0; i < 16; i++) {
        float t_val = i / 15.0f;
        uint8_t r = (uint8_t)((255 * (1.0f - t_val) + 80 * t_val) * global_fade);
        uint8_t g = 0;
        uint8_t b = (uint8_t)((255 * (1.0f - t_val) + 160 * t_val) * global_fade);
        vga_320_palette_set(16 + i, r, g, b);
    }
    // Neon gradients
    for (int i = 0; i < 8; i++) {
        float t_val = i / 7.0f;
        uint8_t r = (uint8_t)((8 * (1.0f - t_val) + 0 * t_val) * global_fade);
        uint8_t g = (uint8_t)((4 * (1.0f - t_val) + 255 * t_val) * global_fade);
        uint8_t b = (uint8_t)((20 * (1.0f - t_val) + 255 * t_val) * global_fade);
        vga_320_palette_set(32 + i, r, g, b);
    }
    for (int i = 0; i < 8; i++) {
        float t_val = i / 7.0f;
        uint8_t r = (uint8_t)((8 * (1.0f - t_val) + 255 * t_val) * global_fade);
        uint8_t g = (uint8_t)((4 * (1.0f - t_val) + 0 * t_val) * global_fade);
        uint8_t b = (uint8_t)((20 * (1.0f - t_val) + 255 * t_val) * global_fade);
        vga_320_palette_set(40 + i, r, g, b);
    }
    for (int i = 0; i < 8; i++) {
        float t_val = i / 7.0f;
        uint8_t r = (uint8_t)((8 * (1.0f - t_val) + 0 * t_val) * global_fade);
        uint8_t g = (uint8_t)((4 * (1.0f - t_val) + 100 * t_val) * global_fade);
        uint8_t b = (uint8_t)((20 * (1.0f - t_val) + 255 * t_val) * global_fade);
        vga_320_palette_set(48 + i, r, g, b);
    }
    for (int i = 0; i < 8; i++) {
        float t_val = i / 7.0f;
        uint8_t r = (uint8_t)((8 * (1.0f - t_val) + 160 * t_val) * global_fade);
        uint8_t g = (uint8_t)((4 * (1.0f - t_val) + 0 * t_val) * global_fade);
        uint8_t b = (uint8_t)((20 * (1.0f - t_val) + 255 * t_val) * global_fade);
        vga_320_palette_set(56 + i, r, g, b);
    }

    // Clear background to dark purple-blue index 0
    memset(fb, 0, VGA_320_W * VGA_320_H);

    // 1. Draw and update flying 3D starfield particles (collapsed to horizon if melting)
    float horizon_y = 100.0f;
    for (int i = 32; i < 128; i++) {
        g_scratch.logo.pz[i] -= 0.025f;
        if (g_scratch.logo.pz[i] < 0.2f) {
            g_scratch.logo.px[i] = (float)((rand() % 300) - 150);
            g_scratch.logo.py[i] = (float)((rand() % 160) - 80);
            g_scratch.logo.pz[i] = 4.0f;
        }

        float sz = g_scratch.logo.pz[i];
        int sx = (int)(160.0f + (g_scratch.logo.px[i] * 160.0f) / sz);
        
        // Squeeze Y height offsets vertically to horizon line during melt
        float py_val = g_scratch.logo.py[i];
        if (is_melting) py_val *= (1.0f - melt_progress);
        int sy = (int)(horizon_y + (py_val * 160.0f) / sz);

        if (sx >= 0 && sx < VGA_320_W && sy >= 0 && sy < VGA_320_H) {
            int star_col = 1 + (int)((sz - 0.2f) / 3.8f * 14.0f);
            if (star_col < 1) star_col = 1;
            if (star_col > 15) star_col = 15;
            fb[sy * VGA_320_W + sx] = (uint8_t)star_col;
        }
    }

    // 2. Draw scrolling perspective vector grid on the floor (collapsing vertically if melting)
    float z_scroll = fmodf(t_sec * 1.2f, 0.4f);
    // Draw transverse (horizontal) floor lines scrolling down
    for (int i = 0; i < 9; i++) {
        float Z = 0.5f + i * 0.4f - z_scroll;
        if (Z < 0.4f || Z > 4.0f) continue;
        
        // Floor height in 3D is 0.8f below center, collapsed to horizon during melt
        float floor_height = 0.8f;
        if (is_melting) floor_height *= (1.0f - melt_progress);
        
        int gy = (int)(horizon_y + (floor_height * 160.0f) / Z);
        if (gy >= 100 && gy < VGA_320_H) {
            int col_grad = (int)((Z - 0.4f) / 3.6f * 15.0f);
            if (col_grad < 0) col_grad = 0;
            if (col_grad > 15) col_grad = 15;
            uint8_t col = 16 + col_grad;
            
            memset(&fb[gy * VGA_320_W], col, VGA_320_W);
        }
    }
    // Draw longitudinal (receding) grid lines converging to vanishing point (160, 100)
    for (float gx = -2.0f; gx <= 2.0f; gx += 0.4f) {
        float floor_height = 0.8f;
        if (is_melting) floor_height *= (1.0f - melt_progress);
        
        float x_near = 160.0f + (gx * 160.0f) / 0.5f;
        float y_near = horizon_y + (floor_height * 160.0f) / 0.5f;
        float x_far = 160.0f + (gx * 160.0f) / 3.8f;
        float y_far = horizon_y + (floor_height * 160.0f) / 3.8f;
        
        draw_line_aa(fb, x_far, y_far, x_near, y_near, 56); // Neon Purple AA (base 56)
    }

    // 3. Render 3D morphing Tesseract
    uint32_t beat_interval = 464;
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.006f);

    float evolve = t_sec / 15.0f;
    if (evolve > 1.0f) evolve = 1.0f;

    // Quadratically accelerate rotations during melt
    float speed_mult = 1.0f;
    if (is_melting) speed_mult += melt_progress * melt_progress * 15.0f;
    
    float rx = t_sec * 0.7f * speed_mult;
    float ry = (t_sec * 1.1f + beat_pulse * 0.25f) * speed_mult;
    float rz = t_sec * 0.4f * speed_mult;

    float cos_x = cosf(rx), sin_x = sinf(rx);
    float cos_y = cosf(ry), sin_y = sinf(ry);
    float cos_z = cosf(rz), sin_z = sinf(rz);

    float screen_x[NUM_VERTICES];
    float screen_y[NUM_VERTICES];

    // Morph coordinates for the 16 Tesseract vertices
    for (int i = 0; i < NUM_VERTICES; i++) {
        float x_a, y_a, z_a;
        if (i < 8) {
            x_a = ((i & 1) ? 1.0f : -1.0f) * 0.85f;
            y_a = ((i & 2) ? 1.0f : -1.0f) * 0.85f;
            z_a = ((i & 4) ? 1.0f : -1.0f) * 0.85f;
        } else {
            int idx = i - 8;
            x_a = ((idx & 1) ? 1.0f : -1.0f) * 0.42f;
            y_a = ((idx & 2) ? 1.0f : -1.0f) * 0.42f;
            z_a = ((idx & 4) ? 1.0f : -1.0f) * 0.42f;
        }

        float x_b, y_b, z_b;
        if (i < 8) {
            x_b = x_a * 1.4f;
            y_b = y_a * 1.4f;
            z_b = z_a * 1.4f;
        } else {
            x_b = y_a * 0.2f;
            y_b = -z_a * 0.2f;
            z_b = -x_a * 0.2f;
        }

        // Interpolate shapes
        float x = x_a * (1.0f - evolve) + x_b * evolve;
        float y = y_a * (1.0f - evolve) + y_b * evolve;
        float z = z_a * (1.0f - evolve) + z_b * evolve;

        // Apply beat scaling & scale to 0 during melt
        float scale = 1.0f + beat_pulse * 0.22f;
        if (is_melting) scale *= (1.0f - melt_progress);
        
        x *= scale; y *= scale; z *= scale;

        // Rotate 3D
        // Y-axis
        float x1 = x * cos_y - z * sin_y;
        float z1 = x * sin_y + z * cos_y;
        float y1 = y;

        // X-axis
        float x2 = x1;
        float y2 = y1 * cos_x - z1 * sin_x;
        float z2 = y1 * sin_x + z1 * cos_x;

        // Z-axis
        float x3 = x2 * cos_z - y2 * sin_z;
        float y3 = x2 * sin_z + y2 * cos_z;
        float z3 = z2 + 2.2f;

        // Perspective projection to screen coordinate (centered at vanishing point 160, 100)
        screen_x[i] = 160.0f + (x3 * 150.0f) / z3;
        screen_y[i] = horizon_y + (y3 * 150.0f) / z3;
    }

    // Draw the 32 edges of the Tesseract using upgraded antialiasing
    for (int i = 0; i < NUM_EDGES; i++) {
        int u = logo_edges[i].u;
        int v = logo_edges[i].v;

        // Base color gradient selection (Cyan, Magenta, Blue)
        int gradient_sel = (i % 3);
        int base_col = 32 + gradient_sel * 8; // base 32, 40, 48

        draw_line_aa(fb, screen_x[u], screen_y[u], screen_x[v], screen_y[v], base_col);
    }
}

void fx_dirty_logo_done(void)
{
}

const effect_t fx_dirty_logo = {
    "Dirty Logo",
    MODE_320,
    fx_dirty_logo_init,
    fx_dirty_logo_frame,
    fx_dirty_logo_done
};
