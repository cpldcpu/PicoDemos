/* Scanout on core 1: two 320x240 pages, doubled to 640x480.
 *
 * Two things here are not VESPER's, and both were worth measuring:
 *
 * 1. The mode is vga_mode_320x240_60, whose yscale is 2, not
 *    vga_mode_640x480_60 with the row index shifted right. scanvideo repeats
 *    a generated scanline buffer for the second physical line by
 *    re-triggering the same DMA (scanvideo.c, y_repeat_target), so core 1
 *    generates 240 buffers a frame instead of 480 for exactly the same
 *    picture. xscale is informational in the DPI backend -- the buffer still
 *    has to carry 640 pixel clocks -- so the horizontal doubling is still
 *    ours. Build with -DCV_SCANOUT_640 to get VESPER's arrangement back; the
 *    reply reports both measured.
 *
 * 2. The doubling writes 32-bit words. Display pixel x takes src[x>>1], and
 *    the composable RAW_RUN header puts display pixel 0 in the token's
 *    argument, so out[4] onward is 4-byte aligned and every source pixel
 *    from 1 to 319 is one `v | v<<16` store. 319 stores, not 639.
 *
 * The page handshake is VESPER's and is the part that must not be
 * simplified: a vblank wait is not enough, because scanvideo queues
 * scanlines ahead of the beam. Core 0 may not reuse a page until core 1 has
 * latched its replacement at a scanline zero.
 */

#include "platform.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "hardware/structs/systick.h"

#if CV_W != 320 || CV_H != 240
#error "the doubling here is written for 320x240"
#endif

static uint16_t g_pages[2][CV_W * CV_H] __attribute__((aligned(4)));

static int              g_back = 1;
static volatile int     g_pending = -1;
static volatile int     g_displayed = 0;
static volatile int     g_ready = 0;

/* core 1 writes these, core 0 only reads them */
static volatile uint32_t g_vsyncs, g_lines, g_copy_cycles, g_copy_worst;
static volatile uint32_t g_pumps, g_pump_cycles, g_pump_worst;
static volatile uint32_t g_latch_vsync;

/* core 0 only */
static uint32_t g_prev_latch, g_last_hold;

#ifdef CV_SCANOUT_640
#  define SCANOUT_MODE  vga_mode_640x480_60
#  define SCANOUT_LINES 480
#  define SRC_ROW(y)    ((y) >> 1)
#else
#  define SCANOUT_MODE  vga_mode_320x240_60
#  define SCANOUT_LINES 240
#  define SRC_ROW(y)    (y)
#endif

#define SYSTICK_MASK 0x00FFFFFFu

static inline uint32_t cyc(void) { return systick_hw->cvr; }
static inline uint32_t cyc_since(uint32_t t) { return (t - systick_hw->cvr) & SYSTICK_MASK; }

static void CV_HOT(scanout)(void)
{
    /* Core 1's own SysTick, free-running off the processor clock. 24 bits at
     * 300 MHz wraps every 56 ms, which is longer than anything measured here
     * and shorter than nothing. */
    systick_hw->rvr = SYSTICK_MASK;
    systick_hw->cvr = 0;
    systick_hw->csr = 0x5u;                    /* ENABLE | CLKSOURCE=core     */

    scanvideo_setup(&SCANOUT_MODE);
    scanvideo_timing_enable(true);
    __atomic_store_n(&g_ready, 1, __ATOMIC_RELEASE);

    int front = 0;
    uint32_t lines = 0, copy_sum = 0, copy_worst = 0;
    uint32_t pumps = 0, pump_sum = 0, pump_worst = 0;

    for (;;) {
        struct scanvideo_scanline_buffer *b = scanvideo_begin_scanline_generation(true);
        const unsigned y = scanvideo_scanline_number(b->scanline_id);

        if (y == 0) {
            const uint32_t v = g_vsyncs + 1u;
            g_vsyncs = v;
            const int p = __atomic_load_n(&g_pending, __ATOMIC_ACQUIRE);
            if (p >= 0) {
                front = p;
                g_latch_vsync = v;
                __atomic_store_n(&g_displayed, front, __ATOMIC_RELEASE);
                __atomic_store_n(&g_pending, -1, __ATOMIC_RELEASE);
            }
        }

        const uint16_t *src = g_pages[front] + SRC_ROW(y) * CV_W;
        uint16_t *out = (uint16_t *)b->data;

        /* COMPOSABLE_RAW_RUN is `| jmp raw_run | colour | n | n+2 colours |`,
         * so the run emits n+3 pixels in total, and COMPOSABLE_EOL_ALIGN has
         * to land on an ODD halfword index -- the `||` in scanvideo.pio's
         * comment is a word boundary, and the token is what aligns to it.
         * 640 pixels would put the token on an even index. So the line is 641
         * pixels long, with a black one past the last visible column, which
         * is spent in the 16-pixel front porch. VESPER's arrangement, and the
         * n=638 here is the whole reason it is: n=637 would give exactly 640
         * pixels and hand the state machine out[642] as a jump target. */
        const uint32_t t0 = cyc();
        out[0] = COMPOSABLE_RAW_RUN;
        out[1] = src[0];                       /* display pixel 0             */
        out[2] = 638;                          /* n: 641 pixels in the run    */
        out[3] = src[0];                       /* display pixel 1             */
        uint32_t *o = (uint32_t *)(out + 4);   /* display pixels 2..639       */
        for (int x = 1; x < CV_W; x++) {
            const uint32_t v = src[x];
            o[x - 1] = v | (v << 16);
        }
        out[642] = 0;                          /* the 641st, in the porch     */
        out[643] = COMPOSABLE_EOL_ALIGN;
        const uint32_t dc = cyc_since(t0);

        b->data_used = 322;
        b->status = SCANLINE_OK;
        scanvideo_end_scanline_generation(b);

        lines++;
        copy_sum += dc;
        if (dc > copy_worst) copy_worst = dc;

        const uint32_t t1 = cyc();
        audio_pump();
        const uint32_t dp = cyc_since(t1);
        pumps++;
        pump_sum += dp;
        if (dp > pump_worst) pump_worst = dp;

        /* Publish once a frame rather than once a line; these are statistics,
         * not a protocol, and the stores are not free. */
        if (y == 0) {
            g_lines = lines;   g_copy_cycles = copy_sum; g_copy_worst = copy_worst;
            g_pumps = pumps;   g_pump_cycles = pump_sum; g_pump_worst = pump_worst;
        }
    }
}

void video_init(void)
{
    multicore_launch_core1(scanout);
    while (!__atomic_load_n(&g_ready, __ATOMIC_ACQUIRE)) tight_loop_contents();
}

uint16_t *video_back(void) { return g_pages[g_back]; }

void video_present(void)
{
    __atomic_store_n(&g_pending, g_back, __ATOMIC_RELEASE);
    while (__atomic_load_n(&g_pending, __ATOMIC_ACQUIRE) >= 0) tight_loop_contents();

    const uint32_t latch = g_latch_vsync;
    g_last_hold = g_prev_latch ? latch - g_prev_latch : 1u;
    g_prev_latch = latch;

    g_back = 1 - __atomic_load_n(&g_displayed, __ATOMIC_ACQUIRE);
}

uint32_t video_last_hold(void) { return g_last_hold; }
uint32_t video_scanout_lines_per_frame(void) { return SCANOUT_LINES; }

void video_prof(video_prof_t *out)
{
    out->vsyncs      = g_vsyncs;
    out->lines       = g_lines;
    out->copy_cycles = g_copy_cycles;
    out->copy_worst  = g_copy_worst;
    out->pumps       = g_pumps;
    out->pump_cycles = g_pump_cycles;
    out->pump_worst  = g_pump_worst;
}
