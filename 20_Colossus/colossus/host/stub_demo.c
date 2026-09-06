/* The platform's stand-in renderer.
 *
 * Phase owns render*.c; until it lands, and afterwards whenever the platform
 * has to be measured on its own, this is what demo.h's contract is filled
 * with. It is deliberately dull and deliberately honest:
 *
 *   - it writes every one of the 76,800 pixels every frame, so the render
 *     time in the telemetry is a real measurement of what it costs to fill a
 *     page and not an idle loop;
 *   - it is pure in `sample`, like the real one has to be, so seeking in the
 *     host player and skipping frames on the device both work here first;
 *   - it reads the score, so the picture is visibly wrong the moment the
 *     clock is wrong -- the bar counter, the chapter name and the flash on
 *     the kick are the cheapest possible test that the frame is drawing the
 *     sample the DAC is playing.
 *
 * Build with -DCOLOSSUS_STUB=ON (CMake) or `make stub=1` (host/Makefile).
 */

#include "demo.h"
#include "colossus.h"
#include "song.h"
#include "stub_font.h"

#include <string.h>

static uint32_t s_bar;

void demo_init(void) { s_bar = 0; }

static void text(uint16_t *page, int x, int y, const char *s, uint16_t colour)
{
    for (; *s; s++, x += FONT8X8_W) {
        if (x < 0 || x + FONT8X8_W > CV_W) continue;
        const uint8_t *g = font8x8_glyph(*s);
        for (int r = 0; r < FONT8X8_H; r++) {
            if (y + r < 0 || y + r >= CV_H) continue;
            uint16_t *row = page + (y + r) * CV_W + x;
            const uint8_t bits = g[r];
            for (int c = 0; c < 8; c++) if (bits & (0x80u >> c)) row[c] = colour;
        }
    }
}

static char *num(char *p, uint32_t v, int width)
{
    char tmp[12];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n < width) tmp[n++] = '0';
    while (n) *p++ = tmp[--n];
    return p;
}

void CV_HOT(demo_render)(uint16_t *page, uint32_t sample)
{
    const uint32_t bar   = cv_bar_of(sample);
    const uint32_t step  = cv_step_of(sample);
    const uint32_t frac  = cv_bar_frac(sample);          /* 0..65535 in the bar */
    const uint32_t phase = (sample % CV_STEP) * 256u / CV_STEP;
    const uint8_t  drums = song_drums(step);
    const int      flash = (drums & DR_KICK) && phase < 48 ? (int)(48 - phase) * 2 : 0;
    const int      energy = song_energy(bar);

    s_bar = bar;

    /* The palette anchors from PLANNING.md section 4, so what comes out of
     * the DAC here is the range the real thing has to live in. */
    const int bar_x = (int)((frac * (CV_W - 24)) >> 16);
    const int horizon = 96 + (int)(energy >> 3);

    for (int y = 0; y < CV_H; y++) {
        uint16_t *row = page + y * CV_W;
        /* deep recess -> bronze shadow down the frame, dawn above the horizon */
        int r, g, b;
        if (y < horizon) {
            const int s = y * 255 / (horizon ? horizon : 1);
            r = 0x10 + (0x28 - 0x10) * s / 255;
            g = 0x18 + (0x38 - 0x18) * s / 255;
            b = 0x20 + (0x40 - 0x20) * s / 255;
        } else {
            const int s = (y - horizon) * 255 / (CV_H - horizon);
            r = 0x30 + (0x70 - 0x30) * s / 255;
            g = 0x28 + (0x60 - 0x28) * s / 255;
            b = 0x20 + (0x48 - 0x20) * s / 255;
        }
        r += flash; g += flash; b += flash / 2;

        for (int x = 0; x < CV_W; x++) {
            /* a slow horizontal ramp keyed to the phrase, so consecutive
             * chapters are visibly different pages and not one long fade */
            const int ramp = (x * (32 + (int)(bar / 8) * 8)) >> 8;
            row[x] = cv_rgb(r + ramp, g + ramp * 3 / 4, b + ramp / 2);
        }

        /* the moving bar: one bronze-light column crossing once a bar */
        if (y > 40 && y < CV_H - 40) {
            const int w = 8 + energy / 24;
            for (int x = bar_x; x < bar_x + w && x < CV_W; x++)
                row[x] = cv_rgb(0xB8, 0x98, 0x68);
        }
    }

    /* a snare tick along the top edge, so both drum voices are visible */
    if (drums & DR_SNARE) {
        for (int x = 0; x < CV_W; x++) page[2 * CV_W + x] = cv_rgb(0xD8, 0xE0, 0xE0);
    }

    char line[64], *p = line;
    const char *name = song_section_name(song_section(bar));
    for (const char *q = name; *q && p < line + 24; q++) *p++ = *q;
    *p++ = ' ';
    p = num(p, bar, 3);
    *p++ = ':';
    p = num(p, (sample % CV_BAR) * 16u / CV_BAR, 2);
    *p = 0;
    text(page, 8, CV_H - 24, line, cv_rgb(0xE0, 0xC0, 0x98));

    p = line;
    p = num(p, sample / CV_RATE / 60u, 1); *p++ = ':';
    p = num(p, sample / CV_RATE % 60u, 2); *p++ = '.';
    p = num(p, sample % CV_RATE * 10u / CV_RATE, 1);
    *p++ = ' '; *p++ = 'S'; *p++ = 'T'; *p++ = 'U'; *p++ = 'B';
    *p = 0;
    text(page, 8, 8, line, cv_rgb(0x80, 0x98, 0xA8));
}

void demo_stats(demo_stats_t *out)
{
    out->triangles = 0;
    out->fill      = CV_W * CV_H;          /* it really does write them all */
    out->particles = 0;
    out->chapter   = (uint8_t)song_section(s_bar);
}
