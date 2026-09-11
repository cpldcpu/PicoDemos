/* TESSERA: Phase adapts Overscan's SLEEPER / HELION platform.
 * Original double-page ownership, scanline transport and stereo DMA preserved. */
#include "device.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/regs/dreq.h"
#ifndef TESSERA_SILENT_PUMP
#define TESSERA_SILENT_PUMP 0
#endif
#define RING 1024u
#define TRANSFERS (DURATION_SAMPLES + SAMPLE_RATE)
static uint32_t left[RING] __attribute__((aligned(4096)));
static uint32_t right[RING] __attribute__((aligned(4096)));
static int channels[2], timer;
static unsigned wr;
static volatile unsigned minimum = RING, win_min = RING, underruns = 0, played = 0;
static volatile int running;
static uint32_t pair(int16_t s) { uint32_t v = ((int32_t)s + 32768) >> 5; return v | (v << 16); }

#if TESSERA_SILENT_PUMP
static uint32_t silent_produced;
#define SYNTH_PRODUCED() silent_produced
#else
#define SYNTH_PRODUCED() synth_position()
#endif
static void fill(unsigned n)
{
    int16_t samples[16];
#if TESSERA_SILENT_PUMP
    while (n) {
        unsigned k = n > 8 ? 8 : n; for (unsigned i = 0; i < k * 2; i++) samples[i] = 0;
        for (unsigned i = 0; i < k; i++) { left[wr] = pair(samples[i * 2]); right[wr] = pair(samples[i * 2 + 1]); wr = (wr + 1) & (RING - 1); }
        silent_produced += k; n -= k;
    }
#else
    while (n) {
        unsigned k = n > 8 ? 8 : n; synth_render(samples, k);
        for (unsigned i = 0; i < k; i++) { left[wr] = pair(samples[i * 2]); right[wr] = pair(samples[i * 2 + 1]); wr = (wr + 1) & (RING - 1); }
        n -= k;
    }
#endif
}
void audio_init(void)
{
    synth_init(); gpio_init(26); gpio_set_dir(26, GPIO_OUT); gpio_put(26, 0);
    timer = dma_claim_unused_timer(true);
    /* At the selected 300 MHz system clock this is exactly 24,000 Hz. */
    hard_assert(clock_get_hz(clk_sys) % SAMPLE_RATE == 0);
    dma_timer_set_fraction(timer, 1, (uint16_t)(clock_get_hz(clk_sys) / SAMPLE_RATE));
    const unsigned pins[2] = { 28, 27 }; uint32_t *rings[2] = { left, right };
    for (int i = 0; i < 2; i++) {
        gpio_set_function(pins[i], GPIO_FUNC_PWM); unsigned slice = pwm_gpio_to_slice_num(pins[i]);
        pwm_set_wrap(slice, 2047); pwm_set_clkdiv(slice, 1); pwm_set_both_levels(slice, 1024, 1024); pwm_set_enabled(slice, true);
        /* Scanvideo has fixed low DMA channels; reserve high ones explicitly. */
        channels[i] = 10 + i; dma_channel_claim(channels[i]); dma_channel_config c = dma_channel_get_default_config(channels[i]);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_32); channel_config_set_read_increment(&c, true); channel_config_set_write_increment(&c, false);
        channel_config_set_ring(&c, false, 12); channel_config_set_dreq(&c, dma_get_timer_dreq(timer));
        /* RP2350 uses the upper four count bits as a mode. UINT32_MAX selects
         * ENDLESS, whose counter does not decrement, and would freeze the
         * clock. A finite normal transfer covers the score plus one second
         * of silence. */
        dma_channel_configure(channels[i], &c, &pwm_hw->slice[slice].cc, rings[i], dma_encode_transfer_count(TRANSFERS), false);
    }
    wr = 0; fill(RING - 1);
}
void audio_start(void) { dma_start_channel_mask((1u << channels[0]) | (1u << channels[1])); __atomic_store_n(&running, 1, __ATOMIC_RELEASE); }
void HOT(audio_pump)(void)
{
    if (!__atomic_load_n(&running, __ATOMIC_ACQUIRE)) return;
    unsigned consumed = TRANSFERS - dma_hw->ch[channels[0]].transfer_count;
    __atomic_store_n(&played, consumed, __ATOMIC_RELEASE);
    unsigned rd = consumed & (RING - 1), room = (rd - wr - 1) & (RING - 1);
    unsigned produced = SYNTH_PRODUCED(), available = produced > consumed ? produced - consumed : 0;
    if (available < minimum) __atomic_store_n(&minimum, available, __ATOMIC_RELEASE);
    if (available < __atomic_load_n(&win_min, __ATOMIC_RELAXED)) __atomic_store_n(&win_min, available, __ATOMIC_RELEASE);
    /* available==0 means the DMA has caught the writer: from here it replays
     * whatever is still in the ring, which is an audible underrun. */
    if (!available) __atomic_store_n(&underruns, underruns + 1u, __ATOMIC_RELEASE);
    if (room > 8) room = 8; if (room) fill(room);
}
uint32_t audio_position(void) { return __atomic_load_n(&played, __ATOMIC_ACQUIRE); }
unsigned audio_min_fill(void) { return __atomic_load_n(&minimum, __ATOMIC_ACQUIRE); }
unsigned audio_underruns(void) { return __atomic_load_n(&underruns, __ATOMIC_ACQUIRE); }
unsigned audio_min_window(void)
{
    unsigned v = __atomic_load_n(&win_min, __ATOMIC_ACQUIRE);
    __atomic_store_n(&win_min, RING, __ATOMIC_RELEASE);
    return v;
}
