/* capture -- the host renderer and the host synth, written out.
 *
 * No SDL and no image library: plain C99 plus the C library, so it builds
 * under WSL with `gcc` and under MinGW with the same command line. The PNG
 * writer emits stored (uncompressed) deflate blocks, which is a legal zlib
 * stream and costs about sixty lines; a 320x240 frame is 230 KB and nobody
 * is shipping these, they are for looking at.
 *
 * This is referee 4's tool. It renders the actual C renderer -- never a
 * separate approximation -- and the actual C synth, so what is checked is
 * what the device runs.
 *
 *   capture --wav score.wav
 *   capture --hashes hashes.txt          per-second FNV latches, for referee 2
 *   capture --out media/frames --every 60
 *   capture --out media/stills --phrases
 *   capture --out media/stills --seconds 12.5,48,301
 *   capture --out media/stills --bars 0,24,40,128
 *   capture --raw --fps 60 | ffmpeg -f rawvideo -pixel_format rgb24 ...
 *
 * Frame N at --fps f is the moment sample = N * CV_RATE / f, which is the
 * same rule the device follows (it asks the DAC instead of multiplying), so
 * a still from here and a photograph of the screen are the same picture.
 */

#include "colossus.h"
#include "demo.h"
#include "song.h"
#include "synth.h"
#include "img_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  define MKDIR(p) mkdir((p), 0777)
#endif

static uint16_t g_page[CV_W * CV_H];
static uint8_t  g_rgb[CV_W * CV_H * 3];

static int red5  (uint16_t p) { return (p & 31) << 3; }
static int green5(uint16_t p) { return ((p >> 6) & 31) << 3; }
static int blue5 (uint16_t p) { return (p >> 11) << 3; }

static void to_rgb24(void)
{
    for (int i = 0; i < CV_W * CV_H; i++) {
        g_rgb[i * 3 + 0] = (uint8_t)red5(g_page[i]);
        g_rgb[i * 3 + 1] = (uint8_t)green5(g_page[i]);
        g_rgb[i * 3 + 2] = (uint8_t)blue5(g_page[i]);
    }
}

/* ------------------------------------------------------------------- WAV -- */

static int write_wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 0; }
    const uint32_t n = CV_TOTAL_SAMPLES, data = n * 4, riff = data + 36;
    const uint32_t rate = CV_RATE, br = rate * 4, fmt = 16;
    const uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&fmt, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);

    synth_reset();
    static int16_t buf[2 * 1024];
    for (uint32_t p = 0; p < n; ) {
        uint32_t k = n - p; if (k > 1024) k = 1024;
        synth_render(buf, (int)k);
        if (fwrite(buf, 4, k, f) != k) { perror(path); fclose(f); return 0; }
        p += k;
    }
    return fclose(f) == 0;
}

/* The device latches an FNV-1a of every emitted int16 once a second and
 * prints it; this writes the same table on the host so serial_read.py can
 * diff the whole 5:07 rather than a spot check. */
static int write_hashes(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return 0; }
    synth_reset();
    static int16_t buf[2 * 1024];
    uint32_t last = 0;
    fprintf(f, "# sample hash  (FNV-1a over every emitted int16, latched every %d frames)\n", CV_RATE);
    for (uint32_t p = 0; p < CV_TOTAL_SAMPLES; ) {
        uint32_t k = CV_TOTAL_SAMPLES - p; if (k > 1024) k = 1024;
        synth_render(buf, (int)k);
        p += k;
        uint32_t pos, hash;
        if (synth_hash_latch(&pos, &hash) && pos != last) {
            fprintf(f, "%lu %08lx\n", (unsigned long)pos, (unsigned long)hash);
            last = pos;
        }
    }
    return fclose(f) == 0;
}

/* ------------------------------------------------------------------ main -- */

static uint32_t g_list[8192];
static int      g_list_n;

static void push(uint32_t s)
{
    if (s >= CV_TOTAL_SAMPLES) s = CV_TOTAL_SAMPLES - 1;
    if (g_list_n < (int)(sizeof g_list / sizeof g_list[0])) g_list[g_list_n++] = s;
}

static void push_csv(const char *s, double scale)
{
    while (*s) {
        char *end;
        const double v = strtod(s, &end);
        if (end == s) break;
        push((uint32_t)(v * scale));
        s = end;
        while (*s == ',' || *s == ' ') s++;
    }
}

static void usage(void)
{
    fprintf(stderr,
        "capture -- COLOSSUS host capture (no SDL, no image library)\n"
        "  --wav FILE          render the whole score to a 24 kHz stereo WAV\n"
        "  --hashes FILE       per-second synth hash table (referee 2)\n"
        "  --out DIR           where frames go (created if missing)\n"
        "  --every N           every Nth frame of the whole run at --fps\n"
        "  --frames N          stop after N frames\n"
        "  --fps N             frame grid, default 60\n"
        "  --samples a,b,c     absolute sample positions\n"
        "  --seconds a,b,c     times in seconds\n"
        "  --bars a,b,c        bar numbers\n"
        "  --phrases           the downbeat of each of the twenty phrases\n"
        "  --png | --ppm       output format, default png\n"
        "  --raw               rgb24 frames to stdout at --fps, for ffmpeg\n"
        "  --bench N           render N frames spread over the run, write nothing\n"
        "  --quiet             no per-frame progress on stderr\n");
}

int main(int argc, char **argv)
{
    const char *wav = NULL, *hashes = NULL, *out = NULL;
    int every = 0, fps = 60, limit = -1, png = 1, raw = 0, quiet = 0, bench = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (!strcmp(a, "--wav")     && i + 1 < argc) wav = argv[++i];
        else if (!strcmp(a, "--hashes")  && i + 1 < argc) hashes = argv[++i];
        else if (!strcmp(a, "--out")     && i + 1 < argc) out = argv[++i];
        else if (!strcmp(a, "--every")   && i + 1 < argc) every = atoi(argv[++i]);
        else if (!strcmp(a, "--frames")  && i + 1 < argc) limit = atoi(argv[++i]);
        else if (!strcmp(a, "--fps")     && i + 1 < argc) fps = atoi(argv[++i]);
        else if (!strcmp(a, "--samples") && i + 1 < argc) push_csv(argv[++i], 1.0);
        else if (!strcmp(a, "--seconds") && i + 1 < argc) push_csv(argv[++i], CV_RATE);
        else if (!strcmp(a, "--bars")    && i + 1 < argc) push_csv(argv[++i], CV_BAR);
        else if (!strcmp(a, "--phrases")) { for (int p = 0; p < CV_BARS / 8; p++) push((uint32_t)p * 8u * CV_BAR); }
        else if (!strcmp(a, "--png"))   png = 1;
        else if (!strcmp(a, "--ppm"))   png = 0;
        else if (!strcmp(a, "--raw"))   raw = 1;
        else if (!strcmp(a, "--bench")   && i + 1 < argc) bench = atoi(argv[++i]);
        else if (!strcmp(a, "--quiet")) quiet = 1;
        else { usage(); return 2; }
    }
    if (fps < 1 || fps > 240) { fprintf(stderr, "capture: --fps out of range\n"); return 2; }
    if (!wav && !hashes && !out && !raw && bench <= 0) { usage(); return 2; }

    synth_init();
    demo_init();

    if (wav    && !write_wav(wav))       return 1;
    if (hashes && !write_hashes(hashes)) return 1;

    /* Bulk timing. clock() has a 1 ms tick on Windows, which is coarser than
     * one frame, so the batch is timed as a whole and divided -- a mean, and
     * honestly only a mean. Per-frame worst cases are the device's job. */
    if (bench > 0) {
        const clock_t t0 = clock();
        for (int i = 0; i < bench; i++)
            demo_render(g_page, (uint32_t)((uint64_t)i * CV_TOTAL_SAMPLES / (uint32_t)bench));
        const double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        fprintf(stderr, "capture: %d frames in %.0f ms, %.3f ms per frame (HOST measurement)\n",
                bench, ms, ms / bench);
    }
    if (!out && !raw) return 0;

    if (out) MKDIR(out);

    if (raw) {
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        const uint32_t total = (uint32_t)((uint64_t)CV_TOTAL_SAMPLES * (uint32_t)fps / CV_RATE);
        for (uint32_t n = 0; n < total; n++) {
            if (limit >= 0 && (int)n >= limit) break;
            demo_render(g_page, (uint32_t)((uint64_t)n * CV_RATE / (uint32_t)fps));
            to_rgb24();
            if (fwrite(g_rgb, 1, sizeof g_rgb, stdout) != sizeof g_rgb) { perror("stdout"); return 1; }
        }
        return 0;
    }

    if (every > 0) {
        const uint32_t total = (uint32_t)((uint64_t)CV_TOTAL_SAMPLES * (uint32_t)fps / CV_RATE);
        for (uint32_t n = 0; n < total; n += (uint32_t)every)
            push((uint32_t)((uint64_t)n * CV_RATE / (uint32_t)fps));
    }
    if (!g_list_n) { fprintf(stderr, "capture: nothing selected; use --every, --samples, --bars or --phrases\n"); return 2; }
    if (limit >= 0 && limit < g_list_n) g_list_n = limit;

    double total_ms = 0, worst_ms = 0;
    char path[1024];
    for (int i = 0; i < g_list_n; i++) {
        const uint32_t s = g_list[i];
        const clock_t t0 = clock();
        demo_render(g_page, s);
        const double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        total_ms += ms; if (ms > worst_ms) worst_ms = ms;
        to_rgb24();
        snprintf(path, sizeof path, "%s/bar%03lu_s%08lu.%s", out,
                 (unsigned long)cv_bar_of(s), (unsigned long)s, png ? "png" : "ppm");
        if (!(png ? cv_write_png(path, g_rgb, CV_W, CV_H)
                  : cv_write_ppm(path, g_rgb, CV_W, CV_H))) return 1;
        if (!quiet) fprintf(stderr, "%s  %s  bar %lu\n", path,
                            song_section_name(song_section(cv_bar_of(s))), (unsigned long)cv_bar_of(s));
    }
    fprintf(stderr, "capture: %d frames, render mean %.2f ms, worst %.2f ms (HOST measurements)\n",
            g_list_n, total_ms / g_list_n, worst_ms);
    return 0;
}
