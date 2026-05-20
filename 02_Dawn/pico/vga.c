#include "vga.h"
#include "dawn.h"
#include "chunky.h"
#include "palette.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"

/* VGA dimensions. The scanvideo mode is 320×240@60 from pico-extras.
 * (Named with the DAWN_ prefix to avoid collision with the SDK's VGA_H
 * register macro pulled in via the scanvideo headers.) */
#define DAWN_VGA_W 320
#define DAWN_VGA_H 240

static void render_scanline(struct scanvideo_scanline_buffer *buf)
{
    const int line = scanvideo_scanline_number(buf->scanline_id);

    /* 2× vertical scale: each chunky row covers two VGA scanlines. */
    int src_row = VPORT_TOP + (line >> 1);
    if (src_row < 0) src_row = 0;
    if (src_row >= SCREEN_H) src_row = SCREEN_H - 1;

    /* Read the live FRONT buffer pointer — chunky_present() on core 0
     * publishes a new one during vblank. */
    uint8_t *front = __atomic_load_n(&chunky_scanout, __ATOMIC_SEQ_CST);
    const uint8_t *src = front + src_row * SCREEN_W;
    uint16_t *out16 = (uint16_t *)buf->data;

    /* pico_scanvideo composable layout:
     *   word 0   = COMPOSABLE_RAW_RUN
     *   word 1   = first pixel
     *   word 2   = count - 3
     *   word 3.. = remaining pixels
     *   word N   = COMPOSABLE_EOL_ALIGN (when total is odd)
     *
     * We emit DAWN_VGA_W (320) RGB-565 pixels. Doubled from 160 chunky pixels
     * by reading each src byte twice. */
    static uint16_t pix[DAWN_VGA_W];
    for (int x = 0; x < SCREEN_W; x++) {
        const uint16_t c = palette_rgb565[src[x] & 0x3F];
        pix[x * 2 + 0] = c;
        pix[x * 2 + 1] = c;
    }

    out16[0] = COMPOSABLE_RAW_RUN;
    out16[1] = pix[0];
    out16[2] = DAWN_VGA_W + 1 - 3;
    for (int i = 1; i < DAWN_VGA_W; i++) {
        out16[2 + i] = pix[i];
    }
    /* Pad to an even word count so EOL_ALIGN sits in the high half of
     * the last 32-bit word — same trick the SOTA VGA backend uses. */
    out16[DAWN_VGA_W + 2] = 0;
    out16[DAWN_VGA_W + 3] = COMPOSABLE_EOL_ALIGN;
    buf->data_used = (DAWN_VGA_W + 4) / 2;  /* in 32-bit words */
    buf->status = SCANLINE_OK;
}

static void core1_main(void)
{
    scanvideo_setup(&vga_mode_320x240_60);
    scanvideo_timing_enable(true);
    while (true) {
        struct scanvideo_scanline_buffer *b =
            scanvideo_begin_scanline_generation(true);
        render_scanline(b);
        scanvideo_end_scanline_generation(b);
    }
}

void vga_start(void)
{
    multicore_launch_core1(core1_main);
}
