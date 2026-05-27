/* SDL2 implementation of vga.h for desktop testing.
 *
 * Mirrors thedemo/vga.c (the RP2350 scanvideo backend), but renders into
 * an SDL_Texture and presents via SDL_RenderCopy at 3× upscale (960×720
 * window). Same public API — effects don't need to know they're on a PC.
 *
 * Implements MODE_320, MODE_160 and MODE_SPLIT_160_OVER_320 — same set
 * as the device backend after the MODE_640 stub was removed.
 *
 * Hidden controls in the SDL window:
 *   ESC / Q : quit
 *   S       : screenshot to screenshots/screenshot_NNN.bmp
 *   SPACE   : skip to next scene (main loop seeks audio + clock)
 */

#include "../vga.h"
#include "../rgb565.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>     /* _mkdir on Windows / MSYS2 mingw */

#define SCALE        3
#define WIN_W        (VGA_320_W * SCALE)
#define WIN_H        (VGA_320_H * SCALE)

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;       /* ARGB8888, 320x240 — RenderCopy upscales */

static uint8_t  fb320[VGA_320_W * VGA_320_H];
static uint32_t pal_argb[256];
static screen_mode_t g_mode = MODE_320;

/* MODE_160 fb declared here so take_screenshot() (which lives above the
 * MODE_160 implementation block) can see it. */
static uint16_t fb160[VGA_160_W * VGA_160_H];

/* --- input / lifecycle hooks ------------------------------------------ */

static int g_quit       = 0;
static int g_paused        = 0;     /* (reserved — not currently wired) */
static int g_skip_request  = 0;     /* set by SPACE; cleared on consume */
static int g_screenshot_seq = 0;
int g_offline = 0;

int  vga_should_quit(void) { return g_quit; }
int  vga_paused(void)      { return g_paused; }

int  vga_consume_skip_request(void)
{
    int r = g_skip_request;
    g_skip_request = 0;
    return r;
}

/* Exported for main_host.c's --screenshot-at flag. */
void vga_screenshot(void);

static void take_screenshot(void)
{
    /* Keep the host folder uncluttered — all BMP captures go into
     * host/screenshots/. _mkdir is idempotent (returns -1 with errno
     * EEXIST if the folder is already there, which we ignore). */
    _mkdir("screenshots");

    char name[80];
    if (g_offline) {
        snprintf(name, sizeof name, "screenshots/frame_%05d.bmp", g_screenshot_seq++);
    } else {
        snprintf(name, sizeof name, "screenshots/screenshot_%03d.bmp", g_screenshot_seq++);
    }
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, VGA_320_W, VGA_320_H, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) { printf("screenshot: surface alloc failed: %s\n", SDL_GetError()); return; }
    uint32_t *px = (uint32_t *)s->pixels;
    int stride = s->pitch / 4;

    /* Compose the screenshot from whichever fb the active mode actually
     * uses. For MODE_SPLIT we need to honour the per-scanline split row
     * just like vga_split_present does at scanout. */
    extern int g_split_row;
    for (int y = 0; y < VGA_320_H; y++) {
        int from_fb160 = (g_mode == MODE_160) ||
                         (g_mode == MODE_SPLIT_160_OVER_320 && y < g_split_row);
        if (from_fb160) {
            int ys = y >> 1;
            if (ys >= VGA_160_H) ys = VGA_160_H - 1;
            for (int x = 0; x < VGA_320_W; x++) {
                int xs = x >> 1;
                uint16_t c = fb160[ys * VGA_160_W + xs];
                int r5 = rgb565_r5(c);
                int g5 = rgb565_g5(c);
                int b5 = rgb565_b5(c);
                int r  = (r5 << 3) | (r5 >> 2);
                int g  = (g5 << 3) | (g5 >> 2);
                int b  = (b5 << 3) | (b5 >> 2);
                px[y * stride + x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        } else {
            for (int x = 0; x < VGA_320_W; x++)
                px[y * stride + x] = pal_argb[fb320[y * VGA_320_W + x]];
        }
    }

    if (SDL_SaveBMP(s, name) != 0) printf("SDL_SaveBMP failed: %s\n", SDL_GetError());
    else                            printf("wrote %s\n", name);
    SDL_FreeSurface(s);
}

void vga_screenshot(void) { take_screenshot(); }

static void pump_events(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) { g_quit = 1; }
        else if (ev.type == SDL_KEYDOWN) {
            switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: case SDLK_q: g_quit = 1; break;
                case SDLK_s:                   take_screenshot(); break;
                case SDLK_SPACE:               g_skip_request =  1; break;
                case SDLK_LEFT:                g_skip_request = -1; break;
                case SDLK_r:                   g_skip_request =  2; break;
                default: break;
            }
        }
    }
}

/* --- public API ------------------------------------------------------- */

void vga_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError());
        return;
    }
    win = SDL_CreateWindow("SLOP (host)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, 0);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return; }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return; }

    /* Integer scaling so the chunky 320×240 image stays crisp. */
    SDL_RenderSetLogicalSize(ren, VGA_320_W, VGA_320_H);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");   /* nearest neighbour */

    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                            SDL_TEXTUREACCESS_STREAMING, VGA_320_W, VGA_320_H);
    if (!tex) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return; }

    /* Grey ramp default (same as device backend). */
    for (int i = 0; i < 256; i++)
        pal_argb[i] = 0xFF000000u | (i << 16) | (i << 8) | i;
    memset(fb320, 0, sizeof fb320);

    g_mode = MODE_320;
    printf("vga_sdl: window %dx%d (3x scale of %dx%d)\n",
           WIN_W, WIN_H, VGA_320_W, VGA_320_H);
}

void vga_set_mode(screen_mode_t mode)
{
    if (mode == g_mode) return;
    printf("vga_sdl: mode change %d -> %d (only MODE_320 implemented for host)\n",
           g_mode, mode);
    g_mode = mode;
}

screen_mode_t vga_current_mode(void) { return g_mode; }

uint8_t *vga_320_back_buffer(void) { return fb320; }

void vga_320_palette_set(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    if ((unsigned)idx >= 256) return;
    pal_argb[idx] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

void vga_320_present(void)
{
    /* Convert palette indices to ARGB8888 directly into the streaming
     * texture. */
    void *pixels = NULL;
    int   pitch  = 0;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture: %s\n", SDL_GetError());
        return;
    }
    uint32_t *out = (uint32_t *)pixels;
    int stride = pitch / 4;
    for (int y = 0; y < VGA_320_H; y++) {
        const uint8_t *src = &fb320[y * VGA_320_W];
        uint32_t *dst = out + y * stride;
        for (int x = 0; x < VGA_320_W; x++) dst[x] = pal_argb[src[x]];
    }
    SDL_UnlockTexture(tex);

    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);

    pump_events();
}

/* --- MODE_160 implementation -----------------------------------------
 *
 * Same display window (960x720) as MODE_320, but the texture is
 * 160x120 RGB565 and SDL_RenderCopy upscales to fit. Output looks
 * identical in shape to the Pico's 2x line-doubled + 2x pixel-doubled
 * render (= 320x240 effective, displayed at 3x).
 *
 * To keep present() simple we lazy-create a second texture for MODE_160
 * the first time it's needed. Mode switches just swap which texture
 * the present routine uses.
 */

static SDL_Texture *tex160 = NULL;

uint16_t *vga_160_back_buffer(void) { return fb160; }

void vga_160_present(void)
{
    if (!tex160) {
        tex160 = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565,
                                   SDL_TEXTUREACCESS_STREAMING, VGA_160_W, VGA_160_H);
        if (!tex160) {
            fprintf(stderr, "SDL_CreateTexture(RGB565): %s\n", SDL_GetError());
            return;
        }
    }
    /* Copy fb160 into the streaming texture. We convert from our custom
     * Pimoroni RGB555-with-gap layout to standard SDL RGB565 layout so the
     * host preview display matches the real RP2350 hardware perfectly. */
    void *pixels = NULL;
    int   pitch  = 0;
    if (SDL_LockTexture(tex160, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture(160): %s\n", SDL_GetError());
        return;
    }
    uint16_t *dst = (uint16_t *)pixels;
    int stride = pitch / 2;
    for (int y = 0; y < VGA_160_H; y++) {
        const uint16_t *src = &fb160[y * VGA_160_W];
        uint16_t *row_dst = dst + y * stride;
        for (int x = 0; x < VGA_160_W; x++) {
            uint16_t c = src[x];
            int r5 = rgb565_r5(c);
            int g5 = rgb565_g5(c);
            int b5 = rgb565_b5(c);
            /* Repack to standard RGB565: [ r5 (5) | g5 shifted to 6-bit (6) | b5 (5) ] */
            row_dst[x] = (uint16_t)((r5 << 11) | (g5 << 6) | b5);
        }
    }
    SDL_UnlockTexture(tex160);

    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex160, NULL, NULL);
    SDL_RenderPresent(ren);

    pump_events();
}

/* --- MODE_SPLIT_160_OVER_320 ------------------------------------------
 *
 * Mirrors the device's per-scanline source-switch: composes a single
 * 320×240 ARGB8888 output where rows [0, split_row) come from fb160
 * (RGB565, 2× pixel-doubled to fit 320×240) and rows [split_row, 240)
 * come from fb320 (palette 8bpp). */

int g_split_row = VGA_320_H;     /* not static — take_screenshot reads it */

void vga_set_split_row(int row)
{
    if (row < 0)         row = 0;
    if (row > VGA_320_H) row = VGA_320_H;
    g_split_row = row;
}

void vga_split_present(void)
{
    static int once = 0;
    if (!once) { printf("[host] vga_split_present called, split_row=%d, fb160[0]=0x%04x\n",
                        g_split_row, fb160[0]); once = 1; }
    void *pixels = NULL;
    int   pitch  = 0;
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture(split): %s\n", SDL_GetError());
        return;
    }
    uint32_t *out = (uint32_t *)pixels;
    int stride = pitch / 4;

    for (int y = 0; y < VGA_320_H; y++) {
        uint32_t *dst = out + y * stride;
        if (y < g_split_row) {
            /* fb160 RGB565 → ARGB8888, 2× horizontal pixel-doubling and
             * 2× line-doubling (each src row covers 2 dst rows). */
            int y_src = y >> 1;
            if (y_src >= VGA_160_H) y_src = VGA_160_H - 1;
            const uint16_t *src = &fb160[y_src * VGA_160_W];
            for (int x = 0; x < VGA_320_W; x++) {
                uint16_t c = src[x >> 1];
                int r5 = rgb565_r5(c);
                int g5 = rgb565_g5(c);
                int b5 = rgb565_b5(c);
                int r  = (r5 << 3) | (r5 >> 2);
                int g  = (g5 << 3) | (g5 >> 2);
                int b  = (b5 << 3) | (b5 >> 2);
                dst[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
            }
        } else {
            const uint8_t *src = &fb320[y * VGA_320_W];
            for (int x = 0; x < VGA_320_W; x++) dst[x] = pal_argb[src[x]];
        }
    }
    SDL_UnlockTexture(tex);

    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);

    pump_events();
}
