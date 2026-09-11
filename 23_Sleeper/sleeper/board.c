/* board.c -- the split-flap board. PLANNING §3: it is the production's only
 * typography and its only transition device. Nothing fades; the board
 * flipping between two texts *is* the cut.
 *
 * 20 columns x 3 rows of 8x12 tiles, each tile a near-black rounded rectangle
 * with a one-pixel hairline across its middle and the glyph in white. A
 * change from text A to text B flips every column whose character differs,
 * over BOARD_FLIP_SAMPLES, starting BOARD_STAGGER_SAMPLES per column from the
 * left -- both constants are song.h's, so the flaps in the score and the
 * flaps on screen are one event and not two that agree.
 *
 * Three phases in three fields, which at 1,200 samples of flip and 400
 * samples of field is exactly one phase a field:
 *
 *   0  the old glyph's top half folds down toward the hinge (squashed into
 *      the rows just above it) while the new glyph's top appears behind it;
 *   1  the flap has passed the hinge: the top half is the new glyph, and the
 *      old top half is now squashed across the top of the *bottom* half,
 *      which is what you see of the flap's back;
 *   2  it settles: one row of the flap left, the new glyph behind it.
 *
 * Characters do not flip *through* the alphabet. At an 8x12 cell the
 * intermediate glyphs are illegible and read as noise; one flip per change
 * reads as a flap. -- Overscan
 */
#include "render.h"
#include "font8x12.h"
#include <string.h>

/* The tile grid, in cell units. Row pitch leaves two rows of black between
 * rows so the three lines read as three lines. */
#define TILE_W    8
#define TILE_H    12
#define ROW_PITCH 14

/* Cell-space plot: at shrink 2 several cell pixels land on one screen pixel,
 * which is the point -- the platform's board is the same code at half size. */
static inline void tile_px(int px0, int py0, int i, int j, int shrink, uint16_t c)
{
    px(px0 + i / shrink, py0 + j / shrink, c, 32);
}

/* Draw source cell rows [sr0,sr1) of a glyph into destination cell rows
 * [dr0,dr1). Squashing is a row remap and nothing else: it is what makes a
 * flap out of a bitmap. */
static void HOT(band)(int px0, int py0, int shrink, const uint8_t *g, int sr0, int sr1, int dr0, int dr1, uint16_t col)
{
    const int dn = dr1 - dr0, sn = sr1 - sr0;
    if (dn <= 0 || sn <= 0) return;
    for (int j = 0; j < dn; j++) {
        const uint8_t bits = g[sr0 + j * sn / dn];
        if (!bits) continue;
        for (int i = 0; i < 8; i++) if (bits & (128 >> i)) tile_px(px0, py0, i, dr0 + j, shrink, col);
    }
}

static void HOT(tile)(int px0, int py0, int shrink, char oldc, char newc, int phase)
{
    /* the flap body: a near-black rounded rectangle */
    for (int j = 0; j < TILE_H; j++)
        for (int i = 0; i < TILE_W; i++) {
            if ((i == 0 || i == TILE_W - 1) && (j == 0 || j == TILE_H - 1)) continue;
            tile_px(px0, py0, i, j, shrink, C_TILE);
        }
    const uint8_t *go = font8x12_glyph(oldc), *gn = font8x12_glyph(newc);
    const uint16_t ink = C_FLUO;
    switch (phase) {
    case 0:                                   /* the old top folding down    */
        band(px0, py0, shrink, gn, 0, 3, 0, 3, ink);
        band(px0, py0, shrink, go, 0, 6, 3, 6, ink);
        band(px0, py0, shrink, go, 6, 12, 6, 12, ink);
        break;
    case 1:                                   /* past the hinge              */
        band(px0, py0, shrink, gn, 0, 6, 0, 6, ink);
        band(px0, py0, shrink, go, 0, 6, 6, 9, ink);
        band(px0, py0, shrink, gn, 9, 12, 9, 12, ink);
        break;
    case 2:                                   /* settling                    */
        band(px0, py0, shrink, gn, 0, 6, 0, 6, ink);
        band(px0, py0, shrink, go, 0, 6, 6, 7, ink);
        band(px0, py0, shrink, gn, 7, 12, 7, 12, ink);
        break;
    default:                                  /* settled                     */
        band(px0, py0, shrink, gn, 0, 12, 0, 12, ink);
        break;
    }
    /* The hairline goes on last, so it cuts the glyph as a real flap does. */
    for (int i = 0; i < TILE_W; i++) tile_px(px0, py0, i, TILE_H / 2, shrink, C_HAIR);
}

/* A board row, centred in `cols` columns, as a fixed-width buffer. The full
 * board is twenty; the station's own is eleven, at full glyph size. */
static void row_text(const char *s, char *out, int cols)
{
    memset(out, ' ', (size_t)cols);
    out[cols] = 0;
    if (!s) return;
    int n = (int)strlen(s);
    if (n > cols) n = cols;
    int at = (cols - n) / 2;
    memcpy(out + at, s, (size_t)n);
}

static int all_null(const board_t *b)
{
    if (!b) return 1;
    for (int r = 0; r < BOARD_ROWS; r++) if (b->row[r]) return 0;
    return 1;
}

void HOT(board_at)(int x0, int y0, int shrink, int cols, const board_t *now, const board_t *prev, uint32_t since)
{
    if (all_null(now)) return;                /* the scheduled black          */
    if (cols > BOARD_COLS) cols = BOARD_COLS;
    char a[BOARD_COLS + 1], b[BOARD_COLS + 1];
    const int pitch_x = TILE_W / shrink, pitch_y = ROW_PITCH / shrink;
    for (int r = 0; r < BOARD_ROWS; r++) {
        if (!now->row[r] && !(prev && prev->row[r]) && cols < BOARD_COLS) continue;
        row_text(prev ? prev->row[r] : NULL, a, cols);
        row_text(now->row[r], b, cols);
        const int py = y0 + r * pitch_y;
        for (int c = 0; c < cols; c++) {
            const int pxx = x0 + c * pitch_x;
            int phase = -1;
            if (a[c] != b[c]) {
                const int32_t t = (int32_t)since - (int32_t)(c * BOARD_STAGGER_SAMPLES);
                if (t < 0) { tile(pxx, py, shrink, a[c], a[c], -1); continue; }
                if (t < (int32_t)BOARD_FLIP_SAMPLES) phase = (int)(t * 3 / (int32_t)BOARD_FLIP_SAMPLES);
            }
            tile(pxx, py, shrink, a[c], b[c], phase);
        }
    }
}

/* The full board: 20 x 8 = 160 wide, three rows at pitch 14 = 40 tall,
 * centred on the page. */
void board_frame(void)
{
    const board_t *prev = NULL; uint32_t since = 0;
    const board_t *now = song_board(F.sample, &prev, &since);
    if (!now) return;
    board_at((WIDTH - BOARD_COLS * TILE_W) / 2, (HEIGHT - (BOARD_ROWS - 1) * ROW_PITCH - TILE_H) / 2,
             1, BOARD_COLS, now, prev, since);
}

/* The station's own board, hung under the roof. Eleven columns at FULL glyph
 * size, not twenty at half: at half size the face is 4x6 and PERSISTENCE is
 * a grey smudge, and this board carries the station's name, which is the
 * one piece of text in the film that has to be read rather than recognised.
 * Eleven columns is the longest station name in the score. */
void board_platform(int x0, int y0)
{
    const board_t *prev = NULL; uint32_t since = 0;
    const board_t *now = song_board(F.sample, &prev, &since);
    if (!now) return;
    board_at(x0, y0, 1, 11, now, prev, since);
}
