/* Device audio: the synth into two PWM slices through DMA, from core 0.
 *
 * GP28 (slice 6A) and GP27 (slice 5B) are different slices, so each channel
 * has its own ring and its own DMA channel, both paced by one DMA timer and
 * started with one mask write (18_Vesper/audio_pwm.c established this; the
 * mono path older demos used only ever drove one of the two jack channels).
 *
 * The timer divides sys_clk so that exactly 400 samples pass per video frame
 * (see audio_init), which is what makes "400 samples per frame" a fact rather
 * than a rounding. The transfer count is finite and sample-counted because on RP2350
 * an all-ones count selects ENDLESS mode, whose counter does not decrement --
 * and that counter is the clock the whole picture follows.
 */

#include "audio.h"
#include "synth.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#define RING       2048u                     /* 85 ms per channel                */
#define TRANSFERS  (PV_TOTAL_SAMPLES + PV_RATE)

static uint32_t s_left [RING] __attribute__((aligned(8192)));
static uint32_t s_right[RING] __attribute__((aligned(8192)));
static int      s_ch[2];
static int      s_timer;
static unsigned s_wr;
static volatile unsigned s_min_fill = RING;
static volatile int s_running;

static inline uint32_t pair(int16_t s)
{
    uint32_t v = ((uint32_t)((int32_t)s + 32768) >> 5) & 0x7FFu;
    return v | (v << 16);
}

static void fill(unsigned n)
{
    int16_t tmp[64];
    while (n) {
        unsigned k = n > 32 ? 32 : n;
        synth_render(tmp, (int)k);
        for (unsigned i = 0; i < k; i++) {
            s_left [s_wr] = pair(tmp[2 * i]);
            s_right[s_wr] = pair(tmp[2 * i + 1]);
            s_wr = (s_wr + 1) & (RING - 1);
        }
        n -= k;
    }
}

void audio_init(void)
{
    gpio_init(26); gpio_set_dir(26, GPIO_OUT); gpio_put(26, 0);   /* I2S DAC muted */

    /* One sample every 12,552 cycles. The mode is 800 x 523 pixel clocks at
     * 25 MHz -- 59.75 Hz, not 60 -- so a frame is 418,400 x 12 = 5,020,800
     * sys cycles, and 400 samples per frame means 12,552 cycles per sample:
     * 23,900.6 Hz. The nominal 24 kHz would have drifted the picture off the
     * music by a frame every four seconds (measured, main.c 'skipped'). The
     * host plays the same samples at 24,000, 0.4% higher; nobody can hear it. */
    hard_assert(clock_get_hz(clk_sys) == 300000000u);
    s_timer = dma_claim_unused_timer(true);
    dma_timer_set_fraction(s_timer, 1, 12552);

    const unsigned pins[2] = { 28, 27 };
    uint32_t *rings[2] = { s_left, s_right };
    for (int i = 0; i < 2; i++) {
        gpio_set_function(pins[i], GPIO_FUNC_PWM);
        unsigned slice = pwm_gpio_to_slice_num(pins[i]);
        pwm_set_wrap(slice, 2047);
        pwm_set_clkdiv(slice, 1);
        pwm_set_both_levels(slice, 1024, 1024);
        pwm_set_enabled(slice, true);

        s_ch[i] = 10 + i;                       /* scanvideo owns the low channels */
        dma_channel_claim(s_ch[i]);
        dma_channel_config c = dma_channel_get_default_config(s_ch[i]);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_ring(&c, false, 13);            /* 8 KB wrap */
        channel_config_set_dreq(&c, dma_get_timer_dreq(s_timer));
        dma_channel_configure(s_ch[i], &c, &pwm_hw->slice[slice].cc, rings[i],
                              dma_encode_transfer_count(TRANSFERS), false);
    }
    s_wr = 0;
    synth_reset();
    fill(RING - 1);
}

void audio_start(void)
{
    dma_start_channel_mask((1u << s_ch[0]) | (1u << s_ch[1]));
    __atomic_store_n(&s_running, 1, __ATOMIC_RELEASE);
}

uint32_t audio_consumed(void)
{
    if (!s_running) return 0;
    return TRANSFERS - dma_hw->ch[s_ch[0]].transfer_count;
}

void audio_pump(void)
{
    if (!s_running) return;
    unsigned consumed = audio_consumed();
    unsigned rd = consumed & (RING - 1);
    unsigned room = (rd - s_wr - 1) & (RING - 1);
    unsigned filled = RING - 1 - room;
    if (filled < s_min_fill) s_min_fill = filled;
    if (room) fill(room);
}

unsigned audio_min_fill(void) { return s_min_fill; }
