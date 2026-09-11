/* PELAGIC -- the song.
 *
 * E major at 125 BPM, eighty bars in ten phrases of eight, lifting to F#
 * for the ascent and staying there for the surface. Written as tables, on
 * purpose: Phase asked for a melody we remember after the screen goes dark,
 * and a melody anyone remembers is a melody somebody chose note by note.
 * Nothing here is generated.
 *
 * ------------------------------------------------------------- the theme --
 *
 * Theme A, the ray's tune, over E | C#m7 | Aadd9 | Bsus4. A rising fourth
 * that opens like a wing, a climb to the third, a fall, and a held note:
 *
 *      B4 ...... E5 ....  |  F#5 .. G#5 .... E5 ..  |  C#5 ...... B4 .. A4 ..  |  B4 ..............
 *      B4 ...... E5 ....  |  F#5 .. G#5 .... B5 ..  |  A5 ....... G#5 .. F#5 . |  E5 ..............
 *
 * The first half asks (it ends on the fifth, over the dominant), the second
 * answers, one note higher at the peak and home on E. The opening interval
 * B-E is the fragment: it is what the droplets play before the tune exists
 * (bars 2-7), what the bells play in the abyss, and what is left at the end,
 * inverted, as the ray leaves.
 *
 * Theme B, the wings, is long notes over Aadd9 | Bsus4 | C#m7 | E -- one or
 * two notes a bar, reaching B5 and then C#6:
 *
 *      E5 ..  |  F#5 . G#5 .  |  B5 ..  |  A5 . G#5 .  |  F#5 ..  |  E5 . F#5 .  |  C#6 ..  |  B5 . G#5 .
 *
 * The descent (bars 40-47) is theme A moved into C# minor and down an
 * octave, in the hollow voice, over C#m | Aadd9 | F#m7 | G#sus4: the same
 * intervals, a darker place. The dissonance is the G# suspension, and it
 * resolves.
 *
 * -------------------------------------------------------------- the form --
 *
 *   bars    picture       music
 *   0-7     opening       drone on E, the pad waking, shimmer, the fragment as droplets
 *   8-23    the reef      bass and droplets; the hollow voice hints the tune (12); THEME A plain (16)
 *   24-39   encounter     theme A with the octave and the whoosh (24); THEME B, choir under it (32)
 *   40-47   descent       the tune in C# minor, an octave down; a boom every other bar
 *   48-55   the abyss     two bars a chord, bells on the fragment, the glass organ blooming
 *   56-63   the bloom     THEME B full: choir, glass, pad, plucks, soft drums -- the climax
 *   64-71   ascent        THEME A up a tone in F#, four soft kicks a bar, shakers
 *   72-79   surface       the harmony lands on F#; percussion leaves; the fragment inverts; fade
 *
 * Every section starts on a multiple of eight, so a phrase's bar is bar & 7.
 *
 * ROW EVENTS. Pitched tables use 0 = hold, 1 = note off, else a MIDI note
 * (or, for patterns relative to a chord, an offset + 2). Held notes carry
 * across bars. A voice whose phrase is "none" is sent a note off on the
 * first step of the bar.
 */

#include "song.h"

/* ---------------------------------------------------------------- pitches -- */

enum {
    A3 = 57, B3 = 59, Cs4 = 61, E4 = 64, Fs4 = 66, Gs4 = 68, A4 = 69, B4 = 71,
    Cs5 = 73, Ds5 = 75, E5 = 76, Fs5 = 78, Gs5 = 80, A5 = 81, B5 = 83, Cs6 = 85
};
#define __  0           /* hold */
#define XX  SONG_OFF    /* note off */

/* ----------------------------------------------------------------- chords -- */

enum { CH_E, CH_CSM, CH_A, CH_B, CH_FSM, CH_GS, CH_A11 };

static const uint8_t chord_root[7] = { 40, 37, 33, 35, 42, 44, 33 };  /* E2 C#2 A1 B1 F#2 G#2 A1 */

/* The pad's four notes, voiced so that neighbours share tones. */
static const uint8_t chord_pad[7][4] = {
    { 59, 64, 66, 68 },     /* Eadd9:     B3 E4 F#4 G#4              */
    { 59, 61, 64, 68 },     /* C#m7:      B3 C#4 E4 G#4              */
    { 57, 59, 61, 64 },     /* Aadd9:     A3 B3 C#4 E4               */
    { 59, 61, 64, 66 },     /* Bsus4add9: B3 C#4 E4 F#4              */
    { 57, 61, 64, 66 },     /* F#m7:      A3 C#4 E4 F#4              */
    { 56, 61, 63, 68 },     /* G#sus4:    G#3 C#4 D#4 G#4            */
    { 57, 61, 63, 68 },     /* Amaj7#11:  A3 C#4 D#4 G#4 -- the abyss */
};

/* Six notes for the droplets, low to high. */
static const uint8_t chord_pluck[7][6] = {
    { 52, 56, 59, 64, 66, 71 },
    { 52, 56, 59, 61, 64, 68 },
    { 52, 57, 59, 61, 64, 69 },
    { 54, 59, 61, 64, 66, 71 },
    { 54, 57, 61, 64, 66, 69 },
    { 56, 61, 63, 68, 73, 75 },
    { 57, 61, 64, 68, 71, 75 },
};

/* ------------------------------------------------------------ lead phrases -- */

enum { LP_NONE, LP_A, LP_B };

static const uint8_t lead_phrase[3][8][16] = {
    { { 0 } },
    /* theme A -- the ray */
    { { B4, __, __, __, __, __, __, __, E5, __, __, __, __, __, __, __ },
      { Fs5,__, __, __, __, __, Gs5,__, __, __, __, __, E5, __, __, __ },
      { Cs5,__, __, __, __, __, __, __, B4, __, __, __, A4, __, __, __ },
      { B4, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ },
      { B4, __, __, __, __, __, __, __, E5, __, __, __, __, __, __, __ },
      { Fs5,__, __, __, __, __, Gs5,__, __, __, __, __, B5, __, __, __ },
      { A5, __, __, __, __, __, __, __, Gs5,__, __, __, Fs5,__, __, __ },
      { E5, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ } },
    /* theme B -- the wings, over A B C#m E */
    { { E5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { Fs5,__, __, __, __, __, __, __, Gs5,__, __, __, __, __, __, __ },
      { B5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { A5, __, __, __, __, __, __, __, Gs5,__, __, __, __, __, __, __ },
      { Fs5,__, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { E5, __, __, __, __, __, __, __, Fs5,__, __, __, __, __, __, __ },
      { Cs6,__, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { B5, __, __, __, __, __, __, __, Gs5,__, __, __, __, __, XX, __ } },
};

enum { L2_NONE, L2_HINT, L2_DESC };

static const uint8_t lead2_phrase[3][8][16] = {
    { { 0 } },
    /* the hint: theme A's first half, an octave down, in bars 12-15 only */
    { { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { B3, __, __, __, __, __, __, __, E4, __, __, __, __, __, __, __ },
      { Fs4,__, __, __, __, __, Gs4,__, __, __, __, __, E4, __, __, __ },
      { Cs4,__, __, __, __, __, __, __, B3, __, __, __, A3, __, __, __ },
      { B3, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ } },
    /* the descent: theme A in C# minor, an octave down, over C#m A F#m G#sus */
    { { Gs4,__, __, __, __, __, __, __, Cs5,__, __, __, __, __, __, __ },
      { Ds5,__, __, __, __, __, E5, __, __, __, __, __, Cs5,__, __, __ },
      { A4, __, __, __, __, __, __, __, Gs4,__, __, __, Fs4,__, __, __ },
      { Gs4,__, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ },
      { Gs4,__, __, __, __, __, __, __, Cs5,__, __, __, __, __, __, __ },
      { Ds5,__, __, __, __, __, E5, __, __, __, __, __, Gs5,__, __, __ },
      { Fs5,__, __, __, __, __, __, __, E5, __, __, __, Ds5,__, __, __ },
      { Cs5,__, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ } },
};

/* ------------------------------------------------------------------ drums -- */

enum { DP_NONE, DP_SOFT, DP_REEF, DP_FLOW, DP_DEEP, DP_CLIMAX, DP_RISE, DP_RISE2, DP_SHAKE };

#define K DR_KICK
#define S DR_BRUSH
#define H DR_SHAKER
#define O DR_OSHAKER
#define B DR_BOOM

static const uint8_t drum_pat[9][16] = {
    { 0 },
    { K,0,0,0, 0,0,0,0, K,0,0,0, 0,0,0,0 },
    { K,0,H,0, S,0,H,0, K,0,H,0, S,0,H,O },
    { K,0,H,0, S,0,H,H, K,0,H,0, S,0,O,0 },
    { B,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { K,0,H,0, S,0,H,0, K,0,H,K, S,0,H,0 },
    { K,0,H,0, K|S,0,H,0, K,0,H,0, K|S,0,H,O },
    { K,H,H,H, K|S,H,H,H, K,H,H,H, K|S,H,O,H },
    { 0,0,H,0, 0,0,H,0, 0,0,H,0, 0,0,H,0 },
};

#undef K
#undef S
#undef H
#undef O
#undef B

/* ------------------------------------------------------------------- bass -- */
/* Offsets from the chord root, + 2; 14 is the octave. */

enum { BP_OFF, BP_GENTLE, BP_FLOW, BP_DEEP, BP_LONG };

static const uint8_t bass_pat[5][16] = {
    { XX,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { 2,0,0,0, 0,XX,2,0, 0,XX,0,0, 2,0,0,XX },     /* one, the and of two, four */
    { 2,0,0,0, 0,0,2,0, 0,0,2,0, 2,0,14,0 },       /* moving, with a lift at the end */
    { 2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },        /* one long note */
    { 2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,XX,0 },       /* a whole note, released before the next */
};

/* ---------------------------------------------------------------- droplets -- */
/* Patterns: index into the six-note voicing, + 2. Phrases: absolute notes,
 * eight bars, for the moments the droplets carry the fragment. */

enum { PK_OFF, PK_SPARSE, PK_FLOW, PK_FLOW2, PK_DEEP, PK_PHRASE0, PK_OPEN = PK_PHRASE0, PK_BELLS, PK_CODA };

static const uint8_t pluck_pat[5][16] = {
    { XX,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { 2,0,0,0, 0,0,4,0, 0,0,0,0, 5,0,0,0 },
    { 4,0,0,5, 0,0,7,0, 0,6,0,0, 5,0,0,4 },
    { 5,0,0,6, 0,0,7,0, 0,5,0,0, 4,0,0,6 },
    { 2,0,0,0, 0,0,0,0, 4,0,0,0, 0,0,0,0 },
};

static const uint8_t pluck_phrase[3][8][16] = {
    /* the opening: the fragment, breathing */
    { { 0 },
      { 0 },
      { B4, 0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 },
      { 0 },
      { B4, 0,0,0, 0,0,E5,0, 0,0,0,0, Fs5,0,0,0 },
      { 0 },
      { B4, 0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 },
      { Gs5,0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 } },
    /* the abyss: bells on the fragment, two bars a chord */
    { { B4, 0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 },
      { 0,  0,0,0, 0,0,0,0, 0,  0,0,0, Gs5,0,0,0 },
      { Fs5,0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 },
      { 0,  0,0,0, 0,0,0,0, B4, 0,0,0, 0,0,0,0 },
      { Cs5,0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 },
      { 0,  0,0,0, 0,0,0,0, Gs5,0,0,0, 0,0,0,0 },
      { Fs5,0,0,0, 0,0,0,0, 0,  0,0,0, 0,0,0,0 },
      { E5, 0,0,0, 0,0,0,0, Ds5,0,0,0, 0,0,0,0 } },
    /* the surface: the fragment, then inverted as the ray leaves */
    { { B4, 0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 },
      { 0 },
      { B4, 0,0,0, 0,0,0,0, E5, 0,0,0, 0,0,0,0 },
      { 0,  0,0,0, 0,0,0,0, Gs5,0,0,0, 0,0,0,0 },
      { E5, 0,0,0, 0,0,0,0, B4, 0,0,0, 0,0,0,0 },
      { 0 },
      { E5, 0,0,0, 0,0,0,0, 0,  0,0,0, 0,0,0,0 },
      { 0 } },
};

/* ------------------------------------------------------------- the order -- */

#define F_WHOOSH 1u     /* the water whoosh and a boom on the downbeat */
#define F_BOOM   2u     /* the boom alone */

typedef struct {
    uint8_t chord, drums, bass, pluck, lead, lead2, flags;
    uint8_t lead_lvl, lead_cut, lead_oct, lead2_lvl;
    uint8_t pad_lvl, pad_cut, drone, drone_note, shimmer, glass, choir;
    uint8_t pluck_lvl, pluck_dec, energy;
} bar_t;

/* chord    drums       bass       pluck      lead     lead2    flags      Llv  Lct Loc L2lv  pad  pct  drn dnt  shm  gls  chr  plv  pdc  nrg */
static const bar_t bars[SONG_BARS] = {
    /* 0-7 opening: the water wakes. The drone on E, the pad opening, shimmer, the fragment. */
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,    0,  30,  60, 40,  50,   0,   0, 150, 220,   8 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,   40,  35, 110,  0,  70,   0,   0, 150, 220,  10 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,   80,  40, 120,  0,  90,   0,   0, 150, 220,  14 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  110,  50, 120,  0, 100,   0,   0, 150, 220,  16 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  130,  60, 120,  0, 100,   0,   0, 150, 220,  20 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  140,  65, 120,  0,  90,   0,   0, 150, 220,  22 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  150,  70, 110,  0,  80,   0,   0, 150, 220,  26 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_OPEN,   LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  160,  75, 100,  0,  70,   0,   0, 150, 220,  30 },
    /* 8-23 the reef: bass and droplets; the hint at 12; theme A plain at 16. */
    { CH_E,   DP_NONE,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  160,  90,  90,  0,  50,   0,   0, 170, 160,  45 },
    { CH_CSM, DP_NONE,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  160,  90,  90,  0,  50,   0,   0, 170, 160,  46 },
    { CH_A,   DP_NONE,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  160,  95,  90,  0,  50,   0,   0, 170, 160,  48 },
    { CH_B,   DP_NONE,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_NONE, 0,          0,   0,   0,   0,  160,  95,  90,  0,  50,   0,   0, 170, 160,  50 },
    { CH_E,   DP_SOFT,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_HINT, 0,          0,   0,   0, 170,  165, 100,  80,  0,  45,   0,   0, 170, 160,  60 },
    { CH_CSM, DP_SOFT,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_HINT, 0,          0,   0,   0, 170,  165, 100,  80,  0,  45,   0,   0, 170, 160,  62 },
    { CH_A,   DP_SOFT,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_HINT, 0,          0,   0,   0, 170,  165, 105,  80,  0,  45,   0,   0, 170, 160,  64 },
    { CH_B,   DP_SOFT,   BP_GENTLE, PK_SPARSE, LP_NONE, L2_HINT, 0,          0,   0,   0, 170,  165, 105,  80,  0,  45,   0,   0, 170, 160,  66 },
    { CH_E,   DP_REEF,   BP_GENTLE, PK_SPARSE, LP_A,    L2_NONE, 0,        200, 110,   0,   0,  170, 110,  60,  0,  40,   0,   0, 170, 150,  80 },
    { CH_CSM, DP_REEF,   BP_GENTLE, PK_SPARSE, LP_A,    L2_NONE, 0,        200, 110,   0,   0,  170, 110,  60,  0,  40,   0,   0, 170, 150,  82 },
    { CH_A,   DP_REEF,   BP_GENTLE, PK_SPARSE, LP_A,    L2_NONE, 0,        205, 115,   0,   0,  170, 110,  60,  0,  40,   0,   0, 170, 150,  84 },
    { CH_B,   DP_REEF,   BP_GENTLE, PK_SPARSE, LP_A,    L2_NONE, 0,        205, 115,   0,   0,  170, 110,  60,  0,  40,   0,   0, 170, 150,  86 },
    { CH_E,   DP_REEF,   BP_GENTLE, PK_FLOW,   LP_A,    L2_NONE, 0,        210, 120,  40,   0,  175, 115,  40,  0,  40,   0,   0, 180, 150,  95 },
    { CH_CSM, DP_REEF,   BP_GENTLE, PK_FLOW2,  LP_A,    L2_NONE, 0,        210, 120,  40,   0,  175, 115,  40,  0,  40,   0,   0, 180, 150, 100 },
    { CH_A,   DP_REEF,   BP_GENTLE, PK_FLOW,   LP_A,    L2_NONE, 0,        215, 125,  40,   0,  175, 115,  30,  0,  40,   0,   0, 180, 150, 105 },
    { CH_B,   DP_REEF,   BP_GENTLE, PK_FLOW2,  LP_A,    L2_NONE, 0,        215, 125,  40,   0,  175, 115,  20,  0,  40,   0,   0, 180, 150, 110 },
    /* 24-39 the encounter: theme A with the octave; then theme B, the choir under it. */
    { CH_E,   DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, F_WHOOSH, 235, 150, 160,   0,  190, 140,   0,  0,  60,   0,   0, 190, 150, 150 },
    { CH_CSM, DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        235, 150, 160,   0,  190, 140,   0,  0,  60,   0,   0, 190, 150, 152 },
    { CH_A,   DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, 0,        235, 155, 160,   0,  190, 140,   0,  0,  60,   0,   0, 190, 150, 154 },
    { CH_B,   DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        235, 155, 160,   0,  190, 145,   0,  0,  60,   0,   0, 190, 150, 156 },
    { CH_E,   DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, 0,        240, 160, 170,   0,  190, 145,   0,  0,  60,   0,  80, 190, 150, 160 },
    { CH_CSM, DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        240, 160, 170,   0,  190, 150,   0,  0,  60,   0, 100, 190, 150, 164 },
    { CH_A,   DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, 0,        240, 165, 170,   0,  190, 150,   0,  0,  60,   0, 120, 190, 150, 168 },
    { CH_B,   DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        240, 165, 170,   0,  190, 155,   0,  0,  60,   0, 140, 190, 150, 172 },
    { CH_A,   DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, F_WHOOSH, 245, 170, 200,   0,  200, 160,   0,  0,  70,  60, 160, 190, 150, 190 },
    { CH_B,   DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        245, 170, 200,   0,  200, 160,   0,  0,  70,  70, 170, 190, 150, 192 },
    { CH_CSM, DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, 0,        245, 175, 200,   0,  200, 160,   0,  0,  70,  80, 180, 190, 150, 196 },
    { CH_E,   DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        245, 175, 200,   0,  200, 165,   0,  0,  70,  90, 190, 190, 150, 200 },
    { CH_A,   DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, 0,        250, 180, 200,   0,  200, 165,   0,  0,  70, 100, 200, 190, 150, 204 },
    { CH_B,   DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        250, 180, 200,   0,  200, 165,   0,  0,  70, 100, 200, 190, 150, 206 },
    { CH_CSM, DP_FLOW,   BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, 0,        250, 185, 200,   0,  200, 170,   0,  0,  70, 100, 200, 190, 150, 208 },
    { CH_E,   DP_FLOW,   BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        250, 185, 200,   0,  200, 170,   0,  0,  70, 100, 200, 190, 150, 210 },
    /* 40-47 the descent: the tune in C# minor, an octave down, in the hollow voice. */
    { CH_CSM, DP_DEEP,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, F_WHOOSH,   0, 120,   0, 220,  170,  70, 140, 37,  30,   0,   0, 150, 220,  90 },
    { CH_A,   DP_NONE,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, 0,          0, 120,   0, 220,  170,  65, 140,  0,  30,   0,   0, 150, 220,  80 },
    { CH_FSM, DP_DEEP,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, 0,          0, 120,   0, 220,  170,  60, 140,  0,  30,   0,   0, 150, 220,  70 },
    { CH_GS,  DP_NONE,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, 0,          0, 120,   0, 220,  170,  55, 140,  0,  30,   0,   0, 150, 220,  65 },
    { CH_CSM, DP_DEEP,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, 0,          0, 120,   0, 220,  170,  55, 140,  0,  30,   0,   0, 150, 220,  60 },
    { CH_A,   DP_NONE,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, 0,          0, 120,   0, 220,  170,  50, 140,  0,  30,   0,   0, 150, 220,  55 },
    { CH_FSM, DP_DEEP,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, 0,          0, 120,   0, 220,  170,  50, 140,  0,  30,   0,   0, 150, 220,  50 },
    { CH_GS,  DP_NONE,   BP_DEEP,   PK_DEEP,   LP_NONE, L2_DESC, 0,          0, 120,   0, 220,  170,  50, 120,  0,  40,   0,   0, 150, 220,  50 },
    /* 48-55 the abyss: two bars a chord, bells on the fragment, the glass organ blooming. */
    { CH_A11, DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, F_BOOM,     0, 120,   0,   0,  150,  80,  60,  0,  60,  90,  40, 210, 235,  50 },
    { CH_A11, DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, 0,          0, 120,   0,   0,  150,  85,   0,  0,  65, 110,  50, 210, 235,  55 },
    { CH_E,   DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, 0,          0, 120,   0,   0,  150,  90,   0,  0,  70, 125,  60, 210, 235,  60 },
    { CH_E,   DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, 0,          0, 120,   0,   0,  150,  95,   0,  0,  75, 140,  70, 210, 235,  65 },
    { CH_CSM, DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, F_BOOM,     0, 120,   0,   0,  150, 100,   0,  0,  80, 155,  85, 210, 235,  75 },
    { CH_CSM, DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, 0,          0, 120,   0,   0,  150, 105,   0,  0,  90, 165, 100, 210, 235,  85 },
    { CH_B,   DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, 0,          0, 120,   0,   0,  150, 110,   0,  0, 100, 175, 115, 210, 235,  95 },
    { CH_B,   DP_NONE,   BP_LONG,   PK_BELLS,  LP_NONE, L2_NONE, 0,          0, 120,   0,   0,  150, 120,   0,  0, 110, 190, 130, 210, 235, 110 },
    /* 56-63 the bloom: theme B full -- the climax. */
    { CH_A,   DP_CLIMAX, BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, F_WHOOSH, 255, 200, 255,   0,  220, 180,   0,  0, 120, 200, 230, 200, 150, 210 },
    { CH_B,   DP_CLIMAX, BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        255, 200, 255,   0,  220, 180,   0,  0, 125, 200, 235, 200, 150, 215 },
    { CH_CSM, DP_CLIMAX, BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, 0,        255, 205, 255,   0,  220, 180,   0,  0, 130, 200, 240, 200, 150, 220 },
    { CH_E,   DP_CLIMAX, BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        255, 205, 255,   0,  220, 185,   0,  0, 135, 200, 245, 200, 150, 225 },
    { CH_A,   DP_CLIMAX, BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, F_BOOM,   255, 210, 255,   0,  220, 185,   0,  0, 140, 200, 250, 200, 150, 235 },
    { CH_B,   DP_CLIMAX, BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        255, 210, 255,   0,  220, 190,   0,  0, 145, 200, 255, 200, 150, 240 },
    { CH_CSM, DP_CLIMAX, BP_FLOW,   PK_FLOW,   LP_B,    L2_NONE, 0,        255, 215, 255,   0,  220, 190,   0,  0, 150, 200, 255, 200, 150, 250 },
    { CH_E,   DP_CLIMAX, BP_FLOW,   PK_FLOW2,  LP_B,    L2_NONE, 0,        255, 215, 255,   0,  220, 195,   0,  0, 160, 200, 255, 200, 150, 255 },
    /* 64-71 the ascent: theme A up a tone (song_transpose), four soft kicks a bar. */
    { CH_E,   DP_RISE,   BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, F_WHOOSH, 255, 230, 255,   0,  225, 210,   0,  0, 110, 160, 210, 210, 150, 255 },
    { CH_CSM, DP_RISE,   BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        255, 230, 255,   0,  225, 210,   0,  0, 110, 160, 210, 210, 150, 255 },
    { CH_A,   DP_RISE,   BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, 0,        255, 230, 255,   0,  225, 210,   0,  0, 110, 160, 210, 210, 150, 255 },
    { CH_B,   DP_RISE,   BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        255, 230, 255,   0,  225, 210,   0,  0, 110, 160, 210, 210, 150, 255 },
    { CH_E,   DP_RISE2,  BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, 0,        255, 235, 255,   0,  225, 215,   0,  0, 110, 160, 215, 210, 150, 255 },
    { CH_CSM, DP_RISE2,  BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        255, 235, 255,   0,  225, 215,   0,  0, 110, 160, 215, 210, 150, 255 },
    { CH_A,   DP_RISE2,  BP_FLOW,   PK_FLOW,   LP_A,    L2_NONE, 0,        255, 240, 255,   0,  225, 215,   0,  0, 110, 160, 220, 210, 150, 255 },
    { CH_B,   DP_RISE2,  BP_FLOW,   PK_FLOW2,  LP_A,    L2_NONE, 0,        255, 240, 255,   0,  225, 215,   0,  0, 110, 160, 220, 210, 150, 255 },
    /* 72-79 the surface: the harmony lands on F#; percussion leaves; the fragment inverts; fade. */
    { CH_E,   DP_SHAKE,  BP_LONG,   PK_CODA,   LP_NONE, L2_NONE, F_WHOOSH,   0, 200,   0,   0,  220, 170,  40, 40, 170, 150, 170, 190, 235, 120 },
    { CH_A,   DP_NONE,   BP_LONG,   PK_CODA,   LP_NONE, L2_NONE, 0,          0, 200,   0,   0,  210, 150,  80,  0, 160, 130, 140, 180, 235, 100 },
    { CH_E,   DP_NONE,   BP_LONG,   PK_CODA,   LP_NONE, L2_NONE, 0,          0, 200,   0,   0,  200, 130, 120,  0, 150, 110, 110, 170, 235,  80 },
    { CH_A,   DP_NONE,   BP_LONG,   PK_CODA,   LP_NONE, L2_NONE, 0,          0, 200,   0,   0,  180, 110, 140,  0, 140,  90,  80, 160, 235,  65 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_CODA,   LP_NONE, L2_NONE, 0,          0, 200,   0,   0,  160, 100, 150,  0, 130,  70,  50, 150, 235,  50 },
    { CH_B,   DP_NONE,   BP_OFF,    PK_CODA,   LP_NONE, L2_NONE, 0,          0, 200,   0,   0,  140,  90, 150,  0, 120,  50,  20, 140, 235,  40 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_CODA,   LP_NONE, L2_NONE, 0,          0, 200,   0,   0,  110,  80, 150,  0, 100,  30,   0, 130, 235,  25 },
    { CH_E,   DP_NONE,   BP_OFF,    PK_CODA,   LP_NONE, L2_NONE, 0,          0, 200,   0,   0,   80,  70, 150,  0,  80,   0,   0, 120, 235,  12 },
};

static const bar_t *row(uint32_t bar)
{
    return &bars[bar < SONG_BARS ? bar : SONG_BARS - 1];
}

/* ------------------------------------------------------------- accessors -- */

int song_transpose(uint32_t bar) { return bar >= 64 ? 2 : 0; }

static const char *const k_section_names[] = {
    "opening", "the reef", "encounter", "descent", "the abyss", "ascent", "surface",
};

int song_section(uint32_t bar)
{
    if (bar < 8)  return 0;
    if (bar < 24) return 1;
    if (bar < 40) return 2;
    if (bar < 48) return 3;
    if (bar < 64) return 4;
    if (bar < 72) return 5;
    return 6;
}

const char *song_section_name(int s) { return (s >= 0 && s < 7) ? k_section_names[s] : "?"; }

uint8_t song_drums(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return 0;
    uint8_t d = drum_pat[row(bar)->drums][s];
    if (s == 0 && (row(bar)->flags & F_WHOOSH)) d |= DR_WHOOSH | DR_BOOM;
    if (s == 0 && (row(bar)->flags & F_BOOM))   d |= DR_BOOM;
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
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    return rel_event(bass_pat[r->bass][s], chord_root[r->chord], bar);
}

int song_pluck(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->pluck >= PK_PHRASE0)
        return abs_event(pluck_phrase[r->pluck - PK_PHRASE0][bar & 7][s], bar);
    const uint8_t e = pluck_pat[r->pluck][s];
    if (e < 2) return e;
    return chord_pluck[r->chord][e - 2] + song_transpose(bar);
}

int song_lead(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->lead == LP_NONE) return s == 0 ? SONG_OFF : 0;
    return abs_event(lead_phrase[r->lead][bar & 7][s], bar);
}

int song_lead2(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->lead2 == L2_NONE) return s == 0 ? SONG_OFF : 0;
    return abs_event(lead2_phrase[r->lead2][bar & 7][s], bar);
}

void song_pad_chord(uint32_t bar, uint8_t out[4])
{
    const bar_t *r = row(bar);
    for (int i = 0; i < 4; i++)
        out[i] = (bar < SONG_BARS && r->pad_lvl)
               ? (uint8_t)(chord_pad[r->chord][i] + song_transpose(bar)) : 0;
}

void song_glass_chord(uint32_t bar, uint8_t out[5])
{
    const bar_t *r = row(bar);
    const int on = bar < SONG_BARS && r->glass;
    out[0] = on ? (uint8_t)(chord_root[r->chord] + 12 + song_transpose(bar)) : 0;
    for (int i = 0; i < 4; i++)
        out[1 + i] = on ? (uint8_t)(chord_pad[r->chord][i] + song_transpose(bar)) : 0;
}

void song_choir_chord(uint32_t bar, uint8_t out[4])
{
    const bar_t *r = row(bar);
    for (int i = 0; i < 4; i++)
        out[i] = (bar < SONG_BARS && r->choir)
               ? (uint8_t)(chord_pad[r->chord][i] + 12 + song_transpose(bar)) : 0;
}

int song_lead_level(uint32_t bar)  { return bar < SONG_BARS ? row(bar)->lead_lvl  : 0; }
int song_lead_cut(uint32_t bar)    { return row(bar)->lead_cut; }
int song_lead_octave(uint32_t bar) { return row(bar)->lead_oct; }
int song_lead2_level(uint32_t bar) { return bar < SONG_BARS ? row(bar)->lead2_lvl : 0; }
int song_pad_level(uint32_t bar)   { return bar < SONG_BARS ? row(bar)->pad_lvl   : 0; }
int song_pad_cut(uint32_t bar)     { return row(bar)->pad_cut; }
int song_drone_level(uint32_t bar) { return bar < SONG_BARS ? row(bar)->drone     : 0; }
int song_drone_note(uint32_t bar)
{
    const int n = row(bar)->drone_note;
    return n ? n + song_transpose(bar) : 0;
}
int song_shimmer(uint32_t bar)     { return bar < SONG_BARS ? row(bar)->shimmer   : 0; }
int song_glass_level(uint32_t bar) { return bar < SONG_BARS ? row(bar)->glass     : 0; }
int song_choir_level(uint32_t bar) { return bar < SONG_BARS ? row(bar)->choir     : 0; }
int song_pluck_level(uint32_t bar) { return bar < SONG_BARS ? row(bar)->pluck_lvl : 0; }
int song_pluck_decay(uint32_t bar) { return row(bar)->pluck_dec; }
int song_energy(uint32_t bar)      { return bar < SONG_BARS ? row(bar)->energy    : 0; }

uint32_t song_voices(uint32_t bar)
{
    if (bar >= SONG_BARS) return 0;
    const bar_t *r = row(bar);
    uint32_t v = 0, dm = 0;
    for (int s = 0; s < 16; s++) dm |= drum_pat[r->drums][s];
    if (dm & DR_KICK)                    v |= SV_KICK;
    if (dm & DR_BRUSH)                   v |= SV_BRUSH;
    if (dm & (DR_SHAKER | DR_OSHAKER))   v |= SV_SHAKER;
    if (r->bass  != BP_OFF)              v |= SV_BASS;
    if (r->pluck != PK_OFF)              v |= SV_PLUCK;
    if (r->lead  != LP_NONE)             v |= SV_LEAD;
    if (r->lead2 != L2_NONE)             v |= SV_LEAD2;
    if (r->pad_lvl)                      v |= SV_PAD;
    if (r->shimmer)                      v |= SV_SHIMMER;
    if (r->drone)                        v |= SV_DRONE;
    if (r->glass)                        v |= SV_GLASS;
    if (r->choir)                        v |= SV_CHOIR;
    return v;
}
