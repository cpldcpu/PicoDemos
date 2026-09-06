/* COLOSSUS -- the song.
 *
 * D minor at 125 BPM, a hundred and sixty bars in twenty phrases of eight,
 * lifting to E minor for the crown and coming home for the reveal. Written as
 * tables, on purpose: PLANNING.md says the music carries this production, and
 * a melody that carries anything is a melody somebody chose note by note.
 * Nothing here is generated.
 *
 * ---------------------------------------------------------------- the hook --
 *
 * Theme A, over Dm | Bb | F | C | Dm | Bb | Gm | A. The Onward shape: climb
 * the chord, fall a step, and the second half climbs higher than the first.
 *
 *      D5 . F5 A5 G5   |  F5 . D5 F5 G5   |  A5 . G5 F5 C5   |  D5 .. E5 ...
 *      D5 . F5 A5 C6   |  D6 . C6 Bb5 A5  |  G5 . A5 Bb5 A5  |  A5 . F5 E5 ..
 *
 * The rhythm is the same in every bar -- a long note on the beat, the second
 * on the "and" of two, the third on three, the fourth on four -- so the lilt
 * is a signature rather than an accident. Bar six is the peak, D6 falling,
 * and is the part that is meant to be hummed.
 *
 * Theme B is the counter-melody: long notes, an arch that rises to C6 and
 * falls to the leading tone, alone in the eye and stacked under A later --
 *
 *      F5 ........ G5  |  A5 ...........  |  C6 ........ A5  |  G5 ..........
 *      F5 ........ E5  |  D5 ...........  |  Bb4 .... D5 ..  |  C#5 .........
 *
 * The one tone: a D, two octaves of it, that opens the demo alone and closes
 * it alone. It is under the plain, gone from the hand to the forge, back for
 * the reveal. The colossus's note.
 *
 * ---------------------------------------------------------------- the form --
 *
 *   phrase  bars    chapter      music
 *   1       0-7     overture     the tone; the pad grows under it
 *   2-3     8-23    the plain    pad, slow bass, arp; kick from 16, hats 20
 *   4-5     24-39   the hand     THEME A, twice; second time full drums
 *   6-7     40-55   the heart    theme A' (the variation), busier
 *   8-9     56-71   the eye      breakdown: THEME B alone, then half-time
 *   10-11   72-87   the forge    stabs over Bb C Dm Dm Bb C A A, riser
 *   12-14   88-111  the spine    A over B; B breathes; A' over B
 *   15-16   112-127 the crown    A over B, A' over B -- UP A TONE
 *   17-18   128-143 the colossus A over B, home in D, the tone under it all
 *   19-20   144-159 coda         B thins; pad; the tone alone
 *
 * Every section starts on a multiple of eight, so a phrase's bar is bar & 7
 * and the order list below only names which phrase a bar plays.
 *
 * ROW EVENTS. Pitched tables use 0 = hold, 1 = note off, else a MIDI note
 * (or, for patterns relative to a chord, an offset + 2). Held notes carry
 * across bars. A voice whose phrase is "none" is sent a note off on the
 * first step of the bar, so switching a voice off in the order list is
 * enough to silence it.
 */

#include "song.h"
#include "colossus.h"

/* ---------------------------------------------------------------- pitches -- */

enum {
    Bb3 = 58, C4 = 60, Cs4 = 61, D4 = 62, E4 = 64, F4 = 65, G4 = 67, A4 = 69, Bb4 = 70,
    C5 = 72, Cs5 = 73, D5 = 74, E5 = 76, F5 = 77, G5 = 79, A5 = 81, Bb5 = 82,
    C6 = 84, Cs6 = 85, D6 = 86, E6 = 88, F6 = 89
};
#define __  0           /* hold */
#define XX  SONG_OFF    /* note off */

/* ----------------------------------------------------------------- chords -- */

enum { CH_DM, CH_BB, CH_F, CH_C, CH_GM, CH_A };

static const uint8_t chord_root[6] = { 38, 34, 41, 36, 31, 33 };   /* D2 Bb1 F2 C2 G1 A1 */

static const uint8_t chord_pad[6][4] = {
    { 57, 62, 65, 69 },     /* Dm: A3 D4 F4 A4                  */
    { 58, 62, 65, 70 },     /* Bb: Bb3 D4 F4 Bb4                */
    { 57, 60, 65, 69 },     /* F:  A3 C4 F4 A4                  */
    { 55, 60, 64, 67 },     /* C:  G3 C4 E4 G4                  */
    { 58, 62, 67, 70 },     /* Gm: Bb3 D4 G4 Bb4                */
    { 57, 61, 64, 69 },     /* A:  A3 C#4 E4 A4 -- the C# is the point */
};

static const uint8_t chord_arp[6][6] = {
    { 50, 53, 57, 62, 65, 69 },
    { 46, 50, 53, 58, 62, 65 },
    { 53, 57, 60, 65, 69, 72 },
    { 48, 52, 55, 60, 64, 67 },
    { 43, 46, 50, 55, 58, 62 },
    { 45, 49, 52, 57, 61, 64 },
};

/* ------------------------------------------------------------ lead phrases -- */

enum { LP_NONE, LP_A, LP_A2, LP_RIS };

static const uint8_t lead_phrase[4][8][16] = {
    { { 0 } },
    /* theme A */
    { { D5, __, __, __, __, __, F5, __, A5, __, __, __, G5, __, __, __ },
      { F5, __, __, __, __, __, D5, __, F5, __, __, __, G5, __, __, __ },
      { A5, __, __, __, __, __, G5, __, F5, __, __, __, C5, __, __, __ },
      { D5, __, __, __, __, __, __, __, E5, __, __, __, __, __, XX, __ },
      { D5, __, __, __, __, __, F5, __, A5, __, __, __, C6, __, __, __ },
      { D6, __, __, __, __, __, C6, __, Bb5,__, __, __, A5, __, __, __ },
      { G5, __, __, __, __, __, A5, __, Bb5,__, __, __, A5, __, __, __ },
      { A5, __, __, __, __, __, F5, __, E5, __, __, __, __, __, XX, __ } },
    /* theme A' -- the same bones, doubled notes, a busier second half */
    { { D5, __, D5, __, __, __, F5, __, A5, __, __, __, G5, __, F5, __ },
      { F5, __, __, __, D5, __, F5, __, G5, __, __, __, F5, __, __, __ },
      { A5, __, A5, __, __, __, G5, __, F5, __, __, __, C5, __, D5, __ },
      { E5, __, __, __, D5, __, __, __, E5, __, __, __, G5, __, __, XX },
      { D5, __, __, __, F5, __, A5, __, C6, __, __, __, D6, __, __, __ },
      { D6, __, __, __, C6, __, Bb5,__, A5, __, __, __, G5, __, A5, __ },
      { Bb5,__, __, __, A5, __, G5, __, A5, __, __, __, Bb5,__, C6, __ },
      { A5, __, __, __, __, __, E5, __, Cs5,__, E5, __, A4, __, __, XX } },
    /* the forge -- stabs, over Bb C Dm Dm Bb C A A; the last bar drops out */
    { { Bb5,__, XX, Bb5,__, XX, Bb5,XX, Bb5,__, XX, Bb5,__, XX, Bb5,XX },
      { C6, __, XX, C6, __, XX, C6, XX, C6, __, XX, C6, __, XX, C6, XX },
      { D6, __, XX, D6, __, XX, D6, XX, D6, __, XX, D6, __, XX, D6, XX },
      { D6, __, XX, D6, __, XX, D6, XX, D6, __, XX, E6, __, XX, F6, XX },
      { Bb5,__, XX, Bb5,__, XX, Bb5,XX, Bb5,__, XX, Bb5,__, XX, Bb5,XX },
      { C6, __, XX, C6, __, XX, C6, XX, C6, __, XX, C6, __, XX, C6, XX },
      { E6, __, XX, E6, __, XX, E6, XX, E6, __, XX, E6, __, XX, E6, XX },
      { E6, __, XX, E6, __, XX, E6, XX, E6, __, __, __, __, XX, __, __ } },
};

enum { L2_NONE, L2_B };

static const uint8_t lead2_phrase[2][8][16] = {
    { { 0 } },
    /* theme B */
    { { F5, __, __, __, __, __, __, __, __, __, __, __, G5, __, __, __ },
      { A5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { C6, __, __, __, __, __, __, __, __, __, __, __, A5, __, __, __ },
      { G5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { F5, __, __, __, __, __, __, __, __, __, __, __, E5, __, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { Bb4,__, __, __, __, __, __, __, D5, __, __, __, __, __, __, __ },
      { Cs5,__, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ } },
};

/* ------------------------------------------------------------------ drums -- */

enum { DP_NONE, DP_KICK, DP_KICKHAT, DP_FULL, DP_FULL2, DP_FILL, DP_HALF,
       DP_ROLL, DP_ROLL2, DP_LAST };

#define K DR_KICK
#define S DR_SNARE
#define H DR_HAT
#define O DR_OHAT

static const uint8_t drum_pat[10][16] = {
    { 0 },
    { K,0,0,0, K,0,0,0, K,0,0,0, K,0,0,0 },
    { K,0,H,0, K,0,H,0, K,0,H,0, K,0,H,0 },
    { K|H,0,H,0, K|S,0,O,0, K|H,0,H,0, K|S,0,O,0 },
    { K|H,0,O,H, K|S,0,O,H, K|H,0,O|K,H, K|S,0,O,H },
    { K|H,0,H,0, K|S,0,H,0, K|S,0,S,0, K|S,S,S,S },
    { K,0,H,0, 0,0,H,0, K,0,H,0, 0,0,H,0 },
    { K,0,0,0, 0,0,0,0, S,S,S,S, S,S,S,S },
    { S,0,S,0, S,0,S,0, S,S,S,S, S,S,S,S },
    { K|DR_CRASH,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
};

#undef K
#undef S
#undef H
#undef O

/* ------------------------------------------------------------------- bass -- */
/* Offsets from the chord root, + 2. */

enum { BP_OFF, BP_SLOW, BP_ROLL, BP_ROLL2, BP_16, BP_ONE };

static const uint8_t bass_pat[6][16] = {
    { XX,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { 2,0,0,0, 0,0,0,0, 2,0,0,0, 0,0,0,0 },
    { 2,2,14,2, 2,14,2,2, 2,2,14,2, 2,14,2,14 },
    { 2,2,14,2, 2,14,2,9, 2,2,14,2, 2,14,9,14 },
    { 2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,14,14 },
    { 2,0,0,0, 0,0,0,0, XX,0,0,0, 0,0,0,0 },
};

/* -------------------------------------------------------------------- arp -- */
/* Index into the six-note voicing, + 2. */

enum { AP_OFF, AP_UP, AP_SLOW };

static const uint8_t arp_pat[3][16] = {
    { XX,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { 2,3,4,5, 6,7,6,5, 4,3,2,3, 4,5,6,7 },
    { 2,0,4,0, 5,0,7,0, 5,0,4,0, 2,0,4,0 },
};

/* ------------------------------------------------------------- the order -- */

#define F_CRASH 1u

typedef struct {
    uint8_t chord, drums, bass, arp, lead, lead2, flags;
    uint8_t lead_lvl, lead_cut, lead2_lvl, pad_lvl, pad_cut, drone, riser, energy;
} bar_t;

/* The main progression is Dm Bb F C Dm Bb Gm A; the forge's is Bb C Dm Dm Bb C A A. */

/* chord      drums       bass      arp      lead     lead2    flags    Llv  Lct L2lv pad  pct  drn  ris  nrg */
static const bar_t bars[CV_BARS] = {
    /* 0-7 overture: the tone alone, then the pad growing under it. */
    { CH_DM, DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0,   0,  40, 255,   0,  10 },
    { CH_DM, DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0,   0,  40, 255,   0,  12 },
    { CH_DM, DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0,  70,  40, 255,   0,  18 },
    { CH_DM, DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0,  90,  50, 255,   0,  22 },
    { CH_BB, DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 120,  60, 255,   0,  28 },
    { CH_F,  DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 140,  70, 240,   0,  32 },
    { CH_GM, DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 160,  80, 220,   0,  36 },
    { CH_A,  DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 170,  90, 200,   0,  40 },
    /* 8-23 the plain: slow bass, the arp, the kick at 16. */
    { CH_DM  , DP_NONE,    BP_SLOW, AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 100, 150,   0,  55 },
    { CH_BB  , DP_NONE,    BP_SLOW, AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 100, 150,   0,  55 },
    { CH_F   , DP_NONE,    BP_SLOW, AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 105, 150,   0,  58 },
    { CH_C   , DP_NONE,    BP_SLOW, AP_OFF,  LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 105, 150,   0,  60 },
    { CH_DM  , DP_NONE,    BP_SLOW, AP_SLOW, LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 110, 140,   0,  65 },
    { CH_BB  , DP_NONE,    BP_SLOW, AP_SLOW, LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 110, 140,   0,  68 },
    { CH_GM  , DP_NONE,    BP_SLOW, AP_SLOW, LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 115, 140,   0,  72 },
    { CH_A   , DP_NONE,    BP_SLOW, AP_SLOW, LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 120, 140,   0,  76 },
    { CH_DM  , DP_KICK,    BP_SLOW, AP_SLOW, LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 120, 130,   0,  90 },
    { CH_BB  , DP_KICK,    BP_SLOW, AP_SLOW, LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 120, 130,   0,  92 },
    { CH_F   , DP_KICK,    BP_SLOW, AP_UP,   LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 125, 120,   0,  96 },
    { CH_C   , DP_KICK,    BP_SLOW, AP_UP,   LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 125, 120,   0, 100 },
    { CH_DM  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 130, 100,   0, 110 },
    { CH_BB  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 130,  80,   0, 112 },
    { CH_GM  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 135,  60,   0, 116 },
    { CH_A   , DP_FILL,    BP_ROLL, AP_UP,   LP_NONE, L2_NONE, 0,         0,   0,   0, 170, 140,  40,   0, 125 },
    /* 24-39 the hand: theme A, quiet then full. */
    { CH_DM  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_A,    L2_NONE, F_CRASH, 200, 120,   0, 150, 140,   0,   0, 150 },
    { CH_BB  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       200, 125,   0, 150, 140,   0,   0, 150 },
    { CH_F   , DP_KICKHAT, BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       205, 130,   0, 150, 140,   0,   0, 152 },
    { CH_C   , DP_KICKHAT, BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       205, 135,   0, 150, 140,   0,   0, 154 },
    { CH_DM  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       210, 140,   0, 150, 145,   0,   0, 156 },
    { CH_BB  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       215, 150,   0, 150, 145,   0,   0, 158 },
    { CH_GM  , DP_KICKHAT, BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       220, 160,   0, 150, 150,   0,   0, 160 },
    { CH_A   , DP_FILL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       230, 170,   0, 150, 150,   0,   0, 170 },
    { CH_DM  , DP_FULL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, F_CRASH, 255, 200,   0, 140, 160,   0,   0, 200 },
    { CH_BB  , DP_FULL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       255, 200,   0, 140, 160,   0,   0, 200 },
    { CH_F   , DP_FULL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       255, 200,   0, 140, 160,   0,   0, 200 },
    { CH_C   , DP_FULL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       255, 200,   0, 140, 160,   0,   0, 200 },
    { CH_DM  , DP_FULL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       255, 205,   0, 140, 165,   0,   0, 205 },
    { CH_BB  , DP_FULL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       255, 205,   0, 140, 165,   0,   0, 205 },
    { CH_GM  , DP_FULL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       255, 210,   0, 140, 170,   0,   0, 210 },
    { CH_A   , DP_FILL,    BP_ROLL, AP_UP,   LP_A,    L2_NONE, 0,       255, 220,   0, 140, 170,   0,   0, 215 },
    /* 40-55 the heart: the variation, busier. */
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, F_CRASH, 255, 220,   0, 130, 170,   0,   0, 220 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 220,   0, 130, 170,   0,   0, 220 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 220,   0, 130, 170,   0,   0, 220 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 220,   0, 130, 170,   0,   0, 220 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 225,   0, 130, 175,   0,   0, 225 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 225,   0, 130, 175,   0,   0, 225 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 230,   0, 130, 180,   0,   0, 230 },
    { CH_A   , DP_FILL,    BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 230,   0, 130, 180,   0,   0, 232 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, F_CRASH, 255, 235,   0, 130, 185,   0,   0, 235 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 235,   0, 130, 185,   0,   0, 235 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 235,   0, 130, 185,   0,   0, 235 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 235,   0, 130, 185,   0,   0, 235 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 240,   0, 130, 190,   0,   0, 238 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 240,   0, 130, 190,   0,   0, 238 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 245,   0, 130, 195,   0,   0, 240 },
    { CH_A   , DP_FILL,    BP_ROLL2, AP_UP,  LP_A2,   L2_NONE, 0,       255, 250,   0, 130, 200,   0,   0, 245 },
    /* 56-71 the eye: breakdown. Theme B alone over the pad; then half-time. */
    { CH_DM  , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255,  60, 120,   0,  70 },
    { CH_BB  , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255,  70, 120,   0,  70 },
    { CH_F   , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255,  80, 120,   0,  72 },
    { CH_C   , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255,  90, 120,   0,  72 },
    { CH_DM  , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255, 100, 120,   0,  75 },
    { CH_BB  , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255, 110, 120,   0,  75 },
    { CH_GM  , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255, 120, 120,   0,  78 },
    { CH_A   , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 255, 130, 120,   0,  80 },
    { CH_DM  , DP_HALF,    BP_SLOW,  AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 230, 140, 100,   0, 100 },
    { CH_BB  , DP_HALF,    BP_SLOW,  AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 255, 230, 145, 100,   0, 102 },
    { CH_F   , DP_HALF,    BP_SLOW,  AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 250, 230, 150, 100,   0, 105 },
    { CH_C   , DP_HALF,    BP_SLOW,  AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 250, 230, 155,  90,   0, 108 },
    { CH_DM  , DP_HALF,    BP_SLOW,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 245, 230, 160,  80,  30, 115 },
    { CH_BB  , DP_HALF,    BP_SLOW,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 240, 230, 165,  60,  50, 120 },
    { CH_GM  , DP_KICKHAT, BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 230, 230, 170,  40,  70, 130 },
    { CH_A   , DP_ROLL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 220, 230, 175,  20,  90, 150 },
    /* 72-87 the forge: stabs, the bass pumping, the riser climbing. */
    { CH_BB   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, F_CRASH, 230, 230,   0, 150, 180,   0,  20, 235 },
    { CH_C    , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       230, 230,   0, 150, 180,   0,  40, 238 },
    { CH_DM   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       230, 235,   0, 150, 180,   0,  60, 240 },
    { CH_DM   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       230, 235,   0, 150, 180,   0,  80, 242 },
    { CH_BB   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       230, 240,   0, 150, 185,   0, 100, 244 },
    { CH_C    , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       230, 240,   0, 150, 185,   0, 120, 246 },
    { CH_A    , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       230, 245,   0, 150, 190,   0, 140, 248 },
    { CH_A    , DP_FILL,   BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       230, 245,   0, 150, 190,   0, 160, 250 },
    { CH_BB   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, F_CRASH, 235, 250,   0, 150, 195,   0, 175, 250 },
    { CH_C    , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       235, 250,   0, 150, 195,   0, 190, 252 },
    { CH_DM   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       235, 250,   0, 150, 200,   0, 205, 252 },
    { CH_DM   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       235, 255,   0, 150, 200,   0, 220, 254 },
    { CH_BB   , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       240, 255,   0, 150, 205,   0, 232, 254 },
    { CH_C    , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       240, 255,   0, 150, 205,   0, 242, 255 },
    { CH_A    , DP_FULL2,  BP_16,    AP_UP,   LP_RIS,  L2_NONE, 0,       240, 255,   0, 150, 210,   0, 250, 255 },
    { CH_A    , DP_ROLL2,  BP_OFF,   AP_UP,   LP_RIS,  L2_NONE, 0,       240, 255,   0, 150, 210,   0, 255, 255 },
    /* 88-111 the spine: A over B; B breathes with the drums; A' over B. */
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    F_CRASH, 255, 230, 200, 150, 200,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 230, 200, 150, 200,   0,   0, 255 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 230, 200, 150, 200,   0,   0, 255 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 230, 200, 150, 200,   0,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 235, 200, 150, 205,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 235, 200, 150, 205,   0,   0, 255 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 240, 200, 150, 210,   0,   0, 255 },
    { CH_A   , DP_FILL,    BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 240, 200, 150, 210,   0,   0, 255 },
    { CH_DM  , DP_FULL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    F_CRASH,   0, 200, 255, 170, 170, 100,   0, 190 },
    { CH_BB  , DP_FULL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 255, 170, 170, 100,   0, 190 },
    { CH_F   , DP_FULL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 255, 170, 170, 100,   0, 192 },
    { CH_C   , DP_FULL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 255, 170, 175, 100,   0, 194 },
    { CH_DM  , DP_FULL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 255, 170, 175, 100,   0, 196 },
    { CH_BB  , DP_FULL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 255, 170, 180,  80,   0, 198 },
    { CH_GM  , DP_FULL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 255, 170, 180,  60,   0, 200 },
    { CH_A   , DP_FILL,    BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 255, 170, 185,  40,   0, 210 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    F_CRASH, 255, 240, 210, 160, 210,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 240, 210, 160, 210,   0,   0, 255 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 240, 210, 160, 215,   0,   0, 255 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 240, 210, 160, 215,   0,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 245, 210, 160, 220,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 245, 210, 160, 220,   0,   0, 255 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 250, 210, 160, 225,   0,   0, 255 },
    { CH_A   , DP_FILL,    BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 250, 210, 160, 225,   0,  60, 255 },
    /* 112-127 the crown: up a tone (song_transpose). A over B, then A' over B. */
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    F_CRASH, 255, 250, 220, 160, 240,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 250, 220, 160, 240,   0,   0, 255 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 250, 220, 160, 240,   0,   0, 255 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 250, 220, 160, 240,   0,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 160, 245,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 160, 245,   0,   0, 255 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 160, 250,   0,   0, 255 },
    { CH_A   , DP_FILL,    BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 160, 250,   0,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    F_CRASH, 255, 255, 220, 160, 255,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 255, 220, 160, 255,   0,   0, 255 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 255, 220, 160, 255,   0,   0, 255 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 255, 220, 160, 255,   0,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 255, 220, 160, 255,   0,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 255, 220, 160, 255,   0,   0, 255 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 255, 220, 160, 255,   0,  80, 255 },
    { CH_A   , DP_ROLL2,   BP_ROLL2, AP_UP,   LP_A2,   L2_B,    0,       255, 255, 220, 160, 255,   0, 200, 255 },
    /* 128-143 the colossus: home in D, the tone back under everything. */
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    F_CRASH, 255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_A   , DP_FILL,    BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    F_CRASH, 255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_F   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_C   , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_DM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_BB  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_GM  , DP_FULL2,   BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    { CH_A   , DP_FILL,    BP_ROLL2, AP_UP,   LP_A,    L2_B,    0,       255, 255, 220, 170, 255, 200,   0, 255 },
    /* 144-159 coda: B thins over the pad; the drums leave; the tone is left. */
    { CH_DM  , DP_KICKHAT, BP_ROLL,  AP_UP,   LP_NONE, L2_B,    F_CRASH,   0, 200, 255, 200, 200, 200,   0, 140 },
    { CH_BB  , DP_KICKHAT, BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 245, 200, 195, 200,   0, 130 },
    { CH_F   , DP_KICKHAT, BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 235, 200, 190, 200,   0, 120 },
    { CH_C   , DP_KICKHAT, BP_ROLL,  AP_UP,   LP_NONE, L2_B,    0,         0, 200, 225, 200, 185, 200,   0, 110 },
    { CH_DM  , DP_KICK,    BP_SLOW,  AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 215, 210, 170, 210,   0,  90 },
    { CH_BB  , DP_KICK,    BP_SLOW,  AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 205, 210, 160, 210,   0,  80 },
    { CH_GM  , DP_NONE,    BP_SLOW,  AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 190, 220, 150, 220,   0,  60 },
    { CH_A   , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 170, 220, 140, 220,   0,  50 },
    { CH_DM  , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 150, 220, 120, 230,   0,  40 },
    { CH_BB  , DP_NONE,    BP_OFF,   AP_SLOW, LP_NONE, L2_B,    0,         0, 200, 130, 220, 110, 230,   0,  36 },
    { CH_F   , DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_B,    0,         0, 200, 110, 210, 100, 240,   0,  30 },
    { CH_C   , DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_B,    0,         0, 200,  90, 200,  90, 240,   0,  26 },
    { CH_DM,   DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0, 200,   0, 170,  80, 250,   0,  20 },
    { CH_DM,   DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0, 200,   0, 120,  70, 255,   0,  16 },
    { CH_DM,   DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0, 200,   0,   0,  60, 255,   0,  12 },
    { CH_DM,   DP_NONE,    BP_OFF,   AP_OFF,  LP_NONE, L2_NONE, 0,         0, 200,   0,   0,  60, 255,   0,   8 },
};

static const bar_t *row(uint32_t bar)
{
    return &bars[bar < CV_BARS ? bar : CV_BARS - 1];
}

/* ------------------------------------------------------------- accessors -- */

int song_transpose(uint32_t bar) { return bar >= 112 && bar < 128 ? 2 : 0; }

static const char *const k_section_names[] = {
    "overture", "the plain", "the hand", "the heart", "the eye",
    "the forge", "the spine", "the crown", "the colossus", "coda",
};

int song_section(uint32_t bar)
{
    if (bar < 8)   return 0;
    if (bar < 24)  return 1;
    if (bar < 40)  return 2;
    if (bar < 56)  return 3;
    if (bar < 72)  return 4;
    if (bar < 88)  return 5;
    if (bar < 112) return 6;
    if (bar < 128) return 7;
    if (bar < 144) return 8;
    return 9;
}

const char *song_section_name(int s) { return (s >= 0 && s < 10) ? k_section_names[s] : "?"; }

uint8_t song_drums(uint32_t step)
{
    const uint32_t bar = step / CV_STEPS_PER_BAR, s = step % CV_STEPS_PER_BAR;
    if (bar >= CV_BARS) return 0;
    uint8_t d = drum_pat[row(bar)->drums][s];
    if (s == 0 && (row(bar)->flags & F_CRASH)) d |= DR_CRASH;
    return d;
}

/* A pattern entry of 0/1 passes through; anything else is an offset + 2
 * on top of `base`, transposed. */
static int rel_event(uint8_t e, int base, uint32_t bar)
{
    if (e < 2) return e;
    return base + (int)e - 2 + song_transpose(bar);
}

static int abs_event(uint8_t e, uint32_t bar)
{
    if (e < 2) return e;
    return (int)e + song_transpose(bar);
}

int song_bass(uint32_t step)
{
    const uint32_t bar = step / CV_STEPS_PER_BAR, s = step % CV_STEPS_PER_BAR;
    if (bar >= CV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    return rel_event(bass_pat[r->bass][s], chord_root[r->chord], bar);
}

int song_arp(uint32_t step)
{
    const uint32_t bar = step / CV_STEPS_PER_BAR, s = step % CV_STEPS_PER_BAR;
    if (bar >= CV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    const uint8_t e = arp_pat[r->arp][s];
    if (e < 2) return e;
    return chord_arp[r->chord][e - 2] + song_transpose(bar);
}

int song_lead(uint32_t step)
{
    const uint32_t bar = step / CV_STEPS_PER_BAR, s = step % CV_STEPS_PER_BAR;
    if (bar >= CV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->lead == LP_NONE) return s == 0 ? SONG_OFF : 0;
    return abs_event(lead_phrase[r->lead][bar & 7][s], bar);
}

int song_lead2(uint32_t step)
{
    const uint32_t bar = step / CV_STEPS_PER_BAR, s = step % CV_STEPS_PER_BAR;
    if (bar >= CV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->lead2 == L2_NONE) return s == 0 ? SONG_OFF : 0;
    return abs_event(lead2_phrase[r->lead2][bar & 7][s], bar);
}

void song_pad_chord(uint32_t bar, uint8_t out[4])
{
    const bar_t *r = row(bar);
    for (int i = 0; i < 4; i++)
        out[i] = (bar < CV_BARS && r->pad_lvl)
               ? (uint8_t)(chord_pad[r->chord][i] + song_transpose(bar)) : 0;
}

int song_lead_level(uint32_t bar)  { return bar < CV_BARS ? row(bar)->lead_lvl  : 0; }
int song_lead_cut(uint32_t bar)    { return row(bar)->lead_cut; }
int song_lead2_level(uint32_t bar) { return bar < CV_BARS ? row(bar)->lead2_lvl : 0; }
int song_pad_level(uint32_t bar)   { return bar < CV_BARS ? row(bar)->pad_lvl   : 0; }
int song_pad_cut(uint32_t bar)     { return row(bar)->pad_cut; }
int song_drone_level(uint32_t bar) { return bar < CV_BARS ? row(bar)->drone     : 0; }
int song_riser(uint32_t bar)       { return bar < CV_BARS ? row(bar)->riser     : 0; }
int song_energy(uint32_t bar)      { return bar < CV_BARS ? row(bar)->energy    : 0; }

uint32_t song_voices(uint32_t bar)
{
    if (bar >= CV_BARS) return 0;
    const bar_t *r = row(bar);
    uint32_t v = 0, dm = 0;
    for (int s = 0; s < 16; s++) dm |= drum_pat[r->drums][s];
    if (dm & DR_KICK)              v |= SV_KICK;
    if (dm & DR_SNARE)             v |= SV_SNARE;
    if (dm & (DR_HAT | DR_OHAT))   v |= SV_HAT;
    if (r->bass  != BP_OFF)        v |= SV_BASS;
    if (r->arp   != AP_OFF)        v |= SV_ARP;
    if (r->lead  != LP_NONE)       v |= SV_LEAD;
    if (r->lead2 != L2_NONE)       v |= SV_LEAD2;
    if (r->pad_lvl)                v |= SV_PAD;
    if (r->riser)                  v |= SV_RISER;
    if (r->drone)                  v |= SV_DRONE;
    return v;
}
