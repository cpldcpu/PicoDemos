/* VGA backend implementation — MODE_320, MODE_160, MODE_SPLIT.
 *
 * Pinout (default pico_scanvideo, matches Pimoroni VGA Demo Base):
 *   GP0..GP4   = Red    (LSB..MSB, 5 bits)
 *   GP5        = (gap)
 *   GP6..GP10  = Green
 *   GP11..GP15 = Blue
 *   GP16       = HSYNC
 *   GP17       = VSYNC
 *
 * Core 1 owns the scanvideo loop. Core 0 (the effect's frame()) writes
 * into the back buffer and calls vga_320_present() to flip.
 */

#include "vga.h"

#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdio.h>

/* Scene-skip buttons on the Pimoroni Pico VGA Demo Base. The three
 * buttons share GPIOs with the lowest DAC bit of each VGA color:
 *
 *   SW_A → GP0   (shares RED0)
 *   SW_B → GP6   (shares GREEN0)
 *   SW_C → GP11  (shares BLUE0)
 *
 * Each press connects +3.3V through a 1 kΩ series resistor to the
 * GPIO. No external pull, so the line floats when idle.
 *
 * Reading these without spoiling the DAC: pico-playground's `popcorn`
 * keeps the pads hi-Z (`gpio_set_oeover(OVERRIDE_LOW)`) for 95 % of the
 * frame and only enables PIO drive during the vsync pulse — that costs
 * the LSB of each colour channel ALL the time, and the resulting
 * banding is visible on smooth gradients.
 *
 * We do the inverse, and it's the obvious choice in retrospect:
 *
 *   - During active video: OEOVER stays NORMAL → PIO drives the DAC
 *     fully, no LSB loss.
 *   - On the VSYNC-active edge: in the ISR, briefly switch each button
 *     pad to OEOVER_LOW (pad goes hi-Z), wait ~5 µs for the internal
 *     pull-down to settle, sample, latch any rising edges, then restore
 *     OEOVER_NORMAL. The vsync pulse is the safest window — PIO is
 *     emitting the sync timing, not pixel data, so a few µs of pad
 *     hi-Z on the colour LSBs has no observable effect (the DAC is
 *     already at the black level for sync).
 *
 * pull-down is enabled once at init and left on (it's a pad-config bit,
 * harmless while the pad is driven). */
#define BTN_NEXT_PIN     0
#define BTN_PREV_PIN     6
#define BTN_RESTART_PIN  11
#define VSYNC_PIN        17

static void buttons_init(void);

static const uint8_t btn_pins[3] = { BTN_NEXT_PIN, BTN_PREV_PIN, BTN_RESTART_PIN };
static volatile uint8_t g_btn_state    = 0;   /* current pressed-bitmask  (bit i = pin i down) */
static volatile uint8_t g_btn_latched  = 0;   /* edge-detected presses, awaiting consume      */
static uint8_t          g_btn_prev     = 0;   /* last sample, for edge detection in ISR        */
static int              g_vsync_active_level = 0;  /* level seen during vblank (active-low → 0) */

/* --- shared state -------------------------------------------------------- */

static screen_mode_t g_mode = MODE_320;
static volatile bool g_scanvideo_up = false;

/* --- MODE_320 framebuffers + palette ------------------------------------ */

static uint8_t fb_a[VGA_320_W * VGA_320_H] __attribute__((aligned(4)));
static uint8_t fb_b[VGA_320_W * VGA_320_H] __attribute__((aligned(4)));
static uint8_t *fb_back  = fb_a;   /* effect writes here */
static uint8_t * volatile fb_front = fb_b;  /* scanline reads from here */

/* 256-entry palette, RGB555 already encoded for fast scanline lookup. */
static uint16_t palette_320[256];

uint8_t *vga_320_back_buffer(void) { return fb_back; }

void vga_320_palette_set(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    if ((unsigned)idx >= 256) return;
    palette_320[idx] = (uint16_t)PICO_SCANVIDEO_PIXEL_FROM_RGB8(r, g, b);
}

void vga_320_present(void)
{
    /* Atomic pointer flip. After this returns the old back is now front;
     * effect can safely overwrite the new back (= previous front). */
    uint8_t *old_back = fb_back;
    fb_back  = (fb_back == fb_a) ? fb_b : fb_a;
    __atomic_store_n(&fb_front, old_back, __ATOMIC_SEQ_CST);
}

/* --- MODE_160: 160x120 RGB565 framebuffer, displayed via the same
 *     320x240 scanvideo timing by pixel-doubling horizontally and
 *     line-doubling vertically. Cheap to scan (just one DMA-like
 *     replicate per source pixel) and lets effects work in true colour
 *     at the cost of half the resolution.
 *
 * Layout: same ping-pong as MODE_320. Effects write fb160_back, then
 * call vga_160_present() to publish. */
static uint16_t fb160_a[VGA_160_W * VGA_160_H] __attribute__((aligned(4)));
static uint16_t fb160_b[VGA_160_W * VGA_160_H] __attribute__((aligned(4)));
static uint16_t *fb160_back  = fb160_a;
static uint16_t * volatile fb160_front = fb160_b;

uint16_t *vga_160_back_buffer(void) { return fb160_back; }

void vga_160_present(void)
{
    uint16_t *old_back = fb160_back;
    fb160_back = (fb160_back == fb160_a) ? fb160_b : fb160_a;
    __atomic_store_n(&fb160_front, old_back, __ATOMIC_SEQ_CST);
}

/* --- scanline rendering (core 1) ---------------------------------------- */

static void render_scanline_320(struct scanvideo_scanline_buffer *buf)
{
    int y = scanvideo_scanline_number(buf->scanline_id);
    uint16_t *out = (uint16_t *)buf->data;

    const uint8_t *front = (const uint8_t *)__atomic_load_n(&fb_front, __ATOMIC_SEQ_CST);
    const uint8_t *src   = front + y * VGA_320_W;

    /* composable_scanline format:
     *   [0] COMPOSABLE_RAW_RUN
     *   [1] pixels[0]
     *   [2] run length = (W + 1 - 3) = 318
     *   [3..322] pixels[1..319]
     *   [323] COMPOSABLE_EOL_ALIGN
     */
    out[0] = COMPOSABLE_RAW_RUN;
    out[1] = palette_320[src[0]];
    out[2] = VGA_320_W + 1 - 3;
    for (int x = 1; x < VGA_320_W; x++) out[2 + x] = palette_320[src[x]];
    out[VGA_320_W + 2] = 0;
    out[VGA_320_W + 3] = COMPOSABLE_EOL_ALIGN;

    buf->data_used = (VGA_320_W + 4) / 2;
    buf->status    = SCANLINE_OK;
}

/* MODE_160: scan from 160x120 RGB565 framebuffer, pixel-double both
 * axes into the 320x240 composable scanline output.
 *
 * fb160 already holds pixels in PIO-native bit order (see
 * thedemo/rgb565.h) — packer + every effect's blend / palette code
 * uses the same canonical layout, so the scanout can pass values
 * straight through with no per-pixel conversion. */
static void render_scanline_160(struct scanvideo_scanline_buffer *buf)
{
    int y     = scanvideo_scanline_number(buf->scanline_id);
    int y_src = y >> 1;        /* vertical 2x — each src row spans 2 dst rows */
    if (y_src >= VGA_160_H) y_src = VGA_160_H - 1;

    const uint16_t *front = (const uint16_t *)__atomic_load_n(&fb160_front, __ATOMIC_SEQ_CST);
    const uint16_t *src   = front + y_src * VGA_160_W;
    uint16_t *out = (uint16_t *)buf->data;

    out[0] = COMPOSABLE_RAW_RUN;
    out[1] = src[0];                          /* output pixel 0  */
    out[2] = VGA_320_W + 1 - 3;
    out[3] = src[0];                          /* output pixel 1  */
    for (int i = 1; i < VGA_160_W; i++) {
        uint16_t c = src[i];
        out[2 + 2*i    ] = c;                 /* output 2i       */
        out[2 + 2*i + 1] = c;                 /* output 2i + 1   */
    }
    out[VGA_320_W + 2] = 0;
    out[VGA_320_W + 3] = COMPOSABLE_EOL_ALIGN;

    buf->data_used = (VGA_320_W + 4) / 2;
    buf->status    = SCANLINE_OK;
}

/* Display row at which MODE_SPLIT_160_OVER_320 switches from fb160 to
 * fb320 as the source. 0..240 — 0 means all fb320, 240 means all fb160. */
static volatile int g_split_row = VGA_320_H;

void vga_set_split_row(int display_row)
{
    if (display_row < 0)        display_row = 0;
    if (display_row > VGA_320_H) display_row = VGA_320_H;
    __atomic_store_n(&g_split_row, display_row, __ATOMIC_RELAXED);
}

void vga_split_present(void)
{
    /* Flip both fb320 and fb160 back buffers atomically (well, in
     * quick succession — both stores are SEQ_CST so the relative
     * order is preserved). Both buffers are read by the scanline
     * dispatch on the next frame. */
    vga_320_present();
    vga_160_present();
}

static void __attribute__((noreturn)) core1_main(void)
{
    /* All modes share the 320x240 scanvideo timing. Mode switches just
     * change which per-scanline renderer the dispatch picks. */
    scanvideo_setup(&vga_mode_320x240_60);
    scanvideo_timing_enable(true);
    g_scanvideo_up = true;

    while (true) {
        struct scanvideo_scanline_buffer *buf = scanvideo_begin_scanline_generation(true);
        screen_mode_t m = __atomic_load_n(&g_mode, __ATOMIC_RELAXED);
        if (m == MODE_SPLIT_160_OVER_320) {
            int y     = scanvideo_scanline_number(buf->scanline_id);
            int split = __atomic_load_n(&g_split_row, __ATOMIC_RELAXED);
            if (y < split) render_scanline_160(buf);
            else           render_scanline_320(buf);
        } else if (m == MODE_160) {
            render_scanline_160(buf);
        } else {
            render_scanline_320(buf);
        }
        scanvideo_end_scanline_generation(buf);
    }
}

/* --- public API --------------------------------------------------------- */

void vga_init(void)
{
    /* Greyscale ramp as a safe default palette: index N maps to grey N.
     * Effects overwrite what they need. */
    for (int i = 0; i < 256; i++) {
        vga_320_palette_set(i, (uint8_t)i, (uint8_t)i, (uint8_t)i);
    }

    /* Black framebuffers. */
    memset(fb_a, 0, sizeof(fb_a));
    memset(fb_b, 0, sizeof(fb_b));

    multicore_launch_core1(core1_main);

    /* Spin until core 1 reports scanvideo is up so the first frame() call
     * doesn't race the timing setup. */
    while (!g_scanvideo_up) {
        tight_loop_contents();
    }

    g_mode = MODE_320;
    buttons_init();
    printf("vga: scanvideo up @ 320x240 60Hz (buttons armed on VSYNC)\n");
}

void vga_set_mode(screen_mode_t mode)
{
    if (mode == g_mode) return;
    /* All modes share the 320x240 scanvideo timing — switching is just a
     * dispatch toggle in the scanline loop. Use atomic store so core 1
     * picks the new value on its next scanline iteration. */
    __atomic_store_n(&g_mode, mode, __ATOMIC_RELAXED);
    printf("vga: mode change -> %d\n", mode);
}

screen_mode_t vga_current_mode(void) { return g_mode; }

int vga_should_quit(void) { return 0; }  /* Pico backend has no quit signal. */

/* GPIO IRQ fires on the VSYNC-active edge once per frame. Open a tiny
 * sampling window: take each button pad hi-Z (OEOVER_LOW) so the
 * pull-down + the 1 kΩ-to-3V3 of a pressed button decides the level,
 * settle, sample, latch rising edges, then put the pad back to PIO
 * control. The whole ISR is < 10 µs once per frame — well inside the
 * vsync pulse where PIO is emitting the sync waveform, not pixels, so
 * the colour DAC is unaffected. */
static void __not_in_flash_func(buttons_vsync_isr)(uint gpio, uint32_t events)
{
    if (gpio != VSYNC_PIN) return;
    gpio_acknowledge_irq(VSYNC_PIN, events);

    /* Only act on the edge that takes vsync to its active state. The
     * "other" edge fires too because we enabled both, but ignore it. */
    if (gpio_get(VSYNC_PIN) != g_vsync_active_level) return;

    /* Release the pad — internal pull-down + button decide the level. */
    for (int i = 0; i < 3; i++) {
        gpio_set_oeover(btn_pins[i], GPIO_OVERRIDE_LOW);
    }
    busy_wait_us(5);   /* pull-down (~50 kΩ) × pad cap settle. Tiny. */

    uint8_t state = 0;
    for (int i = 0; i < 3; i++) {
        if (gpio_get(btn_pins[i])) state |= (uint8_t)(1u << i);
    }

    /* Hand control back to PIO0 before the sync pulse ends. */
    for (int i = 0; i < 3; i++) {
        gpio_set_oeover(btn_pins[i], GPIO_OVERRIDE_NORMAL);
    }

    uint8_t edges = (uint8_t)(state & ~g_btn_prev);
    g_btn_prev    = state;
    g_btn_state   = state;
    g_btn_latched = (uint8_t)(g_btn_latched | edges);
}

static void buttons_init(void)
{
    /* Pull-down stays on permanently. While PIO is driving the pad it's
     * a tiny pull-down current swamped by the 30 Ω PIO driver; during
     * the brief hi-Z sampling window it's what holds the line LOW when
     * the button is idle. */
    for (int i = 0; i < 3; i++) {
        gpio_pull_down(btn_pins[i]);
    }

    /* VSYNC polarity from the active scanvideo mode. Standard VGA timing
     * is active-low (v_sync_polarity = 1). */
    g_vsync_active_level =
        scanvideo_get_mode().default_timing->v_sync_polarity ? 0 : 1;

    gpio_set_irq_enabled_with_callback(VSYNC_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, buttons_vsync_isr);
}

/* Returns +1 (next) / -1 (prev) / +2 (restart) once per press edge. */
int vga_consume_skip_request(void)
{
    /* Snapshot+clear the latched edges atomically against the ISR. */
    uint32_t save = save_and_disable_interrupts();
    uint8_t  edges = g_btn_latched;
    g_btn_latched  = 0;
    restore_interrupts(save);

    if (edges & 0x1) return  1;
    if (edges & 0x2) return -1;
    if (edges & 0x4) return  2;
    return 0;
}
