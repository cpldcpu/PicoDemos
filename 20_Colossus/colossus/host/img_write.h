/* PNG and PPM out of a plain RGB24 buffer, with no image library.
 *
 * Header-only so the capture tool and the SDL player share one writer. The
 * PNG is a real PNG: the IDAT is a zlib stream made of stored (uncompressed)
 * deflate blocks, which every decoder accepts. A 320x240 still is about
 * 230 KB, which is the price of not linking zlib into a tool whose entire
 * job is to let a human look at a frame.
 */

#ifndef CV_IMG_WRITE_H
#define CV_IMG_WRITE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t cv_crc_table[256];
static int      cv_crc_ready;

static inline void cv_crc_init(void)
{
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : (c >> 1);
        cv_crc_table[n] = c;
    }
    cv_crc_ready = 1;
}

static inline uint32_t cv_crc(uint32_t c, const uint8_t *b, size_t n)
{
    if (!cv_crc_ready) cv_crc_init();
    for (size_t i = 0; i < n; i++) c = cv_crc_table[(c ^ b[i]) & 0xFF] ^ (c >> 8);
    return c;
}

static inline void cv_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

static inline void cv_chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len)
{
    uint8_t hdr[4];
    cv_be32(hdr, len);
    fwrite(hdr, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (len) fwrite(data, 1, len, f);
    uint32_t c = cv_crc(0xFFFFFFFFu, (const uint8_t *)type, 4);
    if (len) c = cv_crc(c, data, len);
    cv_be32(hdr, c ^ 0xFFFFFFFFu);
    fwrite(hdr, 1, 4, f);
}

static inline int cv_write_png(const char *path, const uint8_t *rgb, int w, int h)
{
    const size_t stride = 1u + (size_t)w * 3u, rawn = stride * (size_t)h;
    uint8_t *raw = (uint8_t *)malloc(rawn);
    uint8_t *z = (uint8_t *)malloc(rawn + rawn / 65535u * 5u + 64u);
    if (!raw || !z) { free(raw); free(z); return 0; }

    for (int y = 0; y < h; y++) {
        raw[(size_t)y * stride] = 0;                     /* filter: none */
        memcpy(raw + (size_t)y * stride + 1, rgb + (size_t)y * w * 3, (size_t)w * 3);
    }

    size_t zn = 0, off = 0, left = rawn;
    z[zn++] = 0x78; z[zn++] = 0x01;
    while (left) {
        const size_t n = left > 65535u ? 65535u : left;
        z[zn++] = (left == n) ? 1u : 0u;
        z[zn++] = (uint8_t)(n & 0xFF);        z[zn++] = (uint8_t)(n >> 8);
        z[zn++] = (uint8_t)((~n) & 0xFF);     z[zn++] = (uint8_t)(((~n) >> 8) & 0xFF);
        memcpy(z + zn, raw + off, n);
        zn += n; off += n; left -= n;
    }
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < rawn; i++) { a = (a + raw[i]) % 65521u; b = (b + a) % 65521u; }
    cv_be32(z + zn, (b << 16) | a); zn += 4;

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); free(raw); free(z); return 0; }
    static const uint8_t sig[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    cv_be32(ihdr, (uint32_t)w); cv_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    cv_chunk(f, "IHDR", ihdr, 13);
    cv_chunk(f, "IDAT", z, (uint32_t)zn);
    cv_chunk(f, "IEND", NULL, 0);
    const int ok = fclose(f) == 0;
    free(raw); free(z);
    return ok;
}

static inline int cv_write_ppm(const char *path, const uint8_t *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return 0; }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    return fclose(f) == 0;
}

#endif
