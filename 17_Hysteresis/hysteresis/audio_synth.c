/* Device audio: the synth straight into a PWM-DMA ring.
 *
 * The output path is the one every demo in this repo uses (16_Sustain's
 * audio_qoa.c, which follows 05_MDIV): PWM slice 6 on GP27/GP28 fed by a DMA
 * paced off a DREQ timer, with the on-board I2S DAC held muted on GP26. The only
 * thing that changed is what fills the ring -- a QOA decoder became a
 * synth_render() call, and 2.59 MB of flash became about 18 KB of SRAM.
 *
 * ----------------------------------------------------------- WHICH CORE ------
 *
 * Core 1, from inside the scanline loop, a few samples at a time.
 *
 * Core 0 is not available: the field step measures 62.8 cycles/cell over 76,800
 * cells, which is 4.82 M of the 5.0 M cycles a 60 Hz frame has at 300 MHz. There
 * is about 4% of core 0 left and the synth needs most of it, so putting audio
 * there would push the frame over budget -- and in this demo a late frame is not
 * a dropped frame, it is a DIVERGED frame, because the simulation's state depends
 * on how many steps it has taken (sim.h). Core 1 has the whole of the time
 * between scanlines: palette-lookup-and-double costs roughly 1,300 of the 9,525
 * cycles a scanline gets.
 *
 * ------------------------------------------------- WHY NOT A HALF-BUFFER -----
 *
 * The inherited pattern refills half the ring at once from a timer callback.
 * Here that would be 512 samples in one go on the core with the scanline
 * deadline -- roughly 300 microseconds, or nine scanlines of stall, against a
 * queue only sixteen buffers deep.
 *
 * Audio needs 22,050 samples per second and there are 31,500 scanlines per
 * second, so the real requirement is 0.7 samples per scanline. Topping the ring
 * up by at most AUDIO_MAX_FILL each line spreads the identical work flat, and
 * leaves an order of magnitude of catch-up for the 1.4 ms the loop spends
 * blocked in vertical blanking.
 */

#include "audio.h"
#include "synth.h"
#include "hot.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/regs/dreq.h"
#include <stdio.h>

#define AUDIO_L_PIN     28
#define AUDIO_R_PIN     27
#define I2S_DIN_PIN     26
#define PWM_WRAP        2047        /* 11-bit, ~146 kHz carrier at 300 MHz */
#define RING_SAMPLES    1024        /* 46 ms; 4 KB, and the DMA ring wants 2^n */
#define AUDIO_MAX_FILL  6           /* per scanline; 8x the 0.7 actually needed */

/* Each 32-bit entry is the pair of PWM levels, so one DMA write drives both
 * channels of the slice and the mono signal reaches both ears. */
static volatile uint32_t dma_buf[RING_SAMPLES] __attribute__((aligned(4096)));

static int      g_dma;
static uint     g_slice;
static int      g_wr;
static volatile int g_running;

/* Shallowest the ring has ever been, in unplayed samples.
 *
 * The sample hash proves the two targets GENERATE identical audio; it cannot
 * prove the DMA was never starved, because an underrun replays whatever stale
 * words are still in the ring and clicks without altering a single generated
 * sample. That failure is inaudible to every check in this repo and obvious to a
 * listener, so it gets its own number. Full is 1023; anything approaching zero
 * means core 1 is missing its deadline. */
static volatile int g_min_fill = RING_SAMPLES;

/* int16 -> 11-bit unsigned, centre 1024, mirrored into both channels. */
static inline uint32_t pwm_pair(int16_t s)
{
    const uint32_t v = (uint32_t)(((int32_t)s + 32768) >> 5) & 0x7FFu;
    return (v << 16) | v;
}

static void fill(int n)
{
    int16_t tmp[AUDIO_MAX_FILL];
    while (n > 0) {
        const int k = n < AUDIO_MAX_FILL ? n : AUDIO_MAX_FILL;
        synth_render(tmp, k);
        for (int i = 0; i < k; i++) {
            dma_buf[g_wr] = pwm_pair(tmp[i]);
            if (++g_wr >= RING_SAMPLES) g_wr = 0;
        }
        n -= k;
    }
}

void audio_init(void)
{
    /* Mute the on-board PCM5100A: it shares the header and would otherwise
     * output whatever the undriven I2S pins happen to do. */
    gpio_init(I2S_DIN_PIN);
    gpio_set_dir(I2S_DIN_PIN, GPIO_OUT);
    gpio_put(I2S_DIN_PIN, 0);

    gpio_set_function(AUDIO_L_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_R_PIN, GPIO_FUNC_PWM);
    g_slice = pwm_gpio_to_slice_num(AUDIO_L_PIN);
    pwm_set_clkdiv(g_slice, 1.0f);
    pwm_set_wrap(g_slice, PWM_WRAP);
    pwm_set_chan_level(g_slice, PWM_CHAN_A, PWM_WRAP / 2);
    pwm_set_chan_level(g_slice, PWM_CHAN_B, PWM_WRAP / 2);
    pwm_set_enabled(g_slice, true);

    const uint32_t silence = pwm_pair(0);
    for (int i = 0; i < RING_SAMPLES; i++) dma_buf[i] = silence;

    /* Pacing timer: one DREQ every sys_clk/SYNTH_RATE cycles. */
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    const uint32_t div    = (sys_hz + (SYNTH_RATE / 2)) / SYNTH_RATE;
    dma_hw->timer[0] = (1u << 16) | (div & 0xFFFFu);

    /* Channel 11: scanvideo claims the low channels. */
    g_dma = 11;
    dma_channel_claim(g_dma);
    dma_channel_config c = dma_channel_get_default_config(g_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, 12);        /* 4 KB wrap */
    channel_config_set_dreq(&c, DREQ_DMA_TIMER0);
    dma_channel_configure(g_dma, &c, &pwm_hw->slice[g_slice].cc,
                          dma_buf, 0xFFFFFFFFu, true);

    printf("[audio] synth -> PWM @ %d Hz, %d-sample ring, filled from core 1\n",
           SYNTH_RATE, RING_SAMPLES);
}

void audio_start(void)
{
    synth_reset();
    g_wr = 0;

    /* Fill the WHOLE ring here, on core 0, before core 1 is allowed to touch
     * it. Otherwise the demo opens with 46 ms of the pre-loaded silence while
     * core 1 trickles six samples per scanline into a ring the DMA is already
     * draining, and the write pointer spends the first second a hair ahead of
     * the read pointer. Two milliseconds, once. */
    fill(RING_SAMPLES - 1);

    __atomic_store_n(&g_running, 1, __ATOMIC_RELEASE);
}

/* Called once per scanline from core 1 (vga.c). */
void HYST_HOT(audio_pump)(void)
{
    if (!__atomic_load_n(&g_running, __ATOMIC_ACQUIRE)) return;

    const uint32_t ra = (uint32_t)dma_hw->ch[g_dma].read_addr;
    const int rd = (int)(((ra - (uint32_t)dma_buf) / sizeof(uint32_t))
                         % RING_SAMPLES);

    /* Room up to one behind the read pointer, so a full ring is never mistaken
     * for an empty one. */
    int room = rd - g_wr - 1;
    if (room < 0) room += RING_SAMPLES;

    const int filled = RING_SAMPLES - 1 - room;
    if (filled < g_min_fill) g_min_fill = filled;

    if (room > AUDIO_MAX_FILL) room = AUDIO_MAX_FILL;
    if (room > 0) fill(room);
}

int audio_min_fill(void) { return g_min_fill; }

/* Derived from samples actually generated, not from time_us_64().
 *
 * Nothing in this demo drives the picture from it -- the clock is the frame
 * index and that is a correctness property (sim.h) -- but audio.h declares it,
 * and a millisecond count that comes from the sample stream is at least honest
 * about what it is measuring. */
uint32_t audio_now_ms(void)
{
    return (uint32_t)((uint64_t)synth_pos() * 1000u / SYNTH_RATE);
}

void audio_seek_ms(uint32_t target_ms)
{
    /* There is no seek in this demo, by construction: both the field and the
     * synth are state machines with tens of seconds of memory in them, so the
     * only way to a given moment is to get there. Kept because audio.h has it. */
    (void)target_ms;
}
