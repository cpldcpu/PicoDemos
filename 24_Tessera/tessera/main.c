/* TESSERA platform. Adapted by Phase from Overscan's SLEEPER transport. */
#include "device.h"
#include "song.h"
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
 */
void __attribute__((noreturn)) tessera_panic(const char *fmt, ...)
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

int main(void)
{
    vreg_set_voltage(VREG_VOLTAGE_1_20); sleep_ms(10); set_sys_clock_khz(300000, true);
    stdio_init_all();
    
    for (int i = 0; i < 200 && !stdio_usb_connected(); i++) sleep_ms(10);
    /* Windows CDC opens/DTRs the port before its input-buffer purge. Let
     * that settle so BOOT and SELFTEST survive, not merely the later log. */
    if (stdio_usb_connected()) sleep_ms(300);
    printf("\nBOOT TESSERA dma=%d interp=%d sys=%lu Hz\n",
           TESSERA_DMA, TESSERA_INTERP, (unsigned long)clock_get_hz(clk_sys));
    song_init();
    demo_init();
    /* demo_init() already ran accelerator_selftest() against the real SIO
     * registers and panics on failure, so reaching this line means it passed;
     * run it once more so the pass is *printed* rather than inferred. It is
     * side-effect free apart from INTERP1's shift/mask config, which every
     * raster path sets with texture_config() before its first span. */
    printf("SELFTEST accelerator=%d (%s)\n", accelerator_selftest(),
           TESSERA_INTERP ? "real SIO registers" : "software model");
    /* Before scanout owns either page, compare the full hardware renderer
     * at fixed score positions against the desktop reference. */
    for(unsigned t=0;t<=150;t+=30){
        demo_render(video_back(),t*SAMPLE_RATE);
        printf("VHASH s=%lu %08lx\n",(unsigned long)(t*SAMPLE_RATE),(unsigned long)demo_frame_hash(video_back()));
    }
    audio_init(); video_init();
    
    demo_render(video_back(), 0); video_present();
    demo_render(video_back(), 0); video_present();
    audio_start();
    video_arm();
    unsigned count = 0, worst = 0, best = ~0u; uint64_t report = time_us_64(), cost = 0;
    uint32_t p_pumps = 0, p_cycles = 0, p_sample = 0, frames_total = 0;
    uint32_t worst_all_us = 0, over = 0; uint64_t cycles_total = 0;
    video_prof(&p_pumps, &p_cycles, NULL, NULL);
    while (audio_position() < DURATION_SAMPLES) {
        uint64_t before = time_us_64(); uint32_t sample = audio_position(); demo_render(video_back(), sample);
        unsigned us = (unsigned)(time_us_64() - before);
        cost += us; if (us > worst) worst = us; if (us < best) best = us; count++;
        if (us > 16000u) over++;
        video_present(); uint64_t now = time_us_64();
        if (now - report >= 1000000) {
            uint32_t pumps, cycles, wcy, wall; video_prof(&pumps, &cycles, &wcy, &wall);
            uint32_t dn = pumps - p_pumps, dc = cycles - p_cycles, ds = sample - p_sample;
            cycles_total += dc;
            uint64_t dt = now - report; unsigned n = count ? count : 1;
            uint32_t mean = (uint32_t)(cost / n), bar = sample / BAR_SAMPLES;
            demo_stats_t st; demo_stats(&st);
            uint32_t vfields = 0, vfirst = 0, vboot = 0; video_fields(&vfields, &vfirst, &vboot);
            frames_total += count;
            if (worst > worst_all_us) worst_all_us = worst;
            
            printf("T t=%lu.%lu bar=%lu sec=%d dma=%d interp=%d"
                   " | render ms %lu.%02lu/%lu.%02lu/%lu.%02lu | fps %lu.%lu | spans %lu"
                   " | repeat %lu | over %lu | fields %lu firstrep %lu boot %lu"
                   " | fill %u win %lu under %lu"
                   " | synth %lu cy/pump %lu.%02lu Mcy/s %lu cy/sample"
                   " | pump worst %lu.%02lu us peak %lu.%02lu us",
                (unsigned long)(sample / SAMPLE_RATE), (unsigned long)(sample % SAMPLE_RATE * 10 / SAMPLE_RATE),
                (unsigned long)bar, song_section(bar), TESSERA_DMA, TESSERA_INTERP,
                (unsigned long)(best / 1000), (unsigned long)(best % 1000 / 10),
                (unsigned long)(mean / 1000), (unsigned long)(mean % 1000 / 10),
                (unsigned long)(worst / 1000), (unsigned long)(worst % 1000 / 10),
                (unsigned long)((uint64_t)count * 1000000u / dt), (unsigned long)((uint64_t)count * 10000000u / dt % 10),
                (unsigned long)st.spans,
                (unsigned long)video_repeats(), (unsigned long)over,
                (unsigned long)vfields, (unsigned long)vfirst, (unsigned long)vboot,
                audio_min_fill(), (unsigned long)audio_min_window(), (unsigned long)audio_underruns(),
                (unsigned long)(dn ? dc / dn : 0),
                (unsigned long)((uint64_t)dc / dt), (unsigned long)((uint64_t)dc * 100 / dt % 100),
                (unsigned long)(ds ? dc / ds : 0),
                (unsigned long)(wcy / 300), (unsigned long)(wcy % 300 * 100 / 300),
                (unsigned long)(wall / 300), (unsigned long)(wall % 300 * 100 / 300));
            uint32_t hp = 0, hv = 0;
            if (synth_hash_latch(&hp, &hv)) printf(" | AHASH s=%lu %08lx", (unsigned long)hp, (unsigned long)hv);
            printf(" | prep_us=%lu draw_us=%lu tiles=%lu | missed %lu bootmiss %lu", (unsigned long)st.prepare, (unsigned long)st.draw, (unsigned long)st.tiles, (unsigned long)video_missed(), (unsigned long)video_missed_boot());
            printf("\n");
            count = 0; cost = 0; worst = 0; best = ~0u; report = now; p_pumps = pumps; p_cycles = cycles; p_sample = sample;
        }
    }
    demo_render(video_back(), DURATION_SAMPLES); video_present();
    {uint32_t hp,hv;if(synth_hash_latch(&hp,&hv))printf("TAIL AHASH s=%lu %08lx\n",(unsigned long)hp,(unsigned long)hv);}
    if (worst > worst_all_us) worst_all_us = worst;
    {
        uint32_t pumps, cycles, wcy, wall; video_prof(&pumps, &cycles, &wcy, &wall);
        uint32_t vfields = 0, vfirst = 0, vboot = 0; video_fields(&vfields, &vfirst, &vboot);
        printf("DONE frames %lu | worst render %lu.%02lu ms | repeat %lu | over %lu"
               " | fields %lu firstrep %lu boot %lu"
               " | under %lu | fill %u | peak pump %lu.%02lu us"
               " | synth %lu cy/pump %lu cy/sample | audio %lu\n",
            (unsigned long)(frames_total + count),
            (unsigned long)(worst_all_us / 1000), (unsigned long)(worst_all_us % 1000 / 10),
            (unsigned long)video_repeats(), (unsigned long)over,
            (unsigned long)vfields, (unsigned long)vfirst, (unsigned long)vboot,
            (unsigned long)audio_underruns(), audio_min_fill(),
            (unsigned long)(wall / 300), (unsigned long)(wall % 300 * 100 / 300),
            (unsigned long)(pumps ? (cycles_total + (uint32_t)(cycles-p_cycles)) / pumps : 0),
            (unsigned long)(audio_position() ? (cycles_total + (uint32_t)(cycles-p_cycles)) / audio_position() : 0),
            (unsigned long)audio_position());
        printf("FINAL missed %lu bootmiss %lu | ENDHASH s=%lu %08lx\n", (unsigned long)video_missed(), (unsigned long)video_missed_boot(), (unsigned long)DURATION_SAMPLES, (unsigned long)synth_final_hash());
    }
    while (true) tight_loop_contents();
}
