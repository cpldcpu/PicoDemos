/*
 *  qoaconv_s16.c — host build tool. Converts s16le PCM (mono or stereo,
 *  interleaved) to QOA. For 20_TheDemo we feed ffmpeg's s16le output of
 *  the user's MP3.
 *
 *  Build:
 *      gcc qoaconv_s16.c -std=gnu99 -lm -O3 -o qoaconv_s16.exe
 *
 *  Use:
 *      qoaconv_s16 input.raw output.qoa <sample_rate> <channels=1|2>
 *
 *  Example pipeline (run in WSL or PowerShell with ffmpeg installed):
 *      ffmpeg -i music.mp3 -ac 2 -ar 22050 -f s16le music.raw
 *      ./qoaconv_s16 music.raw music.qoa 22050 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define QOA_IMPLEMENTATION
#include "qoa.h"

int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s input.raw output.qoa <sample_rate> <channels=1|2>\n",
                argv[0]);
        return 1;
    }
    int sample_rate = atoi(argv[3]);
    int channels    = atoi(argv[4]);
    if (sample_rate <= 0 || (channels != 1 && channels != 2)) {
        fprintf(stderr, "bad sample_rate or channels\n");
        return 1;
    }

    FILE *fin = fopen(argv[1], "rb");
    if (!fin) { perror(argv[1]); return 1; }
    fseek(fin, 0, SEEK_END);
    long sz_bytes = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (sz_bytes <= 0 || (sz_bytes & 1)) {
        fprintf(stderr, "empty / odd-sized input (%ld bytes)\n", sz_bytes); return 1;
    }

    long total_shorts = sz_bytes / 2;
    long frame_count  = total_shorts / channels;  /* per-channel sample frames */

    short *samples = malloc((size_t)sz_bytes);
    if (!samples) { fprintf(stderr, "OOM\n"); return 1; }
    if (fread(samples, 1, (size_t)sz_bytes, fin) != (size_t)sz_bytes) {
        fprintf(stderr, "input read failed\n"); return 1;
    }
    fclose(fin);

    qoa_desc desc = {
        .samplerate = (unsigned int)sample_rate,
        .channels   = (unsigned int)channels,
        .samples    = (unsigned int)frame_count,
    };

    unsigned int out_size = 0;
    void *encoded = qoa_encode(samples, &desc, &out_size);
    free(samples);
    if (!encoded || out_size == 0) {
        fprintf(stderr, "qoa_encode failed\n"); return 1;
    }

    FILE *fout = fopen(argv[2], "wb");
    if (!fout) { perror(argv[2]); return 1; }
    if (fwrite(encoded, 1, out_size, fout) != out_size) {
        fprintf(stderr, "write failed\n"); return 1;
    }
    fclose(fout);
    free(encoded);

    fprintf(stderr, "%s (%ld bytes, %ld frames, %d ch, %d Hz, %.2f s)\n"
                    "  -> %s (%u bytes, %.1f%% of original)\n",
            argv[1], sz_bytes, frame_count, channels, sample_rate,
            (double)frame_count / sample_rate,
            argv[2], out_size, 100.0 * out_size / (double)sz_bytes);
    return 0;
}
