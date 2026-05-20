/* QOA-decoded PWM-DMA audio for 20_TheDemo.
 *
 * Pattern copied from 05_MDIV/mdiv/pico/s_pico.c (which itself follows the
 * 04_SOTA_DEMO pattern). Adapted for:
 *   - 22050 Hz mono soundtrack (vs MDIV's 11025 Hz)
 *   - Standalone API (audio_init / audio_start / audio_pump / audio_now_ms)
 *     rather than the MDIV s_*() engine hooks.
 *
 * Wiring (Pimoroni VGA Demo Base, unchanged from MDIV):
 *   GP26 = I2S DIN — held LOW to mute the on-board PCM5100A I2S DAC
 *   GP27 = PWM right (slice 6 chan B)
 *   GP28 = PWM left  (slice 6 chan A)
 *
 * The mono soundtrack is mirrored onto both PWM channels so both ears
 * get the same signal out the 3.5 mm jack.
 */

#include "audio.h"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/regs/dreq.h"
#include "hardware/sync.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO
#include "qoa.h"

#define AUDIO_L_PIN     28
#define AUDIO_R_PIN     27
#define I2S_DIN_PIN     26
#define SAMPLE_RATE     22050
#define PWM_WRAP        2047            /* 11-bit, ~122 kHz carrier @ 250 MHz */
#define DMA_BUF_SAMPLES 2048
#define DMA_BUF_HALF    (DMA_BUF_SAMPLES / 2)

/* incbin'd by music_qoa.S */
extern const uint8_t _music_qoa_start[];
extern const uint8_t _music_qoa_end[];

/* DMA ring (8 KB, 8 KB aligned). Each 32-bit entry packs L|R PWM levels. */
static volatile uint32_t dma_buf[DMA_BUF_SAMPLES] __attribute__((aligned(8192)));

/* QOA decoder state. Frame buffer = 5120 int16 samples = 10 KB. */
static qoa_desc qoa_state;
static uint32_t qoa_header_size = 0;
static uint32_t qoa_byte_pos    = 0;
static int16_t  qoa_frame_buf[QOA_FRAME_LEN];
static uint32_t qoa_frame_len   = 0;
static uint32_t qoa_frame_pos   = 0;

static int      audio_dma_ch;
static uint     audio_pwm_slice;
static int      last_filled    = -1;
static bool     sound_running  = false;

/* Demo clock. time_us_64() at audio_start(). */
static uint64_t start_us = 0;

/* -------- QOA frame streaming ------------------------------------------- */

static int16_t next_sample(void)
{
    if (qoa_frame_pos >= qoa_frame_len) {
        const uint8_t *src = _music_qoa_start;
        uint32_t total = (uint32_t)(_music_qoa_end - _music_qoa_start);
        if (qoa_byte_pos >= total) return 0;   /* end of soundtrack */

        unsigned int new_frame_len = 0;
        unsigned int consumed = qoa_decode_frame(
            src + qoa_byte_pos,
            (unsigned int)(total - qoa_byte_pos),
            &qoa_state, qoa_frame_buf, &new_frame_len);
        if (consumed == 0) return 0;           /* decode error → silence */

        qoa_byte_pos += consumed;
        qoa_frame_len = new_frame_len;
        qoa_frame_pos = 0;
    }
    return qoa_frame_buf[qoa_frame_pos++];
}

static void refill_half(int half)
{
    volatile uint32_t *dst = &dma_buf[half * DMA_BUF_HALF];
    for (int i = 0; i < DMA_BUF_HALF; i++) {
        int16_t s = next_sample();
        /* int16 [-32768..32767] → 11-bit unsigned [0..2047], centre=1024.
         * Mirror into both PWM channels so the mono signal comes out of
         * both jack channels. */
        uint32_t v = (uint32_t)(((int32_t)s + 32768) >> 5) & 0x7FFu;
        dst[i] = (v << 16) | v;
    }
}

/* -------- public API ---------------------------------------------------- */

void audio_init(void)
{
    /* Parse the QOA header. */
    uint32_t total = (uint32_t)(_music_qoa_end - _music_qoa_start);
    qoa_header_size = qoa_decode_header(_music_qoa_start, (int)total, &qoa_state);
    if (qoa_header_size == 0) {
        printf("[audio] qoa_decode_header failed (size=%u)\n", (unsigned)total);
        return;
    }
    qoa_byte_pos  = qoa_header_size;
    qoa_frame_len = 0;
    qoa_frame_pos = 0;

    /* Mute the on-board I2S DAC. */
    gpio_init(I2S_DIN_PIN);
    gpio_set_dir(I2S_DIN_PIN, GPIO_OUT);
    gpio_put(I2S_DIN_PIN, 0);

    /* PWM slice 6, both channels driven from the same 32-bit CC write. */
    gpio_set_function(AUDIO_L_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_R_PIN, GPIO_FUNC_PWM);
    audio_pwm_slice = pwm_gpio_to_slice_num(AUDIO_L_PIN);
    pwm_set_clkdiv(audio_pwm_slice, 1.0f);
    pwm_set_wrap  (audio_pwm_slice, PWM_WRAP);
    pwm_set_chan_level(audio_pwm_slice, PWM_CHAN_A, PWM_WRAP / 2);
    pwm_set_chan_level(audio_pwm_slice, PWM_CHAN_B, PWM_WRAP / 2);
    pwm_set_enabled(audio_pwm_slice, true);

    /* Pre-fill ring with silence (centre = 1024). */
    uint32_t silence = ((uint32_t)(PWM_WRAP / 2) << 16) | (uint32_t)(PWM_WRAP / 2);
    for (int i = 0; i < DMA_BUF_SAMPLES; i++) dma_buf[i] = silence;

    /* DMA pacing timer 0 → SAMPLE_RATE. */
    uint32_t sys_hz  = clock_get_hz(clk_sys);
    uint32_t divisor = (sys_hz + (SAMPLE_RATE / 2)) / SAMPLE_RATE;
    dma_hw->timer[0] = (1u << 16) | (divisor & 0xFFFFu);

    /* DMA channel 11 — high to avoid scanvideo's claim (scanvideo grabs
     * the low channels). */
    audio_dma_ch = 11;
    dma_channel_claim(audio_dma_ch);
    dma_channel_config c = dma_channel_get_default_config(audio_dma_ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment (&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, 13);  /* 8 KB wrap (2^13 bytes) */
    channel_config_set_dreq(&c, DREQ_DMA_TIMER0);

    dma_channel_configure(audio_dma_ch, &c,
        &pwm_hw->slice[audio_pwm_slice].cc,
        dma_buf,
        0xFFFFFFFFu,
        true);

    last_filled = -1;
    sound_running = true;
    printf("[audio] QOA PWM @ %u Hz, %u ch, %u samples, %u bytes\n",
           qoa_state.samplerate, qoa_state.channels, qoa_state.samples,
           (unsigned)total);
}

/* Repeating timer that fires audio_pump on a fixed cadence, independent
 * of main-loop pacing. Without this, any frame that takes longer than
 * one DMA half-buffer (~46 ms @ 22050 Hz mono) underruns the audio —
 * which manifests as the music slowing down whenever the voxel scene
 * (or any heavy effect) lags. Decoupled here so audio keeps flowing
 * even when graphics misses budget. */
static repeating_timer_t audio_timer;
static bool              audio_timer_installed = false;

static bool audio_timer_cb(repeating_timer_t *t)
{
    (void)t;
    audio_pump();
    return true;
}

void audio_start(void)
{
    /* Rewind QOA + zero the demo clock so t=0 lines up with sample 0 (after
     * ~46 ms of buffered silence — see audio.h). */
    qoa_byte_pos  = qoa_header_size;
    qoa_frame_len = 0;
    qoa_frame_pos = 0;
    start_us = time_us_64();

    /* Install the pump timer ONCE, after state is initialised. Negative
     * delay = "fire every 15 ms relative to the previous fire" (regular
     * cadence even if a callback runs long). 15 ms gives ~3× safety
     * margin over the 46 ms half-buffer window. */
    if (!audio_timer_installed) {
        add_repeating_timer_ms(-15, audio_timer_cb, NULL, &audio_timer);
        audio_timer_installed = true;
    }
}

void audio_seek_ms(uint32_t target_ms)
{
    /* QOA seek: walk the bitstream from the start, summing each frame's
     * sample count from its 8-byte header, until cumulative samples
     * reaches target_ms's sample position. Then point the decoder at
     * that frame and re-anchor the demo clock so audio_now_ms() returns
     * the actual landing time (frame-quantised, ~232 ms granularity at
     * 22050 Hz / 5120 samples-per-frame — fine for scene navigation).
     *
     * QOA frame header layout (8 bytes, big-endian):
     *   [0]    channels       (8 bits)
     *   [1..3] samplerate     (24 bits)
     *   [4..5] frame samples  (16 bits)
     *   [6..7] frame size     (16 bits, includes the 8-byte header)
     *
     * Runs from the main loop while the audio_pump ISR may fire, so we
     * disable IRQs around the state mutation. */
    uint32_t total_samples = qoa_state.samples;
    if (target_ms == 0 || total_samples == 0) {
        uint32_t s = save_and_disable_interrupts();
        qoa_byte_pos  = qoa_header_size;
        qoa_frame_len = 0;
        qoa_frame_pos = 0;
        start_us = time_us_64();
        restore_interrupts(s);
        return;
    }

    uint64_t target_sample_u64 =
        (uint64_t)target_ms * (uint64_t)SAMPLE_RATE / 1000ULL;
    if (target_sample_u64 >= total_samples) {
        target_sample_u64 = total_samples - 1;
    }
    uint32_t target_sample = (uint32_t)target_sample_u64;

    uint32_t total_bytes = (uint32_t)(_music_qoa_end - _music_qoa_start);
    uint32_t pos         = qoa_header_size;
    uint32_t cum_samples = 0;

    /* Walk frames until we'd cross target_sample — leave `pos` pointing
     * at the start of the frame that CONTAINS target_sample so the next
     * decode produces audio at cum_samples (≤ target). */
    while (pos + 8 <= total_bytes) {
        const uint8_t *p = _music_qoa_start + pos;
        uint16_t fsamples = ((uint16_t)p[4] << 8) | p[5];
        uint16_t fsize    = ((uint16_t)p[6] << 8) | p[7];
        if (fsize == 0 || pos + fsize > total_bytes) break;
        if (cum_samples + fsamples > target_sample) break;
        pos          += fsize;
        cum_samples  += fsamples;
    }

    /* Re-anchor the demo clock to the audio's actual landing sample so
     * the visuals stay locked to the music after the seek. */
    uint32_t actual_ms =
        (uint32_t)((uint64_t)cum_samples * 1000ULL / (uint64_t)SAMPLE_RATE);

    uint32_t s = save_and_disable_interrupts();
    qoa_byte_pos  = pos;
    qoa_frame_len = 0;
    qoa_frame_pos = 0;
    start_us      = time_us_64() - (uint64_t)actual_ms * 1000ULL;
    restore_interrupts(s);
}

void audio_pump(void)
{
    if (!sound_running) return;

    uint32_t read_addr    = (uint32_t)dma_hw->ch[audio_dma_ch].read_addr;
    int      offset       = (int)(((read_addr - (uint32_t)dma_buf) / sizeof(uint32_t)) % DMA_BUF_SAMPLES);
    int      current_half = (offset < DMA_BUF_HALF) ? 0 : 1;
    int      target_half  = 1 - current_half;

    if (target_half != last_filled) {
        refill_half(target_half);
        last_filled = target_half;
    }
}

uint32_t audio_now_ms(void)
{
    if (start_us == 0) return 0;
    return (uint32_t)((time_us_64() - start_us) / 1000ULL);
}
