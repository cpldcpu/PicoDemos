/* Scanvideo transport adapted from the existing LATENT backends.
 * Two pages with an explicit scanline-zero ownership handshake. A vblank wait
 * alone is insufficient: scanvideo queues lines ahead of the physical beam.
 * Core 0 cannot reuse a page until core 1 has latched the replacement.
 * Following COLOSSUS's scanout optimization, yscale=2 repeats each generated
 * row in hardware; aligned 32-bit stores duplicate horizontal pixels.
 */
#include "device.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "hardware/structs/systick.h"
static uint16_t pages[2][WIDTH*HEIGHT] __attribute__((aligned(4)));
/* Core 1's own SysTick, free-running off the processor clock: 24 bits at
 * 300 MHz wraps every 56 ms, far longer than one audio_pump() and far
 * shorter than nothing. Core 0 reads the totals; the window worst is
 * read-and-rearm, so a collision with core 1 can lose one sample of it and
 * never inflates it. */
#define SYSTICK_MASK 0x00FFFFFFu
static volatile uint32_t prof_pumps,prof_cycles,prof_worst,prof_worst_all;
static inline uint32_t cyc(void){return systick_hw->cvr;}
static inline uint32_t cyc_since(uint32_t t){return (t-systick_hw->cvr)&SYSTICK_MASK;}
void video_prof(uint32_t *pumps,uint32_t *cycles,uint32_t *worst_window,uint32_t *worst_all){
    if(pumps)*pumps=__atomic_load_n(&prof_pumps,__ATOMIC_RELAXED);
    if(cycles)*cycles=__atomic_load_n(&prof_cycles,__ATOMIC_RELAXED);
    if(worst_all)*worst_all=__atomic_load_n(&prof_worst_all,__ATOMIC_RELAXED);
    if(worst_window){*worst_window=__atomic_load_n(&prof_worst,__ATOMIC_RELAXED);
        __atomic_store_n(&prof_worst,0,__ATOMIC_RELAXED);}
}
static int back=1;
static volatile int pending=-1,displayed=0,ready=0;
static void HOT(scanout)(void){
    systick_hw->rvr=SYSTICK_MASK;systick_hw->cvr=0;systick_hw->csr=0x5u; /* ENABLE | core clock */
    scanvideo_setup(&vga_mode_320x240_60);scanvideo_timing_enable(true);
    __atomic_store_n(&ready,1,__ATOMIC_RELEASE);
    int front=0;
    while(true){
        struct scanvideo_scanline_buffer *b=scanvideo_begin_scanline_generation(true);
        unsigned y=scanvideo_scanline_number(b->scanline_id);
        if(y==0){
            int p=__atomic_load_n(&pending,__ATOMIC_ACQUIRE);
            if(p>=0){front=p;__atomic_store_n(&displayed,front,__ATOMIC_RELEASE);__atomic_store_n(&pending,-1,__ATOMIC_RELEASE);}
        }
        const uint16_t *src=pages[front]+y*WIDTH;
        uint16_t *out=(uint16_t*)b->data;
        out[0]=COMPOSABLE_RAW_RUN;out[1]=src[0];out[2]=638;
        out[3]=src[0]; uint32_t *pairs=(uint32_t *)(out+4);
        for(int x=1;x<WIDTH;x++){uint32_t v=src[x];pairs[x-1]=v|(v<<16);}
        out[642]=0;out[643]=COMPOSABLE_EOL_ALIGN;b->data_used=322;b->status=SCANLINE_OK;
        scanvideo_end_scanline_generation(b);
        const uint32_t t0=cyc();audio_pump();const uint32_t dp=cyc_since(t0);
        __atomic_store_n(&prof_pumps,prof_pumps+1u,__ATOMIC_RELAXED);
        __atomic_store_n(&prof_cycles,prof_cycles+dp,__ATOMIC_RELAXED);
        if(dp>prof_worst)__atomic_store_n(&prof_worst,dp,__ATOMIC_RELAXED);
        if(dp>prof_worst_all)__atomic_store_n(&prof_worst_all,dp,__ATOMIC_RELAXED);
    }
}
void video_init(void){multicore_launch_core1(scanout);while(!__atomic_load_n(&ready,__ATOMIC_ACQUIRE))tight_loop_contents();}
uint16_t *video_back(void){return pages[back];}
void video_present(void){
    __atomic_store_n(&pending,back,__ATOMIC_RELEASE);
    while(__atomic_load_n(&pending,__ATOMIC_ACQUIRE)>=0)tight_loop_contents();
    back=1-__atomic_load_n(&displayed,__ATOMIC_ACQUIRE);
}
