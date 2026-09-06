/* Core 1: the beam.
 *
 * There is no framebuffer. This loop asks scanvideo for the next line buffer,
 * calls the runner to generate 640 pixels straight into it, hands it back,
 * and does that 31,500 times a second. Nothing else runs on this core.
 *
 * ---------------------------------------------------------------- layout --
 *
 * A composable RAW_RUN puts pixel 0 in the same word as its token, so a run
 * starting at word 0 leaves the pixel array 16-bit aligned and every word
 * store in a kernel unaligned. Two RAW_1P tokens in front fix the parity:
 *
 *   hw[0]=RAW_1P  hw[1]=p0   hw[2]=RAW_1P  hw[3]=p1
 *   hw[4]=RAW_RUN hw[5]=p2   hw[6]=n-3     hw[7]=p3   hw[8]=p4 ... hw[643]=p639
 *   hw[644]=black hw[645]=EOL_ALIGN
 *
 * The kernel renders all 640 pixels into hw[4..643] (word-aligned), then the
 * first three are moved into their token slots and the tokens written over
 * them. Ten cycles a line, and every kernel gets an aligned buffer.
 *
 * ---------------------------------------------------------------- timing --
 *
 * Every line is timed with SysTick (sys_clk, 24-bit, per core). The worst
 * line of the current scene is published for core 0 to print, and if a line
 * exceeds LINE_BUDGET the governor raises g_lod so the scene finishes at half
 * horizontal resolution instead of going black. The count of such events is
 * telemetry: a build that ships with it non-zero is a build that failed.
 */

#include "vga.h"
#include "beam.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "hardware/structs/systick.h"

/* 300 MHz / 31,469 lines per second = 9,533 cycles per line, of which
 * scanvideo's own begin/end bookkeeping takes a few hundred. */
#define LINE_BUDGET  8400u

static volatile int32_t  s_pending = -1;
static volatile uint32_t s_latched;
static volatile uint32_t s_frames_done;
static volatile uint32_t s_worst, s_mean, s_late, s_lod_events, s_slips;
static volatile int      s_ready;

static void __not_in_flash_func(core1_main)(void)
{
    scanvideo_setup(&vga_mode_640x480_60);
    scanvideo_timing_enable(true);

    systick_hw->rvr = 0x00FFFFFF;     /* reload first: enabling with rvr=0 parks it at 0 */
    systick_hw->cvr = 0;
    systick_hw->csr = 0x5;            /* enable, clksource = processor */

    /* Configure frame 0's kernels BEFORE the first line is generated. The
     * scanout core owns the SIO interpolator, so a kernel that uses it is
     * configured here and nowhere else -- and if that is left until the first
     * scanline 0, the lines drawn before it read the interpolator in its reset
     * state and index the texture with an unbounded offset. In the shipping
     * arc the opening scene happens not to use the interpolator, so this only
     * showed up when a single cue was run on its own for profiling: a hang,
     * fifteen scenes away from the code that caused it. */
    beam_line_setup(0);

    __atomic_store_n(&s_ready, 1, __ATOMIC_RELEASE);

    uint32_t f = 0;
    uint32_t worst = 0, acc = 0, nline = 0;
    uint32_t late_this_frame = 0;
    uint32_t slips_seen = 0;
    uint32_t expect_id = 0;
    int have_expect = 0;
    for (;;) {
        struct scanvideo_scanline_buffer *buf = scanvideo_begin_scanline_generation(true);
        const int y = (int)scanvideo_scanline_number(buf->scanline_id);

        /* THE deadline check, and the only honest one.
         *
         * scanvideo hands out scanline ids in order and skips ahead when the
         * beam has already passed the line we were about to generate. So if
         * the id we are given is not the immediate successor of the last one,
         * a line went out unwritten and the viewer saw it. A cycle counter
         * cannot tell us this: core 1 also takes the scanvideo DMA interrupts,
         * so a timer around the kernel measures the kernel PLUS whatever
         * interrupts landed inside it -- which is why removing all of the
         * kernel's work changed the "worst line" figure by six cycles. */
        if (have_expect && buf->scanline_id != expect_id) s_slips++;
        expect_id = buf->scanline_id + 1;      /* ids are (frame << 16) | line */
        if (y == PV_H - 1) { expect_id = 0; have_expect = 0; } else have_expect = 1;

        if (y == 0) {
            int32_t p = __atomic_load_n(&s_pending, __ATOMIC_ACQUIRE);
            if (p >= 0) {
                f = (uint32_t)p;
                __atomic_store_n(&s_pending, -1, __ATOMIC_RELEASE);
                __atomic_store_n(&s_latched, f, __ATOMIC_RELEASE);
            }
            __atomic_store_n(&s_worst, worst, __ATOMIC_RELAXED);
            __atomic_store_n(&s_mean, nline ? acc / nline : 0, __ATOMIC_RELAXED);
            worst = 0; acc = 0; nline = 0;
            /* The governor engages on a kernel that is SYSTEMATICALLY late,
             * not on one slow line. A single outlier is a cache miss or an
             * interrupt; degrading a whole scene's resolution for one is worse
             * than the glitch it is avoiding. Sixteen late lines in one frame
             * is a kernel that does not fit. */
#if PV_PROF == 0
            /* Engage on lines the beam actually missed, not on the timer.
             * The timer counts interrupts it cannot separate from the kernel,
             * and scanvideo's queue absorbs a line that runs long as
             * comfortably as it absorbs one that does not -- so a kernel can
             * sit above LINE_BUDGET all day and never cost the viewer a
             * pixel. A slipped scanline is the thing that is actually wrong. */
            if (s_slips > slips_seen + 2 && !g_lod) { g_lod = 1; s_lod_events++; }
            slips_seen = s_slips;
#endif
            late_this_frame = 0;
            beam_line_setup(f);
            __atomic_store_n(&s_frames_done, s_frames_done + 1, __ATOMIC_RELAXED);
        }

        uint16_t *hw = (uint16_t *)buf->data;
        const uint32_t t0 = systick_hw->cvr;

        beam_line(f, hw + 4, y);

        const uint16_t p0 = hw[4], p1 = hw[5], p2 = hw[6];
        hw[0] = COMPOSABLE_RAW_1P;  hw[1] = p0;
        hw[2] = COMPOSABLE_RAW_1P;  hw[3] = p1;
        hw[4] = COMPOSABLE_RAW_RUN; hw[5] = p2;
        hw[6] = 639 - 3;                         /* p2 .. p639 + one black = 639 */
        hw[644] = 0;
        hw[645] = COMPOSABLE_EOL_ALIGN;
        buf->data_used = 323;
        buf->status = SCANLINE_OK;

        const uint32_t cyc = (t0 - systick_hw->cvr) & 0x00FFFFFFu;
        if (cyc > worst) worst = cyc;
        acc += cyc; nline++;
        if (cyc > LINE_BUDGET) { s_late++; late_this_frame++; }

        scanvideo_end_scanline_generation(buf);
    }
}

void vga_init(void)
{
    multicore_launch_core1(core1_main);
    while (!__atomic_load_n(&s_ready, __ATOMIC_ACQUIRE)) tight_loop_contents();
}

void vga_publish(uint32_t f)
{
    __atomic_store_n(&s_pending, (int32_t)f, __ATOMIC_RELEASE);
}

int vga_wait_latched(uint32_t f, uint32_t timeout_us)
{
    const uint32_t t0 = time_us_32();
    while (__atomic_load_n(&s_latched, __ATOMIC_ACQUIRE) != f) {
        if (time_us_32() - t0 > timeout_us) return 0;
        tight_loop_contents();
    }
    return 1;
}

uint32_t vga_latched(void)     { return __atomic_load_n(&s_latched, __ATOMIC_ACQUIRE); }
uint32_t vga_worst_cycles(void){ return __atomic_load_n(&s_worst, __ATOMIC_RELAXED); }
uint32_t vga_mean_cycles(void) { return __atomic_load_n(&s_mean, __ATOMIC_RELAXED); }
uint32_t vga_slips(void)       { return s_slips; }
uint32_t vga_late_lines(void)  { return s_late; }
uint32_t vga_lod_events(void)  { return s_lod_events; }
uint32_t vga_frames_done(void) { return s_frames_done; }
