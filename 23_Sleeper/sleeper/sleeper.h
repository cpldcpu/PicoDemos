/* sleeper.h -- the contract between the platform, the renderer and the score.
 *
 * SLEEPER / LATENT / 2026. Phosphor owns this header and song.h; they change
 * only by agreement. Overscan owns everything on the platform side of it
 * (main.c, video.c, audio_pwm.c, the build, host/, tools/) and the renderer
 * (render*.c, world*.c, lights.c, board.c, the assets pipeline). Phosphor
 * owns song.c, synth.c, synth.h and tools/song_*.
 *
 * The picture is a function of the audio clock. demo_render() draws the
 * moment `sample` -- the absolute stereo frame the DAC is playing -- into a
 * 320x240 page in the VGA DAC's packing. It must be callable for any sample
 * in any order and produce the same page: camera, lights, the board and the
 * dream derive from `sample` and from the timetable (song.h), never from the
 * previous frame. That is what lets a skipped frame skip, a host seek land,
 * and check.c prove it.
 */
#ifndef SLEEPER_H
#define SLEEPER_H
#include <stdint.h>

#define WIDTH  320
#define HEIGHT 240

#define SAMPLE_RATE      24000u
#define STEP_SAMPLES     2250u                      /* a 16th at 160 BPM      */
#define BEAT_SAMPLES     9000u
#define BAR_SAMPLES      36000u
#define SONG_BARS        128u
#define DURATION_SAMPLES (BAR_SAMPLES * SONG_BARS)  /* 4,608,000 = 3:12.0     */
#define DURATION_SECONDS 192.f

#ifdef PICO_BUILD
#include "pico.h"
#define HOT(name) __not_in_flash_func(name)
#else
#define HOT(name) name
#endif

/* Renderer (Overscan). */
void demo_init(void);                               /* once, after synth_init() */
void demo_render(uint16_t *page, uint32_t sample);  /* the whole page, every call */

typedef struct {
    uint32_t lights;     /* lights drawn (core+halo+streak)                 */
    uint32_t spans;      /* textured/silhouette spans                       */
    uint32_t prepare, world, lights_us, board_us, other;  /* last frame, us */
    uint8_t  shot, world_id, section;               /* what was drawn        */
} demo_stats_t;
void demo_stats(demo_stats_t *out);

/* Synth (Phosphor). Stereo interleaved int16 at 24 kHz, pull model; the
 * output of synth_render() must not depend on the block size. */
void     synth_init(void);
void     synth_render(int16_t *stereo, unsigned frames);
uint32_t synth_position(void);
void     synth_seek(uint32_t sample);
/* FNV-1a over every emitted int16, latched every second (host/device diff).
 * Returns 1 once per new latch, then 0 until the next one. */
int      synth_hash_latch(uint32_t *position, uint32_t *hash);

/* The Pico VGA DAC is RGB555 with an unused bit 5, red in the low bits. */
static inline int clampi(int a, int lo, int hi) { return a < lo ? lo : a > hi ? hi : a; }
static inline uint16_t rgb(int r, int g, int b) {
    return (uint16_t)((clampi(r, 0, 255) >> 3) | ((clampi(g, 0, 255) & 248) << 3) | ((clampi(b, 0, 255) & 248) << 8));
}
static inline int red(uint16_t p)   { return (p & 31) << 3; }
static inline int green(uint16_t p) { return ((p >> 6) & 31) << 3; }
static inline int blue(uint16_t p)  { return (p >> 11) << 3; }
#endif
