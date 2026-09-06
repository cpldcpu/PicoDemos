/* Host harness for the song: renders synth.c + song.c to a WAV with a chosen
 * block size and solo mask, or dumps the note tables as text for the piano
 * roll. Built by song_check.py / song_roll.py; not part of either target.
 *
 *   song_harness --wav out.wav [--chunk N] [--solo MASK]
 *   song_harness --dump              rows and per-bar arrangement, as text
 */

#include "synth.h"
#include "song.h"
#include "persistence.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_wav(const char *path, int chunk, unsigned solo)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    uint32_t n = PV_TOTAL_SAMPLES, data = n * 4, riff = data + 36, rate = PV_RATE, br = rate * 4, fmt = 16;
    uint16_t pcm = 1, ch = 2, align = 4, bits = 16;
    fwrite("RIFF", 1, 4, fp); fwrite(&riff, 4, 1, fp); fwrite("WAVEfmt ", 1, 8, fp);
    fwrite(&fmt, 4, 1, fp); fwrite(&pcm, 2, 1, fp); fwrite(&ch, 2, 1, fp);
    fwrite(&rate, 4, 1, fp); fwrite(&br, 4, 1, fp); fwrite(&align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
    fwrite("data", 1, 4, fp); fwrite(&data, 4, 1, fp);

    synth_solo(solo);
    synth_reset();
    static int16_t buf[2 * 4096];
    for (uint32_t p = 0; p < n;) {
        uint32_t k = n - p; if (k > (uint32_t)chunk) k = (uint32_t)chunk;
        synth_render(buf, (int)k);
        fwrite(buf, 4, k, fp);
        p += k;
    }
    fclose(fp);
    uint32_t hp = 0, hh = 0;
    synth_hash_latch(&hp, &hh);
    printf("%s chunk=%d solo=%u peak=%d hash@%u=%08x\n", path, chunk, solo, (int)synth_peak(), (unsigned)hp, (unsigned)hh);
}

static void dump(void)
{
    printf("# step bass arp lead lead2 drums\n");
    for (uint32_t s = 0; s < (uint32_t)PV_BARS * PV_STEPS_PER_BAR; s++)
        printf("S %u %d %d %d %d %u\n", (unsigned)s, song_bass(s), song_arp(s),
               song_lead(s), song_lead2(s), (unsigned)song_drums(s));
    printf("# bar section transpose pad0 pad1 pad2 pad3 lead_lvl lead_cut lead2_lvl pad_lvl riser energy voices\n");
    for (uint32_t b = 0; b < PV_BARS; b++) {
        uint8_t c[4]; song_pad_chord(b, c);
        printf("B %u %d %d %u %u %u %u %d %d %d %d %d %d %u\n", (unsigned)b, song_section(b),
               song_transpose(b), c[0], c[1], c[2], c[3], song_lead_level(b), song_lead_cut(b),
               song_lead2_level(b), song_pad_level(b), song_riser(b), song_energy(b),
               (unsigned)song_voices(b));
    }
}

int main(int argc, char **argv)
{
    const char *wav = NULL;
    int chunk = 1024;
    unsigned solo = SOLO_ALL;
    int do_dump = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--wav") && i + 1 < argc)        wav = argv[++i];
        else if (!strcmp(argv[i], "--chunk") && i + 1 < argc) chunk = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--solo") && i + 1 < argc)  solo = (unsigned)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--dump"))                  do_dump = 1;
        else { fprintf(stderr, "unknown arg %s\n", argv[i]); return 1; }
    }
    if (chunk < 1) chunk = 1;
    if (chunk > 4096) chunk = 4096;
    synth_init();
    if (do_dump) dump();
    if (wav) write_wav(wav, chunk, solo);
    return 0;
}
