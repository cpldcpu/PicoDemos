/* Host harness for the HELION score: renders synth.c + a song to a WAV with
 * a chosen block size and solo mask, or dumps the note tables as text for
 * the piano roll. Built by song_check.py / song_roll.py / song_audition.py;
 * not part of either target. Linked against song.c it renders the film's
 * score; against tools/audition_song.c, the instrument audition.
 *
 *   song_harness --wav out.wav [--chunk N] [--solo MASK] [--bars N]
 *   song_harness --dump              rows and per-bar arrangement, as text
 *   song_harness --hashes            the per-second FNV-1a latches (pos hash), one a line,
 *                                    to diff against what the device prints
 */

#include "synth.h"
#include "song.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_frames = DURATION_SAMPLES;

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
    static int16_t buf[2 * 4096];
    for (uint32_t p = 0; p < n;) {
        uint32_t k = n - p; if (k > (uint32_t)chunk) k = (uint32_t)chunk;
        synth_render(buf, k);
        fwrite(buf, 4, k, fp);
        p += k;
    }
    fclose(fp);
    uint32_t hp = 0, hh = 0;
    synth_hash_latch(&hp, &hh);
    printf("%s chunk=%d solo=%u peak=%d hash@%u=%08x\n", path, chunk, solo, (int)synth_peak(), (unsigned)hp, (unsigned)hh);
}

static void hashes(void)
{
    synth_init();
    static int16_t buf[2 * 4096];
    for (uint32_t p = 0; p < DURATION_SAMPLES;) {
        uint32_t k = DURATION_SAMPLES - p; if (k > 1024) k = 1024;
        synth_render(buf, k);
        p += k;
        uint32_t hp = 0, hh = 0;
        if (synth_hash_latch(&hp, &hh)) printf("H %u %08x\n", (unsigned)hp, (unsigned)hh);
    }
}

static void dump(void)
{
    printf("# step bass bronze reed harm drums bowgate\n");
    for (uint32_t s = 0; s < SONG_BARS * SONG_STEPS_PER_BAR; s++)
        printf("S %u %d %d %d %d %u %d\n", (unsigned)s, song_bass(s), song_bronze(s),
               song_lead(s), song_harm(s), (unsigned)song_drums(s), song_bow_gate(s));
    printf("# bar section bow0 bow1 bow2 bow3 lead_lvl lead_push bow_lvl bow_open bronze_lvl bronze_dec bass_lvl bass_bite harm_lvl air space energy voices\n");
    for (uint32_t b = 0; b < SONG_BARS; b++) {
        uint8_t c[4]; song_bow_chord(b, c);
        printf("B %u %d %u %u %u %u %d %d %d %d %d %d %d %d %d %d %d %d %u\n", (unsigned)b, song_section(b),
               c[0], c[1], c[2], c[3], song_lead_level(b), song_lead_push(b), song_bow_level(b), song_bow_open(b),
               song_bronze_level(b), song_bronze_decay(b), song_bass_level(b), song_bass_bite(b),
               song_harm_level(b), song_air_level(b), song_space(b), song_energy(b), (unsigned)song_voices(b));
    }
}

int main(int argc, char **argv)
{
    const char *wav = NULL;
    int chunk = 1024;
    unsigned solo = SOLO_ALL;
    int do_dump = 0, do_hashes = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--wav") && i + 1 < argc)        wav = argv[++i];
        else if (!strcmp(argv[i], "--chunk") && i + 1 < argc) chunk = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--solo") && i + 1 < argc)  solo = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--bars") && i + 1 < argc)  g_frames = (uint32_t)atoi(argv[++i]) * SONG_BAR_SAMPLES;
        else if (!strcmp(argv[i], "--dump"))                  do_dump = 1;
        else if (!strcmp(argv[i], "--hashes"))                do_hashes = 1;
        else { fprintf(stderr, "unknown arg %s\n", argv[i]); return 1; }
    }
    if (chunk < 1) chunk = 1;
    if (chunk > 4096) chunk = 4096;
    if (g_frames > DURATION_SAMPLES) g_frames = DURATION_SAMPLES;
    synth_init();
    if (do_dump) dump();
    if (do_hashes) hashes();
    if (wav) write_wav(wav, chunk, solo);
    return 0;
}
