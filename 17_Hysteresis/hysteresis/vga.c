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
#include "audio.h"
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

/* --- Shared framebuffer arena -------------------------------------------
 *
 * Every mode renders into the SAME physical bytes — only one mode is live
 * at a time and mode switches land on a black frame, so no buffer needs to
 * survive a switch. The arena is sized for the largest config: the hires
 * double-buffer (2 × 320×240 × 2 B = 307 200 B). The 8bpp (MODE_320) pages
 * and the 160 pages are just smaller views into the same arena.
 *
 * This is what makes a full 320×240 RGB565 truecolor mode fit in SRAM
 * alongside the palette mode without doubling the framebuffer budget. */
#define FB_PAGE_HIRES (VGA_HIRES_W * VGA_HIRES_H * 2)   /* 153 600 */
static uint8_t fb_arena[2 * FB_PAGE_HIRES] __attribute__((aligned(4)));

/* MODE_320 8bpp views: two 76 800-byte pages in the first half. */
static uint8_t *fb_a = fb_arena;
static uint8_t *fb_b = fb_arena + VGA_320_W * VGA_320_H;
static uint8_t *fb_back  = fb_arena;
static uint8_t * volatile fb_front = fb_arena + VGA_320_W * VGA_320_H;

/* 256-entry palette, RGB555 already encoded for fast scanline lookup. */
static uint16_t palette_320[256];

uint8_t *vga_320_back_buffer(void) { return fb_back; }
const uint8_t *vga_320_front_buffer(void) { return (const uint8_t *)fb_front; }

void vga_320_palette_set(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    if ((unsigned)idx >= 256) return;
    palette_320[idx] = (uint16_t)PICO_SCANVIDEO_PIXEL_FROM_RGB8(r, g, b);
}

void vga_320_present(void)
{
    uint8_t *old_back = fb_back;
    fb_back  = (fb_back == fb_a) ? fb_b : fb_a;
    __atomic_store_n(&fb_front, old_back, __ATOMIC_SEQ_CST);
}

/* MODE_HIRES views: two 153 600-byte RGB565 pages spanning the arena. */
static uint16_t *fbh_a = (uint16_t *)fb_arena;
static uint16_t *fbh_b = (uint16_t *)(fb_arena + FB_PAGE_HIRES);
static uint16_t *fbh_back  = (uint16_t *)fb_arena;
static uint16_t * volatile fbh_front = (uint16_t *)(fb_arena + FB_PAGE_HIRES);

uint16_t *vga_hires_back_buffer(void) { return fbh_back; }

void vga_hires_present(void)
{
    uint16_t *old_back = fbh_back;
    fbh_back = (fbh_back == fbh_a) ? fbh_b : fbh_a;
    __atomic_store_n(&fbh_front, old_back, __ATOMIC_SEQ_CST);
}

/* MODE_160 views (160x120 RGB565, pixel/line-doubled at scanout) — also
 * alias the arena. Kept for completeness / split mode. */
static uint16_t *fb160_a = (uint16_t *)fb_arena;
static uint16_t *fb160_b = (uint16_t *)(fb_arena + VGA_160_W * VGA_160_H * 2);
static uint16_t *fb160_back  = (uint16_t *)fb_arena;
static uint16_t * volatile fb160_front = (uint16_t *)(fb_arena + VGA_160_W * VGA_160_H * 2);

uint16_t *vga_160_back_buffer(void) { return fb160_back; }

void vga_160_present(void)
{
    uint16_t *old_back = fb160_back;
    fb160_back = (fb160_back == fb160_a) ? fb160_b : fb160_a;
    __atomic_store_n(&fb160_front, old_back, __ATOMIC_SEQ_CST);
}

/* --- scanline rendering (core 1) ---------------------------------------- */

/* MODE_320 at full VGA: 320x240 8bpp, palette-looked-up and 2x doubled into
 * the 640-wide line.
 *
 * This REPLACES a 320-wide render_scanline_320 that SUSTAIN inherited and
 * never called. Two things were wrong with the old one and both mattered:
 *
 *   - core1_main dispatched every non-MODE_RACE mode to
 *     render_scanline_hires640, so the 8bpp path was dead code. The beam was
 *     scanning fbh_front, which points into the second half of the shared
 *     arena — a region the 8bpp pages never touch and vga_init zeroes. Hence
 *     a permanently black screen while the simulation ran perfectly.
 *   - it emitted 320 pixels into a 640-pixel mode and indexed rows with the
 *     raw scanline number, so even once dispatched it would have produced a
 *     half-width image reading 240 rows off the end of the buffer.
 *
 * SUSTAIN never noticed because it lived in MODE_HIRES from the first frame. */
static void __not_in_flash_func(render_scanline_320x640)(
        struct scanvideo_scanline_buffer *buf, int y)
{
    int ys = y >> 1; if (ys >= VGA_320_H) ys = VGA_320_H - 1;
    const uint8_t *front = (const uint8_t *)__atomic_load_n(&fb_front, __ATOMIC_SEQ_CST);
    const uint8_t *src   = front + ys * VGA_320_W;
    uint16_t *out = (uint16_t *)buf->data;

    /* VGA_OUT_W is defined further down with the MODE_HIRES code; spell the
     * doubling out instead of moving the define, since W2 is what this
     * function is actually about. */
    enum { W2 = VGA_320_W * 2 };
    out[0] = COMPOSABLE_RAW_RUN;
    out[1] = palette_320[src[0]];
    out[2] = W2 + 1 - 3;
    for (int k = 1; k < W2; k++) out[2 + k] = palette_320[src[k >> 1]];
    out[W2 + 2] = 0;
    out[W2 + 3] = COMPOSABLE_EOL_ALIGN;

    buf->data_used = (W2 + 4) / 2;
    buf->status    = SCANLINE_OK;
}

/* MODE_HIRES: 320x240 RGB565, one source pixel per output pixel, passed
 * straight through (PIO-native bit order, see rgb565.h) — no palette
 * lookup, no doubling. Same scanline length as MODE_320. */
static void render_scanline_hires(struct scanvideo_scanline_buffer *buf)
{
    int y = scanvideo_scanline_number(buf->scanline_id);
    const uint16_t *front = (const uint16_t *)__atomic_load_n(&fbh_front, __ATOMIC_SEQ_CST);
    const uint16_t *src   = front + y * VGA_HIRES_W;
    uint16_t *out = (uint16_t *)buf->data;

    out[0] = COMPOSABLE_RAW_RUN;
    out[1] = src[0];
    out[2] = VGA_HIRES_W + 1 - 3;
    for (int x = 1; x < VGA_HIRES_W; x++) out[2 + x] = src[x];
    out[VGA_HIRES_W + 2] = 0;
    out[VGA_HIRES_W + 3] = COMPOSABLE_EOL_ALIGN;

    buf->data_used = (VGA_HIRES_W + 4) / 2;
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

/* --- FULL VGA (640x480) -------------------------------------------------
 *
 * The whole demo runs on 640x480@60 scanvideo timing. Framebuffer scenes still
 * render into the 320x240 RGB565 arena and are 2x pixel/line-doubled at scanout
 * (render_scanline_hires640). Beam-raced scenes (MODE_RACE: rotozoom, Mode-7)
 * register a per-scanline generator that core 1 runs straight into the 640-wide
 * line — true full-VGA detail with no framebuffer. Both the generator and this
 * core-1 loop are pinned in SRAM to hit the per-line deadline. */
#define VGA_OUT_W 640
#define VGA_OUT_H 480

static void (*volatile g_race_fn)(uint16_t *dst, int y) = NULL;   /* per scanline */
static void (*volatile g_race_setup_fn)(void)           = NULL;   /* core-1 once  */
static volatile int     g_race_dirty                    = 0;      /* run setup()  */

void vga_set_race_fn(void (*scan)(uint16_t *, int), void (*setup)(void))
{
    __atomic_store_n(&g_race_setup_fn, setup, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_race_fn, scan, __ATOMIC_SEQ_CST);
    __atomic_store_n(&g_race_dirty, 1, __ATOMIC_SEQ_CST);   /* core 1 reconfigures */
}

/* Beam-raced scenes copy their textures here (aliases the framebuffer arena —
 * a race scene and a framebuffer scene are never live at the same time). */
uint8_t *vga_race_sram(void)      { return fb_arena; }
unsigned vga_race_sram_size(void) { return (unsigned)sizeof(fb_arena); }

void vga_race_present(void) { /* device: core 1 beam-races continuously */ }

/* 320x240 framebuffer -> 640x480 by 2x pixel + line doubling at scanout. */
static void __not_in_flash_func(render_scanline_hires640)(struct scanvideo_scanline_buffer *buf, int y)
{
    int ys = y >> 1; if (ys >= VGA_HIRES_H) ys = VGA_HIRES_H - 1;
    const uint16_t *front = (const uint16_t *)__atomic_load_n(&fbh_front, __ATOMIC_SEQ_CST);
    const uint16_t *src   = front + ys * VGA_HIRES_W;
    uint16_t *out = (uint16_t *)buf->data;
    out[0] = COMPOSABLE_RAW_RUN;
    out[1] = src[0];
    out[2] = VGA_OUT_W + 1 - 3;
    for (int k = 1; k < VGA_OUT_W; k++) out[2 + k] = src[k >> 1];
    out[VGA_OUT_W + 2] = 0;
    out[VGA_OUT_W + 3] = COMPOSABLE_EOL_ALIGN;
    buf->data_used = (VGA_OUT_W + 4) / 2;
    buf->status    = SCANLINE_OK;
}

static void __not_in_flash_func(core1_main)(void)
{
    scanvideo_setup(&vga_mode_640x480_60);
    scanvideo_timing_enable(true);
    g_scanvideo_up = true;

    static uint16_t race_line[VGA_OUT_W];
    while (true) {
        struct scanvideo_scanline_buffer *buf = scanvideo_begin_scanline_generation(true);
        int y = scanvideo_scanline_number(buf->scanline_id);
        screen_mode_t m = __atomic_load_n(&g_mode, __ATOMIC_RELAXED);

        if (m == MODE_RACE) {
            void (*setup)(void) = __atomic_load_n(&g_race_setup_fn, __ATOMIC_RELAXED);
            void (*scan)(uint16_t *, int) = __atomic_load_n(&g_race_fn, __ATOMIC_RELAXED);
            /* run the scene's one-time interp config on THIS core after it (re)
             * registers — covers the set-mode-before-init ordering. */
            if (__atomic_load_n(&g_race_dirty, __ATOMIC_RELAXED)) {
                if (setup) setup();
                __atomic_store_n(&g_race_dirty, 0, __ATOMIC_RELAXED);
            }
            if (scan) scan(race_line, y);
            else      for (int i = 0; i < VGA_OUT_W; i++) race_line[i] = 0;
            uint16_t *out = (uint16_t *)buf->data;
            out[0] = COMPOSABLE_RAW_RUN;
            out[1] = race_line[0];
            out[2] = VGA_OUT_W + 1 - 3;
            for (int x = 1; x < VGA_OUT_W; x++) out[2 + x] = race_line[x];
            out[VGA_OUT_W + 2] = 0;
            out[VGA_OUT_W + 3] = COMPOSABLE_EOL_ALIGN;
            buf->data_used = (VGA_OUT_W + 4) / 2;
            buf->status    = SCANLINE_OK;
        } else if (m == MODE_320) {
            render_scanline_320x640(buf, y);        /* HYSTERESIS lives here */
        } else {
            render_scanline_hires640(buf, y);       /* RGB565 framebuffer scenes */
        }
        scanvideo_end_scanline_generation(buf);

        /* Audio lives here, after the buffer is handed back and before blocking
         * on the next one. A few samples per line, on the core that has the
         * spare cycles -- see audio_synth.c for why not core 0 and why not a
         * half-buffer refill. */
        audio_pump();
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

    /* Black framebuffers (whole shared arena). */
    memset(fb_arena, 0, sizeof(fb_arena));

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
