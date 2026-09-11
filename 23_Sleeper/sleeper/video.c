/* Scanvideo transport, from HELION's video.c, which is COLOSSUS's before it.
 * Two pages with an explicit scanline-zero ownership handshake: a vblank wait
 * alone is insufficient because scanvideo queues lines ahead of the beam, so
 * core 0 cannot reuse a page until core 1 has latched the replacement.
 * yscale=2 repeats each generated row in hardware; aligned 32-bit stores
 * duplicate horizontal pixels.
 *
 * New for SLEEPER: prof_repeats. PLANNING §2's first claim is that no field
 * ever shows the previous picture, so the handshake has to count the times it
 * does. Scanline zero either finds a pending page and latches it, or it does
 * not, and the second case is exactly one repeated field. Counting starts at
 * the first successful latch: the fields between video_init() and core 0's
 * first video_present() are boot, not film. -- Overscan
 */
#include "device.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "hardware/structs/systick.h"
static uint16_t pages[2][WIDTH * HEIGHT] __attribute__((aligned(4)));
/* Core 1's own SysTick, free-running off the processor clock: 24 bits at
 * 300 MHz wraps every 56 ms, far longer than one audio_pump() and far shorter
 * than nothing. Core 0 reads the totals; the window worst is read-and-rearm,
 * so a collision with core 1 can lose one sample of it and never inflates
 * it. */
#define SYSTICK_MASK 0x00FFFFFFu
static volatile uint32_t prof_pumps, prof_cycles, prof_worst, prof_worst_all;
static volatile uint32_t prof_repeats;
/* Round two, PLANNING §2.1. One repeated field at t = 0.9 s is one too many,
 * and the way to remove it was to find out which field it was: prof_fields
 * counts every scanline-zero the generator has produced and prof_first_rep
 * records the first repeat's field number. The answer on the board was
 * **field 3**, and that named the cause exactly.
 *
 * main() primes the pipeline before starting the audio clock: it renders and
 * presents twice, once for each page, so that the first frame of the film
 * does not pay for the first write to 153,600 bytes of uncached SRAM and the
 * first build of the sky palette. Field 1 latches the first primed page and
 * field 2 the second -- but the *second* priming render is itself a first
 * touch, of the other page, and it overran a field. So field 3 found no
 * pending page. The repeat was real, it was ours, and it was entirely inside
 * boot: the audio clock had not started and the film had not begun.
 *
 * So the counter now starts where the film does. video_arm() is called after
 * audio_start(), and counting begins at the first latch after it -- which is
 * a latch core 0 has just been handed, so it has a whole field to produce the
 * next page. Fields before that are counted separately as `boot`, and both
 * numbers are printed, because a counter that quietly excludes things is
 * worth less than the claim it supports. -- Overscan */
static volatile uint32_t prof_fields, prof_first_rep, prof_boot_repeats;
static volatile int arm_req;
static inline uint32_t cyc(void) { return systick_hw->cvr; }
static inline uint32_t cyc_since(uint32_t t) { return (t - systick_hw->cvr) & SYSTICK_MASK; }
void video_prof(uint32_t *pumps, uint32_t *cycles, uint32_t *worst_window, uint32_t *worst_all)
{
    if (pumps) *pumps = __atomic_load_n(&prof_pumps, __ATOMIC_RELAXED);
    if (cycles) *cycles = __atomic_load_n(&prof_cycles, __ATOMIC_RELAXED);
    if (worst_all) *worst_all = __atomic_load_n(&prof_worst_all, __ATOMIC_RELAXED);
    if (worst_window) {
        *worst_window = __atomic_load_n(&prof_worst, __ATOMIC_RELAXED);
        __atomic_store_n(&prof_worst, 0, __ATOMIC_RELAXED);
    }
}
uint32_t video_repeats(void) { return __atomic_load_n(&prof_repeats, __ATOMIC_ACQUIRE); }
void video_fields(uint32_t *fields, uint32_t *first_repeat, uint32_t *boot_repeats)
{
    if (fields) *fields = __atomic_load_n(&prof_fields, __ATOMIC_ACQUIRE);
    if (first_repeat) *first_repeat = __atomic_load_n(&prof_first_rep, __ATOMIC_ACQUIRE);
    if (boot_repeats) *boot_repeats = __atomic_load_n(&prof_boot_repeats, __ATOMIC_ACQUIRE);
}
void video_arm(void) { __atomic_store_n(&arm_req, 1, __ATOMIC_RELEASE); }
static int back = 1;
static volatile int pending = -1, displayed = 0, ready = 0;
static void HOT(scanout)(void)
{
    systick_hw->rvr = SYSTICK_MASK; systick_hw->cvr = 0; systick_hw->csr = 0x5u; /* ENABLE | core clock */
    scanvideo_setup(&vga_mode_320x240_60); scanvideo_timing_enable(true);
    __atomic_store_n(&ready, 1, __ATOMIC_RELEASE);
    int front = 0, started = 0, counting = 0;
    while (true) {
        struct scanvideo_scanline_buffer *b = scanvideo_begin_scanline_generation(true);
        unsigned y = scanvideo_scanline_number(b->scanline_id);
        if (y == 0) {
            const uint32_t field = prof_fields + 1u;
            __atomic_store_n(&prof_fields, field, __ATOMIC_RELAXED);
            int p = __atomic_load_n(&pending, __ATOMIC_ACQUIRE);
            if (p >= 0) {
                front = p; started = 1;
                __atomic_store_n(&displayed, front, __ATOMIC_RELEASE);
                __atomic_store_n(&pending, -1, __ATOMIC_RELEASE);
                /* The film starts at the first latch after main() has armed
                 * us, which is a latch core 0 has just been handed. */
                if (__atomic_load_n(&arm_req, __ATOMIC_ACQUIRE)) counting = 1;
            } else if (counting) {
                /* No new page: this whole field repeats the last picture. */
                if (!prof_repeats) __atomic_store_n(&prof_first_rep, field, __ATOMIC_RELAXED);
                __atomic_store_n(&prof_repeats, prof_repeats + 1u, __ATOMIC_RELAXED);
            } else if (started) {
                __atomic_store_n(&prof_boot_repeats, prof_boot_repeats + 1u, __ATOMIC_RELAXED);
            }
        }
        const uint16_t *src = pages[front] + y * WIDTH;
        uint16_t *out = (uint16_t *)b->data;
        out[0] = COMPOSABLE_RAW_RUN; out[1] = src[0]; out[2] = 638;
        out[3] = src[0]; uint32_t *pairs = (uint32_t *)(out + 4);
        for (int x = 1; x < WIDTH; x++) { uint32_t v = src[x]; pairs[x - 1] = v | (v << 16); }
        out[642] = 0; out[643] = COMPOSABLE_EOL_ALIGN; b->data_used = 322; b->status = SCANLINE_OK;
        scanvideo_end_scanline_generation(b);
        const uint32_t t0 = cyc(); audio_pump(); const uint32_t dp = cyc_since(t0);
        __atomic_store_n(&prof_pumps, prof_pumps + 1u, __ATOMIC_RELAXED);
        __atomic_store_n(&prof_cycles, prof_cycles + dp, __ATOMIC_RELAXED);
        if (dp > prof_worst) __atomic_store_n(&prof_worst, dp, __ATOMIC_RELAXED);
        if (dp > prof_worst_all) __atomic_store_n(&prof_worst_all, dp, __ATOMIC_RELAXED);
    }
}
void video_init(void) { multicore_launch_core1(scanout); while (!__atomic_load_n(&ready, __ATOMIC_ACQUIRE)) tight_loop_contents(); }
uint16_t *video_back(void) { return pages[back]; }
void video_present(void)
{
    __atomic_store_n(&pending, back, __ATOMIC_RELEASE);
    while (__atomic_load_n(&pending, __ATOMIC_ACQUIRE) >= 0) tight_loop_contents();
    back = 1 - __atomic_load_n(&displayed, __ATOMIC_ACQUIRE);
}
