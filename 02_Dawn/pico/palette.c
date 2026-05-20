#include "palette.h"
#include "pico/scanvideo.h"

volatile uint16_t palette_rgb565[PAL_SIZE];

static const uint16_t scheme_table[5][3] = {
    {65000, 60000, 35000},  /* colors1 — main warm */
    {65000, 50000, 35000},  /* colors2 — "by"      */
    {45000, 50000, 35000},  /* colors3 — "azure"   */
    {60000, 60000, 60000},  /* colors4 — finale gray */
    {60000, 50000, 10000},  /* colors5 — golden    */
};

/* Apply the original's intensity curves (dawn_final.s:982-1023). The 68k
 * code's exact bit-fiddling is preserved in the web port (palette.ts) — we
 * stick to that same path, then convert to RGB-565 at the end. */
static inline uint8_t clamp_byte(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void palette_set_scheme(pal_scheme_t s)
{
    if ((unsigned)s > PAL_SCHEME_5) s = PAL_SCHEME_1;
    const uint32_t baseR = scheme_table[s][0];
    const uint32_t baseG = scheme_table[s][1];
    const uint32_t baseB = scheme_table[s][2];

    for (int d7 = 63; d7 >= 0; d7--) {
        uint32_t d0 = (uint32_t)(d7 ^ 0x3F) & 0xFFFFu;

        /* Each channel takes the HIGH word (>>16) of a 32-bit unsigned
         * product, which corresponds to the original 68k `swap` + low-byte
         * extraction (and to web_port/palette.ts:64-66's `(d1 >>> 16)`,
         * `(d1 >>> 8) & 0xFF`, `d1 & 0xFF` after swaps). Without the >>16
         * the channel saturates at 255 for almost every index, which
         * washed the whole gradient out to white. */

        /* Red: ((d0² >> 4) * baseR) high word. */
        uint32_t sq = (d0 * d0) >> 4;
        uint32_t rprod = (sq & 0xFFFFu) * baseR;
        uint32_t red_v = (rprod >> 16) & 0xFFFFu;

        /* Green: ((d0³ >> 10) * baseG) high word. */
        uint32_t cube = (d0 * d0 * d0) >> 10;
        uint32_t gprod = (cube & 0xFFFFu) * baseG;
        uint32_t green_v = (gprod >> 16) & 0xFFFFu;

        /* Blue: ((d0 << 2) * baseB) high word. */
        uint32_t bprod = ((d0 << 2) & 0xFFFFu) * baseB;
        uint32_t blue_v = (bprod >> 16) & 0xFFFFu;

        uint8_t R = clamp_byte((int)red_v);
        uint8_t G = clamp_byte((int)green_v);
        uint8_t B = clamp_byte((int)blue_v);

        palette_rgb565[d0 & 0x3F] =
            (uint16_t)PICO_SCANVIDEO_PIXEL_FROM_RGB8(R, G, B);
    }
}
