/* song.h -- the timetable. SLEEPER / LATENT / 2026. Phosphor.
 *
 * One table drives the music and the picture. The synth reads the notes,
 * the drums and the events; the renderer reads the cut list, the speed, the
 * distance, the light and the board; both read the rail joints from the
 * same distance function, so the clack in the score and the sleeper under
 * the camera are one number.
 *
 * Time: 160 BPM, a 16th = 2,250 samples (STEP_SAMPLES), a beat 9,000, a bar
 * 36,000; 128 bars. Steps are 0..15 within a bar; a beat is 4 steps.
 */
#ifndef SLEEPER_SONG_H
#define SLEEPER_SONG_H
#include <stdint.h>
#include "sleeper.h"

#define STEPS_PER_BAR 16

/* ---------------------------------------------------------------- sections */
enum {
    SEC_DEPARTURE, SEC_SPEED, SEC_LINE, SEC_TUNNEL, SEC_DROP1, SEC_BRIDGE,
    SEC_CITY, SEC_ARRIVAL, SEC_SLEEPER, SEC_RISER, SEC_DROP2, SEC_DROP2B,
    SEC_BLUE, SEC_DAWN, SEC_TERMINUS, SEC_CODA, SEC_COUNT
};
int song_section(uint32_t bar);                 /* bar >= 128 -> SEC_CODA */

/* ---------------------------------------------------------- the timetable */
/* Speed of the train as a fraction of cruise, Q16 (65536 = cruise, up to
 * 1.25 x cruise in the second drop). Piecewise linear between knots. */
int32_t  song_speed(uint32_t sample);
/* Distance travelled, in "cruise samples" (at cruise, one unit a sample),
 * Q8. The integral of song_speed(); exact integer arithmetic, so host and
 * device agree bit for bit. At cruise a rail joint passes every beat
 * (JOINT_UNITS) and 32 sleepers pass a beat. */
uint32_t song_distance(uint32_t sample);
#define JOINT_UNITS      (BEAT_SAMPLES << 8)     /* Q8 distance between rail joints */
#define SLEEPERS_PER_BEAT 32u
/* The joint just passed and how far past it we are, both Q8 units. */
static inline uint32_t song_joint_phase(uint32_t sample) { return song_distance(sample) % JOINT_UNITS; }

/* ------------------------------------------------------------------ shots */
enum {                    /* the camera */
    SHOT_BLACK, SHOT_BOARD, SHOT_SIDE, SHOT_AHEAD, SHOT_UNDER, SHOT_UP,
    SHOT_TUNNEL, SHOT_RAILS, SHOT_WHEEL, SHOT_KALEIDO, SHOT_COUNT
};
enum {                    /* the world outside */
    WORLD_NONE, WORLD_PLATFORM, WORLD_SUBURBS, WORLD_LINE, WORLD_TUNNEL,
    WORLD_OPEN, WORLD_BRIDGE, WORLD_CITY, WORLD_YARD, WORLD_STATION,
    WORLD_FIELDS, WORLD_COAST, WORLD_TERMINUS, WORLD_DREAM, WORLD_COUNT
};
#define CUT_RAIN      1u  /* droplets on the window                          */
#define CUT_PASSING   2u  /* the other train crosses during this shot, from its first beat */
#define CUT_POINTS    4u  /* the rails multiply and cross                    */
#define CUT_EXIT      8u  /* a tunnel exit grows as a white disc             */
#define CUT_MOUTH    16u  /* a tunnel mouth grows ahead                      */
#define CUT_CROSSING 32u  /* a level crossing passes: red lights, the bell   */
#define CUT_ROOF     64u  /* under a station roof                            */
#define CUT_STOPPED 128u  /* the train is standing (lights, no streaks)      */

typedef struct {
    uint16_t bar;         /* when: bar and beat (0..3) of the cut            */
    uint8_t  beat;
    uint8_t  shot;        /* SHOT_*                                          */
    uint8_t  world;       /* WORLD_*                                         */
    uint8_t  flags;       /* CUT_*                                           */
    uint8_t  variant;     /* a free parameter for the shot                   */
} cut_t;
/* The cut in force at `sample`; *since (may be NULL) receives samples since
 * it began. Cuts sit on beats by construction; song_check asserts it. */
const cut_t *song_cut(uint32_t sample, uint32_t *since);
unsigned      song_cut_index(uint32_t sample);     /* index into the list of the cut in force */
unsigned      song_cut_count(void);
const cut_t  *song_cut_at(unsigned index);
static inline uint32_t cut_sample(const cut_t *c) { return c->bar * BAR_SAMPLES + c->beat * BEAT_SAMPLES; }

/* ------------------------------------------------------------------ board */
/* The split-flap board. Every text change is a flip; each character that
 * changes flips over BOARD_FLIP_SAMPLES, starting BOARD_STAGGER_SAMPLES
 * after the one to its left. The renderer draws that; the synth clicks it. */
#define BOARD_COLS 20
#define BOARD_ROWS 3
#define BOARD_FLIP_SAMPLES     1200u             /* 50 ms: three fields      */
#define BOARD_STAGGER_SAMPLES   300u             /* 12.5 ms between columns  */
typedef struct {
    uint16_t bar; uint8_t beat; uint8_t step;    /* when it is set           */
    const char *row[BOARD_ROWS];                 /* up to BOARD_COLS each; NULL = blank */
} board_t;
/* The board text in force at `sample` (NULL before the first), the one
 * before it (NULL if none) for the flip, and samples since the change. */
const board_t *song_board(uint32_t sample, const board_t **previous, uint32_t *since);
unsigned       song_board_count(void);
const board_t *song_board_at(unsigned index);

/* ------------------------------------------------------------ per bar data */
typedef struct {
    uint8_t chord;        /* CH_* below                                      */
    uint8_t light;        /* 0 night .. 255 full dawn (renderer palette)     */
    uint8_t energy;       /* 0..255, the score's own loudness for the picture */
    uint8_t drums;        /* DP_* pattern id, 0 = none                       */
    uint8_t bass;         /* BP_* pattern id, 0 = none                       */
    uint8_t lead;         /* LP_* lead phrase id, 0 = none                   */
    uint8_t piano;        /* PP_* comping pattern, 0 = none                  */
    uint8_t pad;          /* pad level 0..255                                */
    uint8_t choir;        /* choir level 0..255                              */
    uint8_t filter;       /* the master lowpass, 0 closed (tunnel) .. 255 open */
    uint8_t space;        /* plate and delay send, 0..255                    */
    uint8_t clack;        /* joint clack level 0..255 (0 in the station)     */
} bar_t;
/* Safe to call from both cores at once: the pointer is into a slot chosen by
 * the bar, and a caller may hold it only while it is asking about that bar. */
const bar_t *song_bar(uint32_t bar);

enum { CH_Em, CH_C, CH_G, CH_D, CH_Bm, CH_Am, CH_Cmaj7, CH_Em9, CH_Gmaj7, CH_Dsus, CH_B7, CH_COUNT };
/* root MIDI note and the four chord tones above it (MIDI), for the voices */
int  song_chord_root(int chord);
const int8_t *song_chord_tones(int chord);      /* 4 notes, root first, octave 4 */

/* ---------------------------------------------------------------- events */
#define EV_KICK      1u
#define EV_SNARE     2u
#define EV_GHOST     4u
#define EV_HAT       8u
#define EV_OPEN     16u
#define EV_RIDE     32u
#define EV_CRASH    64u
#define EV_BELL    128u   /* the departure bell                              */
#define EV_CHIME   256u   /* the door chime                                  */
#define EV_HORN    512u   /* the other train: pitch bends down, pans R -> L  */
#define EV_BRAKE  1024u   /* the squeal begins (lasts two bars)              */
#define EV_POINTS 2048u   /* the rattle fill (one beat)                      */
#define EV_XING   4096u   /* the level crossing bell (four beats)            */
#define EV_ROLL   8192u   /* snare roll step (riser)                         */
#define EV_FLASH 16384u   /* a visual accent the renderer may use            */
unsigned song_events(uint32_t bar, unsigned step);   /* drums + events at a step */

/* Melodic events at a step, one voice each. Notes are MIDI, 0 = rest,
 * N_OFF = release, and N_TIE holds the previous note. */
#define N_OFF  1
#define N_TIE  2
int song_lead(uint32_t bar, unsigned step);     /* the tune                 */
int song_bass(uint32_t bar, unsigned step);     /* the Reese                */
int song_piano(uint32_t bar, unsigned step, int voice);   /* 4 voices        */
int song_pad(uint32_t bar, int voice);          /* 4 voices, one chord a bar */
int song_choir(uint32_t bar, unsigned step);    /* theme B on "oo"          */
/* Portamento: the lead slides into this note from the previous one. */
int song_lead_slide(uint32_t bar, unsigned step);

void song_init(void);                           /* builds the distance table; idempotent */
#endif
