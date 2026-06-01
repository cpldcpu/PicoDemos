/* SDL2 backend for vga.h — FULL VGA (640x480) host preview, matching the device.
 *
 * The device scans out at 640x480@60: framebuffer (MODE_HIRES) scenes render
 * into a 320x240 RGB565 buffer and are 2x pixel/line-doubled at scanout, and
 * MODE_RACE scenes generate each 640-wide line live via the registered beam-race
 * generator (the interpolator emulator). This backend reproduces both so the
 * host preview is pixel-identical to the hardware.
 *
 * Keys: ESC/Q quit, S screenshot, SPACE next scene, LEFT prev, R restart. */

#include "../vga.h"
#include "../rgb565.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>

#define OW VGA_RACE_W          /* 640 */
#define OH VGA_RACE_H          /* 480 */
#define WINSCALE 2             /* window = 1280x960 */

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;                 /* ARGB8888 640x480 */
static uint32_t      g_screen[OW * OH];   /* shadow, for screenshots */

static uint16_t      fb_hires[VGA_HIRES_W * VGA_HIRES_H];   /* 320x240 effects buffer */
static screen_mode_t g_mode = MODE_320;

static void (*g_race_fn)(uint16_t *, int) = NULL;
static void (*g_race_setup_fn)(void) = NULL;
static uint8_t g_race_sram[384 * 1024] __attribute__((aligned(4)));

void vga_screenshot(void);    /* forward decl (defined below, called by present) */

static int g_quit = 0, g_skip = 0, g_shot_seq = 0;
int g_offline = 0;
int  vga_should_quit(void) { return g_quit; }
int  vga_consume_skip_request(void) { int r = g_skip; g_skip = 0; return r; }

static inline uint32_t pio565_to_argb(uint16_t c) {
    int r5 = rgb565_r5(c), g5 = rgb565_g5(c), b5 = rgb565_b5(c);
    int r = (r5 << 3) | (r5 >> 2), g = (g5 << 3) | (g5 >> 2), b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static void present_screen(void) {
    SDL_UpdateTexture(tex, NULL, g_screen, OW * 4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) g_quit = 1;
        else if (ev.type == SDL_KEYDOWN) switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE: case SDLK_q: g_quit = 1; break;
            case SDLK_s: vga_screenshot(); break;
            case SDLK_SPACE: g_skip = 1; break;
            case SDLK_LEFT:  g_skip = -1; break;
            case SDLK_r:     g_skip = 2; break;
            default: break;
        }
    }
}

void vga_screenshot(void) {
    _mkdir("screenshots");
    char name[80];
    snprintf(name, sizeof name, g_offline ? "screenshots/frame_%05d.bmp"
                                          : "screenshots/screenshot_%03d.bmp", g_shot_seq++);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, OW, OH, 32, SDL_PIXELFORMAT_ARGB8888);
    memcpy(s->pixels, g_screen, sizeof g_screen);
    if (SDL_SaveBMP(s, name) == 0) printf("wrote %s\n", name);
    SDL_FreeSurface(s);
}

void vga_init(void) {
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    win = SDL_CreateWindow("QUICKSILVER (host, full VGA)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, OW * WINSCALE, OH * WINSCALE, 0);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, OW, OH);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, OW, OH);
    memset(fb_hires, 0, sizeof fb_hires);
    g_mode = MODE_320;
    printf("vga_sdl: full VGA %dx%d (window %dx%d)\n", OW, OH, OW * WINSCALE, OH * WINSCALE);
}

void vga_set_mode(screen_mode_t mode) {
    if (mode == g_mode) return;
    g_mode = mode;
    if (mode == MODE_RACE && g_race_setup_fn) g_race_setup_fn();
}
screen_mode_t vga_current_mode(void) { return g_mode; }

uint16_t *vga_hires_back_buffer(void) { return fb_hires; }

void vga_hires_present(void) {                 /* 320x240 -> 640x480, 2x doubled */
    for (int y = 0; y < OH; y++) {
        const uint16_t *src = &fb_hires[(y >> 1) * VGA_HIRES_W];
        uint32_t *dst = &g_screen[y * OW];
        for (int x = 0; x < OW; x++) dst[x] = pio565_to_argb(src[x >> 1]);
    }
    present_screen();
}

void vga_set_race_fn(void (*scan)(uint16_t *, int), void (*setup)(void)) {
    g_race_fn = scan; g_race_setup_fn = setup;
    if (g_mode == MODE_RACE && setup) setup();
}
uint8_t *vga_race_sram(void)      { return g_race_sram; }
unsigned vga_race_sram_size(void) { return (unsigned)sizeof(g_race_sram); }

void vga_race_present(void) {                  /* run the generator over 640x480 */
    static uint16_t line[OW];
    for (int y = 0; y < OH; y++) {
        if (g_race_fn) g_race_fn(line, y);
        else           memset(line, 0, sizeof line);
        uint32_t *dst = &g_screen[y * OW];
        for (int x = 0; x < OW; x++) dst[x] = pio565_to_argb(line[x]);
    }
    present_screen();
}

/* --- unused 320/160/split API (kept for link compatibility) ------------ */
static uint8_t  fb320[VGA_320_W * VGA_320_H];
static uint16_t fb160[VGA_160_W * VGA_160_H];
int g_split_row = VGA_320_H;
uint8_t  *vga_320_back_buffer(void) { return fb320; }
void      vga_320_palette_set(int i, uint8_t r, uint8_t g, uint8_t b) { (void)i;(void)r;(void)g;(void)b; }
void      vga_320_present(void) { present_screen(); }
uint16_t *vga_160_back_buffer(void) { return fb160; }
void      vga_160_present(void) { present_screen(); }
void      vga_set_split_row(int r) { g_split_row = r; }
void      vga_split_present(void) { present_screen(); }
