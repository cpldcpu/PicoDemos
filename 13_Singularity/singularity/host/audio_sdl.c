/* SDL2 implementation of audio.h for desktop testing.
 *
 * Decodes the same music.qoa baked into the device build (via the same
 * music_qoa.S incbin — mingw GAS handles it identically to arm-as).
 * Pre-decodes the entire stream into one malloc'd int16 buffer up front,
 * then a tiny SDL_OpenAudioDevice callback copies samples to the audio
 * card.
 *
 * Pre-decode (instead of streaming like the device) for simplicity —
 * 246 s × 22050 Hz × 2 B mono = 10.9 MB, trivial on desktop.
 *
 * audio_now_ms() returns wall-clock since audio_start(), same as the
 * device — both clocks are sys-stable so visuals stay in sync with the
 * music either way.
 */

#include "../audio.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define QOA_IMPLEMENTATION
#define QOA_NO_STDIO
#include "../qoa.h"

extern const uint8_t _music_qoa_start[];
extern const uint8_t _music_qoa_end[];

static int16_t       *g_samples       = NULL;     /* full decoded mono PCM */
static uint32_t       g_total_frames  = 0;        /* sample frames in g_samples */
static unsigned       g_sample_rate   = 22050;
static volatile uint32_t g_play_pos    = 0;       /* next frame the callback will emit */
static SDL_AudioDeviceID g_dev = 0;

/* The demo clock is wall-time-since-audio_start plus a signed offset
 * that audio_seek_ms() adjusts. Using a separate offset (instead of
 * sliding g_anchor_ms backwards) avoids the underflow that hits when
 * we seek to a time later than the SDL clock has yet reached — e.g.
 * pressing SPACE in the first few seconds to skip past the title. */
static int      g_started       = 0;
static uint64_t g_anchor_ms     = 0;     /* SDL_GetTicks64() at audio_start() */
static int64_t  g_offset_ms     = 0;     /* added to wall-elapsed */

/* SDL audio callback. SDL gives us a byte count; we map to int16 mono
 * samples and dump from g_samples[g_play_pos++]. */
static void audio_cb(void *userdata, Uint8 *stream, int len_bytes)
{
    (void)userdata;
    int16_t *out = (int16_t *)stream;
    int      need = len_bytes / sizeof(int16_t);

    uint32_t pos = g_play_pos;
    for (int i = 0; i < need; i++) {
        out[i] = (pos < g_total_frames) ? g_samples[pos++] : 0;
    }
    g_play_pos = pos;
}

void audio_init(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "audio_sdl: SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
        return;
    }

    /* --- decode the whole QOA stream into memory ---------------------- */
    qoa_desc desc = {0};
    unsigned int header_size = qoa_decode_header(_music_qoa_start,
        (int)(_music_qoa_end - _music_qoa_start), &desc);
    if (header_size == 0) {
        fprintf(stderr, "audio_sdl: qoa_decode_header failed\n");
        return;
    }
    g_sample_rate  = desc.samplerate;
    g_total_frames = desc.samples;
    printf("audio_sdl: QOA %u Hz, %u ch, %u frames (%.2f s)\n",
           desc.samplerate, desc.channels, desc.samples,
           (double)desc.samples / desc.samplerate);

    /* Allocate the full PCM buffer (mono only — channels==1 matches our
     * device build; if ever stereo, would need ×channels). */
    if (desc.channels != 1) {
        fprintf(stderr, "audio_sdl: only mono supported, got %u channels\n", desc.channels);
        return;
    }
    g_samples = (int16_t *)malloc((size_t)g_total_frames * sizeof(int16_t));
    if (!g_samples) {
        fprintf(stderr, "audio_sdl: malloc(%u B) failed\n",
                (unsigned)(g_total_frames * sizeof(int16_t)));
        return;
    }

    /* Decode frame-by-frame. */
    const uint8_t *src    = _music_qoa_start + header_size;
    uint32_t       remain = (uint32_t)(_music_qoa_end - _music_qoa_start) - header_size;
    uint32_t       out_pos = 0;
    int16_t        frame[QOA_FRAME_LEN];
    while (out_pos < g_total_frames && remain > 0) {
        unsigned int fl = 0;
        unsigned int consumed = qoa_decode_frame(src, remain, &desc, frame, &fl);
        if (consumed == 0 || fl == 0) break;
        if (out_pos + fl > g_total_frames) fl = g_total_frames - out_pos;
        memcpy(&g_samples[out_pos], frame, fl * sizeof(int16_t));
        out_pos += fl;
        src     += consumed;
        remain  -= consumed;
    }
    printf("audio_sdl: decoded %u frames\n", out_pos);

    /* --- open SDL audio device ---------------------------------------- */
    SDL_AudioSpec want = {0}, have = {0};
    want.freq     = (int)g_sample_rate;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    want.samples  = 1024;
    want.callback = audio_cb;
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_dev == 0) {
        fprintf(stderr, "audio_sdl: SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }
    /* leave paused until audio_start() */
}

void audio_start(void)
{
    g_play_pos   = 0;
    g_anchor_ms  = SDL_GetTicks64();
    g_offset_ms  = 0;
    g_started    = 1;
    if (g_dev) SDL_PauseAudioDevice(g_dev, 0);
}

void audio_pump(void) { /* SDL audio runs on its own thread — nothing to do here. */ }

void audio_seek_ms(uint32_t target_ms)
{
    /* Map ms → sample frame index and snap the playback pointer. The
     * SDL audio callback reads g_play_pos atomically so this is safe
     * from the main thread.
     *
     * For the demo clock: we want audio_now_ms() to return target_ms
     * starting now, then keep counting up. Compute the offset that
     * makes that true: offset = target − wall_elapsed_since_anchor. */
    uint64_t target_frames = (uint64_t)target_ms * g_sample_rate / 1000ULL;
    if (target_frames > g_total_frames) target_frames = g_total_frames;
    g_play_pos = (uint32_t)target_frames;

    int64_t elapsed = (int64_t)SDL_GetTicks64() - (int64_t)g_anchor_ms;
    g_offset_ms = (int64_t)target_ms - elapsed;
}

uint32_t audio_now_ms(void)
{
    if (!g_started) return 0;
    int64_t elapsed = (int64_t)SDL_GetTicks64() - (int64_t)g_anchor_ms;
    int64_t t = elapsed + g_offset_ms;
    return (t < 0) ? 0 : (uint32_t)t;
}
