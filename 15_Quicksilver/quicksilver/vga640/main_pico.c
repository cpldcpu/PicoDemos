/* main_pico.c — QUICKSILVER "FULL VGA" tech demo for RP2350.
 *
 * Real 640x480x60 VGA with NO framebuffer: core 1 generates each scanline live
 * via the SIO interpolator (POP self-stepping affine) straight into the
 * scanvideo line buffer as the beam races. A 640x480 truecolor framebuffer is
 * 614 KB > 520 KB SRAM, so beam-racing is the only way to do full VGA here.
 *
 * To hit the per-line deadline: the texture lives in SRAM (race.c), the
 * scanline generator AND this core-1 assembly loop are pinned in SRAM
 * (__not_in_flash_func), and core 0 precomputes the per-frame affine basis +
 * per-row flex so core 1 does no transcendentals.
 */

#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/scanvideo/scanvideo_base.h"
#include "pico/multicore.h"
#include "pico/platform.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include <math.h>

#include "race.h"

#define W 640
#define H 480
#define LED_PIN 25

static qs_race_params g_p;
static volatile bool  g_up = false;

static void __not_in_flash_func(core1_main)(void)
{
    scanvideo_setup(&vga_mode_640x480_60);
    scanvideo_timing_enable(true);
    qs_race_setup();                 /* core 1's interp0 + SRAM texture copy */
    g_up = true;

    static uint16_t line[W];
    while (true) {
        struct scanvideo_scanline_buffer *buf = scanvideo_begin_scanline_generation(true);
        int y = scanvideo_scanline_number(buf->scanline_id);
        qs_race_params p = g_p;                       /* snapshot */
        qs_race_scanline(line, y, W, H, &p);          /* interp POP beam-race */

        uint16_t *out = (uint16_t *)buf->data;
        out[0] = COMPOSABLE_RAW_RUN;
        out[1] = line[0];
        out[2] = W + 1 - 3;
        for (int x = 1; x < W; x++) out[2 + x] = line[x];   /* SRAM->SRAM copy */
        out[W + 2] = 0;
        out[W + 3] = COMPOSABLE_EOL_ALIGN;
        buf->data_used = (W + 4) / 2;
        buf->status = SCANLINE_OK;
        scanvideo_end_scanline_generation(buf);
    }
}

int main(void)
{
    gpio_init(LED_PIN); gpio_set_dir(LED_PIN, GPIO_OUT);
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(300000, true);
    stdio_init_all();

    static float flex[H];

    multicore_launch_core1(core1_main);
    while (!g_up) tight_loop_contents();

    uint32_t frame = 0;
    while (true) {
        float t = frame / 60.0f;
        float ang  = t * 0.5f;
        float zoom = 1.4f + 0.9f * sinf(t * 0.4f);
        for (int y = 0; y < H; y++)
            flex[y] = 1.0f + 0.15f * sinf(y * 0.018f + t * 2.0f);

        qs_race_params p = {
            .ca0 = cosf(ang) * zoom,
            .sa0 = sinf(ang) * zoom,
            .cu  = 128.0f + 50.0f * sinf(t * 0.21f),
            .cv  = 128.0f + 50.0f * cosf(t * 0.17f),
            .flex = flex,
        };
        scanvideo_wait_for_vblank();   /* publish params in the blanking interval */
        g_p = p;
        gpio_put(LED_PIN, frame & 1);
        frame++;
    }
}
