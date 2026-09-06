#include "palette.h"
#include "vga.h"

static const pal_ramp_t g_ramps[PAL_COUNT] = {
    /* COLD — the opening. Black floor so a single lit cell reads as the only
     * thing in the world, then straight into steel blue. */
    [PAL_COLD]  = {{ {  0,  0,  0}, {  6, 14, 28}, { 40, 84,126}, {188,214,238} }},

    /* EMBER — growth. The low end lifts off black so structure in the dark
     * becomes visible; this is where most of the demo's middle lives. */
    [PAL_EMBER] = {{ {  4,  2,  6}, { 62, 14, 10}, {178, 72, 16}, {248,196, 96}, }},

    /* BLOOM — the peak. High contrast with a white shoulder, so the saturated
     * regions clip to light rather than to a colour. */
    [PAL_BLOOM] = {{ { 12,  4, 18}, {124, 24, 92}, {242,110, 52}, {255,255,240} }},

    /* ASH — the decay. Desaturated and compressed: as the field flattens
     * toward equilibrium the palette flattens with it, so the ending reads as
     * the system running down rather than as a fade-out applied on top. */
    [PAL_ASH]   = {{ {  0,  0,  0}, { 18, 18, 22}, { 58, 56, 60}, {126,124,130} }},

    /* Black. The endcard is held by a hard threshold in the react curve, which
     * means its cells sit at 254 and will not fade by getting darker -- the
     * threshold puts them straight back. So the last two seconds fade through
     * the READOUT instead, which is the one thing this demo has always said it
     * is allowed to do with t (palette.h). The state does not change at all
     * while the picture goes to black, which is a fairly exact statement of
     * what the demo is about. */
    [PAL_BLACK] = {{ {  0,  0,  0}, {  0,  0,  0}, {  0,  0,  0}, {  0,  0,  0} }},
};

static inline int lerp(int a, int b, int w) { return a + (((b - a) * w) >> 8); }

/* Four control points across 256 entries: three segments of 85, 85, 86. */
static void ramp_eval(const pal_ramp_t *r, int i, int *out)
{
    int seg = i / 85; if (seg > 2) seg = 2;
    int t   = ((i - seg * 85) * 256) / 85; if (t > 255) t = 255;
    for (int c = 0; c < 3; c++)
        out[c] = lerp(r->rgb[seg][c], r->rgb[seg + 1][c], t);
}

void palette_apply(int ramp_a, int ramp_b, uint8_t w)
{
    if (ramp_a < 0 || ramp_a >= PAL_COUNT) ramp_a = 0;
    if (ramp_b < 0 || ramp_b >= PAL_COUNT) ramp_b = 0;

    for (int i = 0; i < 256; i++) {
        int a[3], b[3];
        ramp_eval(&g_ramps[ramp_a], i, a);
        ramp_eval(&g_ramps[ramp_b], i, b);
        vga_320_palette_set(i,
            (uint8_t)lerp(a[0], b[0], w),
            (uint8_t)lerp(a[1], b[1], w),
            (uint8_t)lerp(a[2], b[2], w));
    }
}
