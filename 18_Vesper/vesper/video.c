/* Scanvideo transport adapted from the existing LATENT backends.
 * Two pages with an explicit scanline-zero ownership handshake. A vblank wait
 * alone is insufficient: scanvideo queues lines ahead of the physical beam.
 * Core 0 cannot reuse a page until core 1 has latched the replacement.
 */
#include "device.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
static uint16_t pages[2][WIDTH*HEIGHT] __attribute__((aligned(4)));
static int back=1;
static volatile int pending=-1,displayed=0,ready=0;
static void HOT(scanout)(void){
    scanvideo_setup(&vga_mode_640x480_60);scanvideo_timing_enable(true);
    __atomic_store_n(&ready,1,__ATOMIC_RELEASE);
    int front=0;
    while(true){
        struct scanvideo_scanline_buffer *b=scanvideo_begin_scanline_generation(true);
        unsigned y=scanvideo_scanline_number(b->scanline_id);
        if(y==0){
            int p=__atomic_load_n(&pending,__ATOMIC_ACQUIRE);
            if(p>=0){front=p;__atomic_store_n(&displayed,front,__ATOMIC_RELEASE);__atomic_store_n(&pending,-1,__ATOMIC_RELEASE);}
        }
        const uint16_t *src=pages[front]+(y>>1)*WIDTH;
        uint16_t *out=(uint16_t*)b->data;
        out[0]=COMPOSABLE_RAW_RUN;out[1]=src[0];out[2]=638;
        for(int x=1;x<640;x++)out[x+2]=src[x>>1];
        out[642]=0;out[643]=COMPOSABLE_EOL_ALIGN;b->data_used=322;b->status=SCANLINE_OK;
        scanvideo_end_scanline_generation(b);audio_pump();
    }
}
void video_init(void){multicore_launch_core1(scanout);while(!__atomic_load_n(&ready,__ATOMIC_ACQUIRE))tight_loop_contents();}
uint16_t *video_back(void){return pages[back];}
void video_present(void){
    __atomic_store_n(&pending,back,__ATOMIC_RELEASE);
    while(__atomic_load_n(&pending,__ATOMIC_ACQUIRE)>=0)tight_loop_contents();
    back=1-__atomic_load_n(&displayed,__ATOMIC_ACQUIRE);
}
