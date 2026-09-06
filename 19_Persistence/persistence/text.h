/* Text with no framebuffer.
 *
 * Nothing rasterises a glyph into a buffer, because there is no buffer to
 * rasterise into. A string occupies a band of screen rows; when the beam
 * reaches row y, the row inside that band is drawn straight out of the font
 * as a handful of horizontal runs. The whole title costs about four hundred
 * filled pixels on the rows it touches and nothing at all on the rest.
 *
 * The 8x8 font's glyphs sit in bit columns 1..6, so advancing by 7*scale puts
 * one blank column of gap between letters at any size.
 */

#ifndef PV_TEXT_H
#define PV_TEXT_H

#include <stdint.h>

#define TEXT_ADVANCE(scale) (7 * (scale))

int  text_width(const char *s, int scale);

/* Which row of the glyphs does screen row y correspond to, for a block whose
 * top edge is y0 and whose scale is `scale`? -1 if y is outside the block. */
static inline int text_glyph_row(int y, int y0, int scale)
{
    const int d = y - y0;
    if (d < 0) return -1;
    const int g = d / scale;
    return g < 8 ? g : -1;
}

/* Draw glyph row `gy` of `s` into the scanline at x0, in colour c. */
void text_row(uint16_t *px, int x0, const char *s, int scale, int gy, uint16_t c);

/* The same, centred on the screen. */
void text_row_centred(uint16_t *px, const char *s, int scale, int gy, uint16_t c);

#endif
