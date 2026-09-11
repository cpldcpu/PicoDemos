/* Host harness for the SLEEPER score: renders synth.c + song.c to a WAV with
 * a chosen block size and solo mask, or dumps the tables as text for the
 * piano roll and the referees. Built by song_check.py / song_roll.py /
 * cut_check.py; not part of either target.
 *
 *   song_harness --wav out.wav [--chunk N] [--solo MASK] [--bars N] [--from BAR]
 *   song_harness --dump              per-bar arrangement and every note, as text
 *   song_harness --hashes            the per-second FNV-1a latches (pos hash), one a line,
 *                                    to diff against what the device prints
 *   song_harness --cuts              the cut list: sample bar beat shot world flags variant
 *   song_harness --boards            the board texts: sample bar beat rows...
 *   song_harness --timetable         speed and distance per beat
 */

#include "synth.h"
#include "song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_frames = DURATION_SAMPLES;
static uint32_t g_from = 0;

static void write_wav(const char *path, int chunk, unsigned solo)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    uint32_t n = g_frames, data = n * 4, riff = data + 36, rate = SAMPLE_RATE, br = rate * 4, fmt = 16;
    uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, fp); fwrite(&riff, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
    fwrite(&fmt, 4, 1, fp); fwrite(&pcm, 2, 1, fp); fwrite(&ch, 2, 1, fp);
    fwrite(&rate, 4, 1, fp); fwrite(&br, 4, 1, fp); fwrite(&align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&data, 4, 1, fp);

    synth_init();
    synth_solo(solo);
    if (g_from) synth_seek(g_from);
    static int16_t buf[2 * 4096];
    for (uint32_t p = 0; p < n;) {
        uint32_t k = n - p; if (k > (uint32_t)chunk) k = (uint32_t)chunk;
        synth_render(buf, k);
        fwrite(buf, 4, k, fp);
        p += k;
    }
    fclose(fp);
    fprintf(stderr, "%s: %u frames, chunk %d, solo %u, peak %d\n", path, n, chunk, solo, (int)synth_peak());
}

static void dump(void)
{
    /* B bar section chord light energy drums bass lead piano pad choir filter space clack */
    for (uint32_t bar = 0; bar < SONG_BARS; bar++) {
        const bar_t *b = song_bar(bar);
        printf("B %u %d %d %u %u %u %u %u %u %u %u %u %u %u\n", bar, song_section(bar), b->chord, b->light,
               b->energy, b->drums, b->bass, b->lead, b->piano, b->pad, b->choir, b->filter, b->space, b->clack);
    }
    /* N bar step voice note slide   (voices: 0 lead, 1 bass, 2..5 piano, 6 choir, 7..10 pad) */
    for (uint32_t bar = 0; bar < SONG_BARS; bar++) {
        for (unsigned st = 0; st < STEPS_PER_BAR; st++) {
            int n;
            if ((n = song_lead(bar, st)))  printf("N %u %u 0 %d %d\n", bar, st, n, song_lead_slide(bar, st));
            if ((n = song_bass(bar, st)))  printf("N %u %u 1 %d 0\n", bar, st, n);
            for (int v = 0; v < 4; v++) if ((n = song_piano(bar, st, v))) printf("N %u %u %d %d 0\n", bar, st, 2 + v, n);
            if ((n = song_choir(bar, st))) printf("N %u %u 6 %d 0\n", bar, st, n);
            const unsigned e = song_events(bar, st);
            if (e) printf("E %u %u %u\n", bar, st, e);
        }
        for (int v = 0; v < 4; v++) { const int n = song_pad(bar, v); if (n) printf("N %u 0 %d %d 0\n", bar, 7 + v, n); }
    }
}

static void hashes(void)
{
    synth_init();
    static int16_t buf[2 * 300];
    for (uint32_t p = 0; p < DURATION_SAMPLES; p += 300) {
        synth_render(buf, 300);
        uint32_t hp = 0, hv = 0;
        if (synth_hash_latch(&hp, &hv)) printf("%u %08x\n", (unsigned)hp, (unsigned)hv);
    }
}

static void cuts(void)
{
    for (unsigned i = 0; i < song_cut_count(); i++) {
        const cut_t *c = song_cut_at(i);
        printf("%u %u %u %u %u %u %u\n", (unsigned)cut_sample(c), c->bar, c->beat, c->shot, c->world, c->flags, c->variant);
    }
}

static void boards(void)
{
    for (unsigned i = 0; i < song_board_count(); i++) {
        const board_t *b = song_board_at(i);
        printf("%u %u %u %u", b->bar * BAR_SAMPLES + b->beat * BEAT_SAMPLES + b->step * STEP_SAMPLES, b->bar, b->beat, b->step);
        for (int r = 0; r < BOARD_ROWS; r++) printf("	%s", b->row[r] ? b->row[r] : "");
        printf("\n");
    }
}

static void timetable(void)
{
    song_init();
    for (uint32_t s = 0; s <= DURATION_SAMPLES; s += BEAT_SAMPLES)
        printf("%u %u %d %u %u\n", s / BAR_SAMPLES, (s / BEAT_SAMPLES) & 3, song_speed(s), song_distance(s), song_distance(s) / JOINT_UNITS);
}

int main(int argc, char **argv)
{
    const char *wav = NULL;
    int chunk = 512;
    unsigned solo = SOLO_ALL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else if (!strcmp(argv[i], "--chunk") && i + 1 < argc) chunk = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--solo") && i + 1 < argc) solo = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--bars") && i + 1 < argc) g_frames = (uint32_t)atoi(argv[++i]) * BAR_SAMPLES;
        else if (!strcmp(argv[i], "--from") && i + 1 < argc) g_from = (uint32_t)atoi(argv[++i]) * BAR_SAMPLES;
        else if (!strcmp(argv[i], "--dump")) { dump(); return 0; }
        else if (!strcmp(argv[i], "--hashes")) { hashes(); return 0; }
        else if (!strcmp(argv[i], "--cuts")) { cuts(); return 0; }
        else if (!strcmp(argv[i], "--boards")) { boards(); return 0; }
        else if (!strcmp(argv[i], "--timetable")) { timetable(); return 0; }
        else { fprintf(stderr, "song_harness --wav out.wav [--chunk N] [--solo MASK] [--bars N] [--from BAR] | --dump | --hashes | --cuts | --boards | --timetable\n"); return 2; }
    }
    if (chunk < 1 || chunk > 4096) { fprintf(stderr, "chunk 1..4096\n"); return 2; }
    if (!wav) { fprintf(stderr, "nothing to do\n"); return 2; }
    write_wav(wav, chunk, solo);
    return 0;
}
