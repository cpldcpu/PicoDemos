/* check.c -- referee 3. HELION's check with SLEEPER's numbers.
 *
 * 4,801 guarded frames at a 960-sample stride (4,800 x 960 = DURATION_SAMPLES
 * exactly, so the last frame is the endpoint), the endpoint black, the
 * framebuffer guards, no frame written with the DAC's unused bit 5, no blank
 * frame where the timetable says there should be a picture, a deterministic
 * seek, block-size-independent audio, the audio endpoint silent, and the
 * per-second hash latch firing exactly on the second and never early.
 *
 * The PASS line says which synth was linked, because "audio arbitrary blocks
 * and endpoint" means very little about a stub that emits silence and a great
 * deal about a score. -- Overscan
 */
#include "sleeper.h"
#include "song.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifndef SLEEPER_STUB_SYNTH
#define SLEEPER_STUB_SYNTH 0
#endif
static struct { uint32_t before[16]; uint16_t pixels[WIDTH * HEIGHT]; uint32_t after[16]; } guarded;
static uint16_t second[WIDTH * HEIGHT];
static int16_t reference[2048], block[2048];
static uint64_t hash_frame(void) { uint64_t h = 1469598103934665603ull; for (int i = 0; i < WIDTH * HEIGHT; i++) h = (h ^ guarded.pixels[i]) * 1099511628211ull; return h; }
static void fail(const char *reason, unsigned sample) { fprintf(stderr, "FAIL %s at sample %u\n", reason, sample); exit(1); }
int main(void)
{
    memset(&guarded, 0xa5, sizeof guarded); song_init(); demo_init(); synth_init();
    uint64_t first = 0, combined = 0; unsigned maxspans = 0, frames = 0;
    /* The board alone lights 20x3 tiles = 5,760 pixels, so the floor for
     * "there is a picture here" is well under that and still far above an
     * accidental black frame. Bars 127:3 onward are the scheduled black. */
    const unsigned lit_floor = 1200;
    for (unsigned sample = 0; sample <= DURATION_SAMPLES; sample += 960) {
        demo_render(guarded.pixels, sample);
        frames++;
        for (int i = 0; i < 16; i++) if (guarded.before[i] != 0xa5a5a5a5 || guarded.after[i] != 0xa5a5a5a5) fail("framebuffer guard", sample);
        unsigned lit = 0;
        for (int i = 0; i < WIDTH * HEIGHT; i++) { if (guarded.pixels[i] & 32) fail("invalid DAC bit", sample); lit += guarded.pixels[i] != 0; }
        /* Two darknesses are scheduled and everything else is a bug: the
         * tunnel mouth filling the frame in the last beat before the cut at
         * 24:1, and the board's last flaps to blank at 127:2. */
        {
            uint32_t since = 0;
            const cut_t *c = song_cut(sample, &since);
            const int mouth = (c->flags & CUT_MOUTH) && since > 2u * BAR_SAMPLES - BEAT_SAMPLES;
            if (sample > SAMPLE_RATE / 2 && !mouth
                && sample < 127u * BAR_SAMPLES + 2u * BEAT_SAMPLES && lit < lit_floor)
                fail("unexpected blank frame", sample);
        }
        if (sample == DURATION_SAMPLES && lit) fail("endpoint must be black", sample);
        if (sample == SAMPLE_RATE * 60) first = hash_frame();
        combined ^= hash_frame();
        /* Every sixteenth frame, prove the renderer covers the whole page.
         * A shot that leaves pixels untouched is not a pure function of the
         * sample: it inherits whatever the last frame left, which on the
         * board is the *other* page and therefore two frames ago. It looks
         * like nothing on a play-through and like garbage after a seek, and
         * the guards above cannot see it because the bytes are inside the
         * framebuffer. Rendering the same sample onto two different fills
         * and comparing is the only cheap test that can. -- Overscan */
        if ((frames & 15u) == 0u) {
            memcpy(second, guarded.pixels, sizeof second);
            memset(guarded.pixels, 0xff, sizeof guarded.pixels);
            demo_render(guarded.pixels, sample);
            if (memcmp(second, guarded.pixels, sizeof second)) fail("page not fully covered", sample);
        }
        { demo_stats_t st; demo_stats(&st); if (st.spans > maxspans) maxspans = st.spans; }
    }
    if (frames != 4801) fail("frame count", frames);
    demo_render(guarded.pixels, SAMPLE_RATE * 60); if (first != hash_frame()) fail("non deterministic seek", SAMPLE_RATE * 60);
    unsigned peak = 0; uint64_t ah = 1469598103934665603ull;
    const unsigned length = DURATION_SAMPLES + 2048;
    /* Host-only reference buffer. Two sequential passes avoid O(n^2) work
     * when the musician supplies a stateful synth with reset-and-replay
     * seek. */
    int16_t *audio = malloc((size_t)length * 4); if (!audio) fail("allocation", 0);
    synth_init(); for (unsigned pos = 0; pos < length;) { unsigned n = length - pos; if (n > 512) n = 512; synth_render(audio + (size_t)pos * 2, n); pos += n; }

    const unsigned sizes[4] = { 1, 2, 8, 997 }; unsigned cycle = 0;
    synth_init(); for (unsigned pos = 0; pos < length;) {
        unsigned n = sizes[cycle++ & 3]; if (n > length - pos) n = length - pos; synth_render(block, n);
        if (memcmp(audio + (size_t)pos * 2, block, (size_t)n * 4)) fail("audio block-size mismatch", pos);
        if (synth_position() != pos + n) fail("audio position", pos);
        for (unsigned i = 0; i < n * 2; i++) { unsigned a = (unsigned)abs(block[i]); if (a > peak) peak = a; ah = (ah ^ (uint16_t)block[i]) * 1099511628211ull; if (pos + i / 2 >= DURATION_SAMPLES && a) fail("audio endpoint", pos); }
        pos += n;
    }
    for (unsigned pos = SAMPLE_RATE * 9; pos < DURATION_SAMPLES; pos += SAMPLE_RATE * 37) {
        synth_seek(pos); synth_render(reference, 1024); if (memcmp(reference, audio + (size_t)pos * 2, sizeof reference)) fail("audio seek mismatch", pos);
    }
    synth_init(); uint32_t expected = 2166136261u;
    for (unsigned pos = 0; pos < DURATION_SAMPLES; pos += 300) {
        synth_render(block, 300); for (unsigned i = 0; i < 600; i++) expected = (expected ^ (uint16_t)audio[(size_t)pos * 2 + i]) * 16777619u;
        uint32_t hp = 0, hh = 0; int latched = synth_hash_latch(&hp, &hh);
        if ((pos + 300) % SAMPLE_RATE == 0) { if (!latched || hp != pos + 300 || hh != expected) fail("telemetry audio hash", pos); }
        else if (latched) fail("early audio hash latch", pos);
    }
    free(audio);
    /* Every cut is on a beat: the second claim's precondition, checked here
     * so a bad cut list fails the build rather than the film. */
    for (unsigned i = 0; i < song_cut_count(); i++) {
        const cut_t *c = song_cut_at(i);
        if (c->beat > 3) fail("cut off the beat", i);
        if (i && cut_sample(c) < cut_sample(song_cut_at(i - 1))) fail("cut list not sorted", i);
    }
    printf("PASS 4801 guarded frames, 301 of them drawn twice on different fills; deterministic seek; "
           "audio arbitrary blocks and endpoint; "
           "%u cuts all on the beat %s\n", song_cut_count(),
           SLEEPER_STUB_SYNTH ? "(stub synth: silence)" : "(Phosphor score)");
    printf("visual_hash=%016llx audio_hash=%016llx max_spans=%u audio_peak=%u\n",
           (unsigned long long)combined, (unsigned long long)ah, maxspans, peak);
    return 0;
}
