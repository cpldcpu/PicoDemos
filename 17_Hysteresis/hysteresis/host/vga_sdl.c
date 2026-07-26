/* SDL2 backend for vga.h — HYSTERESIS host preview.
 *
 * Written fresh rather than inherited. SUSTAIN's host backend STUBBED the
 * MODE_320 path (no palette, one buffer) because that demo never left
 * MODE_HIRES. Here MODE_320 is the whole demo: the 8bpp byte is both the
 * simulation state and the pixel, and the double buffer IS the feedback
 * ping-pong. So the 320 path has to be real, and the others can go.
 *
 * Output is 640x480 to match the device (320x240 pixel- and line-doubled at
 * scanout).
 *
 * Keys: ESC/Q quit, S screenshot, SPACE pause, R restart.
 */

#include "../vga.h"
#include "../rgb565.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <direct.h>
#include <io.h>
#include <fcntl.h>

#define OW 640
#define OH 480
#define WINSCALE 1

static SDL_Window   *win;
static SDL_Renderer *ren;
static SDL_Texture  *tex;
static uint32_t      g_screen[OW * OH];

/* The field, as a true double buffer — same layout and same swap semantics as
 * the device (vga.c fb_a/fb_b), because the demo depends on those semantics. */
static uint8_t  fb_a[VGA_320_W * VGA_320_H];
static uint8_t  fb_b[VGA_320_W * VGA_320_H];
static uint8_t *fb_back  = fb_a;
static uint8_t *fb_front = fb_b;

static uint32_t g_pal[256];

static screen_mode_t g_mode = MODE_320;
static int g_quit = 0, g_skip = 0, g_shot_seq = 0;
int g_offline = 0;
int g_rawpipe = 0;
int g_fielddump = 0;      /* raw 320x240 field bytes to stdout — see below */
int g_headless = 0;       /* no window at all; for the referee */

void vga_screenshot(void);

int vga_should_quit(void) { return g_quit; }
int vga_consume_skip_request(void) { int r = g_skip; g_skip = 0; return r; }

uint8_t       *vga_320_back_buffer(void)  { return fb_back; }
const uint8_t *vga_320_front_buffer(void) { return fb_front; }

void vga_320_palette_set(int i, uint8_t r, uint8_t g, uint8_t b)
{
    if (i < 0 || i > 255) return;
    /* Round-trip through RGB565 so the host preview shows the colours the
     * hardware will actually produce, not the ones we asked for. Palette
     * banding that only appears on device is a bad surprise. */
    uint16_t c = rgb565_pack(r, g, b);
    int r5 = rgb565_r5(c), g5 = rgb565_g5(c), b5 = rgb565_b5(c);
    int rr = (r5 << 3) | (r5 >> 2), gg = (g5 << 3) | (g5 >> 2), bb = (b5 << 3) | (b5 >> 2);
    g_pal[i] = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
}

/* ------------------------------------------------------------ raw output -- */

static int g_vidfd = -1;

/* The pipe must carry ONLY pixels: any stray printf on stdout shifts the whole
 * stream. Dup the real stdout to a private fd, then point fd 1 at stderr.
 * (Inherited from SUSTAIN, where this cost a confusing hour.) */
void vga_raw_begin(void)
{
    fflush(stdout);
    g_vidfd = _dup(_fileno(stdout));
    _setmode(g_vidfd, _O_BINARY);
    _dup2(_fileno(stderr), _fileno(stdout));
}

static void raw_write(const void *buf, int n)
{
    const char *p = (const char *)buf;
    while (n > 0) {
        int w = _write(g_vidfd, p, n);
        if (w <= 0) break;
        p += w; n -= w;
    }
}

void vga_raw_emit(void) { raw_write(g_screen, OW * OH * 4); }

/* The referee measures the FIELD, not the picture.
 *
 * Dumping the 320x240 state bytes rather than 640x480 ARGB is 20x less data,
 * but the real reason is correctness: the divergence test asks whether the
 * SYSTEM took a different path, and the palette is allowed to be f(t). Two runs
 * could show different colours while holding identical state, or identical
 * colours while diverging underneath if a ramp happened to be flat there.
 * Measuring state answers the question that was actually asked. */
void vga_field_emit(void) { raw_write(fb_front, VGA_320_W * VGA_320_H); }

/* Compose the visible image from the field through the palette. Kept separate
 * from present() because a headless run never composes -- and a screenshot that
 * silently captures an un-composed buffer is a black PNG and a wasted hour. */
static void compose(void)
{
    for (int y = 0; y < OH; y++) {
        const uint8_t *src = fb_front + (y >> 1) * VGA_320_W;
        uint32_t *dst = &g_screen[y * OW];
        for (int x = 0; x < OW; x++) dst[x] = g_pal[src[x >> 1]];
    }
}

void vga_screenshot(void)
{
    compose();
    _mkdir("screenshots");
    char name[80];
    snprintf(name, sizeof name, g_offline ? "screenshots/frame_%05d.bmp"
                                          : "screenshots/shot_%03d.bmp", g_shot_seq++);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, OW, OH, 32, SDL_PIXELFORMAT_ARGB8888);
    memcpy(s->pixels, g_screen, sizeof g_screen);
    if (SDL_SaveBMP(s, name) == 0) fprintf(stderr, "wrote %s\n", name);
    SDL_FreeSurface(s);
}

/* ----------------------------------------------------------------- init --- */

void vga_init(void)
{
    memset(fb_a, 0, sizeof fb_a);
    memset(fb_b, 0, sizeof fb_b);
    for (int i = 0; i < 256; i++) g_pal[i] = 0xFF000000u;

    if (g_headless) return;

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    win = SDL_CreateWindow("HYSTERESIS (host)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        OW * WINSCALE, OH * WINSCALE, 0);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(ren, OW, OH);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, OW, OH);
    fprintf(stderr, "vga_sdl: MODE_320 %dx%d -> %dx%d\n",
            VGA_320_W, VGA_320_H, OW, OH);
}

void vga_set_mode(screen_mode_t mode) { g_mode = mode; }
screen_mode_t vga_current_mode(void)  { return g_mode; }

/* ------------------------------------------------------------- present ---- */

void vga_320_present(void)
{
    /* Same swap as the device: what we just wrote becomes the front (displayed,
     * and next frame's source), and the stale front becomes the next back. */
    uint8_t *ob = fb_back;
    fb_back  = fb_front;
    fb_front = ob;

    if (!g_headless) {
        compose();
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
                case SDLK_r:     g_skip = 2; break;
                default: break;
            }
        }
    } else if (g_rawpipe) {
        compose();          /* video capture still needs the ARGB image */
    }
}

/* --- unused modes, kept only so vga.h stays one header for both builds --- */
static uint16_t fb_hires_unused[VGA_HIRES_W * VGA_HIRES_H];
static uint16_t fb160_unused[VGA_160_W * VGA_160_H];
int g_split_row = VGA_320_H;
uint16_t *vga_hires_back_buffer(void) { return fb_hires_unused; }
void      vga_hires_present(void) { }
uint16_t *vga_160_back_buffer(void) { return fb160_unused; }
void      vga_160_present(void) { }
void      vga_set_split_row(int r) { g_split_row = r; }
void      vga_split_present(void) { }
void      vga_set_race_fn(void (*scan)(uint16_t *, int), void (*setup)(void)) { (void)scan; (void)setup; }
uint8_t  *vga_race_sram(void) { return NULL; }
unsigned  vga_race_sram_size(void) { return 0; }
void      vga_race_present(void) { }
