/* VGA bring-up test — v15: bands + 250 MHz overclock + USB CDC printf
 * heartbeat. Lets us verify both the overclock and the USB-serial debug
 * path in isolation before relying on them in the full engine build. */
#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#define WIDTH  320
#define HEIGHT 240

static void render_scanline(struct scanvideo_scanline_buffer *buffer)
{
    uint32_t y = scanvideo_scanline_number(buffer->scanline_id);

    uint16_t color;
    if (y < HEIGHT / 3) {
        color = (uint16_t)PICO_SCANVIDEO_PIXEL_FROM_RGB8(255, 0, 0);
    } else if (y < 2 * HEIGHT / 3) {
        color = (uint16_t)PICO_SCANVIDEO_PIXEL_FROM_RGB8(0, 255, 0);
    } else {
        color = (uint16_t)PICO_SCANVIDEO_PIXEL_FROM_RGB8(0, 0, 255);
    }

    uint16_t *out = (uint16_t *)buffer->data;
    *out++ = COMPOSABLE_COLOR_RUN;
    *out++ = color;
    *out++ = WIDTH - 3;
    *out++ = COMPOSABLE_EOL_ALIGN;
    buffer->data_used = 2;
    buffer->status    = SCANLINE_OK;
}

static void core1_main(void)
{
    scanvideo_setup(&vga_mode_320x240_60);
    scanvideo_timing_enable(true);
    while (true) {
        struct scanvideo_scanline_buffer *buf = scanvideo_begin_scanline_generation(true);
        render_scanline(buf);
        scanvideo_end_scanline_generation(buf);
    }
}

int main(void)
{
    /* Overclock FIRST so when stdio_init_all brings USB up, USB
     * enumerates at the final 250 MHz clock; changing sys_clk after USB
     * has registered with the host causes the host to drop the device. */
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(250000, true);

    stdio_init_all();

    for (uint i = 0; i <= 17; i++) {
        gpio_set_function(i, GPIO_FUNC_PIO0);
    }

    sleep_ms(200);
    multicore_launch_core1(core1_main);

    /* USB heartbeat. If we see this on serial, USB CDC works at 250 MHz. */
    uint32_t tick = 0;
    while (true) {
        printf("[tick %lu] alive @ %lu kHz\n",
               (unsigned long)tick++,
               (unsigned long)(clock_get_hz(clk_sys) / 1000));
        sleep_ms(1000);
    }
}
