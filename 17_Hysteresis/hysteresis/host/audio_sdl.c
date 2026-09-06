/* Host audio: the same synth, into an SDL audio device, pacing the picture.
 *
 * This exists so the demo can be reviewed as a demo. The WAV renderer proves
 * the music is deterministic and lets it be auditioned on its own, but whether
 * an impact lands with its blob is a question about the two together, and it
 * cannot be answered by listening to a file and watching a window separately.
 *
 * THE AUDIO CLOCK IS THE MASTER, which is worth being explicit about because it
 * inverts the usual arrangement. The video would otherwise be paced by the
 * monitor's vsync, and a 144 Hz or 50 Hz panel then plays the demo at 144 or 50
 * frames per second while the soundtrack still takes 210 seconds -- and since
 * every visual event is scheduled in frames (sim.h), the picture and the music
 * would slide apart by minutes. So the renderer's vsync is switched off when
 * audio is on and each frame waits for the sample counter instead.
 *
 * That is also what the device does, one layer down: score.h fixes 30 frames to
 * 11025 samples, and here the wait enforces the same ratio on a machine that
 * has no vblank worth trusting.
 */

#include <SDL2/SDL.h>
#include <stdio.h>

#include "../synth.h"
#include "../score.h"

static SDL_AudioDeviceID g_dev;
static volatile uint32_t g_filled;      /* samples handed to SDL */
static uint32_t          g_lead;        /* samples SDL holds before they sound */
static int               g_on;

int  hostaudio_active(void) { return g_on; }

static void SDLCALL fill(void *ud, Uint8 *stream, int len)
{
    (void)ud;
    const int n = len / (int)sizeof(int16_t);
    synth_render((int16_t *)stream, n);
    g_filled += (uint32_t)n;
}

void hostaudio_init(void)
{
    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof want);
    want.freq     = SYNTH_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 1;
    /* 1024 samples is 46 ms -- about three frames of lead, subtracted back out
     * in hostaudio_wait_frame so the picture is not early by that much. Smaller
     * buffers glitch under a window resize; larger ones make the sync test
     * measure the buffer. */
    want.samples  = 1024;
    want.callback = fill;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "audio: no SDL audio (%s); running silent\n", SDL_GetError());
        return;
    }
    g_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!g_dev) {
        fprintf(stderr, "audio: cannot open device (%s); running silent\n",
                SDL_GetError());
        return;
    }
    if (have.freq != SYNTH_RATE || have.channels != 1) {
        /* Refuse rather than resample. A resampled clock is a drifting clock,
         * and the whole point of this file is that the clock does not drift. */
        fprintf(stderr, "audio: got %d Hz %d ch, wanted %d Hz mono; running silent\n",
                have.freq, have.channels, SYNTH_RATE);
        SDL_CloseAudioDevice(g_dev);
        g_dev = 0;
        return;
    }

    g_lead = have.samples;
    synth_reset();
    g_on = 1;
    fprintf(stderr, "audio: %d Hz mono, %u-sample buffer, driving the frame clock\n",
            have.freq, g_lead);
}

/* Opened paused and started separately, so the window exists before sample zero
 * -- otherwise the first frames of music play against no picture, which is
 * exactly the misalignment this file is meant to be able to see. */
void hostaudio_start(void)
{
    if (g_on) SDL_PauseAudioDevice(g_dev, 0);
}

/* Block until the audio that should accompany `frame` has been reached.
 *
 * 30 frames per beat and 11025 samples per beat, so a frame is 367.5 samples --
 * hence the multiply-then-divide rather than a per-frame constant, which would
 * accumulate half a sample of error every frame and two and a half seconds over
 * the demo. */
void hostaudio_wait_frame(uint32_t frame)
{
    if (!g_on) return;

    const uint32_t want = (uint32_t)(((uint64_t)frame * SCORE_SAMPLES_PER_BEAT)
                                     / SCORE_FRAMES_PER_BEAT);
    for (;;) {
        const uint32_t filled = g_filled;
        const uint32_t heard  = filled > g_lead ? filled - g_lead : 0;
        if (heard >= want) return;
        const uint32_t behind = want - heard;
        /* Sleep in whole milliseconds, one less than needed, then spin the
         * remainder -- SDL_Delay's granularity is coarser than a frame. */
        const uint32_t ms = behind * 1000u / SYNTH_RATE;
        if (ms > 1) SDL_Delay(ms - 1);
        else        SDL_Delay(0);
    }
}

/* Restart the music from sample zero. Locked, because synth_reset() rewrites
 * the state the callback thread is reading -- and the callback is where every
 * sample in this program comes from. */
void hostaudio_reset(void)
{
    if (!g_on) return;
    SDL_LockAudioDevice(g_dev);
    synth_reset();
    g_filled = 0;
    SDL_UnlockAudioDevice(g_dev);
}

void hostaudio_shutdown(void)
{
    if (g_dev) { SDL_CloseAudioDevice(g_dev); g_dev = 0; }
    g_on = 0;
}
