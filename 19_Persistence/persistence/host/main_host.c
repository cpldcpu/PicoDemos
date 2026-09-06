/* Desktop player for PERSISTENCE.
 *
 * The one place a whole frame is ever assembled -- and it is assembled the way
 * the device does it: beam_frame(f) once, then beam_line() 480 times in order
 * into a 640x480 buffer that stands in for the phosphor.
 *
 * The audio clock is the master when audio is on (17_Hysteresis/host/audio_sdl.c
 * explains why: a monitor at anything but 60 Hz would otherwise drift the
 * picture off the music). Frame f is shown when the SDL callback has consumed
 * 400 f samples, which is the same rule the device uses with its DMA counter.
 *
 *   --start S       seek to S seconds (this demo CAN seek: it stores nothing)
 *   --frames N      run N frames then exit
 *   --headless      no window
 *   --rawpipe       raw 640x480 BGRA frames to stdout (capture.py)
 *   --shot N        screenshot at frame N (repeatable), as BMP
 *   --shotdir DIR   where screenshots go (default screenshots/)
 *   --mute          no audio; vsync paces the frames
 *   --wav FILE      render the soundtrack to FILE and exit
 *   --lod 1         force the half-resolution fallback, to see what it looks like
 *   --hash          print per-frame video hashes (VHASH) every 60 frames
 *
 * Keys: ESC quit, SPACE pause, LEFT/RIGHT seek 5 s, R restart, S screenshot,
 *       F fullscreen, L toggle lod.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>

#include "../beam.h"
#include "../tables.h"
#include "../synth.h"
#include "../rgb565.h"

#define MAX_SHOTS 64

static uint16_t s_line[PV_W + 8];
static uint32_t s_screen[PV_W * PV_H];
static uint32_t s_lut[65536];

static volatile uint32_t s_consumed;     /* samples SDL has taken            */
static uint32_t          s_lead;
static SDL_AudioDeviceID s_dev;
static int               s_audio_on;

static void SDLCALL audio_cb(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    int frames = len / 4;
    synth_render((int16_t *)stream, frames);
    s_consumed += (uint32_t)frames;
}

static void build_lut(void)
{
    for (int c = 0; c < 65536; c++) {
        int r5 = rgb565_r5((uint16_t)c), g5 = rgb565_g5((uint16_t)c), b5 = rgb565_b5((uint16_t)c);
        int r = (r5 << 3) | (r5 >> 2), g = (g5 << 3) | (g5 >> 2), b = (b5 << 3) | (b5 >> 2);
        s_lut[c] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

static void render_frame(uint32_t f)
{
    beam_frame(f);
    beam_line_setup(f);
    for (int y = 0; y < PV_H; y++) {
        beam_line(f, s_line, y);
        uint32_t *dst = &s_screen[y * PV_W];
        for (int x = 0; x < PV_W; x++) dst[x] = s_lut[s_line[x]];
    }
}

static uint32_t frame_hash(void)
{
    uint32_t h = 2166136261u;
    const uint8_t *p = (const uint8_t *)s_screen;
    for (size_t i = 0; i < sizeof s_screen; i++) h = (h ^ p[i]) * 16777619u;
    return h;
}

static int   s_vidfd = -1;
static void raw_begin(void)
{
    fflush(stdout);
    s_vidfd = _dup(_fileno(stdout));
    _setmode(s_vidfd, _O_BINARY);
    _dup2(_fileno(stderr), _fileno(stdout));
}
static void raw_emit(void)
{
    const char *p = (const char *)s_screen; int n = (int)sizeof s_screen;
    while (n > 0) { int w = _write(s_vidfd, p, n); if (w <= 0) break; p += w; n -= w; }
}

static const char *s_shotdir = "screenshots";
static int s_shotseq = 0;
static void screenshot(uint32_t f)
{
    _mkdir(s_shotdir);
    char name[256];
    snprintf(name, sizeof name, "%s/f%05u.bmp", s_shotdir, (unsigned)f);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, PV_W, PV_H, 32, SDL_PIXELFORMAT_ARGB8888);
    memcpy(s->pixels, s_screen, sizeof s_screen);
    if (SDL_SaveBMP(s, name) == 0) fprintf(stderr, "wrote %s\n", name);
    SDL_FreeSurface(s);
    s_shotseq++;
}

static void write_wav(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    uint32_t n = PV_TOTAL_SAMPLES, data = n * 4, riff = data + 36, rate = PV_RATE, br = rate * 4, fmt = 16;
    uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, fp); fwrite(&riff, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
    fwrite(&fmt, 4, 1, fp); fwrite(&pcm, 2, 1, fp); fwrite(&ch, 2, 1, fp);
    fwrite(&rate, 4, 1, fp); fwrite(&br, 4, 1, fp); fwrite(&align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&data, 4, 1, fp);
    synth_reset();
    int16_t buf[2048];
    for (uint32_t p = 0; p < n;) {
        uint32_t k = n - p; if (k > 1024) k = 1024;
        synth_render(buf, (int)k);
        fwrite(buf, 4, k, fp);
        p += k;
    }
    fclose(fp);
    fprintf(stderr, "wrote %s: %u samples, peak %d\n", path, (unsigned)n, (int)synth_peak());
}

int main(int argc, char **argv)
{
    uint32_t start_frame = 0, frames = 0;
    int headless = 0, rawpipe = 0, mute = 0, hashes = 0, lod = 0;
    const char *wav = NULL;
    uint32_t shots[MAX_SHOTS]; int nshots = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--start") && i + 1 < argc)        start_frame = (uint32_t)(atof(argv[++i]) * PV_FPS);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)  frames = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--headless"))                headless = 1;
        else if (!strcmp(argv[i], "--rawpipe"))                 { rawpipe = 1; headless = 1; }
        else if (!strcmp(argv[i], "--mute"))                    mute = 1;
        else if (!strcmp(argv[i], "--hash"))                    hashes = 1;
        else if (!strcmp(argv[i], "--lod") && i + 1 < argc)     lod = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc)     wav = argv[++i];
        else if (!strcmp(argv[i], "--shotdir") && i + 1 < argc) s_shotdir = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
            if (nshots < MAX_SHOTS) shots[nshots++] = (uint32_t)atoi(argv[++i]);
        } else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
    }
    if (nshots) headless = 1;

    pv_tables_init();
    synth_init();
    beam_init();
    build_lut();
    g_lod = (uint8_t)lod;

    if (wav) { write_wav(wav); return 0; }
    if (rawpipe) raw_begin();

    if (SDL_Init(0) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }

    SDL_Window *win = NULL; SDL_Renderer *ren = NULL; SDL_Texture *tex = NULL;
    if (!headless) {
        if (!mute) {
            SDL_AudioSpec want, have; SDL_memset(&want, 0, sizeof want);
            want.freq = PV_RATE; want.format = AUDIO_S16SYS; want.channels = 2;
            want.samples = 1024; want.callback = audio_cb;
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
                s_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
                if (s_dev && have.freq == PV_RATE && have.channels == 2) {
                    s_lead = have.samples; s_audio_on = 1;
                } else if (s_dev) { SDL_CloseAudioDevice(s_dev); s_dev = 0; }
            }
            if (!s_audio_on) fprintf(stderr, "audio: unavailable, running on vsync\n");
        }
        SDL_InitSubSystem(SDL_INIT_VIDEO);
        win = SDL_CreateWindow("PERSISTENCE / LATENT / 2026", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               PV_W, PV_H, SDL_WINDOW_RESIZABLE);
        Uint32 rf = SDL_RENDERER_ACCELERATED | (s_audio_on ? 0 : SDL_RENDERER_PRESENTVSYNC);
        ren = SDL_CreateRenderer(win, -1, rf);
        SDL_RenderSetLogicalSize(ren, PV_W, PV_H);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, PV_W, PV_H);
    }

    if (!frames) frames = PV_TOTAL_FRAMES - start_frame;
    uint32_t end_frame = start_frame + frames;
    if (end_frame > PV_TOTAL_FRAMES) end_frame = PV_TOTAL_FRAMES;

    uint32_t f = start_frame;
    if (s_audio_on) {
        synth_seek(start_frame * PV_SPF);
        s_consumed = start_frame * PV_SPF;
        SDL_PauseAudioDevice(s_dev, 0);
    }

    int si = 0, quit = 0, paused = 0;
    uint64_t t0 = SDL_GetPerformanceCounter(), pf = SDL_GetPerformanceFrequency();
    uint32_t wall_base = start_frame;

    while (f < end_frame && !quit) {
        if (!headless) {
            SDL_Event ev;
            int seek_to = -1;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) quit = 1;
                else if (ev.type == SDL_KEYDOWN) switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE: case SDLK_q: quit = 1; break;
                    case SDLK_s: screenshot(f); break;
                    case SDLK_l: g_lod = !g_lod; break;
                    case SDLK_f: SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP); break;
                    case SDLK_SPACE: paused = !paused; if (s_audio_on) SDL_PauseAudioDevice(s_dev, paused); break;
                    case SDLK_r: seek_to = 0; break;
                    case SDLK_RIGHT: seek_to = (int)f + 5 * PV_FPS; break;
                    case SDLK_LEFT:  seek_to = (int)f - 5 * PV_FPS; if (seek_to < 0) seek_to = 0; break;
                    default: break;
                }
            }
            if (seek_to >= 0) {
                if (seek_to >= (int)PV_TOTAL_FRAMES) seek_to = (int)PV_TOTAL_FRAMES - 1;
                f = (uint32_t)seek_to; end_frame = PV_TOTAL_FRAMES;
                beam_reset();
                if (s_audio_on) {
                    SDL_LockAudioDevice(s_dev);
                    synth_seek(f * PV_SPF); s_consumed = f * PV_SPF;
                    SDL_UnlockAudioDevice(s_dev);
                } else { t0 = SDL_GetPerformanceCounter(); wall_base = f; }
            }
            if (paused) { SDL_Delay(10); continue; }

            if (s_audio_on) {
                /* frame f is due when 400 f samples have been HEARD */
                for (;;) {
                    uint32_t c = s_consumed, heard = c > s_lead ? c - s_lead : 0;
                    if (heard >= f * PV_SPF) break;
                    uint32_t behind = f * PV_SPF - heard, ms = behind * 1000u / PV_RATE;
                    SDL_Delay(ms > 1 ? ms - 1 : 0);
                }
                /* if we fell behind, jump: this demo can seek */
                uint32_t c = s_consumed, heard = c > s_lead ? c - s_lead : 0;
                uint32_t due = heard / PV_SPF;
                if (due > f + 1) f = due;
            } else if (!mute || 1) {
                uint64_t now = SDL_GetPerformanceCounter();
                uint32_t due = wall_base + (uint32_t)((now - t0) * PV_FPS / pf);
                if (due > f + 1) f = due;
            }
        }

        render_frame(f);

        if (rawpipe) raw_emit();
        while (si < nshots && shots[si] <= f) { if (shots[si] == f) screenshot(f); si++; }
        if (hashes && (f % 60) == 0) fprintf(stderr, "VHASH f=%-5u %08x\n", (unsigned)f, frame_hash());

        if (!headless) {
            SDL_UpdateTexture(tex, NULL, s_screen, PV_W * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, NULL, NULL);
            SDL_RenderPresent(ren);
        }
        f++;
    }

    if (s_dev) SDL_CloseAudioDevice(s_dev);
    SDL_Quit();
    return 0;
}
