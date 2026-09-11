/* HELION on the board. Phase's loop, with the telemetry the score has to be
 * judged by: render cost, frame rate, the audio ring's fill, underruns, what
 * audio_pump() costs core 1, and the synth's per-second hash latch so the
 * device's samples can be diffed against the host's. -- Overscan
 */
#include "device.h"
#include "synth.h"
#include "accelerator.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include <stdarg.h>
#include <stdio.h>

/* A panic you can still talk to (COLOSSUS main.c, same reasoning).
 *
 * The SDK's panic ends in `bkpt #0`; with no debugger attached that escalates
 * to a HardFault whose handler spins at priority -1 with every interrupt
 * masked. USB dies with it: the board stays enumerated but unreadable and
 * `picotool reboot -f -u` cannot reach it, so it needs a human to unplug the
 * cable. Spinning in sleep_ms() instead leaves interrupts on, pico_stdio_usb
 * keeps servicing CDC, the message is readable and the board is reflashable.
 *
 * (The SDK's panic() is `naked` and pushes lr before branching here, so a
 * vararg that travelled on the stack -- the fifth onwards -- would be read
 * four bytes off. Nothing in this tree panics with that many arguments.)
 */
void __attribute__((noreturn)) helion_panic(const char *fmt, ...)
{
    puts("\n*** PANIC ***");
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        puts("");
    }
    printf("PANIC audio=%lu fill=%u under=%lu\n",
           (unsigned long)audio_position(), audio_min_fill(),
           (unsigned long)audio_underruns());
    for (;;) sleep_ms(250);
}

/* Phase's seven chapters, by bar. */
static int section_of(uint32_t bar)
{
    return bar < 8 ? 0 : bar < 24 ? 1 : bar < 40 ? 2 : bar < 56 ? 3 : bar < 64 ? 4 : bar < 72 ? 5 : 6;
}

int main(void){
    vreg_set_voltage(VREG_VOLTAGE_1_20);sleep_ms(10);set_sys_clock_khz(300000,true);
    stdio_init_all();
    /* PELAGIC lost its BOOT line and its first two telemetry windows to USB
     * enumeration, which is also why nobody had seen the SIO self-test print.
     * Wait for CDC, but bounded: a board on bare power (no data host) is
     * delayed 2 s at most and then plays exactly as before. */
    for(int i=0;i<200&&!stdio_usb_connected();i++)sleep_ms(10);
    printf("\nBOOT HELION dma=%d interp=%d sys=%lu Hz\n",
           HELION_DMA,HELION_INTERP,(unsigned long)clock_get_hz(clk_sys));
    demo_init();
    /* demo_init() already ran accelerator_selftest() against the real SIO
     * registers and panics on failure, so reaching this line means it passed;
     * run it once more so the pass is *printed* rather than inferred. It is
     * side-effect free apart from INTERP1's shift/mask config, which every
     * raster path sets with texture_config() before its first span. */
    printf("SELFTEST accelerator=%d (%s)\n",accelerator_selftest(),
           HELION_INTERP?"real SIO registers":"software model");
    audio_init();video_init();audio_start();
    unsigned count=0,worst=0,best=~0u;uint64_t report=time_us_64(),cost=0;
    uint32_t p_pumps=0,p_cycles=0,p_sample=0,frames_total=0;
    uint32_t worst_all_us=0;
    video_prof(&p_pumps,&p_cycles,NULL,NULL);
    while(audio_position()<DURATION_SAMPLES){
        uint64_t before=time_us_64();uint32_t sample=audio_position();demo_render(video_back(),sample);
        unsigned us=(unsigned)(time_us_64()-before);cost+=us;if(us>worst)worst=us;if(us<best)best=us;count++;
        video_present();uint64_t now=time_us_64();
        if(now-report>=1000000){
            uint32_t pumps,cycles,wcy,wall;video_prof(&pumps,&cycles,&wcy,&wall);
            uint32_t dn=pumps-p_pumps,dc=cycles-p_cycles,ds=sample-p_sample;
            uint64_t dt=now-report;unsigned n=count?count:1;
            uint32_t mean=(uint32_t)(cost/n),bar=sample/(BEAT_SAMPLES*4);
            frames_total+=count;
            if(worst>worst_all_us)worst_all_us=worst;
            /* Token spellings follow COLOSSUS's telemetry line so
             * tools/serial_read.py parses this unchanged. */
            printf("T t=%lu.%lu bar=%lu sec=%d dma=%d interp=%d"
                   " | render ms %lu.%02lu/%lu.%02lu/%lu.%02lu | fps %lu.%lu | tri %lu"
                   " | fill %u win %lu under %lu"
                   " | synth %lu cy/pump %lu.%02lu Mcy/s %lu cy/sample"
                   " | pump worst %lu.%02lu us peak %lu.%02lu us",
                (unsigned long)(sample/SAMPLE_RATE),(unsigned long)(sample%SAMPLE_RATE*10/SAMPLE_RATE),
                (unsigned long)bar,section_of(bar),HELION_DMA,HELION_INTERP,
                (unsigned long)(best/1000),(unsigned long)(best%1000/10),
                (unsigned long)(mean/1000),(unsigned long)(mean%1000/10),
                (unsigned long)(worst/1000),(unsigned long)(worst%1000/10),
                (unsigned long)((uint64_t)count*1000000u/dt),(unsigned long)((uint64_t)count*10000000u/dt%10),
                (unsigned long)demo_triangles(),
                audio_min_fill(),(unsigned long)audio_min_window(),(unsigned long)audio_underruns(),
                (unsigned long)(dn?dc/dn:0),
                (unsigned long)((uint64_t)dc/dt),(unsigned long)((uint64_t)dc*100/dt%100),
                (unsigned long)(ds?dc/ds:0),
                (unsigned long)(wcy/300),(unsigned long)(wcy%300*100/300),
                (unsigned long)(wall/300),(unsigned long)(wall%300*100/300));
            uint32_t hp=0,hv=0;
            if(synth_hash_latch(&hp,&hv))printf(" | AHASH s=%lu %08lx",(unsigned long)hp,(unsigned long)hv);
            DemoProfile dp=demo_profile();
            printf(" | last_us prep=%lu wait=%lu field=%lu mesh=%lu other=%lu texels=%u",
                (unsigned long)dp.prepare,(unsigned long)dp.wait,(unsigned long)dp.field,
                (unsigned long)dp.mesh,(unsigned long)dp.other,demo_texels());
            printf("\n");
            count=0;cost=0;worst=0;best=~0u;report=now;p_pumps=pumps;p_cycles=cycles;p_sample=sample;
        }
    }
    demo_render(video_back(),DURATION_SAMPLES);video_present();
    if(worst>worst_all_us)worst_all_us=worst;
    {
        uint32_t pumps,cycles,wcy,wall;video_prof(&pumps,&cycles,&wcy,&wall);
        printf("DONE frames %lu | worst render %lu.%02lu ms | under %lu | fill %u | peak pump %lu.%02lu us"
               " | synth %lu cy/pump %lu cy/sample | audio %lu\n",
            (unsigned long)(frames_total+count),
            (unsigned long)(worst_all_us/1000),(unsigned long)(worst_all_us%1000/10),
            (unsigned long)audio_underruns(),audio_min_fill(),
            (unsigned long)(wall/300),(unsigned long)(wall%300*100/300),
            (unsigned long)(pumps?cycles/pumps:0),
            (unsigned long)(audio_position()?cycles/audio_position():0),
            (unsigned long)audio_position());
    }
    while(true)tight_loop_contents();
}
