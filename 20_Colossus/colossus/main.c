/* COLOSSUS -- RP2350 entry point. Core 0 draws; core 1 shows and plays.
 *
 *   core 0                                 core 1
 *   ------                                 ------
 *   sample = audio_position()              scanline 0: latch the pending page
 *   demo_render(back, sample)              240 rows, each doubled to 640 px
 *   video_present()  ---- publish ---->    audio_pump() after each row
 *                    <--- latched -----
 *
 * Which sample gets drawn is read off the DMA's counter, never counted, so a
 * slow frame skips a moment instead of falling permanently behind the music.
 * demo_render() is pure in `sample` (demo.h), which is what makes that legal.
 *
 * Telemetry once a second and once a phrase over USB CDC. The numbers are
 * referee 3 (min/avg/max render, worst displayed-frame gap, missed
 * deadlines, underruns) and referee 2 (the synth hash latch, which
 * tools/serial_read.py diffs against the host WAV over the whole 5:07).
 *
 * PICO_STACK_SIZE / PICO_CORE1_STACK_SIZE live in SCRATCH_Y / SCRATCH_X and
 * do not come out of the heap; the heap is __end__ .. __StackLimit and is
 * what pico_scanvideo mallocs its scanline buffers from at video_init(). The
 * linker cannot see that allocation, which is how PERSISTENCE linked cleanly
 * and panicked at boot. So main.c measures the heap before and after, prints
 * both, and tools/ledger_check.py fails the build if the first is below the
 * floor measured with -DCV_BALLAST.
 */

#include "platform.h"
#include "demo.h"
#include "song.h"
#include "synth.h"

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/gpio.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define LED_PIN 25

/* Ballast for measuring the boot floor: link N bytes of .bss that nothing
 * uses, bisect N until the firmware stops booting, and the last heap_free it
 * printed is the floor. See the reply. */
#ifndef CV_BALLAST
#define CV_BALLAST 0
#endif
#if CV_BALLAST > 0
static volatile uint8_t g_ballast[CV_BALLAST];
#endif

extern char __StackLimit, __end__;
extern void *_sbrk(int incr);

uint32_t heap_region(void) { return (uint32_t)(&__StackLimit - &__end__); }

/* ------------------------------------------------------------------ panic -- */

/* A panic you can still talk to. PICO_PANIC_FUNCTION points the SDK's panic()
 * here (CMakeLists.txt), and because this never returns, the `bkpt #0` that
 * would otherwise follow is never executed.
 *
 * That difference is the whole reason this function exists. The stock panic
 * ends in a breakpoint, and a breakpoint with no debugger attached escalates
 * to a HardFault whose handler spins at priority -1 with every other
 * interrupt masked. USB dies with it: the board is enumerated, unreadable,
 * and cannot even be rebooted into BOOTSEL by picotool. It needs a human to
 * unplug it -- which is expensive when the panic you are hunting is a memory
 * panic and the experiment is "keep adding ballast until it stops booting".
 *
 * Spinning in sleep_ms() instead leaves interrupts on, so pico_stdio_usb
 * keeps servicing CDC, the message above is readable, and
 * `picotool reboot -f -u` still works. The LED blinks fast, which is the
 * signal when nobody is listening on the wire.
 *
 * One caveat inherited from the SDK: its panic() is `naked` and does
 * `push {lr}` before branching here, so any vararg that had to travel on the
 * stack -- the fifth argument onwards -- is read from four bytes off. Every
 * panic in the SDK and in this tree fits in r1-r3, so nothing here is
 * affected, but a very wordy panic format would print rubbish rather than
 * crash.
 */
void __attribute__((noreturn)) cv_panic(const char *fmt, ...)
{
    puts("\n*** PANIC ***");
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        puts("");
    }
    printf("PANIC heap_region=%lu heap_free=%lu ballast=%lu\n",
           (unsigned long)heap_region(), (unsigned long)heap_free(),
           (unsigned long)CV_BALLAST);
    for (;;) {
        gpio_put(LED_PIN, 1); sleep_ms(60);
        gpio_put(LED_PIN, 0); sleep_ms(60);
    }
}

/* How much heap is left, measured without allocating anything.
 *
 * The obvious way to measure this is to malloc bigger and bigger blocks until
 * one fails. Do not: PICO_MALLOC_PANIC defaults to 1, so the SDK treats a
 * failed allocation as fatal, and the probe panics the firmware it was
 * written to protect -- with "Out of memory", the exact message PERSISTENCE's
 * postmortem is about, produced deliberately by the instrument. Asking sbrk
 * where the break is costs nothing and cannot fail. It measures the
 * unallocated tail, which is the number that matters, because that is where
 * pico_scanvideo's scanline buffers come from at video_init(). */
uint32_t heap_free(void)
{
    char *brk = (char *)_sbrk(0);
    if (brk == (char *)-1) return 0;
    return (uint32_t)(&__StackLimit - brk);
}

/* ---------------------------------------------------------------- window -- */

typedef struct {
    uint32_t frames;
    uint32_t render_min, render_max;
    uint64_t render_sum;
    uint32_t gap_max;
    uint32_t miss, late;
    uint32_t hold_max;
    video_prof_t prof0;
    uint64_t t0;
} window_t;

static void window_open(window_t *w, uint64_t now)
{
    w->frames = 0; w->render_min = 0xFFFFFFFFu; w->render_max = 0; w->render_sum = 0;
    w->gap_max = 0; w->miss = 0; w->late = 0; w->hold_max = 0;
    video_prof(&w->prof0);
    w->t0 = now;
}

static void window_report(const char *tag, window_t *w, uint64_t now, uint32_t sample)
{
    video_prof_t p; video_prof(&p);
    const uint64_t us = (now - w->t0) ? (now - w->t0) : 1;
    const uint32_t dlines = p.lines - w->prof0.lines;
    const uint32_t dcopy  = p.copy_cycles - w->prof0.copy_cycles;
    const uint32_t dpumps = p.pumps - w->prof0.pumps;
    const uint32_t dpump  = p.pump_cycles - w->prof0.pump_cycles;
    const uint32_t n      = w->frames ? w->frames : 1;
    const uint32_t rmin   = w->frames ? w->render_min : 0;
    const uint32_t ravg   = (uint32_t)(w->render_sum / n);

    const uint32_t bar = cv_bar_of(sample);
    demo_stats_t d; demo_stats(&d);
    uint32_t hpos = 0, hval = 0;
    const int have_hash = synth_hash_latch(&hpos, &hval);

    printf("%s t=%lu.%lu bar=%lu ph=%lu %s | render ms %lu.%02lu/%lu.%02lu/%lu.%02lu | fps %lu.%lu"
           " | hold %lu miss %lu late %lu | gap ms %lu.%02lu | under %lu fill %u"
           " | copy %lu cy/line %lu.%02lu Mcy/s | synth %lu cy/pump %lu.%02lu Mcy/s %lu cy/sample"
           " | tri %lu px %lu part %lu ch %u",
        tag,
        (unsigned long)(sample / CV_RATE), (unsigned long)((sample % CV_RATE) * 10 / CV_RATE),
        (unsigned long)bar, (unsigned long)(bar / 8 + 1), song_section_name(song_section(bar)),
        (unsigned long)(rmin / 1000), (unsigned long)(rmin % 1000 / 10),
        (unsigned long)(ravg / 1000), (unsigned long)(ravg % 1000 / 10),
        (unsigned long)(w->render_max / 1000), (unsigned long)(w->render_max % 1000 / 10),
        (unsigned long)((uint64_t)w->frames * 1000000u / us),
        (unsigned long)((uint64_t)w->frames * 10000000u / us % 10),
        (unsigned long)w->hold_max, (unsigned long)w->miss, (unsigned long)w->late,
        (unsigned long)(w->gap_max / 1000), (unsigned long)(w->gap_max % 1000 / 10),
        (unsigned long)audio_underruns(), audio_min_fill(),
        (unsigned long)(dlines ? dcopy / dlines : 0),
        (unsigned long)((uint64_t)dcopy / us), (unsigned long)((uint64_t)dcopy * 100 / us % 100),
        (unsigned long)(dpumps ? dpump / dpumps : 0),
        (unsigned long)((uint64_t)dpump / us), (unsigned long)((uint64_t)dpump * 100 / us % 100),
        (unsigned long)((uint64_t)dpump * 1000000u / us / CV_RATE),
        (unsigned long)d.triangles, (unsigned long)d.fill, (unsigned long)d.particles, d.chapter);
    if (have_hash) printf(" | AHASH s=%lu %08lx", (unsigned long)hpos, (unsigned long)hval);
    printf("\n");
}

/* ------------------------------------------------------------------ main -- */

int main(void)
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(300000, true);

    stdio_init_all();

    /* Wait, briefly and with a bound, for somebody to open the USB CDC.
     *
     * The SDK drops stdout when nothing is listening, and a panic message is
     * stdout. The first boot of this firmware panicked before the port had
     * been opened, so what reached the outside world was a board that had
     * stopped answering USB and could not even be rebooted into BOOTSEL --
     * the diagnosis was in a printf nobody could have received. Five seconds
     * covers re-enumeration after `picotool load -x` plus the open; after
     * that the demo runs whether or not anyone is watching.
     *
     * The LED is solid through init and starts counting bars once the loop
     * is running, so a solid LED is "stuck before the first frame".
     */
    gpio_put(LED_PIN, 1);
    for (int i = 0; i < 500 && !stdio_usb_connected(); i++) sleep_ms(10);
    sleep_ms(100);

    printf("\n=== COLOSSUS / LATENT / 2026 === sys_clk=%lu\n", (unsigned long)clock_get_hz(clk_sys));
    printf("BOOT heap_region=%lu heap_free=%lu ballast=%lu total_samples=%lu bars=%d\n",
           (unsigned long)heap_region(), (unsigned long)heap_free(),
           (unsigned long)CV_BALLAST, (unsigned long)CV_TOTAL_SAMPLES, CV_BARS);
#if CV_BALLAST > 0
    g_ballast[0] = 1; g_ballast[CV_BALLAST - 1] = 1;   /* keep it linked in */
#endif

    synth_init();
    demo_init();
    audio_init();

    printf("BOOT heap_free_after_init=%lu\n", (unsigned long)heap_free());
    video_init();
    printf("BOOT heap_free_after_video=%lu scanout_lines=%lu\n",
           (unsigned long)heap_free(), (unsigned long)video_scanout_lines_per_frame());

    /* Frame 0 exists before the DMA starts, so sample 0 is on screen when
     * sample 0 leaves the DAC. */
    demo_render(video_back(), 0);
    video_present();
    gpio_put(LED_PIN, 0);
    audio_start();

    window_t sec, phr;
    uint64_t now = time_us_64();
    window_open(&sec, now);
    window_open(&phr, now);

    uint32_t last_phrase = 0;
    uint64_t last_present = now;
    uint32_t run_render_max = 0, run_gap_max = 0, run_miss = 0, run_late = 0, run_frames = 0;

    for (;;) {
        const uint32_t sample = audio_position();
        if (sample >= CV_TOTAL_SAMPLES) break;

        const uint64_t t0 = time_us_64();
        demo_render(video_back(), sample);
        const uint32_t render = (uint32_t)(time_us_64() - t0);

        video_present();
        now = time_us_64();

        const uint32_t gap = (uint32_t)(now - last_present);
        last_present = now;
        const uint32_t hold = video_last_hold();

        run_frames++;
        if (render > run_render_max) run_render_max = render;
        if (gap > run_gap_max) run_gap_max = gap;
        if (hold > 2) run_miss++;
        if (hold > 1) run_late++;

        window_t *ws[2] = { &sec, &phr };
        for (int i = 0; i < 2; i++) {
            window_t *w = ws[i];
            w->frames++;
            w->render_sum += render;
            if (render < w->render_min) w->render_min = render;
            if (render > w->render_max) w->render_max = render;
            if (gap > w->gap_max) w->gap_max = gap;
            if (hold > w->hold_max) w->hold_max = hold;
            if (hold > 2) w->miss++;
            if (hold > 1) w->late++;
        }

        gpio_put(LED_PIN, (int)((sample / CV_BAR) & 1u));

        const uint32_t phrase = cv_bar_of(sample) / 8u;
        if (phrase != last_phrase) {
            window_report("PHRASE", &phr, now, sample);
            window_open(&phr, now);
            last_phrase = phrase;
        }
        if (now - sec.t0 >= 1000000ull) {
            window_report("T", &sec, now, sample);
            window_open(&sec, now);
        }
    }

    window_report("PHRASE", &phr, time_us_64(), CV_TOTAL_SAMPLES - 1);
    demo_render(video_back(), CV_TOTAL_SAMPLES - 1);
    video_present();

    video_prof_t p; video_prof(&p);
    printf("DONE frames=%lu render_max_us=%lu gap_max_us=%lu miss=%lu late=%lu under=%lu"
           " min_fill=%u copy_worst_cy=%lu pump_worst_cy=%lu vsyncs=%lu heap_free=%lu peak=%ld\n",
           (unsigned long)run_frames, (unsigned long)run_render_max, (unsigned long)run_gap_max,
           (unsigned long)run_miss, (unsigned long)run_late, (unsigned long)audio_underruns(),
           audio_min_fill(), (unsigned long)p.copy_worst, (unsigned long)p.pump_worst,
           (unsigned long)p.vsyncs, (unsigned long)heap_free(), (long)synth_peak());

    for (;;) { gpio_put(LED_PIN, 1); sleep_ms(500); gpio_put(LED_PIN, 0); sleep_ms(500); }
}
