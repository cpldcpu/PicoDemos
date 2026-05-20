#ifndef BITPLANE_COMPOSE_H
#define BITPLANE_COMPOSE_H

#include <stdint.h>

/* Compose one scanline of the 240-wide display.
 *
 * planes:    six row pointers, each pointing at the start of the scanline
 *            in the corresponding bitplane (stride is the per-row byte
 *            count of that plane; pass NULL pointers for unused planes).
 * palette:   64-entry palette of 0xAARRGGBB (matches SDL_PIXELFORMAT_ARGB8888,
 *            which is what the engine writes via backend_set_palette and the
 *            choreographer's palette commands — see e.g. build_demo.py
 *            literals like 0xff110022 = alpha ff, R 11, G 00, B 22).
 * out_565:   240 uint16_t output pixels, byte-swapped to ST7789 wire
 *            order (i.e. ready to DMA to the panel).
 */
void compose_scanline_565(const uint8_t * const planes[6],
                          const uint32_t palette[64],
                          uint16_t out_565[240]);

#endif
