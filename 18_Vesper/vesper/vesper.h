#ifndef VESPER_H
#define VESPER_H
#include <stdint.h>
#define WIDTH 320
#define HEIGHT 240
#define SAMPLE_RATE 24000
#define BEAT_SAMPLES 11250
#define DURATION_SAMPLES (BEAT_SAMPLES * 256u)
#define DURATION_SECONDS 120
#ifdef PICO_BUILD
#include "pico.h"
#define HOT(name) __not_in_flash_func(name)
#else
#define HOT(name) name
#endif
typedef struct { float beat, pulse, bar_phase; int bar, chapter; } Score;
Score score_at(uint32_t sample);
void demo_init(void);
void demo_render(uint16_t *pixels, uint32_t sample);
unsigned demo_triangles(void);
void synth_init(void);
void synth_render(int16_t *stereo, unsigned frames);
uint32_t synth_position(void);
void synth_seek(uint32_t sample);
/* The Pico VGA DAC is RGB555 with an unused bit 5, red in low bits. */
static inline int clampi(int a,int lo,int hi) {return a<lo?lo:a>hi?hi:a;}
static inline uint16_t rgb(int r,int g,int b) {
    return (uint16_t)((clampi(r,0,255)>>3)|((clampi(g,0,255)&248)<<3)|((clampi(b,0,255)&248)<<8));
}
static inline int red(uint16_t p){return (p&31)<<3;}
static inline int green(uint16_t p){return ((p>>6)&31)<<3;}
static inline int blue(uint16_t p){return (p>>11)<<3;}
#endif
