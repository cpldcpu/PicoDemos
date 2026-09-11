/* PERSISTENCE -- the song.
 *
 * A tracker tune in A minor at 144 BPM, ninety bars, modulating to B minor
 * for the last chorus. Written as tables, on purpose: PLANNING.md section 6
 * says the hook has to survive being hummed, and a melody you can hum is a
 * melody somebody chose note by note. Nothing here is generated.
 *
 * ---------------------------------------------------------------- the hook --
 *
 * Theme A, over Am | F | C | G, eight bars. A leap up a fifth and a walk
 * back down, then the same shape a third lower as the answer:
 *
 *      A4  E5 D5 C5 B4  |  C5  A4 G4 A4  |  G4  C5 E5 D5 C5  |  D5  B4 A4 B4
 *      (repeat)         |                 |  G4  C5 E5 G5 A5  |  B5 A5 G5 E5 D5
 *
 * Bars 7-8 lift the second answer up to the octave, which is the bit people
 * remember. The rhythm is the same in every bar -- a long note on the beat,
 * the second note on beat two, the third on the "a" of two -- so the
 * syncopation is a signature rather than an accident.
 *
 * Theme B is the counter-melody, over the same changes so the two can be
 * stacked in the finale: long notes an octave up, an arch that falls --
 *
 *      E5 ...... C6 B5  |  A5 ........ G5 F5  |  G5 .... E5 G5  |  D5 ......
 *      E5 ...... C6 B5  |  A5 ........ C6     |  G5 .... E5 D5  |  D5 .. B4
 *
 * ---------------------------------------------------------------- the form --
 *
 *   bar  0   intro       pad, arp; kick from 4; hats from 6         Am F C G
 *   bar  8   plasma      bass + hats in; theme A stated quiet, dark
 *   bar 16   kefrens     theme A, full
 *   bar 24   twister     theme A', the variation, busier drums
 *   bar 32   tunnel      breakdown: no drums, pad, filtered lead    Am Am F F Dm Dm E E
 *   bar 40   plane       theme B on its own
 *   bar 48   plane       theme A over theme B
 *   bar 56   split       riser: stabs, pumping bass, noise sweep    F G Am Am F G E E
 *   bar 64   finale      A + B together
 *   bar 72   finale      the same, up a tone: B minor
 *   bar 80   scroller    outro, B melody thinning out
 *   bar 88   power-off   one hit and the chord ringing; then nothing
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
#include "persistence.h"

/* ---------------------------------------------------------------- pitches -- */

enum {
    F4 = 65, G4 = 67, A4 = 69, B4 = 71,
    C5 = 72, D5 = 74, E5 = 76, F5 = 77, G5 = 79, Gs5 = 80, A5 = 81, B5 = 83,
    C6 = 84
};
#define __  0           /* hold */
#define XX  SONG_OFF    /* note off */

/* ----------------------------------------------------------------- chords -- */

enum { CH_AM, CH_F, CH_C, CH_G, CH_DM, CH_E };

static const uint8_t chord_root[6] = { 33, 41, 36, 31, 38, 40 };   /* A1 F2 C2 G1 D2 E2 */

static const uint8_t chord_pad[6][4] = {
    { 57, 60, 64, 69 },     /* Am  */
    { 57, 60, 65, 69 },     /* F   */
    { 55, 60, 64, 67 },     /* C   */
    { 55, 59, 62, 67 },     /* G   */
    { 57, 62, 65, 69 },     /* Dm  */
    { 56, 59, 64, 68 },     /* E   -- the G# is the point */
};

static const uint8_t chord_arp[6][6] = {
    { 45, 48, 52, 57, 60, 64 },
    { 41, 45, 48, 53, 57, 60 },
    { 48, 52, 55, 60, 64, 67 },
    { 43, 47, 50, 55, 59, 62 },
    { 50, 53, 57, 62, 65, 69 },
    { 52, 56, 59, 64, 68, 71 },
};

/* ------------------------------------------------------------ lead phrases -- */

enum { LP_NONE, LP_A, LP_A2, LP_BRK, LP_RIS };

static const uint8_t lead_phrase[5][8][16] = {
    { { 0 } },
    /* theme A */
    { { A4, __, __, __, E5, __, __, D5, __, C5, __, __, B4, __, __, __ },
      { C5, __, __, __, A4, __, __, __, __, __, G4, __, A4, __, __, __ },
      { G4, __, __, __, C5, __, __, E5, __, D5, __, __, C5, __, __, __ },
      { D5, __, __, __, B4, __, __, __, __, __, A4, __, B4, __, XX, __ },
      { A4, __, __, __, E5, __, __, D5, __, C5, __, __, B4, __, __, __ },
      { C5, __, __, __, A4, __, __, __, __, __, G4, __, A4, __, __, __ },
      { G4, __, __, __, C5, __, __, E5, __, G5, __, __, A5, __, __, __ },
      { B5, __, __, A5, __, __, G5, __, E5, __, __, __, D5, __, XX, __ } },
    /* theme A' -- the same bones, doubled notes and a lifted second half */
    { { A4, __, A4, __, E5, __, __, D5, __, C5, __, __, B4, __, C5, __ },
      { C5, __, __, __, A4, __, __, __, C5, __, A4, __, G4, __, A4, __ },
      { G4, __, G4, __, C5, __, __, E5, __, D5, __, __, C5, __, D5, __ },
      { D5, __, __, __, B4, __, __, __, D5, __, B4, __, A4, __, B4, XX },
      { E5, __, __, __, A5, __, __, G5, __, E5, __, __, D5, __, __, __ },
      { C5, __, __, __, F5, __, __, E5, __, C5, __, __, A4, __, __, __ },
      { G5, __, __, E5, __, __, C5, __, E5, __, __, __, G5, __, __, __ },
      { B5, __, __, G5, __, __, D5, __, B4, __, __, __, A4, __, XX, __ } },
    /* breakdown -- the hook at half speed, over Am Am F F Dm Dm E E */
    { { A4, __, __, __, __, __, __, __, E5, __, __, __, __, __, __, __ },
      { D5, __, __, __, C5, __, __, __, B4, __, __, __, __, __, __, __ },
      { C5, __, __, __, __, __, __, __, A4, __, __, __, __, __, __, __ },
      { G4, __, __, __, A4, __, __, __, __, __, __, __, __, __, __, __ },
      { F4, __, __, __, __, __, __, __, A4, __, __, __, __, __, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { E5, __, __, __, __, __, __, __, Gs5,__, __, __, __, __, __, __ },
      { B5, __, __, __, __, __, __, __, __, __, __, __, XX, __, __, __ } },
    /* riser -- stabs, over F G Am Am F G E E; the last bar drops out */
    { { F5, __, XX, F5, __, XX, F5, XX, F5, __, XX, F5, __, XX, F5, XX },
      { G5, __, XX, G5, __, XX, G5, XX, G5, __, XX, G5, __, XX, G5, XX },
      { A5, __, XX, A5, __, XX, A5, XX, A5, __, XX, A5, __, XX, A5, XX },
      { A5, __, XX, A5, __, XX, A5, XX, A5, __, XX, B5, __, XX, C6, XX },
      { F5, __, XX, F5, __, XX, F5, XX, F5, __, XX, F5, __, XX, F5, XX },
      { G5, __, XX, G5, __, XX, G5, XX, G5, __, XX, G5, __, XX, G5, XX },
      { B5, __, XX, B5, __, XX, B5, XX, B5, __, XX, B5, __, XX, B5, XX },
      { B5, __, XX, B5, __, XX, B5, XX, B5, __, __, __, __, XX, __, __ } },
};

enum { L2_NONE, L2_B };

static const uint8_t lead2_phrase[2][8][16] = {
    { { 0 } },
    /* theme B */
    { { E5, __, __, __, __, __, __, __, C6, __, __, __, B5, __, __, __ },
      { A5, __, __, __, __, __, __, __, __, __, __, __, G5, __, F5, __ },
      { G5, __, __, __, __, __, __, __, E5, __, __, __, G5, __, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { E5, __, __, __, __, __, __, __, C6, __, __, __, B5, __, __, __ },
      { A5, __, __, __, __, __, __, __, __, __, __, __, C6, __, __, __ },
      { G5, __, __, __, __, __, __, __, E5, __, __, __, D5, __, __, __ },
      { D5, __, __, __, __, __, __, __, B4, __, __, __, __, __, XX, __ } },
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
    { K,0,0,0, 0,0,0,0, K,0,0,0, 0,0,0,0 },
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

enum { BP_OFF, BP_ROLL, BP_ROLL2, BP_16, BP_ONE };

static const uint8_t bass_pat[5][16] = {
    { XX,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
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
    uint8_t lead_lvl, lead_cut, lead2_lvl, pad_lvl, riser, energy;
} bar_t;

/* chord      drums       bass      arp       lead     lead2    flags    Llv  Lct  L2lv pad  ris  nrg */
static const bar_t bars[PV_BARS] = {
    /* 0-7 intro: the beam. Pad breathes, arp from bar 2, kick from 4. */
    { CH_AM, DP_NONE,    BP_OFF,   AP_OFF,   LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  20 },
    { CH_F,  DP_NONE,    BP_OFF,   AP_OFF,   LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  25 },
    { CH_C,  DP_NONE,    BP_OFF,   AP_SLOW,  LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  35 },
    { CH_G,  DP_NONE,    BP_OFF,   AP_SLOW,  LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  40 },
    { CH_AM, DP_KICK,    BP_OFF,   AP_UP,    LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  60 },
    { CH_F,  DP_KICK,    BP_OFF,   AP_UP,    LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  65 },
    { CH_C,  DP_KICKHAT, BP_OFF,   AP_UP,    LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  75 },
    { CH_G,  DP_FILL,    BP_OFF,   AP_UP,    LP_NONE, L2_NONE, 0,        0,   0,   0, 200,   0,  90 },
    /* 8-15 plasma: bass and hats, the hook quiet and dark. */
    { CH_AM, DP_KICKHAT, BP_ROLL,  AP_UP,    LP_A,    L2_NONE, F_CRASH, 120,  70,   0, 150,   0, 120 },
    { CH_F,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       120,  70,   0, 150,   0, 120 },
    { CH_C,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       120,  80,   0, 150,   0, 125 },
    { CH_G,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       120,  80,   0, 150,   0, 125 },
    { CH_AM, DP_KICKHAT, BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       130,  90,   0, 150,   0, 130 },
    { CH_F,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       130,  90,   0, 150,   0, 130 },
    { CH_C,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       140, 100,   0, 150,   0, 135 },
    { CH_G,  DP_FILL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       150, 110,   0, 150,   0, 150 },
    /* 16-23 kefrens: theme A, full. */
    { CH_AM, DP_FULL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, F_CRASH, 255, 200,   0, 140,   0, 200 },
    { CH_F,  DP_FULL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       255, 200,   0, 140,   0, 200 },
    { CH_C,  DP_FULL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       255, 200,   0, 140,   0, 200 },
    { CH_G,  DP_FULL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       255, 200,   0, 140,   0, 200 },
    { CH_AM, DP_FULL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       255, 200,   0, 140,   0, 205 },
    { CH_F,  DP_FULL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       255, 200,   0, 140,   0, 205 },
    { CH_C,  DP_FULL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       255, 210,   0, 140,   0, 210 },
    { CH_G,  DP_FILL,    BP_ROLL,  AP_UP,    LP_A,    L2_NONE, 0,       255, 220,   0, 140,   0, 215 },
    /* 24-31 twister: the variation. */
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 220,   0, 140,   0, 220 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 220,   0, 140,   0, 220 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 220,   0, 140,   0, 220 },
    { CH_G,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 220,   0, 140,   0, 220 },
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 230,   0, 140,   0, 225 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 230,   0, 140,   0, 225 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 240,   0, 140,   0, 230 },
    { CH_G,  DP_FILL,    BP_ROLL2, AP_UP,    LP_A2,   L2_NONE, 0,       255, 240,   0, 140,   0, 235 },
    /* 32-39 tunnel: breakdown. Drums out, pad up, the lead opens its filter. */
    { CH_AM, DP_NONE,    BP_OFF,   AP_SLOW,  LP_BRK,  L2_NONE, 0,       220,  30,   0, 255,   0,  60 },
    { CH_AM, DP_NONE,    BP_OFF,   AP_SLOW,  LP_BRK,  L2_NONE, 0,       220,  50,   0, 255,   0,  60 },
    { CH_F,  DP_NONE,    BP_OFF,   AP_SLOW,  LP_BRK,  L2_NONE, 0,       220,  75,   0, 255,   0,  65 },
    { CH_F,  DP_NONE,    BP_OFF,   AP_SLOW,  LP_BRK,  L2_NONE, 0,       220, 105,   0, 255,   0,  65 },
    { CH_DM, DP_NONE,    BP_OFF,   AP_SLOW,  LP_BRK,  L2_NONE, 0,       230, 140,   0, 255,   0,  70 },
    { CH_DM, DP_NONE,    BP_OFF,   AP_SLOW,  LP_BRK,  L2_NONE, 0,       230, 180,   0, 255,   0,  75 },
    { CH_E,  DP_HALF,    BP_OFF,   AP_SLOW,  LP_BRK,  L2_NONE, 0,       240, 220,   0, 255,  30,  90 },
    { CH_E,  DP_ROLL,    BP_OFF,   AP_UP,    LP_BRK,  L2_NONE, 0,       255, 255,   0, 255,  90, 120 },
    /* 40-47 the plane: theme B alone. */
    { CH_AM, DP_FULL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    F_CRASH,   0, 200, 255, 160,   0, 180 },
    { CH_F,  DP_FULL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 255, 160,   0, 180 },
    { CH_C,  DP_FULL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 255, 160,   0, 180 },
    { CH_G,  DP_FULL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 255, 160,   0, 180 },
    { CH_AM, DP_FULL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 255, 160,   0, 185 },
    { CH_F,  DP_FULL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 255, 160,   0, 185 },
    { CH_C,  DP_FULL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 255, 160,   0, 190 },
    { CH_G,  DP_FILL,    BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 255, 160,   0, 195 },
    /* 48-55 the plane: theme A over theme B. */
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    F_CRASH, 230, 200, 200, 140,   0, 220 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       230, 200, 200, 140,   0, 220 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       230, 200, 200, 140,   0, 220 },
    { CH_G,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       230, 200, 200, 140,   0, 220 },
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       230, 210, 200, 140,   0, 225 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       230, 210, 200, 140,   0, 225 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       230, 220, 200, 140,   0, 230 },
    { CH_G,  DP_FILL,    BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       230, 220, 200, 140,   0, 235 },
    /* 56-63 raster split: the riser. */
    { CH_F,  DP_FULL2,   BP_16,    AP_UP,    LP_RIS,  L2_NONE, 0,       230, 230,   0, 150,  20, 235 },
    { CH_G,  DP_FULL2,   BP_16,    AP_UP,    LP_RIS,  L2_NONE, 0,       230, 230,   0, 150,  50, 240 },
    { CH_AM, DP_FULL2,   BP_16,    AP_UP,    LP_RIS,  L2_NONE, 0,       230, 235,   0, 150,  80, 240 },
    { CH_AM, DP_FULL2,   BP_16,    AP_UP,    LP_RIS,  L2_NONE, 0,       230, 235,   0, 150, 110, 245 },
    { CH_F,  DP_FULL2,   BP_16,    AP_UP,    LP_RIS,  L2_NONE, 0,       230, 240,   0, 150, 150, 245 },
    { CH_G,  DP_FULL2,   BP_16,    AP_UP,    LP_RIS,  L2_NONE, 0,       230, 240,   0, 150, 190, 250 },
    { CH_E,  DP_FULL2,   BP_16,    AP_UP,    LP_RIS,  L2_NONE, 0,       230, 250,   0, 150, 230, 250 },
    { CH_E,  DP_ROLL2,   BP_OFF,   AP_UP,    LP_RIS,  L2_NONE, 0,       230, 255,   0, 150, 255, 255 },
    /* 64-71 finale: A + B. */
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    F_CRASH, 255, 230, 200, 150,   0, 255 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 230, 200, 150,   0, 255 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 230, 200, 150,   0, 255 },
    { CH_G,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 230, 200, 150,   0, 255 },
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 230, 200, 150,   0, 255 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 230, 200, 150,   0, 255 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 240, 200, 150,   0, 255 },
    { CH_G,  DP_FILL,    BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 240, 200, 150,   0, 255 },
    /* 72-79 finale, up a tone (song_transpose). */
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    F_CRASH, 255, 240, 210, 150,   0, 255 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 240, 210, 150,   0, 255 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 240, 210, 150,   0, 255 },
    { CH_G,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 240, 210, 150,   0, 255 },
    { CH_AM, DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 240, 210, 150,   0, 255 },
    { CH_F,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 240, 210, 150,   0, 255 },
    { CH_C,  DP_FULL2,   BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 250, 210, 150,   0, 255 },
    { CH_G,  DP_FILL,    BP_ROLL2, AP_UP,    LP_A,    L2_B,    0,       255, 250, 210, 150,   0, 255 },
    /* 80-87 scroller: outro. Theme B thins out. */
    { CH_AM, DP_KICKHAT, BP_ROLL,  AP_UP,    LP_NONE, L2_B,    F_CRASH,   0, 200, 255, 200,   0, 140 },
    { CH_F,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 240, 200,   0, 130 },
    { CH_C,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 220, 200,   0, 120 },
    { CH_G,  DP_KICKHAT, BP_ROLL,  AP_UP,    LP_NONE, L2_B,    0,         0, 200, 200, 200,   0, 110 },
    { CH_AM, DP_KICK,    BP_OFF,   AP_SLOW,  LP_NONE, L2_B,    0,         0, 200, 180, 210,   0,  80 },
    { CH_F,  DP_KICK,    BP_OFF,   AP_SLOW,  LP_NONE, L2_B,    0,         0, 200, 160, 210,   0,  70 },
    { CH_C,  DP_NONE,    BP_OFF,   AP_SLOW,  LP_NONE, L2_B,    0,         0, 200, 130, 220,   0,  50 },
    { CH_G,  DP_NONE,    BP_OFF,   AP_SLOW,  LP_NONE, L2_B,    0,         0, 200, 100, 220,   0,  40 },
    /* 88-89 power-off: one hit, the chord rings, then nothing. */
    { CH_AM, DP_LAST,    BP_ONE,   AP_OFF,   LP_NONE, L2_NONE, 0,         0, 200,   0, 230,   0,  60 },
    { CH_AM, DP_NONE,    BP_OFF,   AP_OFF,   LP_NONE, L2_NONE, 0,         0, 200,   0,   0,   0,   0 },
};

static const bar_t *row(uint32_t bar)
{
    return &bars[bar < PV_BARS ? bar : PV_BARS - 1];
}

/* ------------------------------------------------------------- accessors -- */

int song_transpose(uint32_t bar) { return bar >= 72 && bar < PV_BARS ? 2 : 0; }

int song_section(uint32_t bar)
{
    if (bar < 8)  return 0;     /* the beam / title */
    if (bar < 16) return 1;     /* plasma           */
    if (bar < 24) return 2;     /* kefrens          */
    if (bar < 32) return 3;     /* twister          */
    if (bar < 40) return 4;     /* tunnel           */
    if (bar < 56) return 5;     /* the plane        */
    if (bar < 64) return 6;     /* raster split     */
    if (bar < 80) return 7;     /* finale           */
    if (bar < 88) return 8;     /* scroller         */
    return 9;                   /* power-off        */
}

uint8_t song_drums(uint32_t step)
{
    const uint32_t bar = step / PV_STEPS_PER_BAR, s = step % PV_STEPS_PER_BAR;
    if (bar >= PV_BARS) return 0;
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
    const uint32_t bar = step / PV_STEPS_PER_BAR, s = step % PV_STEPS_PER_BAR;
    if (bar >= PV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    return rel_event(bass_pat[r->bass][s], chord_root[r->chord], bar);
}

int song_arp(uint32_t step)
{
    const uint32_t bar = step / PV_STEPS_PER_BAR, s = step % PV_STEPS_PER_BAR;
    if (bar >= PV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    const uint8_t e = arp_pat[r->arp][s];
    if (e < 2) return e;
    return chord_arp[r->chord][e - 2] + song_transpose(bar);
}

int song_lead(uint32_t step)
{
    const uint32_t bar = step / PV_STEPS_PER_BAR, s = step % PV_STEPS_PER_BAR;
    if (bar >= PV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->lead == LP_NONE) return s == 0 ? SONG_OFF : 0;
    return abs_event(lead_phrase[r->lead][bar & 7][s], bar);
}

int song_lead2(uint32_t step)
{
    const uint32_t bar = step / PV_STEPS_PER_BAR, s = step % PV_STEPS_PER_BAR;
    if (bar >= PV_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->lead2 == L2_NONE) return s == 0 ? SONG_OFF : 0;
    return abs_event(lead2_phrase[r->lead2][bar & 7][s], bar);
}

void song_pad_chord(uint32_t bar, uint8_t out[4])
{
    const bar_t *r = row(bar);
    for (int i = 0; i < 4; i++)
        out[i] = (bar < PV_BARS && r->pad_lvl)
               ? (uint8_t)(chord_pad[r->chord][i] + song_transpose(bar)) : 0;
}

int song_lead_level(uint32_t bar)  { return bar < PV_BARS ? row(bar)->lead_lvl  : 0; }
int song_lead_cut(uint32_t bar)    { return row(bar)->lead_cut; }
int song_lead2_level(uint32_t bar) { return bar < PV_BARS ? row(bar)->lead2_lvl : 0; }
int song_pad_level(uint32_t bar)   { return bar < PV_BARS ? row(bar)->pad_lvl   : 0; }
int song_riser(uint32_t bar)       { return bar < PV_BARS ? row(bar)->riser     : 0; }
int song_energy(uint32_t bar)      { return bar < PV_BARS ? row(bar)->energy    : 0; }

uint32_t song_voices(uint32_t bar)
{
    if (bar >= PV_BARS) return 0;
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
    return v;
}
