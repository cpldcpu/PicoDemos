/* Device audio: synth_render() into two PWM slices through DMA, pumped from
 * core 1 between scanlines.
 *
 * GP28 (slice 6, channel A) and GP27 (slice 5, channel B) are on DIFFERENT
 * slices, so each stereo channel needs its own ring and its own DMA channel.
 * Both are paced by one DMA timer and started with one mask write, so the two
 * channels cannot skew. GP26 is held low to keep the VGA board's I2S DAC
 * quiet. All of that is VESPER's, and the README there records why.
 *
 * Two RP2350 facts that cost other people time:
 *   - the top four bits of TRANS_COUNT are a mode, and all-ones selects
 *     ENDLESS, whose counter does not decrement. The counter is the clock the
 *     whole demo follows, so the transfer count is finite: the score plus one
 *     second of silence.
 *   - scanvideo owns the low DMA channels. Claim high ones explicitly.
 *
 * The rate is exactly 24,000 Hz: 300,000,000 / 24,000 = 12,500 sys cycles a
 * sample, no remainder. PERSISTENCE had to pull its rate 0.4% flat to keep an
 * integer number of samples per video frame, because there every frame was a
 * function of the frame index. Here the picture is a function of the sample
 * the DAC is playing, so the video may run at whatever 59.75 Hz it likes and
 * the audio stays at the rate the host renders at. That is what makes the
 * host WAV and the device output the same bytes.
 */

#include "platform.h"
#include "synth.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

#define RING       512u                               /* 21.3 ms per channel */
#define RING_BITS  11                                 /* 512 * 4 bytes       */
#define TRANSFERS  (CV_TOTAL_SAMPLES + CV_RATE)

/* Pump policy. A pump happens once per generated scanline -- about 14,340
 * times a second at 240 lines a frame -- so the ring only needs 1.7 frames of
 * work each time. Rendering one or two frames per synth_render() call would
 * pay the block prologue 14,000 times a second, so the pump waits until there
 * is a chunk's worth of room and then fills it. CHUNK samples is about 5,000
 * cycles of synth; scanvideo's queued buffers absorb that burst. */
#define CHUNK      16u
#define MAX_FILL   64u

/* The DMA ring wrap is an address mask, so each buffer has to be aligned to
 * its own size. 512 frames is 21 ms of audio a side and 4,096 bytes for the
 * pair, which is what LEDGER.md reserved for the rings; 1,024 would have been
 * twice the ledger for slack that the measured min fill says is not needed. */
static uint32_t s_left [RING] __attribute__((aligned(2048)));
static uint32_t s_right[RING] __attribute__((aligned(2048)));
static int16_t  s_tmp[2 * MAX_FILL];

static int      s_ch[2];
static int      s_timer;
static unsigned s_wr;
static volatile unsigned s_min_fill = RING;
static volatile uint32_t s_underruns;
static volatile int      s_running;

/* 11 bits into a 2048-wrap PWM slice, the same value in both halves of the
 * 32-bit write so one store sets the compare register. */
static inline uint32_t pair(int16_t s)
{
    const uint32_t v = ((uint32_t)((int32_t)s + 32768) >> 5) & 0x7FFu;
    return v | (v << 16);
}

static void CV_HOT(fill)(unsigned n)
{
    while (n) {
        const unsigned k = n > MAX_FILL ? MAX_FILL : n;
        synth_render(s_tmp, (int)k);
        for (unsigned i = 0; i < k; i++) {
            s_left [s_wr] = pair(s_tmp[2 * i]);
            s_right[s_wr] = pair(s_tmp[2 * i + 1]);
            s_wr = (s_wr + 1) & (RING - 1);
        }
        n -= k;
    }
}

void audio_init(void)
{
    gpio_init(26); gpio_set_dir(26, GPIO_OUT); gpio_put(26, 0);   /* I2S mute */

    hard_assert(clock_get_hz(clk_sys) == 300000000u);
    hard_assert(clock_get_hz(clk_sys) % CV_RATE == 0u);
    s_timer = dma_claim_unused_timer(true);
    dma_timer_set_fraction(s_timer, 1, (uint16_t)(clock_get_hz(clk_sys) / CV_RATE));

    const unsigned pins[2] = { 28, 27 };
    uint32_t *rings[2] = { s_left, s_right };
    for (int i = 0; i < 2; i++) {
        gpio_set_function(pins[i], GPIO_FUNC_PWM);
        const unsigned slice = pwm_gpio_to_slice_num(pins[i]);
        pwm_set_wrap(slice, 2047);
        pwm_set_clkdiv(slice, 1);
        pwm_set_both_levels(slice, 1024, 1024);
        pwm_set_enabled(slice, true);

        s_ch[i] = 10 + i;
        dma_channel_claim(s_ch[i]);
        dma_channel_config c = dma_channel_get_default_config(s_ch[i]);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
        channel_config_set_read_increment(&c, true);
        channel_config_set_write_increment(&c, false);
        channel_config_set_ring(&c, false, RING_BITS);
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

uint32_t CV_HOT(audio_position)(void)
{
    if (!__atomic_load_n(&s_running, __ATOMIC_ACQUIRE)) return 0;
    return TRANSFERS - dma_hw->ch[s_ch[0]].transfer_count;
}

void CV_HOT(audio_pump)(void)
{
    if (!__atomic_load_n(&s_running, __ATOMIC_ACQUIRE)) return;

    const uint32_t consumed = TRANSFERS - dma_hw->ch[s_ch[0]].transfer_count;
    const uint32_t produced = synth_pos();

    /* An underrun is the DAC having played a sample the synth never wrote.
     * The ring does not fault when that happens, it repeats 42 ms of the
     * past, so it has to be counted rather than heard. */
    const uint32_t ahead = produced > consumed ? produced - consumed : 0u;
    if (ahead < s_min_fill) s_min_fill = ahead;
    if (produced <= consumed) s_underruns++;

    unsigned room = ((consumed & (RING - 1)) - s_wr - 1u) & (RING - 1u);
    if (room < CHUNK) return;
    if (room > MAX_FILL) room = MAX_FILL;
    fill(room);
}

unsigned audio_min_fill(void)  { return s_min_fill; }
uint32_t audio_underruns(void) { return __atomic_load_n(&s_underruns, __ATOMIC_ACQUIRE); }
