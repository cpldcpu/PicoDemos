/* HELION -- the song.
 *
 * D minor at 120 BPM, eighty bars in ten phrases of eight. The star's
 * fire is the resolution: the whole flight is in D minor, the orbital climax
 * (bar 56) arrives in D major, the eclipse suspends everything over A, the
 * dominant, and the coda lets it settle into D major quietly. Written as
 * tables, on purpose: Phase asked for one memorable melodic identity, and a
 * melody anyone remembers is a melody somebody chose note by note. Nothing
 * here is generated.
 *
 * ------------------------------------------------------------- the theme --
 *
 * The signal is a rising fifth, D to A. Theme A, the star's tune, over
 * Dm | Bb | F | C, twice: the fifth, a step up to the leaning note and back,
 * a sigh down through the third, and a held note:
 *
 *   D5 ..... A5 .....  |  Bb5 .. A5 .. F5 ....  |  E5 ..... D5 ..... C5 ..  |  D5 ..............
 *   D5 ..... A5 .....  |  Bb5 .. C6 .. D6 ....  |  C6 ..... A5 ..... G5 ..  |  A5 ..............
 *
 * The first half asks (it ends on the ninth over C), the second climbs to
 * the octave and answers on the fifth. The Bb is the melancholy inside the
 * grandeur: in the coda the tune returns with a B natural and an F#, the
 * same shape in D major, and that is the fire remembered.
 *
 * Theme B, the orbital line, is long notes rising over D | A | Bm | G, the
 * climax that the tunnel earns:
 *
 *   F#5 ..  |  E5 . A5 .  |  B5 ..  |  A5 . B5 .  |  D6 ..  |  C#6 . A5 .  |  B5 .. A5  |  G5 . F#5 .
 *
 * The signal alone, D-A and the Bb-A sigh, is what the bowed harmonic plays
 * at the start, in the breath at bar 32, and exposed in the eclipse.
 *
 * -------------------------------------------------------------- the form --
 *
 *   bars    picture       music
 *   0-7     the corona    a soft tam-tam, air, the bowed harmonic's signal, a low open fifth
 *   8-23    the plain     bass pulse and frame drum; the reed low and intimate (12); THEME A (16)
 *   24-39   unfolding     THEME A confident, bowed chords and bronze replies; a breath at 32; the answer (36)
 *   40-55   the tunnel    tam-tam; the driving bass, dry tight drums, bronze riff; THEME A returns (48)
 *   56-63   orbits        tam-tam; D major: THEME B over full bowed chords -- the climax
 *   64-71   eclipse       tam-tam, then almost nothing: the harmonic's signal over A, Bb/A, Asus, A
 *   72-79   return        D major: the reed answers gently; percussion leaves; fade
 *
 * Every section starts on a multiple of eight, so a phrase's bar is bar & 7.
 *
 * ROW EVENTS. Pitched tables use 0 = hold, 1 = note off, else a MIDI note
 * (or, for patterns relative to a chord, an encoded offset). Held notes
 * carry across bars. A voice whose phrase is "none" is sent a note off on
 * the first step of the bar.
 */

#include "song.h"

/* ---------------------------------------------------------------- pitches -- */

enum {
    C4 = 60, Cs4 = 61, D4 = 62, E4 = 64, F4 = 65, Fs4 = 66, A4 = 69, Bb4 = 70,
    C5 = 72, Cs5 = 73, D5 = 74, E5 = 76, F5 = 77, Fs5 = 78, G5 = 79, A5 = 81, Bb5 = 82, B5 = 83,
    C6 = 84, Cs6 = 85, D6 = 86
};
#define __  0           /* hold */
#define XX  SONG_OFF    /* note off */

/* ----------------------------------------------------------------- chords -- */

enum { CH_DM, CH_BB, CH_F, CH_C, CH_A, CH_GM, CH_D, CH_BM, CH_G, CH_BBA, CH_ASUS, CH_APED, CH_DOPEN };
#define NCHORDS 13

/* The bass root. */
static const uint8_t chord_root[NCHORDS] = { 38, 34, 41, 36, 33, 43, 38, 35, 43, 33, 33, 33, 38 };

/* The bowed metal's four notes, close, neighbours sharing tones, the top
 * voice moving by step where it can. */
static const uint8_t chord_bow[NCHORDS][4] = {
    { 50, 57, 62, 65 },     /* Dm:     D3 A3 D4 F4   */
    { 50, 53, 58, 65 },     /* Bb:     D3 F3 Bb3 F4  */
    { 48, 57, 60, 65 },     /* F:      C3 A3 C4 F4   */
    { 48, 55, 60, 64 },     /* C:      C3 G3 C4 E4   */
    { 52, 57, 61, 64 },     /* A:      E3 A3 C#4 E4  */
    { 50, 55, 58, 62 },     /* Gm:     D3 G3 Bb3 D4  */
    { 50, 57, 62, 66 },     /* D:      D3 A3 D4 F#4  */
    { 50, 54, 59, 62 },     /* Bm:     D3 F#3 B3 D4  */
    { 50, 55, 59, 62 },     /* G:      D3 G3 B3 D4   */
    { 45, 53, 58, 62 },     /* Bb/A:   A2 F3 Bb3 D4  -- the eclipse's suspension */
    { 45, 52, 57, 62 },     /* Asus4:  A2 E3 A3 D4   */
    { 45, 52, 57, 64 },     /* A pedal: A2 E3 A3 E4  */
    { 50, 57, 62, 69 },     /* D open: D3 A3 D4 A4   -- the corona's low resonance */
};

/* Six notes for the struck bar, low to high. */
static const uint8_t chord_bronze[NCHORDS][6] = {
    { 57, 62, 65, 69, 74, 77 },
    { 53, 58, 62, 65, 70, 74 },
    { 57, 60, 65, 69, 72, 77 },
    { 55, 60, 64, 67, 72, 76 },
    { 57, 61, 64, 69, 73, 76 },
    { 55, 58, 62, 67, 70, 74 },
    { 57, 62, 66, 69, 74, 78 },
    { 54, 59, 62, 66, 71, 74 },
    { 55, 59, 62, 67, 71, 74 },
    { 53, 57, 62, 65, 69, 74 },
    { 57, 62, 64, 69, 74, 76 },
    { 57, 64, 69, 76, 81, 88 },
    { 57, 62, 69, 74, 81, 86 },
};

/* ------------------------------------------------------------ reed phrases -- */

enum { LP_NONE, LP_QLOW, LP_A, LP_A2, LP_B, LP_CODA };

static const uint8_t lead_phrase[6][8][16] = {
    { { 0 } },
    /* the first hearing: theme A's question an octave down, bars 12-15 only */
    { { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { D4, __, __, __, __, __, A4, __, __, __, __, __, __, __, __, __ },
      { Bb4,__, __, __, A4, __, __, __, F4, __, __, __, __, __, __, __ },
      { E4, __, __, __, __, __, D4, __, __, __, __, __, C4, __, __, __ },
      { D4, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ } },
    /* theme A -- the star's tune */
    { { D5, __, __, __, __, __, A5, __, __, __, __, __, __, __, __, __ },
      { Bb5,__, __, __, A5, __, __, __, F5, __, __, __, __, __, __, __ },
      { E5, __, __, __, __, __, D5, __, __, __, __, __, C5, __, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ },
      { D5, __, __, __, __, __, A5, __, __, __, __, __, __, __, __, __ },
      { Bb5,__, __, __, C6, __, __, __, D6, __, __, __, __, __, __, __ },
      { C6, __, __, __, __, __, A5, __, __, __, __, __, G5, __, __, __ },
      { A5, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ } },
    /* the breath and the answer: two bars of rest, a pickup, then theme A's
     * second half */
    { { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, A4, __, C5, __ },
      { D5, __, __, __, __, __, A5, __, __, __, __, __, __, __, __, __ },
      { Bb5,__, __, __, C6, __, __, __, D6, __, __, __, __, __, __, __ },
      { C6, __, __, __, __, __, A5, __, __, __, __, __, G5, __, __, __ },
      { A5, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ } },
    /* theme B -- the orbital line, over D A Bm G */
    { { Fs5,__, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { E5, __, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { B5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { A5, __, __, __, __, __, __, __, B5, __, __, __, __, __, __, __ },
      { D6, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { Cs6,__, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { B5, __, __, __, __, __, __, __, __, __, __, __, A5, __, __, __ },
      { G5, __, __, __, __, __, Fs5,__, __, __, __, __, __, __, XX, __ } },
    /* the coda: theme A's question in D major, then the signal once more */
    { { D5, __, __, __, __, __, A5, __, __, __, __, __, __, __, __, __ },
      { B5, __, __, __, A5, __, __, __, Fs5,__, __, __, __, __, __, __ },
      { E5, __, __, __, __, __, D5, __, __, __, __, __, Cs5,__, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, XX, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { D5, __, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { Fs5,__, __, __, __, __, __, __, E5, __, __, __, __, __, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, XX, __, __, __, __, __ } },
};

/* ------------------------------------------------------- harmonic phrases -- */

enum { HP_NONE, HP_OPEN, HP_BREATH, HP_ECLIPSE };

static const uint8_t harm_phrase[4][8][16] = {
    { { 0 } },
    /* the corona: the signal, twice, the second time with the sigh */
    { { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { A5, __, __, __, __, __, __, __, __, __, XX, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { D5, __, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { Bb5,__, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { __, __, __, __, __, __, __, __, XX, __, __, __, __, __, __, __ } },
    /* the breath at 32 */
    { { D5, __, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { __, __, __, __, __, __, __, __, __, __, __, __, XX, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ } },
    /* the eclipse: the signal exposed, the sigh held over the dominant */
    { { XX, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { A5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
      { Bb5,__, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { __, __, __, __, __, __, __, __, XX, __, __, __, __, __, __, __ },
      { D5, __, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
      { Bb5,__, __, __, __, __, A5, __, __, __, __, __, __, __, __, __ },
      { __, __, __, __, __, __, __, __, __, __, __, __, XX, __, __, __ } },
};

/* ------------------------------------------------------------------ drums -- */

enum { DP_NONE, DP_WEIGHT, DP_WEIGHT2, DP_FLIGHT, DP_FLIGHT2, DP_UNFOLD, DP_UNFOLD2,
       DP_TUNNEL, DP_TUNNEL2, DP_TUNNEL3, DP_CLIMAX, DP_CLIMAX2, DP_GHOST1, DP_FILL };

#define D DR_DRUM
#define G DR_GHOST
#define R DR_RIM
#define S DR_SCRAPE
#define L DR_SCRAPEL

static const uint8_t drum_pat[14][16] = {
    { 0 },
    { D,0,0,0, 0,0,0,G, D,0,0,0, R,0,0,0 },
    { D,0,0,0, 0,0,0,G, D,0,0,G, R,0,0,L },
    { D,0,0,0, R,0,0,G, D,0,S,0, R,0,0,G },
    { D,0,0,S, R,0,0,G, D,0,S,0, R,0,S,L },
    { D,0,S,0, R,0,0,G, 0,0,D,0, R,0,S,0 },
    { D,0,S,0, R,0,0,G, 0,0,D,S, R,0,L,0 },
    { D,0,S,0, R,0,0,D, 0,0,D,S, R,0,S,0 },
    { D,0,S,S, R,0,D,0, D,0,S,S, R,S,D,S },
    { D,0,S,0, R,0,D,0, D,S,S,0, R,0,D|S,L },
    { D,0,S,0, R,0,0,D, 0,S,D,0, R,0,S,L },
    { D,0,S,0, R,0,0,D, 0,S,D,0, R,S,S,S },
    { G,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { D,0,S,S, R,S,D,S, D,S,D,S, R,R,R,L },
};

#undef D
#undef G
#undef R
#undef S
#undef L

/* ------------------------------------------------------------------- bass -- */
/* Offsets from the chord root encoded as offset + 8 (so 8 is the root, 15
 * the fifth, 20 the octave, 3 the fifth below); bit 0x40 slides into the
 * note. Short notes, gaps, the occasional held one. */

enum { BP_OFF, BP_PULSE, BP_PULSE2, BP_DRIVE, BP_DRIVE2, BP_LONG, BP_CLIMAX };

#define RT 8
#define F5 15
#define OC 20
#define LO 3
#define SL 0x40

static const uint8_t bass_pat[7][16] = {
    { XX,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { RT,0,XX,0, 0,0,RT,0, XX,0,RT,0, XX,0,F5,0 },              /* the pulse */
    { RT,0,XX,0, 0,0,RT,0, XX,0,RT,0, RT,0,LO,XX },             /* the pulse, dipping */
    { RT,XX,RT,XX, RT,XX,RT,XX, RT,XX,F5,XX, RT,XX,RT,XX },     /* the tunnel, eighths */
    { RT,XX,RT,XX, RT,XX,RT,XX, RT,XX,RT,XX, OC|SL,0,0,XX },    /* eighths, then a slide to the octave */
    { RT,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,XX,0 },                   /* a whole note */
    { RT,0,XX,0, RT,0,RT,0, XX,0,RT,0, RT,0,XX,0 },             /* the climax: long and short */
};

#undef RT
#undef F5
#undef OC
#undef LO
#undef SL

/* ----------------------------------------------------------------- bronze -- */
/* Index into the six-note voicing, + 2. Answers, off the beat. */

enum { BR_OFF, BR_SPARSE, BR_ANS1, BR_ANS2, BR_ANS3, BR_RIFF, BR_RIFF2, BR_CLIMAX, BR_CODA };

static const uint8_t bronze_pat[9][16] = {
    { 0 },
    { 0,0,0,0, 0,0,4,0, 0,0,0,0, 0,0,0,0 },
    { 0,0,0,0, 0,0,4,0, 0,0,6,0, 0,5,0,0 },
    { 0,0,0,5, 0,0,0,4, 0,0,6,0, 0,0,7,0 },
    { 0,0,4,0, 0,0,0,6, 0,0,0,0, 0,5,0,0 },
    { 5,0,0,5, 0,0,5,0, 0,7,0,0, 5,0,6,0 },
    { 5,0,0,5, 0,0,7,0, 0,5,0,0, 6,0,5,0 },
    { 7,0,0,6, 0,0,7,0, 5,0,0,6, 0,0,7,0 },
    { 0,0,0,0, 0,0,0,0, 4,0,0,0, 0,0,0,0 },
};

/* --------------------------------------------------------------- the bow -- */
/* Gate patterns: 0 keep, XX release, ON a fresh stroke. */

enum { BW_OFF, BW_HOLD, BW_CONT, BW_PHRASE, BW_STAB, BW_REL };
#define ON SONG_BOW_ON

static const uint8_t bow_pat[6][16] = {
    { XX,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { ON,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },                    /* a stroke, held into the next bar */
    { 0 },                                                      /* keep bowing; the chord may change */
    { ON,0,0,0, 0,0,0,0, 0,0,0,0, XX,0,0,0 },                   /* a stroke, released on four */
    { ON,0,0,XX, 0,0,0,0, 0,0,ON,0, 0,XX,0,0 },                 /* two short strokes */
    { 0,0,0,0, 0,0,0,0, 0,0,0,0, XX,0,0,0 },                    /* keep, then release */
};

#undef ON

/* ------------------------------------------------------------- the order -- */

#define F_GONG     1u    /* the tam-tam struck on the downbeat */
#define F_GONGSOFT 2u    /* touched */

typedef struct {
    uint8_t chord, drums, bass, bronze, lead, harm, bow, flags;
    uint8_t lead_lvl, lead_push, bow_lvl, bow_open, bronze_lvl, bronze_dec;
    uint8_t bass_lvl, bass_bite, harm_lvl, air, space, energy;
} bar_t;

/* chord     drums       bass       bronze     lead      harm        bow        flags       Rlv  Rps  Wlv  Wop  Zlv  Zdc  Blv  Bbt  Hlv  air  spc  nrg */
static const bar_t bars[SONG_BARS] = {
    /* 0-7 the corona: a touched tam-tam, air, the harmonic's signal, the low open fifth. */
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_OFF,    F_GONGSOFT,   0,   0,   0,   0,   0,   0,   0,   0, 170, 110, 255,   8 },
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_OFF,    0,            0,   0,   0,   0,   0,   0,   0,   0, 170, 130, 255,  10 },
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_OFF,    0,            0,   0,   0,   0,   0,   0,   0,   0, 175, 150, 255,  14 },
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_OFF,    0,            0,   0,   0,   0,   0,   0,   0,   0, 175, 150, 255,  16 },
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_HOLD,   0,            0,   0,  70,  40,   0,   0,   0,   0, 180, 150, 255,  20 },
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_CONT,   0,            0,   0,  90,  50,   0,   0,   0,   0, 185, 140, 255,  22 },
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_CONT,   0,            0,   0, 110,  60,   0,   0,   0,   0, 190, 130, 255,  26 },
    { CH_DOPEN, DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_OPEN,    BW_REL,    0,            0,   0, 120,  70,   0,   0,   0,   0, 190, 120, 255,  30 },
    /* 8-23 the plain: the pulse and the frame drum; the reed low at 12; theme A at 16. */
    { CH_DM,    DP_WEIGHT,  BP_PULSE,  BR_SPARSE, LP_NONE,  HP_NONE,    BW_OFF,    0,            0,   0,   0,   0, 150, 120, 200, 120,   0,  60, 150,  60 },
    { CH_BB,    DP_WEIGHT,  BP_PULSE,  BR_ANS1,   LP_NONE,  HP_NONE,    BW_OFF,    0,            0,   0,   0,   0, 150, 120, 200, 120,   0,  50, 150,  62 },
    { CH_F,     DP_WEIGHT,  BP_PULSE2, BR_SPARSE, LP_NONE,  HP_NONE,    BW_OFF,    0,            0,   0,   0,   0, 155, 120, 200, 125,   0,  50, 150,  64 },
    { CH_C,     DP_WEIGHT2, BP_PULSE,  BR_ANS2,   LP_NONE,  HP_NONE,    BW_OFF,    0,            0,   0,   0,   0, 155, 120, 205, 125,   0,  40, 150,  68 },
    { CH_DM,    DP_WEIGHT2, BP_PULSE,  BR_ANS1,   LP_QLOW,  HP_NONE,    BW_PHRASE, 0,          190,  40,  90,  60, 160, 120, 205, 130,   0,  40, 150,  80 },
    { CH_BB,    DP_WEIGHT2, BP_PULSE2, BR_ANS3,   LP_QLOW,  HP_NONE,    BW_PHRASE, 0,          190,  40,  95,  60, 160, 120, 205, 130,   0,  40, 150,  82 },
    { CH_F,     DP_WEIGHT2, BP_PULSE,  BR_ANS1,   LP_QLOW,  HP_NONE,    BW_HOLD,   0,          195,  45, 100,  65, 160, 120, 205, 130,   0,  30, 150,  86 },
    { CH_C,     DP_WEIGHT2, BP_PULSE2, BR_ANS2,   LP_QLOW,  HP_NONE,    BW_PHRASE, 0,          195,  45, 100,  65, 160, 120, 210, 135,   0,  30, 150,  90 },
    { CH_DM,    DP_FLIGHT,  BP_PULSE,  BR_ANS1,   LP_A,     HP_NONE,    BW_PHRASE, 0,          215,  80, 120,  90, 170, 130, 215, 140,   0,  20, 150, 100 },
    { CH_BB,    DP_FLIGHT,  BP_PULSE2, BR_ANS2,   LP_A,     HP_NONE,    BW_PHRASE, 0,          215,  80, 120,  90, 170, 130, 215, 140,   0,  20, 150, 104 },
    { CH_F,     DP_FLIGHT2, BP_PULSE,  BR_ANS3,   LP_A,     HP_NONE,    BW_HOLD,   0,          215,  85, 125,  95, 170, 130, 215, 140,   0,  20, 150, 108 },
    { CH_C,     DP_FLIGHT2, BP_PULSE2, BR_ANS1,   LP_A,     HP_NONE,    BW_PHRASE, 0,          220,  85, 125,  95, 170, 130, 215, 145,   0,  20, 150, 112 },
    { CH_DM,    DP_FLIGHT,  BP_PULSE,  BR_ANS2,   LP_A,     HP_NONE,    BW_PHRASE, 0,          220,  90, 130, 100, 175, 130, 220, 145,   0,  10, 155, 118 },
    { CH_BB,    DP_FLIGHT,  BP_PULSE2, BR_ANS3,   LP_A,     HP_NONE,    BW_PHRASE, 0,          220,  90, 130, 100, 175, 130, 220, 145,   0,  10, 155, 122 },
    { CH_F,     DP_FLIGHT2, BP_PULSE,  BR_ANS1,   LP_A,     HP_NONE,    BW_HOLD,   0,          225,  95, 135, 105, 175, 130, 220, 150,   0,  10, 155, 128 },
    { CH_C,     DP_FLIGHT2, BP_PULSE2, BR_ANS2,   LP_A,     HP_NONE,    BW_PHRASE, 0,          225,  95, 135, 105, 175, 130, 220, 150,   0,   0, 155, 135 },
    /* 24-39 unfolding: theme A confident, bowed chords with entrances; the breath at 32; the answer at 36. */
    { CH_DM,    DP_UNFOLD,  BP_PULSE2, BR_ANS2,   LP_A,     HP_NONE,    BW_PHRASE, 0,          235, 140, 150, 120, 185, 130, 220, 160,   0,   0, 160, 150 },
    { CH_BB,    DP_UNFOLD,  BP_PULSE2, BR_ANS3,   LP_A,     HP_NONE,    BW_PHRASE, 0,          235, 140, 150, 120, 185, 130, 220, 160,   0,   0, 160, 154 },
    { CH_F,     DP_UNFOLD2, BP_PULSE2, BR_ANS1,   LP_A,     HP_NONE,    BW_PHRASE, 0,          235, 145, 150, 125, 185, 130, 220, 160,   0,   0, 160, 158 },
    { CH_C,     DP_UNFOLD2, BP_PULSE2, BR_ANS2,   LP_A,     HP_NONE,    BW_PHRASE, 0,          235, 145, 150, 125, 185, 130, 220, 165,   0,   0, 160, 162 },
    { CH_DM,    DP_UNFOLD,  BP_PULSE2, BR_ANS3,   LP_A,     HP_NONE,    BW_HOLD,   0,          240, 150, 155, 130, 190, 130, 225, 165,   0,   0, 160, 166 },
    { CH_BB,    DP_UNFOLD,  BP_PULSE2, BR_ANS1,   LP_A,     HP_NONE,    BW_CONT,   0,          240, 150, 155, 130, 190, 130, 225, 165,   0,   0, 160, 170 },
    { CH_F,     DP_UNFOLD2, BP_PULSE2, BR_ANS2,   LP_A,     HP_NONE,    BW_HOLD,   0,          240, 155, 160, 135, 190, 130, 225, 170,   0,   0, 160, 172 },
    { CH_C,     DP_UNFOLD2, BP_PULSE2, BR_ANS3,   LP_A,     HP_NONE,    BW_REL,    0,          240, 155, 160, 135, 190, 130, 225, 170,   0,   0, 160, 175 },
    { CH_BB,    DP_NONE,    BP_LONG,   BR_OFF,    LP_A2,    HP_BREATH,  BW_HOLD,   0,          235, 120, 170, 100,   0, 150, 150,  80, 160,  60, 220,  90 },
    { CH_BB,    DP_NONE,    BP_LONG,   BR_SPARSE, LP_A2,    HP_BREATH,  BW_CONT,   0,          235, 120, 170, 100, 150, 150, 150,  80, 160,  60, 220,  85 },
    { CH_GM,    DP_GHOST1,  BP_PULSE,  BR_ANS3,   LP_A2,    HP_NONE,    BW_PHRASE, 0,          235, 130, 150, 110, 170, 130, 200, 140,   0,  30, 180, 120 },
    { CH_A,     DP_FLIGHT,  BP_PULSE2, BR_ANS2,   LP_A2,    HP_NONE,    BW_PHRASE, 0,          235, 140, 150, 115, 170, 130, 205, 150,   0,  30, 180, 140 },
    { CH_DM,    DP_UNFOLD2, BP_PULSE2, BR_ANS1,   LP_A2,    HP_NONE,    BW_HOLD,   0,          240, 170, 160, 130, 185, 130, 220, 160,   0,  20, 160, 180 },
    { CH_BB,    DP_UNFOLD2, BP_PULSE2, BR_ANS2,   LP_A2,    HP_NONE,    BW_PHRASE, 0,          240, 175, 160, 130, 185, 130, 220, 165,   0,  20, 160, 185 },
    { CH_F,     DP_UNFOLD2, BP_PULSE2, BR_ANS3,   LP_A2,    HP_NONE,    BW_HOLD,   0,          245, 180, 165, 135, 185, 130, 225, 170,   0,  10, 160, 190 },
    { CH_C,     DP_FILL,    BP_DRIVE,  BR_ANS1,   LP_A2,    HP_NONE,    BW_PHRASE, 0,          245, 185, 165, 135, 190, 120, 230, 190,   0,   0, 150, 200 },
    /* 40-55 the tunnel: the tam-tam; the driving bass, dry drums, the bronze riff; theme A returns at 48. */
    { CH_DM,    DP_TUNNEL,  BP_DRIVE,  BR_RIFF,   LP_NONE,  HP_NONE,    BW_STAB,   F_GONG,       0, 200, 140, 140, 200, 100, 235, 210,   0,   0, 110, 200 },
    { CH_BB,    DP_TUNNEL,  BP_DRIVE,  BR_RIFF2,  LP_NONE,  HP_NONE,    BW_STAB,   0,            0, 200, 140, 140, 200, 100, 235, 210,   0,   0, 110, 202 },
    { CH_GM,    DP_TUNNEL,  BP_DRIVE,  BR_RIFF,   LP_NONE,  HP_NONE,    BW_STAB,   0,            0, 200, 140, 145, 200, 100, 235, 210,   0,   0, 110, 204 },
    { CH_A,     DP_TUNNEL,  BP_DRIVE2, BR_RIFF2,  LP_NONE,  HP_NONE,    BW_STAB,   0,            0, 200, 140, 145, 200, 100, 235, 215,   0,   0, 110, 206 },
    { CH_DM,    DP_TUNNEL2, BP_DRIVE,  BR_RIFF,   LP_NONE,  HP_NONE,    BW_STAB,   0,            0, 200, 145, 150, 200, 100, 235, 215,   0,   0, 110, 208 },
    { CH_BB,    DP_TUNNEL2, BP_DRIVE,  BR_RIFF2,  LP_NONE,  HP_NONE,    BW_STAB,   0,            0, 200, 145, 150, 200, 100, 235, 215,   0,   0, 110, 210 },
    { CH_GM,    DP_TUNNEL2, BP_DRIVE,  BR_RIFF,   LP_NONE,  HP_NONE,    BW_STAB,   0,            0, 200, 145, 155, 200, 100, 235, 220,   0,   0, 110, 212 },
    { CH_A,     DP_TUNNEL2, BP_DRIVE2, BR_RIFF2,  LP_NONE,  HP_NONE,    BW_STAB,   0,            0, 200, 145, 155, 200, 100, 235, 220,   0,   0, 110, 215 },
    { CH_DM,    DP_TUNNEL2, BP_DRIVE,  BR_RIFF,   LP_A,     HP_NONE,    BW_STAB,   0,          245, 210, 150, 150, 200, 110, 240, 220,   0,   0, 130, 220 },
    { CH_BB,    DP_TUNNEL3, BP_DRIVE2, BR_ANS2,   LP_A,     HP_NONE,    BW_STAB,   0,          245, 210, 150, 150, 200, 110, 240, 220,   0,   0, 130, 224 },
    { CH_GM,    DP_TUNNEL2, BP_DRIVE,  BR_RIFF2,  LP_A,     HP_NONE,    BW_STAB,   0,          245, 215, 155, 155, 200, 110, 240, 220,   0,   0, 130, 228 },
    { CH_A,     DP_TUNNEL3, BP_DRIVE2, BR_ANS1,   LP_A,     HP_NONE,    BW_STAB,   0,          245, 215, 155, 155, 200, 110, 240, 220,   0,   0, 130, 232 },
    { CH_DM,    DP_TUNNEL3, BP_DRIVE,  BR_RIFF,   LP_A,     HP_NONE,    BW_HOLD,   0,          250, 220, 165, 165, 200, 110, 240, 225,   0,   0, 130, 236 },
    { CH_BB,    DP_TUNNEL3, BP_DRIVE,  BR_ANS2,   LP_A,     HP_NONE,    BW_PHRASE, 0,          250, 220, 175, 175, 200, 110, 240, 225,   0,   0, 130, 240 },
    { CH_GM,    DP_TUNNEL3, BP_DRIVE2, BR_RIFF2,  LP_A,     HP_NONE,    BW_HOLD,   0,          250, 225, 185, 185, 200, 110, 240, 230,   0,   0, 130, 245 },
    { CH_A,     DP_FILL,    BP_DRIVE2, BR_RIFF,   LP_A,     HP_NONE,    BW_PHRASE, 0,          250, 225, 190, 190, 200, 110, 240, 230,   0,   0, 130, 250 },
    /* 56-63 orbits: the tam-tam; D major; theme B over full bowed chords -- the climax. */
    { CH_D,     DP_CLIMAX,  BP_LONG,   BR_CLIMAX, LP_B,     HP_NONE,    BW_HOLD,   F_GONG,     255, 255, 220, 220, 200, 140, 240, 180,   0,   0, 200, 255 },
    { CH_A,     DP_CLIMAX2, BP_CLIMAX, BR_ANS2,   LP_B,     HP_NONE,    BW_CONT,   0,          255, 255, 220, 220, 200, 140, 240, 180,   0,   0, 200, 255 },
    { CH_BM,    DP_CLIMAX,  BP_LONG,   BR_CLIMAX, LP_B,     HP_NONE,    BW_HOLD,   0,          255, 255, 220, 220, 200, 140, 240, 180,   0,   0, 200, 255 },
    { CH_G,     DP_CLIMAX2, BP_CLIMAX, BR_ANS3,   LP_B,     HP_NONE,    BW_CONT,   0,          255, 255, 220, 220, 200, 140, 240, 180,   0,   0, 200, 255 },
    { CH_D,     DP_CLIMAX,  BP_CLIMAX, BR_CLIMAX, LP_B,     HP_NONE,    BW_HOLD,   0,          255, 255, 225, 225, 200, 140, 240, 185,   0,   0, 200, 255 },
    { CH_A,     DP_CLIMAX2, BP_CLIMAX, BR_ANS2,   LP_B,     HP_NONE,    BW_CONT,   0,          255, 255, 225, 225, 200, 140, 240, 185,   0,   0, 200, 255 },
    { CH_BM,    DP_CLIMAX,  BP_CLIMAX, BR_CLIMAX, LP_B,     HP_NONE,    BW_HOLD,   0,          255, 255, 225, 225, 200, 140, 240, 185,   0,   0, 200, 255 },
    { CH_G,     DP_CLIMAX2, BP_DRIVE,  BR_CLIMAX, LP_B,     HP_NONE,    BW_PHRASE, 0,          255, 255, 225, 225, 200, 140, 240, 185,   0,   0, 200, 255 },
    /* 64-71 the eclipse: the tam-tam, then the harmonic alone over the A pedal, Bb/A, Asus, A. */
    { CH_APED,  DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_OFF,    F_GONG,       0,   0,   0,   0,   0,   0,   0,   0, 220, 120, 255,  30 },
    { CH_APED,  DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_HOLD,   0,            0,   0,  60,  30,   0,   0,   0,   0, 220, 140, 255,  25 },
    { CH_APED,  DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_CONT,   0,            0,   0,  70,  30,   0,   0,   0,   0, 220, 140, 255,  25 },
    { CH_APED,  DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_CONT,   0,            0,   0,  80,  35,   0,   0,   0,   0, 220, 140, 255,  28 },
    { CH_BBA,   DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_HOLD,   0,            0,   0, 110,  70,   0,   0,   0,   0, 220, 130, 255,  35 },
    { CH_BBA,   DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_CONT,   0,            0,   0, 120,  70,   0,   0,   0,   0, 220, 120, 255,  38 },
    { CH_ASUS,  DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_HOLD,   0,            0,   0, 130,  80,   0,   0,   0,   0, 220, 110, 255,  40 },
    { CH_A,     DP_NONE,    BP_OFF,    BR_OFF,    LP_NONE,  HP_ECLIPSE, BW_HOLD,   0,            0,   0, 140,  90,   0,   0,   0,   0, 220, 100, 255,  45 },
    /* 72-79 the return: D major; the reed answers gently; percussion leaves; fade. */
    { CH_D,     DP_GHOST1,  BP_LONG,   BR_OFF,    LP_CODA,  HP_NONE,    BW_HOLD,   0,          200,  60, 160, 120, 120, 200, 130,  60,   0,  60, 240,  80 },
    { CH_BM,    DP_NONE,    BP_LONG,   BR_CODA,   LP_CODA,  HP_NONE,    BW_HOLD,   0,          200,  60, 150, 120, 120, 200, 130,  60,   0,  60, 240,  70 },
    { CH_A,     DP_GHOST1,  BP_LONG,   BR_OFF,    LP_CODA,  HP_NONE,    BW_HOLD,   0,          195,  60, 150, 115, 120, 200, 130,  60,   0,  60, 240,  65 },
    { CH_D,     DP_NONE,    BP_LONG,   BR_CODA,   LP_CODA,  HP_NONE,    BW_HOLD,   0,          190,  55, 140, 110, 120, 200, 125,  60,   0,  70, 240,  60 },
    { CH_G,     DP_NONE,    BP_OFF,    BR_OFF,    LP_CODA,  HP_NONE,    BW_HOLD,   0,          180,  50, 130, 100, 110, 200,   0,  60,   0,  70, 245,  50 },
    { CH_D,     DP_NONE,    BP_OFF,    BR_CODA,   LP_CODA,  HP_NONE,    BW_HOLD,   0,          170,  50, 120,  95, 110, 200,   0,  60,   0,  80, 245,  40 },
    { CH_A,     DP_NONE,    BP_OFF,    BR_OFF,    LP_CODA,  HP_NONE,    BW_HOLD,   0,          160,  45, 100,  80,   0, 200,   0,  60,   0,  90, 250,  25 },
    { CH_D,     DP_NONE,    BP_OFF,    BR_OFF,    LP_CODA,  HP_NONE,    BW_HOLD,   0,          150,  40,  80,  60,   0, 200,   0,  60,   0, 100, 250,  12 },
};

static const bar_t *row(uint32_t bar)
{
    return &bars[bar < SONG_BARS ? bar : SONG_BARS - 1];
}

/* ------------------------------------------------------------- accessors -- */

static const char *const k_section_names[] = {
    "the corona", "the plain", "unfolding", "the tunnel", "orbits", "eclipse", "return",
};

int song_section(uint32_t bar)
{
    if (bar < 8)  return 0;
    if (bar < 24) return 1;
    if (bar < 40) return 2;
    if (bar < 56) return 3;
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
    if (s == 0 && (row(bar)->flags & F_GONG))     d |= DR_GONG;
    if (s == 0 && (row(bar)->flags & F_GONGSOFT)) d |= DR_GONGSOFT;
    return d;
}

int song_bass(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    const uint8_t e = bass_pat[r->bass][s];
    if (e < 2) return e;
    const int note = (int)chord_root[r->chord] + (int)(e & 0x3f) - 8;
    return (e & 0x40) ? (note | SONG_SLIDE) : note;
}

int song_bronze(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return 0;
    const bar_t *r = row(bar);
    const uint8_t e = bronze_pat[r->bronze][s];
    if (e < 2) return 0;
    return chord_bronze[r->chord][e - 2];
}

int song_lead(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->lead == LP_NONE) return s == 0 ? SONG_OFF : 0;
    return lead_phrase[r->lead][bar & 7][s];
}

int song_harm(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    const bar_t *r = row(bar);
    if (r->harm == HP_NONE) return s == 0 ? SONG_OFF : 0;
    return harm_phrase[r->harm][bar & 7][s];
}

void song_bow_chord(uint32_t bar, uint8_t out[4])
{
    const bar_t *r = row(bar);
    const int on = bar < SONG_BARS && r->bow != BW_OFF && r->bow_lvl;
    for (int i = 0; i < 4; i++) out[i] = on ? chord_bow[r->chord][i] : 0;
}

int song_bow_gate(uint32_t step)
{
    const uint32_t bar = step / SONG_STEPS_PER_BAR, s = step % SONG_STEPS_PER_BAR;
    if (bar >= SONG_BARS) return s == 0 ? SONG_OFF : 0;
    return bow_pat[row(bar)->bow][s];
}

int song_lead_level(uint32_t bar)   { return bar < SONG_BARS ? row(bar)->lead_lvl   : 0; }
int song_lead_push(uint32_t bar)    { return row(bar)->lead_push; }
int song_bow_level(uint32_t bar)    { return bar < SONG_BARS ? row(bar)->bow_lvl    : 0; }
int song_bow_open(uint32_t bar)     { return row(bar)->bow_open; }
int song_bronze_level(uint32_t bar) { return bar < SONG_BARS ? row(bar)->bronze_lvl : 0; }
int song_bronze_decay(uint32_t bar) { return row(bar)->bronze_dec; }
int song_bass_level(uint32_t bar)   { return bar < SONG_BARS ? row(bar)->bass_lvl   : 0; }
int song_bass_bite(uint32_t bar)    { return row(bar)->bass_bite; }
int song_harm_level(uint32_t bar)   { return bar < SONG_BARS ? row(bar)->harm_lvl   : 0; }
int song_air_level(uint32_t bar)    { return bar < SONG_BARS ? row(bar)->air        : 0; }
int song_space(uint32_t bar)        { return row(bar)->space; }
int song_energy(uint32_t bar)       { return bar < SONG_BARS ? row(bar)->energy     : 0; }

uint32_t song_voices(uint32_t bar)
{
    if (bar >= SONG_BARS) return 0;
    const bar_t *r = row(bar);
    uint32_t v = 0, dm = 0;
    for (int s = 0; s < 16; s++) dm |= drum_pat[r->drums][s];
    if (dm & (DR_DRUM | DR_GHOST))          v |= SV_DRUM;
    if (dm & DR_RIM)                        v |= SV_RIM;
    if (dm & (DR_SCRAPE | DR_SCRAPEL))      v |= SV_SCRAPE;
    if (r->flags & (F_GONG | F_GONGSOFT))   v |= SV_GONG;
    if (r->bass != BP_OFF && r->bass_lvl)   v |= SV_BASS;
    if (r->bronze != BR_OFF && r->bronze_lvl) v |= SV_BRONZE;
    if (r->lead != LP_NONE && r->lead_lvl)  v |= SV_REED;
    if (r->bow != BW_OFF && r->bow_lvl)     v |= SV_BOW;
    if (r->harm != HP_NONE && r->harm_lvl)  v |= SV_HARM;
    if (r->air)                             v |= SV_AIR;
    return v;
}
