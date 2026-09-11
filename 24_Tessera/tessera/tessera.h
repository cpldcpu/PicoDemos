/* TESSERA / LATENT / 2026. Phase (GPT-6 Astra). */
#ifndef TESSERA_H
#define TESSERA_H
#include <stdint.h>
#define WIDTH 320
#define HEIGHT 240
#define SAMPLE_RATE 24000u
#define STEP_SAMPLES 2400u
#define BEAT_SAMPLES 9600u
#define BAR_SAMPLES 38400u
#define SONG_BARS 96u
#define DURATION_SAMPLES (BAR_SAMPLES * SONG_BARS)
#define DURATION_SECONDS 153.6f
#ifdef PICO_BUILD
#include "pico.h"
#define HOT(name) __not_in_flash_func(name)
#else
#define HOT(name) name
#endif
void demo_init(void);
void demo_render(uint16_t *page, uint32_t sample);
unsigned demo_triangles(void);
typedef struct { uint32_t spans, tiles, prepare, draw; uint8_t section; } demo_stats_t;
void demo_stats(demo_stats_t *out);
void synth_init(void);
void synth_render(int16_t *stereo, unsigned frames);
uint32_t synth_position(void);
void synth_seek(uint32_t sample);
int synth_hash_latch(uint32_t *position, uint32_t *hash);
uint32_t synth_final_hash(void);
static inline int clampi(int a,int lo,int hi){return a<lo?lo:a>hi?hi:a;}
static inline uint16_t rgb(int r,int g,int b){return (uint16_t)((clampi(r,0,255)>>3)|((clampi(g,0,255)&248)<<3)|((clampi(b,0,255)&248)<<8));}
static inline int red(uint16_t p){return (p&31)<<3;}
static inline int green(uint16_t p){return ((p>>6)&31)<<3;}
static inline int blue(uint16_t p){return ((p>>11)&31)<<3;}
static inline uint32_t demo_frame_hash(const uint16_t *p){uint32_t h=2166136261u;for(unsigned i=0;i<WIDTH*HEIGHT;i++)h=(h^p[i])*16777619u;return h;}
#endif
