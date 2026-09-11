#pragma once
#include <stdint.h>
#define WIDTH 320
#define HEIGHT 240
#define SAMPLE_RATE 24000u
#define DURATION_SAMPLES (80u*SAMPLE_RATE)
#define DURATION_SECONDS 80.0f
#ifdef PICO_BUILD
#include "pico.h"
#define HOT(name) __not_in_flash_func(name)
#else
#define HOT(name) name
#endif
void demo_init(void);
void demo_render(uint8_t *page,uint32_t sample);
extern uint16_t demo_palette[256];
unsigned demo_triangles(void);
typedef struct {uint32_t spans,tiles,prepare,draw;uint8_t section;} demo_stats_t;
void demo_stats(demo_stats_t *out);
void synth_init(void);
void synth_render(int16_t*,unsigned);
uint32_t synth_position(void);
void synth_seek(uint32_t);
int synth_hash_latch(uint32_t*,uint32_t*);
uint32_t synth_final_hash(void);
static inline int clampi(int a,int lo,int hi){return a<lo?lo:a>hi?hi:a;}
static inline uint16_t rgb(int r,int g,int b){return (r>>3)|((g&248)<<3)|((b&248)<<8);}
static inline int red(uint16_t p){return (p&31)<<3;}
static inline int green(uint16_t p){return ((p>>6)&31)<<3;}
static inline int blue(uint16_t p){return ((p>>11)&31)<<3;}
static inline uint32_t demo_frame_hash(const uint8_t *p){uint32_t h=2166136261u;for(unsigned i=0;i<WIDTH*HEIGHT;i++)h=(h^demo_palette[p[i]])*16777619u;return h;}
