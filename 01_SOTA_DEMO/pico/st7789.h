#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>

#define ST7789_WIDTH  240
#define ST7789_HEIGHT 240

/* Initialise the display. Call once after stdio_init_all(). */
void st7789_init(void);

/* Pack 8-bit-per-channel RGB into the 16-bit RGB-565 word that the panel
 * expects on the wire (bytes go out big-endian, so this also handles the
 * MSB/LSB swap by returning the value already byte-swapped). */
static inline uint16_t st7789_rgb565_be(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t v = ((uint16_t)(r >> 3) << 11) | ((uint16_t)(g >> 2) << 5) | (b >> 3);
    return (uint16_t)((v >> 8) | (v << 8));
}

/* Set the pixel write window to the full panel (0,0)-(239,239) and issue
 * RAMWR. After this the display is ready to accept a stream of 240*240
 * 16-bit pixels. */
void st7789_begin_frame(void);

/* Set the pixel write window to a sub-rectangle and issue RAMWR.
 * Coordinates are inclusive. After this the display accepts
 * (x1-x0+1)*(y1-y0+1) 16-bit pixels. */
void st7789_begin_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

/* Push N 16-bit pixels (already byte-swapped to big-endian wire order) to
 * the display. Blocks until DMA completes. Caller can break a full frame
 * into as many chunks as it wants. */
void st7789_write_pixels(const uint16_t *pixels, uint32_t count);

#endif
