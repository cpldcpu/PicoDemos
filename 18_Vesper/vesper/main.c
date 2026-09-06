#include "device.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include <stdio.h>
int main(void){
    vreg_set_voltage(VREG_VOLTAGE_1_20);sleep_ms(10);set_sys_clock_khz(300000,true);
    stdio_init_all();demo_init();audio_init();video_init();audio_start();
    unsigned count=0,worst=0;uint64_t report=time_us_64(),cost=0;
    while(audio_position()<DURATION_SAMPLES){
        uint64_t before=time_us_64();uint32_t sample=audio_position();demo_render(video_back(),sample);
        unsigned us=(unsigned)(time_us_64()-before);cost+=us;if(us>worst)worst=us;count++;
        video_present();uint64_t now=time_us_64();
        if(now-report>=1000000){
            printf("VESPER t=%lu ms fps=%lu render_mean=%lu us worst=%u us triangles=%u audio_min=%u/1023\n",
                (unsigned long)(sample/24),(unsigned long)((uint64_t)count*1000000/(now-report)),
                (unsigned long)(cost/count),worst,demo_triangles(),audio_min_fill());
            count=0;cost=0;worst=0;report=now;
        }
    }
    demo_render(video_back(),DURATION_SAMPLES);video_present();
    while(true)tight_loop_contents();
}
