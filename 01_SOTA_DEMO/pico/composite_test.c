/*
 * Composite / S-Video Y-channel bring-up test for the RP2040 Zero.
 *
 * Two-resistor "1-bit + sync" output on GP14 (video, 1 kΩ) and GP15
 * (sync, 430 Ω), summed to either an RCA composite jack or the Y pin of
 * an S-Video connector.
 *
 *   PIO clk      125 MHz / 20 = 6.25 MHz   (one PIO instruction = one pixel)
 *   Line        400 px  =  64.0 µs           (50 Hz vertical exactly)
 *      HSYNC     30 px      4.8 µs   sync=0 video=0
 *      back     30 px      4.8 µs   sync=1 video=0
 *      active   320 px     51.2 µs  sync=1 video=test pattern
 *      front    20 px      3.2 µs   sync=1 video=0
 *   Frame       312 lines = 19.97 ms
 *      VSYNC      3 lines (whole line sync=0)
 *      vbl-top  22 lines (HSYNC only, no video)
 *      active  256 lines (test pattern)
 *      vbl-bot  31 lines (HSYNC only, no video)
 *
 * Each pixel is 2 bits packed bit0=video, bit1=sync. 16 pixels per uint32_t.
 * Per line: 400 / 16 = 25 uint32_t = 100 bytes.
 * Per frame: 312 * 100 = 31200 bytes (in BSS, statically allocated).
 *
 * What you should see on the TV: 8 vertical bars alternating black/white,
 * stable, no rolling, no tearing. If you see no signal or rolling, sync
 * timing is off. If everything is one solid bar, the bit packing is off.
 */

#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"

#include "composite.pio.h"

#define PIN_VIDEO 14
#define PIN_SYNC  15

#define LINE_PX            400
#define HSYNC_PX           32
#define BACK_PORCH_PX      32
#define ACTIVE_PX          320
#define FRONT_PORCH_PX     16

#define LINES_VSYNC        3
#define LINES_VBL_TOP      22
#define LINES_ACTIVE       256
#define LINES_VBL_BOT      31
#define LINES_TOTAL        (LINES_VSYNC + LINES_VBL_TOP + LINES_ACTIVE + LINES_VBL_BOT)
/* sanity */
_Static_assert(LINES_TOTAL == 312, "PAL line count");

/* 2 bits per pixel: bit0=video (GP14, 1kΩ), bit1=sync (GP15, 430Ω) */
#define PIX_SYNC_LOW   0b00   /* sync tip       */
#define PIX_BLACK      0b10   /* blanking/black */
#define PIX_WHITE      0b11   /* white          */

#define WORDS_PER_LINE (LINE_PX / 16)   /* 25 */
static uint32_t framebuf[LINES_TOTAL][WORDS_PER_LINE];

/* Pack 16 pixels (2 bits each) into one uint32_t, LSB first. */
static inline uint32_t pack_pixels16(const uint8_t *p) {
    uint32_t w = 0;
    for (int i = 0; i < 16; i++) w |= ((uint32_t)(p[i] & 3u)) << (i * 2);
    return w;
}

static void encode_line(uint32_t *line, const uint8_t active[ACTIVE_PX], int has_active) {
    uint8_t pix[LINE_PX];
    int x = 0;
    for (int i = 0; i < HSYNC_PX;       i++) pix[x++] = PIX_SYNC_LOW;
    for (int i = 0; i < BACK_PORCH_PX;  i++) pix[x++] = PIX_BLACK;
    if (has_active) {
        for (int i = 0; i < ACTIVE_PX;  i++) pix[x++] = active[i];
    } else {
        for (int i = 0; i < ACTIVE_PX;  i++) pix[x++] = PIX_BLACK;
    }
    for (int i = 0; i < FRONT_PORCH_PX; i++) pix[x++] = PIX_BLACK;

    for (int w = 0; w < WORDS_PER_LINE; w++) {
        line[w] = pack_pixels16(&pix[w * 16]);
    }
}

static void encode_vsync_line(uint32_t *line) {
    /* Whole line sync-low — broad pulse style, the most TV-tolerant
     * way to mark vertical retrace short of full PAL serration. */
    uint8_t pix[LINE_PX];
    for (int i = 0; i < LINE_PX; i++) pix[i] = PIX_SYNC_LOW;
    for (int w = 0; w < WORDS_PER_LINE; w++) {
        line[w] = pack_pixels16(&pix[w * 16]);
    }
}

static void build_frame(void) {
    /* 8 vertical bars across 320 active pixels, alternating black/white */
    uint8_t bar[ACTIVE_PX];
    int bar_w = ACTIVE_PX / 8;          /* 40 */
    for (int x = 0; x < ACTIVE_PX; x++) {
        bar[x] = ((x / bar_w) & 1) ? PIX_WHITE : PIX_BLACK;
    }

    int line = 0;
    for (int i = 0; i < LINES_VSYNC;   i++) encode_vsync_line(framebuf[line++]);
    for (int i = 0; i < LINES_VBL_TOP; i++) encode_line(framebuf[line++], NULL, 0);
    for (int i = 0; i < LINES_ACTIVE;  i++) encode_line(framebuf[line++], bar,  1);
    for (int i = 0; i < LINES_VBL_BOT; i++) encode_line(framebuf[line++], NULL, 0);
}

static int dma_chan;
static uint32_t dma_total_words;

/* DMA completion IRQ: when the framebuffer ends, re-arm and re-trigger
 * for the next frame. Chain triggers alone don't work on RP2040 — the
 * trans_count register doesn't auto-reload. */
static void __isr dma_complete_isr(void) {
    dma_hw->ints0 = 1u << dma_chan;
    dma_channel_set_trans_count(dma_chan, dma_total_words, false);
    dma_channel_set_read_addr(dma_chan, framebuf, true);   /* trigger */
}

static void start_pio_dma(void) {
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &composite_video_2bit_program);

    /* PIO clk = sysclk / 20 = 6.25 MHz → 1 px = 160 ns → 400 px = 64 µs line */
    float clkdiv = (float)clock_get_hz(clk_sys) / (6250000.0f);
    composite_video_2bit_init(pio, sm, offset, PIN_VIDEO, clkdiv);

    dma_chan = dma_claim_unused_channel(true);
    dma_total_words = LINES_TOTAL * WORDS_PER_LINE;

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(dma_chan, &c, &pio->txf[sm],
                          framebuf, dma_total_words, false);

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_complete_isr);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_start(dma_chan);
}

int main(void) {
    stdio_init_all();
    sleep_ms(200);

    build_frame();
    start_pio_dma();

    while (true) tight_loop_contents();
}
