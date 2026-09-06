/* player -- COLOSSUS on the desktop, for Azure and for the director.
 *
 * SDL2, the real synth on the audio callback and the real renderer on the
 * main thread. The picture follows the audio exactly as it does on the
 * device: the callback publishes synth_pos() and the frame draws whatever
 * sample that is. When the audio device will not open, a wall clock stands
 * in, and the window title says so.
 *
 *   space          pause
 *   left/right     seek one phrase (8 bars, 15.36 s)
 *   , / .          seek one bar
 *   0..9           jump to phrase 1..10; shift for 11..20
 *   home           back to the start
 *   s              screenshot, PNG, native size, into the working directory
 *   n              native size window (320x240) / g back to 960x720
 *   f              fullscreen
 *   esc            quit
 *
 * Seeking is what tests demo.h's purity: synth_seek() renders and discards
 * to the target so the music is bit-identical to a straight play-through,
 * and demo_render() is handed the new sample with no other state changed.
 * If a scene looks different after a seek than it did on the way past, the
 * renderer is keeping state it promised not to keep.
 */

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "colossus.h"
#include "demo.h"
#include "song.h"
#include "synth.h"
#include "img_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHRASE_SAMPLES (8u * CV_BAR)

static uint16_t g_page[CV_W * CV_H];
static uint8_t  g_rgb[CV_W * CV_H * 3];
static SDL_atomic_t g_played;

static void audio_cb(void *user, Uint8 *stream, int len)
{
    (void)user;
    synth_render((int16_t *)stream, len / 4);
    SDL_AtomicSet(&g_played, (int)synth_pos());
}

static void to_rgb24(void)
{
    for (int i = 0; i < CV_W * CV_H; i++) {
        const uint16_t p = g_page[i];
        g_rgb[i * 3 + 0] = (uint8_t)((p & 31) << 3);
        g_rgb[i * 3 + 1] = (uint8_t)(((p >> 6) & 31) << 3);
        g_rgb[i * 3 + 2] = (uint8_t)((p >> 11) << 3);
    }
}

static uint32_t clamp_sample(long long s)
{
    if (s < 0) return 0;
    if (s >= (long long)CV_TOTAL_SAMPLES) return CV_TOTAL_SAMPLES - 1;
    return (uint32_t)s;
}

int main(int argc, char **argv)
{
    double start = 0.0;
    int mute = 0, scale = 3;
    for (int i = 1; i < argc; i++) {
        if      (!strcmp(argv[i], "--start") && i + 1 < argc) start = atof(argv[++i]);
        else if (!strcmp(argv[i], "--phrase") && i + 1 < argc) start = (atoi(argv[++i]) - 1) * 15.36;
        else if (!strcmp(argv[i], "--mute")) mute = 1;
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else {
            fprintf(stderr, "player [--start SECONDS] [--phrase 1..20] [--mute] [--scale N]\n");
            return 2;
        }
    }
    if (scale < 1 || scale > 6) scale = 3;

    synth_init();
    demo_init();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_Window *win = SDL_CreateWindow("COLOSSUS / LATENT / 2026",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, CV_W * scale, CV_H * scale,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC) : NULL;
    if (win && !ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!win || !ren) { fprintf(stderr, "Video: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(ren, CV_W, CV_H);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, CV_W, CV_H);
    if (!tex) { fprintf(stderr, "Texture: %s\n", SDL_GetError()); return 1; }

    SDL_AudioDeviceID dev = 0;
    if (!mute) {
        SDL_AudioSpec want; SDL_zero(want);
        want.freq = CV_RATE; want.format = AUDIO_S16SYS; want.channels = 2;
        want.samples = 512; want.callback = audio_cb;
        dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
        if (!dev) fprintf(stderr, "Audio: %s (falling back to the wall clock)\n", SDL_GetError());
    }

    uint32_t sample = clamp_sample((long long)(start * CV_RATE));
    synth_seek(sample);
    SDL_AtomicSet(&g_played, (int)sample);
    if (dev) SDL_PauseAudioDevice(dev, 0);

    Uint64 epoch = SDL_GetPerformanceCounter();
    const Uint64 freq = SDL_GetPerformanceFrequency();
    uint32_t epoch_sample = sample;

    int quit = 0, paused = 0, shots = 0;
    double total_ms = 0, worst_ms = 0;
    long frames = 0;
    char title[192];

    while (!quit) {
        SDL_Event e;
        long long seek = -1;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = 1;
            if (e.type != SDL_KEYDOWN) continue;
            const SDL_Keycode k = e.key.keysym.sym;
            const int shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
            if (k == SDLK_ESCAPE) quit = 1;
            else if (k == SDLK_f)
                SDL_SetWindowFullscreen(win, (SDL_GetWindowFlags(win) & SDL_WINDOW_FULLSCREEN_DESKTOP)
                                             ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            else if (k == SDLK_n) SDL_SetWindowSize(win, CV_W, CV_H);
            else if (k == SDLK_g) SDL_SetWindowSize(win, CV_W * 3, CV_H * 3);
            else if (k == SDLK_SPACE) {
                paused = !paused;
                if (dev) SDL_PauseAudioDevice(dev, paused);
                if (!paused) { epoch = SDL_GetPerformanceCounter(); epoch_sample = sample; }
            }
            else if (k == SDLK_s) {
                char path[128];
                snprintf(path, sizeof path, "colossus_bar%03lu_%02d.png",
                         (unsigned long)cv_bar_of(sample), shots++);
                to_rgb24();
                if (cv_write_png(path, g_rgb, CV_W, CV_H)) fprintf(stderr, "wrote %s\n", path);
            }
            else if (k == SDLK_RIGHT) seek = (long long)sample + PHRASE_SAMPLES;
            else if (k == SDLK_LEFT)  seek = (long long)sample - PHRASE_SAMPLES;
            else if (k == SDLK_PERIOD) seek = (long long)sample + CV_BAR;
            else if (k == SDLK_COMMA)  seek = (long long)sample - CV_BAR;
            else if (k == SDLK_HOME)   seek = 0;
            else if (k >= SDLK_0 && k <= SDLK_9) {
                const int n = (k == SDLK_0 ? 10 : (int)(k - SDLK_0)) + (shift ? 10 : 0);
                seek = (long long)(n - 1) * PHRASE_SAMPLES;
            }
        }

        if (seek >= 0) {
            const uint32_t target = clamp_sample(seek);
            if (dev) SDL_LockAudioDevice(dev);
            synth_seek(target);
            SDL_AtomicSet(&g_played, (int)target);
            if (dev) SDL_UnlockAudioDevice(dev);
            sample = target; epoch_sample = target; epoch = SDL_GetPerformanceCounter();
        }

        if (!paused) {
            sample = dev ? (uint32_t)SDL_AtomicGet(&g_played)
                         : clamp_sample((long long)epoch_sample +
                             (long long)((SDL_GetPerformanceCounter() - epoch) * CV_RATE / freq));
            if (sample >= CV_TOTAL_SAMPLES) { sample = CV_TOTAL_SAMPLES - 1; paused = 1; if (dev) SDL_PauseAudioDevice(dev, 1); }
        }

        const Uint64 t0 = SDL_GetPerformanceCounter();
        demo_render(g_page, sample);
        const double ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)freq;
        total_ms += ms; if (ms > worst_ms) worst_ms = ms; frames++;

        to_rgb24();
        SDL_UpdateTexture(tex, NULL, g_rgb, CV_W * 3);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        const uint32_t bar = cv_bar_of(sample);
        demo_stats_t st; demo_stats(&st);
        snprintf(title, sizeof title,
                 "COLOSSUS  %lu:%02lu  bar %lu/%d  phrase %lu/%d  %s  |  %.2f ms  tri %lu  px %lu%s%s",
                 (unsigned long)(sample / CV_RATE / 60), (unsigned long)(sample / CV_RATE % 60),
                 (unsigned long)bar, CV_BARS, (unsigned long)(bar / 8 + 1), CV_BARS / 8,
                 song_section_name(song_section(bar)), ms,
                 (unsigned long)st.triangles, (unsigned long)st.fill,
                 paused ? "  [paused]" : "", dev ? "" : "  [wall clock]");
        SDL_SetWindowTitle(win, title);
        if (paused) SDL_Delay(8);
    }

    fprintf(stderr, "player: %ld frames, render mean %.2f ms, worst %.2f ms (HOST measurements)\n",
            frames, frames ? total_ms / (double)frames : 0.0, worst_ms);

    if (dev) SDL_CloseAudioDevice(dev);
    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
