/* QOA-driven PWM audio for the Pico VGA build.
 *
 * Pipeline:
 *   build_demo.py --audio-format=qoa transcodes each .mod / .wav to mono
 *   QOA at SAMPLE_RATE and packs the QOA bytes into the WAD in place of
 *   the original. Engine choreography references audio by WAD file index
 *   exactly like the mikmod / SDL_mixer backends — only the bytes those
 *   indices point to are different.
 *
 * Two concurrent streams: one music + one sample (SOTA's heartbeat plays
 * simultaneously with the music MOD during a few scenes). Software mix
 * to int16, scale to 11-bit, DMA-fed to a PWM slice on GP28 paced at
 * exactly SAMPLE_RATE Hz via DMA pacing timer 0.
 *
 * RAM cost: 2 x 10 KB QOA frame scratch + 1 KB DMA ring + ~100 B state.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/regs/dreq.h"

#include "sound.h"
#include "backend.h"

#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO
#include "qoa.h"

#define AUDIO_L_PIN      28                /* VGABOARD_PWM_L_PIN, slice 6 chan A — also I2S LRCK when DAC is active */
#define AUDIO_R_PIN      27                /* VGABOARD_PWM_R_PIN, slice 6 chan B — also I2S BCK when DAC is active */
#define I2S_DIN_PIN      26                /* VGABOARD_I2S_DIN_PIN — keep LOW to mute the on-board PCM5100A I2S DAC */
#define SAMPLE_RATE      11025
#define PWM_WRAP         2047              /* 11-bit PWM -> ~122 kHz carrier @ 250 MHz sys_clk */

/* Each DMA word is one full PWM slice CC register write: low 16 bits go
 * to channel A (left = GP28), high 16 bits to channel B (right = GP27).
 * We mirror the mono sample into both halves so both audio jack
 * channels emit the same signal.
 *
 * Buffer size 2048 samples = ~186 ms @ 11025 Hz (93 ms per half). That
 * gives the engine plenty of slack — the 3D scene's polygon rasterizer
 * can take 50-80 ms per frame on core 0 without underrunning the audio
 * DMA. */
#define DMA_BUF_SAMPLES  2048
#define DMA_BUF_HALF     (DMA_BUF_SAMPLES / 2)
static volatile uint32_t dma_buf[DMA_BUF_SAMPLES] __attribute__((aligned(8192)));

static int  audio_dma_ch;
static uint audio_pwm_slice;
static int  last_filled = -1;              /* which half we last wrote (-1 = nothing yet) */
static bool nosound_mode;

typedef struct {
    const uint8_t *qoa_bytes;
    size_t         qoa_size;
    qoa_desc       desc;
    uint32_t       header_size;            /* offset of first frame in qoa_bytes */
    uint32_t       byte_pos;               /* next byte to decode from */
    int16_t        frame_buf[QOA_FRAME_LEN]; /* 5120 samples = 10 KB */
    uint32_t       frame_len;
    uint32_t       frame_pos;
    bool           active;
    bool           loop;
} qoa_stream_t;

static qoa_stream_t music;
static qoa_stream_t sample;

/* -------- per-stream decode --------------------------------------------- */

static bool stream_open(qoa_stream_t *s, int file_idx, bool loop)
{
    size_t size = 0;
    const void *data = backend_wad_load_file(file_idx, &size);
    if (!data || size < 8) return false;

    memset(s, 0, sizeof(*s));
    s->qoa_bytes   = (const uint8_t *)data;
    s->qoa_size    = size;
    s->header_size = qoa_decode_header(s->qoa_bytes, (int)s->qoa_size, &s->desc);
    if (s->header_size == 0) {
        printf("[sound] qoa_decode_header failed on file %d (size=%u)\n",
               file_idx, (unsigned)size);
        return false;
    }
    s->byte_pos = s->header_size;
    s->loop     = loop;
    s->active   = true;
    return true;
}

static int16_t __not_in_flash_func(stream_next_sample)(qoa_stream_t *s)
{
    if (!s->active) return 0;

    if (s->frame_pos >= s->frame_len) {
        if (s->byte_pos >= s->qoa_size) {
            if (s->loop) {
                s->byte_pos = s->header_size;
            } else {
                s->active = false;
                return 0;
            }
        }
        unsigned int frame_len = 0;
        unsigned int consumed = qoa_decode_frame(
            s->qoa_bytes + s->byte_pos,
            (unsigned int)(s->qoa_size - s->byte_pos),
            &s->desc, s->frame_buf, &frame_len);
        if (consumed == 0) {
            s->active = false;
            return 0;
        }
        s->byte_pos  += consumed;
        s->frame_len  = frame_len;
        s->frame_pos  = 0;
    }
    /* QOA stored mono so we just take channel 0 directly. */
    return s->frame_buf[s->frame_pos++];
}

/* -------- mix + scale --------------------------------------------------- */

static void __not_in_flash_func(refill_half)(int half)
{
    volatile uint32_t *dst = &dma_buf[half * DMA_BUF_HALF];
    for (int i = 0; i < DMA_BUF_HALF; i++) {
        int32_t m = stream_next_sample(&music);
        int32_t s = stream_next_sample(&sample);
        int32_t sum = m + s;
        if      (sum >  32767) sum =  32767;
        else if (sum < -32768) sum = -32768;
        /* 16-bit signed -> 11-bit unsigned (centre = 1024). Mirror into
         * both 16-bit halves so chan A (GP28) and chan B (GP27) emit the
         * same mono signal. */
        uint32_t v = (uint32_t)(((sum + 32768) >> 5) & 0x7FFu);
        dst[i] = (v << 16) | v;
    }
}

/* -------- sound.h API --------------------------------------------------- */

bool sound_init(bool nosound)
{
    nosound_mode = nosound;
    memset(&music,  0, sizeof(music));
    memset(&sample, 0, sizeof(sample));
    if (nosound) return true;

    /* The on-board PCM5100A I2S DAC shares pins with the PWM audio path.
     * GP26 = I2S DIN — hold it LOW so the DAC has no data to latch and
     * stays silent regardless of whatever PWM signals it sees on the
     * BCK/LRCK lines (GP27/GP28). */
    gpio_init(I2S_DIN_PIN);
    gpio_set_dir(I2S_DIN_PIN, GPIO_OUT);
    gpio_put(I2S_DIN_PIN, 0);

    /* PWM slice 6 driving both GP28 (chan A = left) and GP27 (chan B =
     * right) at the same carrier (~122 kHz, 11-bit). Both channels
     * receive the same mono sample so audio comes out of both jack
     * channels. */
    gpio_set_function(AUDIO_L_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_R_PIN, GPIO_FUNC_PWM);
    audio_pwm_slice = pwm_gpio_to_slice_num(AUDIO_L_PIN);   /* slice 6 */
    pwm_set_clkdiv (audio_pwm_slice, 1.0f);
    pwm_set_wrap   (audio_pwm_slice, PWM_WRAP);
    pwm_set_chan_level(audio_pwm_slice, PWM_CHAN_A, PWM_WRAP / 2);  /* centre = silence */
    pwm_set_chan_level(audio_pwm_slice, PWM_CHAN_B, PWM_WRAP / 2);
    pwm_set_enabled(audio_pwm_slice, true);

    /* Initialise the ring buffer to centre = silence on both channels.
     * Each entry packs the same mid-PWM value into both halves of the
     * slice CC register. */
    uint32_t silence = ((uint32_t)(PWM_WRAP / 2) << 16) | (uint32_t)(PWM_WRAP / 2);
    for (int i = 0; i < DMA_BUF_SAMPLES; i++) dma_buf[i] = silence;

    /* DMA pacing timer 0: freq = sys_clk * X/Y. X=1, so Y = sys_clk /
     * SAMPLE_RATE rounded. At 250 MHz / 11025 Hz this gives Y=22676 →
     * 11025.66 Hz, audibly indistinguishable from 11025. Computing from
     * the live clock means the same code works for both RP2040 @ 250 MHz
     * and RP2350 at whatever clock main.c picks. */
    uint32_t sys_hz   = clock_get_hz(clk_sys);
    uint32_t divisor  = (sys_hz + (SAMPLE_RATE / 2)) / SAMPLE_RATE;
    dma_hw->timer[0]  = (1u << 16) | (divisor & 0xFFFFu);

    /* Use an explicit high DMA channel so we can't race with scanvideo on
     * core 1, which claims channels 0..2 (and optionally 3..5 depending on
     * plane count) inside scanvideo_setup. dma_claim_unused_channel from
     * core 0 *before* core 1 finishes its setup could otherwise steal one
     * of those, after which scanvideo's own claim panics, video never
     * starts, and the engine's wait_for_vblank loop spins forever. */
    audio_dma_ch = 11;
    dma_channel_claim(audio_dma_ch);
    dma_channel_config c = dma_channel_get_default_config(audio_dma_ch);
    /* 32-bit writes — one DMA word == both CC channels (A in low half =
     * GP28, B in high half = GP27). */
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment (&c, true);
    channel_config_set_write_increment(&c, false);
    /* Ring on the READ address. size_bits=13 wraps every 2^13 = 8192
     * bytes = 2048 uint32_t entries. dma_buf is aligned to 8192. */
    channel_config_set_ring (&c, false, 13);
    channel_config_set_dreq (&c, DREQ_DMA_TIMER0);

    dma_channel_configure(audio_dma_ch, &c,
        &pwm_hw->slice[audio_pwm_slice].cc,  /* whole 32-bit CC register */
        dma_buf,                              /* read addr */
        0xFFFFFFFFu,                          /* transfer count: effectively forever */
        true);                                /* start now */

    last_filled = -1;
    printf("[sound] PWM audio @ %d Hz, GP%d+GP%d, ring=%d samples\n",
           SAMPLE_RATE, AUDIO_L_PIN, AUDIO_R_PIN, DMA_BUF_SAMPLES);
    return true;
}

bool sound_deinit(void)
{
    if (nosound_mode) return true;
    dma_channel_abort(audio_dma_ch);
    pwm_set_enabled(audio_pwm_slice, false);
    return true;
}

bool sound_mod_play(int mod)
{
    if (nosound_mode) return true;
    return stream_open(&music, mod, /*loop=*/false);
}

bool sound_mod_stop(void)
{
    music.active = false;
    return true;
}

bool sound_mp3_play(int mp3)  { return sound_mod_play(mp3); }
bool sound_mp3_stop(void)     { return sound_mod_stop(); }

bool sound_sample_play(int sample_idx)
{
    if (nosound_mode) return true;
    return stream_open(&sample, sample_idx, /*loop=*/false);
}

void __not_in_flash_func(sound_update)(void)
{
    if (nosound_mode) return;

    /* Find which half of the ring buffer the DMA is currently reading.
     * dma_buf is ring-aligned so the read addr stays inside dma_buf.
     * Entries are uint32_t (4 bytes) now that we write both PWM
     * channels per DMA word. */
    uint32_t read_addr = (uint32_t)dma_hw->ch[audio_dma_ch].read_addr;
    int      offset    = (int)(((read_addr - (uint32_t)dma_buf) / sizeof(uint32_t)) % DMA_BUF_SAMPLES);
    int      current_half = (offset < DMA_BUF_HALF) ? 0 : 1;
    int      target_half  = 1 - current_half;

    if (target_half != last_filled) {
        refill_half(target_half);
        last_filled = target_half;
    }
}
