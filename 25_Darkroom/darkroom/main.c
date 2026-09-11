/* DARKROOM RP2350 transport: indexed VGA, PWM stereo and USB telemetry. */
#include "device.h"
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include <stdarg.h>
#include <stdio.h>

/* Keep USB interrupts alive after a fatal error so the diagnostic remains
 * readable and picotool can still put the board back into BOOTSEL. */
void __attribute__((noreturn)) darkroom_panic(const char *fmt, ...)
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

static void render_checkpoints(void)
{
    uint8_t *page = video_back();
    demo_init();
    for (unsigned seconds = 0; seconds <= DURATION_SECONDS; seconds += 10) {
        const uint32_t sample = seconds * SAMPLE_RATE;
        printf("VCHECK s=%lu begin\n", (unsigned long)sample);
        demo_render(page, sample);
        printf("VHASH s=%lu %08lx\n", (unsigned long)sample,
               (unsigned long)demo_frame_hash(page));
    }
    /* The renderer advances its reconstructed Amiga state to reach each
     * checkpoint. The live run must start from a clean sample-zero state. */
    demo_init();
}

int main(void)
{
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(300000, true);
    stdio_init_all();

    for (int i = 0; i < 200 && !stdio_usb_connected(); i++) sleep_ms(10);
    if (stdio_usb_connected()) sleep_ms(300);
    printf("\nBOOT DARKROOM sys=%lu Hz sample_rate=%u duration=%lu "
           "video=indexed8-rgb555 engine=native-software-blitter\n",
           (unsigned long)clock_get_hz(clk_sys), SAMPLE_RATE,
           (unsigned long)DURATION_SAMPLES);

    render_checkpoints();
    audio_init();
    video_init();

    demo_render(video_back(), 0);
    video_present();
    demo_render(video_back(), 0);
    video_present();
    audio_start();
    video_arm();

    unsigned count = 0, worst = 0, best = ~0u;
    uint64_t report = time_us_64(), cost = 0, cycles_total = 0;
    uint32_t p_pumps = 0, p_cycles = 0, p_sample = 0, frames_total = 0;
    uint32_t worst_all_us = 0, over = 0;
    video_prof(&p_pumps, &p_cycles, NULL, NULL);

    while (audio_position() < DURATION_SAMPLES) {
        const uint64_t before = time_us_64();
        const uint32_t sample = audio_position();
        demo_render(video_back(), sample);
        const unsigned us = (unsigned)(time_us_64() - before);
        cost += us;
        if (us > worst) worst = us;
        if (us < best) best = us;
        if (us > 16000u) over++;
        count++;
        video_present();

        const uint64_t now = time_us_64();
        if (now - report >= 1000000u) {
            uint32_t pumps, cycles, worst_cycles, peak_cycles;
            video_prof(&pumps, &cycles, &worst_cycles, &peak_cycles);
            const uint32_t dn = pumps - p_pumps;
            const uint32_t dc = cycles - p_cycles;
            const uint32_t ds = sample - p_sample;
            const uint64_t dt = now - report;
            const unsigned n = count ? count : 1;
            const uint32_t mean = (uint32_t)(cost / n);
            demo_stats_t st;
            demo_stats(&st);
            uint32_t fields = 0, first_repeat = 0, boot_repeats = 0;
            video_fields(&fields, &first_repeat, &boot_repeats);
            cycles_total += dc;
            frames_total += count;
            if (worst > worst_all_us) worst_all_us = worst;

            printf("T t=%lu.%lu section=%u"
                   " | render ms %lu.%02lu/%lu.%02lu/%lu.%02lu"
                   " | fps %lu.%lu | blits %lu"
                   " | repeat %lu | over %lu | fields %lu firstrep %lu boot %lu"
                   " | fill %u win %lu under %lu"
                   " | synth %lu cy/pump %lu.%02lu Mcy/s %lu cy/sample"
                   " | pump worst %lu.%02lu us peak %lu.%02lu us",
                   (unsigned long)(sample / SAMPLE_RATE),
                   (unsigned long)(sample % SAMPLE_RATE * 10 / SAMPLE_RATE),
                   st.section,
                   (unsigned long)(best / 1000),
                   (unsigned long)(best % 1000 / 10),
                   (unsigned long)(mean / 1000),
                   (unsigned long)(mean % 1000 / 10),
                   (unsigned long)(worst / 1000),
                   (unsigned long)(worst % 1000 / 10),
                   (unsigned long)((uint64_t)count * 1000000u / dt),
                   (unsigned long)((uint64_t)count * 10000000u / dt % 10),
                   (unsigned long)st.spans,
                   (unsigned long)video_repeats(), (unsigned long)over,
                   (unsigned long)fields, (unsigned long)first_repeat,
                   (unsigned long)boot_repeats,
                   audio_min_fill(), (unsigned long)audio_min_window(),
                   (unsigned long)audio_underruns(),
                   (unsigned long)(dn ? dc / dn : 0),
                   (unsigned long)((uint64_t)dc / dt),
                   (unsigned long)((uint64_t)dc * 100 / dt % 100),
                   (unsigned long)(ds ? dc / ds : 0),
                   (unsigned long)(worst_cycles / 300),
                   (unsigned long)(worst_cycles % 300 * 100 / 300),
                   (unsigned long)(peak_cycles / 300),
                   (unsigned long)(peak_cycles % 300 * 100 / 300));
            uint32_t hash_sample = 0, hash = 0;
            if (synth_hash_latch(&hash_sample, &hash))
                printf(" | AHASH s=%lu %08lx", (unsigned long)hash_sample,
                       (unsigned long)hash);
            printf(" | prep_us=%lu draw_us=%lu tiles=%lu"
                   " | missed %lu bootmiss %lu\n",
                   (unsigned long)st.prepare, (unsigned long)st.draw,
                   (unsigned long)st.tiles, (unsigned long)video_missed(),
                   (unsigned long)video_missed_boot());

            count = 0;
            cost = 0;
            worst = 0;
            best = ~0u;
            report = now;
            p_pumps = pumps;
            p_cycles = cycles;
            p_sample = sample;
        }
    }

    demo_render(video_back(), DURATION_SAMPLES);
    video_present();
    {
        uint32_t hash_sample, hash;
        if (synth_hash_latch(&hash_sample, &hash))
            printf("TAIL AHASH s=%lu %08lx\n", (unsigned long)hash_sample,
                   (unsigned long)hash);
    }
    if (worst > worst_all_us) worst_all_us = worst;
    {
        uint32_t pumps, cycles, worst_cycles, peak_cycles;
        uint32_t fields = 0, first_repeat = 0, boot_repeats = 0;
        video_prof(&pumps, &cycles, &worst_cycles, &peak_cycles);
        video_fields(&fields, &first_repeat, &boot_repeats);
        const uint64_t total_cycles = cycles_total + (uint32_t)(cycles - p_cycles);
        printf("DONE frames %lu | worst render %lu.%02lu ms"
               " | repeat %lu | over %lu"
               " | fields %lu firstrep %lu boot %lu"
               " | under %lu | fill %u | peak pump %lu.%02lu us"
               " | synth %lu cy/pump %lu cy/sample | audio %lu\n",
               (unsigned long)(frames_total + count),
               (unsigned long)(worst_all_us / 1000),
               (unsigned long)(worst_all_us % 1000 / 10),
               (unsigned long)video_repeats(), (unsigned long)over,
               (unsigned long)fields, (unsigned long)first_repeat,
               (unsigned long)boot_repeats,
               (unsigned long)audio_underruns(), audio_min_fill(),
               (unsigned long)(peak_cycles / 300),
               (unsigned long)(peak_cycles % 300 * 100 / 300),
               (unsigned long)(pumps ? total_cycles / pumps : 0),
               (unsigned long)(audio_position() ? total_cycles / audio_position() : 0),
               (unsigned long)audio_position());
        printf("FINAL missed %lu bootmiss %lu | ENDHASH s=%lu %08lx\n",
               (unsigned long)video_missed(),
               (unsigned long)video_missed_boot(),
               (unsigned long)DURATION_SAMPLES,
               (unsigned long)synth_final_hash());
    }
    /* The measured production and audio score end above. Keep the original
     * closing rays alive until reset, paced by elapsed wall time because the
     * finite audio DMA counter stops one second after the scored window. */
    const uint64_t ending_epoch = time_us_64();
    while (true) {
        const uint64_t elapsed = time_us_64() - ending_epoch;
        const uint32_t sample = DURATION_SAMPLES +
            (uint32_t)(elapsed * SAMPLE_RATE / 1000000u);
        demo_render(video_back(), sample);
        video_present();
    }
}
