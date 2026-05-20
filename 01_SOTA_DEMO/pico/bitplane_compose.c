/* 6-bitplane -> palette[64] -> RGB-565 (big-endian, ST7789 wire order)
 * scanline composer. Mirrors the loop in sota/native/posix_sdl2_backend.c
 * (backend_render) but writes to a 16-bit line buffer instead of a 32-bit
 * framebuffer.
 *
 * Hot path: roughly 5 ALU ops per pixel + a 32-bit palette load. At
 * 125 MHz on Cortex-M0+ that's ~50 cycles/pixel headroom; 240 px * 240 lines
 * = 57600 px per frame ~= ~25 fps trivially. Reduce SPI baud, not this
 * function, if anything is too tight.
 */

#include "bitplane_compose.h"

void compose_scanline_565(const uint8_t * const planes[6],
                          const uint32_t palette[64],
                          uint16_t out_565[240])
{
    const uint8_t *p0 = planes[0];
    const uint8_t *p1 = planes[1];
    const uint8_t *p2 = planes[2];
    const uint8_t *p3 = planes[3];
    const uint8_t *p4 = planes[4];
    const uint8_t *p5 = planes[5];

    /* 240 pixels = 30 bytes per plane, 8 pixels per byte. */
    for (int byte_x = 0; byte_x < 30; byte_x++) {
        uint8_t b0 = p0 ? p0[byte_x] : 0;
        uint8_t b1 = p1 ? p1[byte_x] : 0;
        uint8_t b2 = p2 ? p2[byte_x] : 0;
        uint8_t b3 = p3 ? p3[byte_x] : 0;
        uint8_t b4 = p4 ? p4[byte_x] : 0;
        uint8_t b5 = p5 ? p5[byte_x] : 0;

        for (uint8_t bit = 0x80; bit; bit >>= 1) {
            uint32_t idx =
                  ((b0 & bit) ? 1u  : 0)
                | ((b1 & bit) ? 2u  : 0)
                | ((b2 & bit) ? 4u  : 0)
                | ((b3 & bit) ? 8u  : 0)
                | ((b4 & bit) ? 16u : 0)
                | ((b5 & bit) ? 32u : 0);

            uint32_t argb = palette[idx];
            uint8_t r = (uint8_t)((argb >> 16) & 0xFF);
            uint8_t g = (uint8_t)((argb >>  8) & 0xFF);
            uint8_t b = (uint8_t)( argb        & 0xFF);

            uint16_t v = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            *out_565++ = (uint16_t)((v >> 8) | (v << 8));
        }
    }
}
