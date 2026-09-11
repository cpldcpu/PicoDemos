/* The SDL desktop player, from HELION's host/main.c. Same flags, --fps
 * defaulting to 60 because SLEEPER is a 60 fps production and a 30 fps
 * capture would not show what the claim is about. One flag is new:
 * --samples/--outdir writes a list of stills from one process, which is what
 * the contact sheet and the round stills want -- 58 cuts is 58 process
 * launches otherwise. -- Overscan
 */
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "sleeper.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static uint16_t pixels[WIDTH * HEIGHT];
static uint8_t rgb24[WIDTH * HEIGHT * 3];
static SDL_atomic_t played;
static void callback(void *unused, Uint8 *stream, int len) { (void)unused; synth_render((int16_t *)stream, (unsigned)len / 4); SDL_AtomicSet(&played, (int)synth_position()); }
static void convert(void) { for (int i = 0; i < WIDTH * HEIGHT; i++) { rgb24[i * 3] = (uint8_t)red(pixels[i]); rgb24[i * 3 + 1] = (uint8_t)green(pixels[i]); rgb24[i * 3 + 2] = (uint8_t)blue(pixels[i]); } }
static int write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb"); if (!f) { perror(path); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    fwrite(rgb24, 1, sizeof rgb24, f); return fclose(f) != 0;
}
static void wav_header(FILE *f, uint32_t frames) { uint32_t size = frames * 4, riff = size + 36, rate = SAMPLE_RATE, br = rate * 4; uint16_t one = 1, two = 2, align = 4, bits = 16; fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); uint32_t n = 16; fwrite(&n, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&two, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&size, 4, 1, f); }
int main(int argc, char **argv)
{
    int raw = 0, headless = 0, mute = 0, frames = -1, fps = 60; float start = 0;
    const char *shot = NULL, *wav = NULL, *samples = NULL, *outdir = ".", *dispatch = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--raw")) { raw = 1; headless = 1; }
        else if (!strcmp(argv[i], "--headless")) headless = 1;
        else if (!strcmp(argv[i], "--mute")) mute = 1;
        else if (!strcmp(argv[i], "--start") && i + 1 < argc) start = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fps") && i + 1 < argc) fps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) { shot = argv[++i]; headless = 1; frames = 1; }
        else if (!strcmp(argv[i], "--samples") && i + 1 < argc) { samples = argv[++i]; headless = 1; }
        else if (!strcmp(argv[i], "--outdir") && i + 1 < argc) outdir = argv[++i];
        else if (!strcmp(argv[i], "--dispatch") && i + 1 < argc) dispatch = argv[++i];
        else if (!strcmp(argv[i], "--wav") && i + 1 < argc) { wav = argv[++i]; headless = 1; }
        else { fprintf(stderr, "SLEEPER: --start seconds --shot file.ppm --samples a,b,c --outdir dir --dispatch log.txt --wav file.wav --raw --frames N --fps N --headless --mute\n"); return 2; }
    }
    if (start < 0 || start >= DURATION_SECONDS || fps < 1 || fps > 240 || frames < -1) { fprintf(stderr, "Invalid time, frame count or frame rate\n"); return 2; }
    demo_init(); synth_init();
    if (samples) {
        /* One process, many stills: `name` is the sample so a caller can find
         * the file it asked for without knowing the bar arithmetic. */
        char path[1024]; const char *p = samples; int made = 0;
        double total = 0, worst = 0;
        while (*p) {
            char *end; unsigned long s = strtoul(p, &end, 10);
            if (end == p) break;
            uint64_t t = SDL_GetPerformanceCounter();
            demo_render(pixels, (uint32_t)s);
            double us = (double)(SDL_GetPerformanceCounter() - t) * 1e6 / (double)SDL_GetPerformanceFrequency();
            total += us; if (us > worst) worst = us;
            convert();
            snprintf(path, sizeof path, "%s/s%08lu.ppm", outdir, s);
            if (write_ppm(path)) return 1;
            /* stdout is the machine-readable half: sample, bar, microseconds */
            printf("%lu %lu %.1f\n", s, (unsigned long)(s / BAR_SAMPLES), us);
            made++;
            p = *end == ',' ? end + 1 : end;
        }
        fprintf(stderr, "SLEEPER: %d stills, render mean %.2f ms, worst %.2f ms (HOST measurements)\n",
                made, made ? total / made / 1000 : 0, worst / 1000);
        return 0;
    }
    if (wav) { FILE *f = fopen(wav, "wb"); if (!f) { perror(wav); return 1; } wav_header(f, DURATION_SAMPLES); int16_t b[1024]; for (unsigned p = 0; p < DURATION_SAMPLES;) { unsigned n = DURATION_SAMPLES - p; if (n > 512) n = 512; synth_render(b, n); if (fwrite(b, 4, n, f) != n) return 1; p += n; } return fclose(f) != 0; }
#ifdef _WIN32
    if (raw) _setmode(_fileno(stdout), _O_BINARY);
#endif
    uint32_t start_sample = (uint32_t)(start * SAMPLE_RATE);
    if (frames < 0) frames = headless ? (int)((DURATION_SECONDS - start) * fps) : INT_MAX;
    SDL_Window *w = NULL; SDL_Renderer *r = NULL; SDL_Texture *texture = NULL; SDL_AudioDeviceID device = 0;
    if (!headless) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
        w = SDL_CreateWindow("SLEEPER / LATENT / 2026", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        r = SDL_CreateRenderer(w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC); if (!r) r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
        if (!w || !r) { fprintf(stderr, "Video: %s\n", SDL_GetError()); return 1; }
        SDL_RenderSetLogicalSize(r, WIDTH, HEIGHT); SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
        if (!texture) { fprintf(stderr, "Texture: %s\n", SDL_GetError()); return 1; }
        if (!mute) { SDL_AudioSpec spec = { 0 }; spec.freq = SAMPLE_RATE; spec.format = AUDIO_S16SYS; spec.channels = 2; spec.samples = 512; spec.callback = callback; device = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0); if (!device) fprintf(stderr, "Audio: %s (using wall clock)\n", SDL_GetError()); }
        synth_seek(start_sample); SDL_AtomicSet(&played, (int)start_sample); if (device) SDL_PauseAudioDevice(device, 0);
    }
    /* Phase: "a discontinuity detector cannot prove all cuts by itself:
     * matched cuts may be quiet and lamp flashes loud. Pair it with
     * dispatch logging." This is that log: one line a captured frame,
     * naming the cut the renderer actually drew, so cut_check.py can say
     * which scheduled cut a discontinuity belongs to instead of guessing
     * from its timestamp. */
    FILE *dlog = dispatch ? fopen(dispatch, "w") : NULL;
    if (dispatch && !dlog) { perror(dispatch); return 1; }
    if (dlog) fprintf(dlog, "# frame sample cut shot world us lights spans\n");
    uint64_t epoch = SDL_GetPerformanceCounter(), freq = SDL_GetPerformanceFrequency(), total_us = 0, max_us = 0;
    unsigned max_spans = 0; int rendered = 0, quit = 0, paused = 0;
    uint32_t held = start_sample;
    while (rendered < frames && !quit) {
        uint32_t sample = start_sample + (uint32_t)((uint64_t)rendered * SAMPLE_RATE / fps);
        if (!headless) {
            SDL_Event e; while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) quit = 1;
                if (e.type == SDL_KEYDOWN) {
                    SDL_Keycode k = e.key.keysym.sym; if (k == SDLK_ESCAPE) quit = 1;
                    if (k == SDLK_f) SDL_SetWindowFullscreen(w, (SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    if (k == SDLK_SPACE) { paused = !paused; if (device) SDL_PauseAudioDevice(device, paused); if (!paused) { start_sample = held; epoch = SDL_GetPerformanceCounter(); } }
                    if (k == SDLK_r || k == SDLK_RIGHT || k == SDLK_LEFT) {
                        uint32_t target = k == SDLK_r ? 0 : (uint32_t)clampi((int)held + (k == SDLK_RIGHT ? 15 : -15) * SAMPLE_RATE, 0, (int)(DURATION_SECONDS - 1) * SAMPLE_RATE);
                        if (device) SDL_LockAudioDevice(device); synth_seek(target); SDL_AtomicSet(&played, (int)target); if (device) SDL_UnlockAudioDevice(device);
                        start_sample = held = target; epoch = SDL_GetPerformanceCounter(); rendered = 0;
                    }
                }
            }
            if (paused) { SDL_Delay(10); continue; }
            sample = device ? (uint32_t)SDL_AtomicGet(&played) : start_sample + (uint32_t)((SDL_GetPerformanceCounter() - epoch) * SAMPLE_RATE / freq);
            held = sample;
        }
        if (sample >= DURATION_SAMPLES) break;
        uint64_t t = SDL_GetPerformanceCounter(); demo_render(pixels, sample); uint64_t us = (SDL_GetPerformanceCounter() - t) * 1000000 / freq;
        total_us += us; if (us > max_us) max_us = us;
        { demo_stats_t st; demo_stats(&st); if (st.spans > max_spans) max_spans = st.spans;
          if (dlog) fprintf(dlog, "%d %u %u %u %u %llu %u %u\n", rendered, sample, demo_cut_index(),
                            st.shot, st.world_id, (unsigned long long)us, st.lights, st.spans); }
        if (raw || shot || !headless) convert();
        if (raw && fwrite(rgb24, 1, sizeof rgb24, stdout) != sizeof rgb24) { perror("raw output"); return 1; }
        if (shot && write_ppm(shot)) return 1;
        if (!headless) { SDL_UpdateTexture(texture, NULL, rgb24, WIDTH * 3); SDL_RenderClear(r); SDL_RenderCopy(r, texture, NULL, NULL); SDL_RenderPresent(r); }
        rendered++;
    }
    fprintf(stderr, "SLEEPER: %d frames, render mean %.2f ms, worst %.2f ms, max %u spans (HOST measurements)\n", rendered, rendered ? (double)total_us / rendered / 1000 : 0, (double)max_us / 1000, max_spans);
    if (dlog) fclose(dlog);
    if (device) SDL_CloseAudioDevice(device); if (texture) SDL_DestroyTexture(texture); if (r) SDL_DestroyRenderer(r); if (w) SDL_DestroyWindow(w); SDL_Quit(); return 0;
}
