/* Minimal ST7789 240x240 driver for the SOTA pico port.
 *
 * SPI hardware: SPI0 @ ~31.25 MHz (sysclk 125 MHz / 4). ST7789 spec maxes
 * out at 62.5 MHz so we have headroom; 31 MHz is a comfortable starting
 * point that should work with most jumper-wire setups.
 *
 * Pixel format: RGB-565, big-endian on the wire.
 */

#include "st7789.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"

#define ST7789_SPI       spi0
/* ST7789 spec max is 62.5 MHz (sysclk/2). If you see shimmering snow on
 * the panel, drop this to 31250 * 1000 — most jumper-wire setups can do
 * 62.5 MHz fine but flying-lead breadboard wiring is sometimes flaky. */
#define ST7789_BAUD_HZ   (62500 * 1000)

#define PIN_SCK   2
#define PIN_MOSI  3
#define PIN_RESET 5
#define PIN_DC    6

static int dma_chan = -1;

/* DC line: low = command byte, high = data bytes. */
static inline void st7789_dc_command(void) { gpio_put(PIN_DC, 0); }
static inline void st7789_dc_data(void)    { gpio_put(PIN_DC, 1); }

static void st7789_cmd(uint8_t cmd)
{
    st7789_dc_command();
    spi_write_blocking(ST7789_SPI, &cmd, 1);
}

static void st7789_cmd_with_data(uint8_t cmd, const uint8_t *data, size_t n)
{
    st7789_cmd(cmd);
    if (n) {
        st7789_dc_data();
        spi_write_blocking(ST7789_SPI, data, n);
    }
}

void st7789_init(void)
{
    spi_init(ST7789_SPI, ST7789_BAUD_HZ);
    spi_set_format(ST7789_SPI, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_RESET); gpio_set_dir(PIN_RESET, GPIO_OUT);
    gpio_init(PIN_DC);    gpio_set_dir(PIN_DC,    GPIO_OUT);

    /* Hardware reset: low for 50 ms, high for 50 ms. */
    gpio_put(PIN_RESET, 1); sleep_ms(50);
    gpio_put(PIN_RESET, 0); sleep_ms(50);
    gpio_put(PIN_RESET, 1); sleep_ms(150);

    st7789_cmd(0x01); sleep_ms(150);                          /* SWRESET */
    st7789_cmd(0x11); sleep_ms(120);                          /* SLPOUT  */
    st7789_cmd_with_data(0x3A, (const uint8_t[]){0x55}, 1);   /* COLMOD = 16bpp */
    st7789_cmd_with_data(0x36, (const uint8_t[]){0x00}, 1);   /* MADCTL */
    /* Most 240x240 ST7789 boards ship with the panel wired so colors come
     * out inverted unless you turn INVON on. If yours looks like a
     * photographic negative, comment this line out. */
    st7789_cmd(0x21);                                         /* INVON  */
    st7789_cmd(0x13);                                         /* NORON  */
    sleep_ms(10);
    st7789_cmd(0x29); sleep_ms(20);                           /* DISPON */

    /* Allocate one DMA channel for streaming pixel data. */
    if (dma_chan < 0) {
        dma_chan = dma_claim_unused_channel(true);
    }
}

void st7789_begin_frame(void)
{
    st7789_begin_window(0, 0, 239, 239);
}

void st7789_begin_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    uint8_t cas[4] = { 0, x0, 0, x1 };
    uint8_t ras[4] = { 0, y0, 0, y1 };
    st7789_cmd_with_data(0x2A, cas, 4);
    st7789_cmd_with_data(0x2B, ras, 4);
    st7789_cmd(0x2C);
    st7789_dc_data();
}

void st7789_write_pixels(const uint16_t *pixels, uint32_t count)
{
    /* SPI peripheral expects 8-bit transfers in the format we set up
     * (we configured SPI_8 above). The pixels buffer is already
     * byte-swapped to big-endian wire order by st7789_rgb565_be(), so
     * we DMA it as a flat byte stream of count*2 bytes. */
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(ST7789_SPI, true));

    dma_channel_configure(
        dma_chan, &c,
        &spi_get_hw(ST7789_SPI)->dr,    /* destination: SPI TX FIFO */
        pixels,                          /* source */
        count * 2,                       /* byte count */
        true                             /* start now */
    );
    dma_channel_wait_for_finish_blocking(dma_chan);

    /* Wait for the SPI shifter to drain so the next CS-toggle / command
     * doesn't clip the last byte. */
    while (spi_is_busy(ST7789_SPI)) {
        tight_loop_contents();
    }
}
