/* Paula-style 4-channel audio for Dawn.
 *
 * The asm at dawn_final.s:1092-1106 sets up four Paula channels each
 * playing the same 8-byte sample (line 1137) at slightly detuned periods
 * (9724/9740/9756/9772 PAL cycles per byte). The result is a soft bass
 * drone with chorus from the four-channel detune.
 *
 * On the Pico we synthesize the same thing with software mixing into a
 * PWM-DMA ring buffer. Each engine frame calls audio_update() which
 * refills the half of the ring the DMA isn't currently reading.
 *
 * Hardware path mirrors 04_SOTA_DEMO/sota/pico/sound_qoa.c:
 *   - GP28 = PWM_L (slice 6 chan A)
 *   - GP27 = PWM_R (slice 6 chan B)
 *   - GP26 = I2S_DIN held LOW to mute the on-board PCM5100A DAC that
 *     shares pins with PWM.
 *   - DMA channel 11 (explicit — scanvideo on core 1 grabs 0..2 and
 *     dma_claim_unused_channel races against that).
 *   - DMA paced by timer 0 at exactly SAMPLE_RATE Hz.
 *   - 32-bit ring entries packing both channel CC values (A in low half,
 *     B in high half) so one DMA word == one stereo sample.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/regs/dreq.h"

#include "audio.h"

#define AUDIO_L_PIN     28
#define AUDIO_R_PIN     27
#define I2S_DIN_PIN     26
#define SAMPLE_RATE     11025
#define PWM_WRAP        2047            /* 11-bit, ~122 kHz carrier @ 250 MHz */
#define PAL_CLOCK       3579545
#define NUM_CHANNELS    4

#define DMA_BUF_SAMPLES 2048            /* ~186 ms at 11025 Hz */
#define DMA_BUF_HALF    (DMA_BUF_SAMPLES / 2)

/* 8 KB ring, aligned for the DMA ring-wrap (size_bits=13 → 8192). */
static volatile uint32_t dma_buf[DMA_BUF_SAMPLES] __attribute__((aligned(8192)));

/* The asm's 8-byte sample is 0x00..0x7F — all positive bytes, a half-sine
 * pulse (Amiga audio is AC-coupled so DC offset doesn't matter on real
 * hardware). We pre-subtract 64 to centre near zero; the residual +15
 * DC after summing one cycle (123/8) gets filtered by the cap on the
 * Pimoroni audio jack. */
static const int8_t sample_centered[8] = {
    -64, -16, 25, 53, 63, 53, 25, -16,
};

/* Per-channel periods from dawn_final.s:1095/1104 (base 9724, +16 each
 * channel). Step computed in 16.16 fixed-point per output sample so
 * sample index is (phase >> 16) & 7. */
static const uint16_t ch_period[NUM_CHANNELS] = { 9724, 9740, 9756, 9772 };
static uint32_t       ch_phase [NUM_CHANNELS];
static uint32_t       ch_step  [NUM_CHANNELS];

static int  audio_dma_ch;
static uint audio_pwm_slice;
static int  last_filled = -1;

static void __not_in_flash_func(refill_half)(int half)
{
    volatile uint32_t *dst = &dma_buf[half * DMA_BUF_HALF];
    for (int i = 0; i < DMA_BUF_HALF; i++) {
        int32_t mix = 0;
        for (int c = 0; c < NUM_CHANNELS; c++) {
            const int idx = (int)((ch_phase[c] >> 16) & 7u);
            mix += sample_centered[idx];
            ch_phase[c] += ch_step[c];
        }
        /* mix ∈ [-256, 252]. Scale ×4 then centre at PWM_WRAP/2 so the
         * peak waveform uses ~half the 11-bit PWM range. */
        int32_t v = (mix << 2) + (PWM_WRAP / 2 + 1);
        if (v < 0)        v = 0;
        if (v > PWM_WRAP) v = PWM_WRAP;
        const uint32_t v32 = (uint32_t)v;
        dst[i] = (v32 << 16) | v32;
    }
}

void audio_init(void)
{
    /* Mute the I2S DAC that shares GP27/GP28 with PWM. */
    gpio_init(I2S_DIN_PIN);
    gpio_set_dir(I2S_DIN_PIN, GPIO_OUT);
    gpio_put(I2S_DIN_PIN, 0);

    /* Both audio pins driven by slice 6 (the slice GP27 and GP28 map to).
     * Chan A is the lower-numbered pin in a slice → GP28 = A = left. */
    gpio_set_function(AUDIO_L_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_R_PIN, GPIO_FUNC_PWM);
    audio_pwm_slice = pwm_gpio_to_slice_num(AUDIO_L_PIN);
    pwm_set_clkdiv  (audio_pwm_slice, 1.0f);
    pwm_set_wrap    (audio_pwm_slice, PWM_WRAP);
    pwm_set_chan_level(audio_pwm_slice, PWM_CHAN_A, PWM_WRAP / 2);
    pwm_set_chan_level(audio_pwm_slice, PWM_CHAN_B, PWM_WRAP / 2);
    pwm_set_enabled (audio_pwm_slice, true);

    /* Per-channel step: bytes/sec = PAL_CLOCK / period; convert to 16.16
     * advance per output sample by dividing by SAMPLE_RATE. */
    for (int c = 0; c < NUM_CHANNELS; c++) {
        ch_phase[c] = 0;
        ch_step[c]  = (uint32_t)(((uint64_t)PAL_CLOCK << 16) /
                                 ((uint64_t)ch_period[c] * SAMPLE_RATE));
    }

    /* Fill ring with silence (= PWM mid-level both channels). */
    const uint32_t silence = ((uint32_t)(PWM_WRAP / 2) << 16) | (uint32_t)(PWM_WRAP / 2);
    for (int i = 0; i < DMA_BUF_SAMPLES; i++) dma_buf[i] = silence;

    /* DMA pacing timer 0: fires DREQ at sys_clk * X/Y. With X=1 and
     * Y = round(sys_hz / SAMPLE_RATE) the pacing rate ≈ SAMPLE_RATE. */
    const uint32_t sys_hz  = clock_get_hz(clk_sys);
    const uint32_t divisor = (sys_hz + (SAMPLE_RATE / 2)) / SAMPLE_RATE;
    dma_hw->timer[0] = (1u << 16) | (divisor & 0xFFFFu);

    /* Explicit channel 11 — scanvideo claims 0..2 on core 1 and a
     * dma_claim_unused_channel from core 0 could win the race and steal
     * one of those before scanvideo_setup runs. */
    audio_dma_ch = 11;
    dma_channel_claim(audio_dma_ch);
    dma_channel_config cfg = dma_channel_get_default_config(audio_dma_ch);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment (&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_ring(&cfg, false, 13);          /* 8192-byte ring on read addr */
    channel_config_set_dreq(&cfg, DREQ_DMA_TIMER0);
    dma_channel_configure(audio_dma_ch, &cfg,
        &pwm_hw->slice[audio_pwm_slice].cc,            /* 32-bit CC = A|B */
        dma_buf,
        0xFFFFFFFFu,                                   /* effectively forever */
        true);

    last_filled = -1;
}

/* Refill the half of the ring the DMA isn't reading. Buffer is 186 ms;
 * engine frame is 16.7 ms — there's >5× slack before underrun. */
void __not_in_flash_func(audio_update)(int frame_count)
{
    (void)frame_count;
    const uint32_t read_addr = (uint32_t)dma_hw->ch[audio_dma_ch].read_addr;
    const int      offset    = (int)(((read_addr - (uint32_t)dma_buf) / sizeof(uint32_t)) % DMA_BUF_SAMPLES);
    const int      current_half = (offset < DMA_BUF_HALF) ? 0 : 1;
    const int      target_half  = 1 - current_half;
    if (target_half != last_filled) {
        refill_half(target_half);
        last_filled = target_half;
    }
}
