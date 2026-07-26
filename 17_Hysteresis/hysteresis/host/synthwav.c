/* Render the soundtrack to a WAV file.
 *
 *   make synthwav && ./synthwav.exe -o hysteresis.wav
 *
 * Links synth.c and score.c and nothing else -- no SDL, no field, no palette.
 * The point is that the music can be auditioned and rejected in about a second
 * without a Pico or a display in the room, which is the whole mitigation for
 * "I have not written music before" in PLANNING.md section 6.
 *
 *   -o FILE     output (default hysteresis.wav)
 *   --seconds N render only the first N seconds
 *   --from N    skip to N seconds -- renders from 0 and discards, because the
 *               synth has state and there is no seek here either
 *   --levels    per-second peak and RMS to stderr, as a numeric read of the arc
 *   --solo LIST any of bass,pad,noise,impact,reverb -- mute the rest. This is
 *               how the mix gets set: render one voice, read its RMS, decide.
 *   --chunk N   render in blocks of N samples. Exists to be varied: the device
 *               asks for a handful of samples per scanline and the host asks for
 *               thousands, so if the output depends on N at all then the two
 *               targets cannot agree and the referee's audio diff is measuring
 *               the block size instead of the synth.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../synth.h"
#include "../score.h"

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v); p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static void put16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v); p[1] = (unsigned char)(v >> 8);
}

static void write_header(FILE *f, uint32_t nsamples)
{
    unsigned char h[44];
    const uint32_t data = nsamples * 2u;
    memcpy(h, "RIFF", 4);          put32(h + 4, 36u + data);
    memcpy(h + 8, "WAVEfmt ", 8);  put32(h + 16, 16u);
    put16(h + 20, 1);              put16(h + 22, 1);          /* PCM, mono */
    put32(h + 24, SYNTH_RATE);     put32(h + 28, SYNTH_RATE * 2u);
    put16(h + 32, 2);              put16(h + 34, 16);
    memcpy(h + 36, "data", 4);     put32(h + 40, data);
    fwrite(h, 1, sizeof h, f);
}

#define CHUNK 4096

int main(int argc, char *argv[])
{
    const char *out = "hysteresis.wav";
    uint32_t seconds = 0, from = 0;
    unsigned solo = SOLO_ALL;
    int levels = 0, chunk = CHUNK;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc)             out = argv[++i];
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc)  seconds = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--from") && i + 1 < argc)     from = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--levels"))                   levels = 1;
        else if (!strcmp(argv[i], "--chunk") && i + 1 < argc) {
            chunk = atoi(argv[++i]);
            if (chunk < 1 || chunk > CHUNK) {
                fprintf(stderr, "--chunk wants 1..%d\n", CHUNK); return 1;
            }
        }
        else if (!strcmp(argv[i], "--solo") && i + 1 < argc) {
            const char *s = argv[++i];
            solo = 0;
            if (strstr(s, "bass"))   solo |= SOLO_BASS;
            if (strstr(s, "pad"))    solo |= SOLO_PAD;
            if (strstr(s, "noise"))  solo |= SOLO_NOISE;
            if (strstr(s, "impact")) solo |= SOLO_IMPACT;
            if (strstr(s, "reverb")) solo |= SOLO_REVERB;
            if (!solo) { fprintf(stderr, "--solo wants bass,pad,noise,impact,reverb\n"); return 1; }
        }
        else { fprintf(stderr, "unknown arg: %s\n", argv[i]); return 1; }
    }

    synth_solo(solo);
    synth_reset();

    const uint32_t total = synth_total_samples();
    uint32_t end = seconds ? from + seconds * SYNTH_RATE : total;
    if (end > total) end = total;
    const uint32_t skip = from * SYNTH_RATE;

    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    write_header(f, end > skip ? end - skip : 0);

    int16_t buf[CHUNK];
    uint32_t pos = 0;
    /* Per-second accumulators for --levels. */
    double sq = 0; int32_t pk = 0; long nsec = 0;

    while (pos < end) {
        uint32_t want = end - pos;
        if (want > (uint32_t)chunk) want = (uint32_t)chunk;
        synth_render(buf, (int)want);

        for (uint32_t i = 0; i < want; i++) {
            const int32_t s = buf[i];
            const int32_t a = s < 0 ? -s : s;
            if (a > pk) pk = a;
            sq += (double)s * s;
            nsec++;
            if (nsec == SYNTH_RATE) {
                if (levels)
                    fprintf(stderr, "t=%3us  peak=%6d  rms=%6.0f\n",
                            (unsigned)((pos + i) / SYNTH_RATE), pk,
                            sqrt(sq / SYNTH_RATE));
                sq = 0; pk = 0; nsec = 0;
            }
        }

        if (pos + want > skip) {
            const uint32_t off = pos >= skip ? 0 : skip - pos;
            fwrite(buf + off, 2, want - off, f);
        }
        pos += want;
    }
    fclose(f);

    fprintf(stderr, "wrote %s: %u samples (%.1f s), peak %d/32767\n",
            out, end - skip, (double)(end - skip) / SYNTH_RATE, synth_peak());
    return 0;
}
