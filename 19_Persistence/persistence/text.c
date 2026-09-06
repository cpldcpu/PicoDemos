#include "text.h"
#include "beam.h"
#include "font8x8.h"

int text_width(const char *s, int scale)
{
    int n = 0;
    while (s[n]) n++;
    return n * TEXT_ADVANCE(scale);
}

void PV_HOT(text_row)(uint16_t *px, int x0, const char *s, int scale, int gy, uint16_t c)
{
    if (gy < 0 || gy > 7) return;
    const int adv = TEXT_ADVANCE(scale);
    for (int i = 0; s[i]; i++, x0 += adv) {
        if (x0 >= PV_W) break;
        if (x0 + adv <= 0) continue;
        const unsigned bits = font8x8_glyph(s[i])[gy];
        if (!bits) continue;
        /* columns 1..6 carry the ink; walk the set bits and fill a run each */
        for (int b = 6; b >= 1; b--) {
            if (!(bits & (1u << b))) continue;
            const int x = x0 + (7 - b) * scale;
            pv_fill(px, x, x + scale, c);
        }
    }
}

void PV_HOT(text_row_centred)(uint16_t *px, const char *s, int scale, int gy, uint16_t c)
{
    text_row(px, (PV_W - text_width(s, scale)) / 2, s, scale, gy, c);
}
