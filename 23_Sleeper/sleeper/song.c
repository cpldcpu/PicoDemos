/* song.c -- the timetable and the score. SLEEPER / LATENT / 2026. Phosphor.
 *
 * Everything the music and the picture share lives here: the speed profile
 * and its exact integral, the cut list, the board texts, the section ladder,
 * the per-bar arrangement, the drums and events, and the notes. Nothing in
 * this file depends on the previous call; every accessor is a pure function
 * of (bar, step) or of the sample.
 *
 * E minor, 160 BPM, 128 bars. Dawn is the relative major, G, from bar 96.
 */

#include "song.h"
#include <string.h>

/* ================================================================ sections */

static const uint8_t k_section_start[SEC_COUNT] = {
    0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120
};

int song_section(uint32_t bar)
{
    int s = SEC_COUNT - 1;
    while (s > 0 && bar < k_section_start[s]) s--;
    return s;
}

/* ============================================================== timetable */

/* Speed knots: bar, Q16 fraction of cruise. Linear between them. */
typedef struct { uint16_t bar; int32_t v; } knot_t;
static const knot_t k_knots[] = {
    {   0,     0 }, {   6,     0 }, {  16, 65536 }, {  58, 65536 },
    {  62,     0 }, {  72,     0 }, {  80, 81920 }, {  88, 81920 },
    {  96, 65536 }, { 114, 65536 }, { 119,     0 }, { 128,     0 },
};
#define KNOTS ((int)(sizeof k_knots / sizeof k_knots[0]))

/* Q8 distance at each knot, built once. */
static uint32_t g_knot_dist[KNOTS];
static int g_built;

/* Exact distance from knot i to sample s (s within the segment), Q8. */
static uint32_t seg_distance(int i, uint32_t s)
{
    const uint32_t s0 = k_knots[i].bar * BAR_SAMPLES, s1 = k_knots[i + 1].bar * BAR_SAMPLES;
    const int64_t  v0 = k_knots[i].v, v1 = k_knots[i + 1].v;
    const int64_t  dt = (int64_t)(s - s0), len = (int64_t)(s1 - s0);
    /* Q16: dt*v0 + dt^2*(v1-v0)/(2*len); then to Q8. */
    int64_t d = dt * v0 + (dt * dt * (v1 - v0)) / (2 * len);
    return (uint32_t)(d >> 8);
}

void song_init(void)
{
    if (g_built) return;
    g_knot_dist[0] = 0;
    for (int i = 0; i + 1 < KNOTS; i++)
        g_knot_dist[i + 1] = g_knot_dist[i] + seg_distance(i, k_knots[i + 1].bar * BAR_SAMPLES);
    g_built = 1;
}

static int knot_index(uint32_t sample)
{
    int i = 0;
    while (i + 2 < KNOTS && sample >= (uint32_t)k_knots[i + 1].bar * BAR_SAMPLES) i++;
    return i;
}

int32_t song_speed(uint32_t sample)
{
    if (sample >= DURATION_SAMPLES) return 0;
    const int i = knot_index(sample);
    const uint32_t s0 = k_knots[i].bar * BAR_SAMPLES, s1 = k_knots[i + 1].bar * BAR_SAMPLES;
    const int64_t  v0 = k_knots[i].v, v1 = k_knots[i + 1].v;
    return (int32_t)(v0 + (v1 - v0) * (int64_t)(sample - s0) / (int64_t)(s1 - s0));
}

uint32_t song_distance(uint32_t sample)
{
    song_init();
    if (sample >= DURATION_SAMPLES) sample = DURATION_SAMPLES;
    const int i = knot_index(sample);
    return g_knot_dist[i] + seg_distance(i, sample);
}

/* =================================================================== cuts */

#define C(bar, beat, shot, world, flags, variant) { bar, beat, SHOT_##shot, WORLD_##world, flags, variant }
static const cut_t k_cuts[] = {
    C(  0, 0, BOARD,   NONE,     0, 0),
    C(  4, 0, SIDE,    PLATFORM, CUT_ROOF | CUT_STOPPED, 0),
    C( 12, 0, SIDE,    SUBURBS,  CUT_CROSSING, 0),
    C( 14, 0, UNDER,   LINE,     0, 0),
    C( 16, 0, AHEAD,   LINE,     0, 0),
    C( 20, 0, UNDER,   LINE,     0, 1),
    C( 22, 0, AHEAD,   LINE,     CUT_MOUTH, 0),
    C( 24, 0, TUNNEL,  TUNNEL,   0, 0),
    C( 26, 0, SIDE,    TUNNEL,   0, 0),
    C( 28, 0, TUNNEL,  TUNNEL,   0, 1),
    C( 30, 0, TUNNEL,  TUNNEL,   CUT_EXIT, 0),
    C( 32, 0, SIDE,    OPEN,     0, 0),
    C( 34, 0, AHEAD,   OPEN,     CUT_PASSING, 1),   /* the headlight in the last beat */
    C( 36, 0, SIDE,    OPEN,     CUT_PASSING, 0),   /* the other train crosses */
    C( 38, 0, UNDER,   OPEN,     0, 0),
    C( 40, 0, AHEAD,   BRIDGE,   0, 0),
    C( 44, 0, SIDE,    BRIDGE,   0, 0),
    C( 48, 0, SIDE,    CITY,     0, 0),
    C( 50, 0, UP,      CITY,     CUT_RAIN, 0),
    C( 52, 0, AHEAD,   CITY,     CUT_RAIN, 0),
    C( 54, 0, SIDE,    CITY,     CUT_RAIN, 1),
    C( 56, 0, AHEAD,   YARD,     CUT_POINTS | CUT_RAIN, 0),
    C( 58, 0, SIDE,    YARD,     CUT_RAIN, 0),
    C( 60, 0, SIDE,    STATION,  CUT_ROOF | CUT_RAIN, 0),
    C( 64, 0, SIDE,    STATION,  CUT_ROOF | CUT_STOPPED | CUT_RAIN, 1),   /* one lamp, the rain on the glass; sleep */
    C( 66, 0, RAILS,   DREAM,    0, 0),
    C( 68, 0, WHEEL,   DREAM,    0, 0),
    C( 70, 0, KALEIDO, DREAM,    0, 0),
    C( 72, 0, RAILS,   DREAM,    0, 1),
    C( 76, 0, BOARD,   DREAM,    0, 1),
    C( 78, 0, WHEEL,   DREAM,    0, 1),
    C( 79, 0, KALEIDO, DREAM,    0, 1),
    C( 79, 1, RAILS,   DREAM,    0, 2),
    C( 79, 2, WHEEL,   DREAM,    0, 2),
    C( 79, 3, KALEIDO, DREAM,    0, 2),
    C( 80, 0, SIDE,    OPEN,     0, 1),
    C( 82, 0, AHEAD,   OPEN,     0, 0),
    C( 84, 0, UNDER,   OPEN,     0, 1),
    C( 85, 0, SIDE,    OPEN,     CUT_PASSING, 0),
    C( 87, 0, AHEAD,   OPEN,     0, 1),
    C( 88, 0, SIDE,    CITY,     0, 2),              /* the city thinning: far, receding */
    C( 89, 0, AHEAD,   OPEN,     0, 0),
    C( 90, 0, SIDE,    OPEN,     0, 1),              /* the last glow on the horizon */
    C( 91, 0, UNDER,   OPEN,     0, 1),
    C( 92, 0, SIDE,    OPEN,     0, 0),
    C( 93, 0, AHEAD,   FIELDS,   0, 0),
    C( 94, 0, SIDE,    FIELDS,   0, 2),              /* the first field, still dark */
    C( 95, 0, AHEAD,   FIELDS,   0, 1),
    C( 96, 0, SIDE,    FIELDS,   0, 0),
    C(100, 0, SIDE,    FIELDS,   0, 1),              /* the sea beyond */
    C(102, 0, AHEAD,   COAST,    0, 0),
    C(104, 0, SIDE,    COAST,    0, 0),
    C(108, 0, AHEAD,   COAST,    0, 1),
    C(110, 0, SIDE,    COAST,    0, 1),              /* the sun clears the haze */
    C(112, 0, AHEAD,   TERMINUS, CUT_POINTS, 0),
    C(116, 0, SIDE,    TERMINUS, CUT_ROOF, 0),
    C(120, 0, BOARD,   NONE,     0, 2),
};
#undef C
#define CUTS ((unsigned)(sizeof k_cuts / sizeof k_cuts[0]))

unsigned song_cut_count(void) { return CUTS; }
const cut_t *song_cut_at(unsigned index) { return &k_cuts[index < CUTS ? index : CUTS - 1]; }

unsigned song_cut_index(uint32_t sample)
{
    unsigned i = 0;
    while (i + 1 < CUTS && sample >= cut_sample(&k_cuts[i + 1])) i++;
    return i;
}

const cut_t *song_cut(uint32_t sample, uint32_t *since)
{
    const unsigned i = song_cut_index(sample);
    if (since) *since = sample - cut_sample(&k_cuts[i]);
    return &k_cuts[i];
}

/* ================================================================== board */

static const board_t k_boards[] = {
    {   0, 0, 0, { "LATENT",      "PRESENTS",              NULL } },
    {   2, 0, 0, { "SLEEPER",     "23:00  NIGHT SERVICE",  NULL } },
    {  60, 0, 0, { "PERSISTENCE", "23:47",                 NULL } },
    {  76, 0, 0, { "QUICKSILVER", NULL, NULL } },
    {  76, 2, 0, { "HELION",      NULL, NULL } },
    {  77, 0, 0, { "PERSISTENCE", NULL, NULL } },
    { 116, 0, 0, { "LATENT",      "06:12",                 "TERMINUS" } },
    { 120, 0, 0, { "SLEEPER",     "LATENT",                "2026" } },
    { 121, 3, 0, { "DIRECTION AND MUSIC", "PHOSPHOR",      "CLAUDE FABLE 5.1" } },
    { 123, 1, 0, { "CODE AND HARDWARE",   "OVERSCAN",      "CLAUDE OPUS 5" } },
    { 124, 3, 0, { "SKIES",       "PHASE",                 "GPT-6 ASTRA" } },
    { 126, 0, 0, { "FOR AZURE",   NULL,                    "UNTIL THE NEXT TRAIN" } },
    { 127, 2, 0, { NULL, NULL, NULL } },      /* the last flaps are the last sound; black */
};
#define BOARDS ((unsigned)(sizeof k_boards / sizeof k_boards[0]))

static inline uint32_t board_sample(const board_t *b)
{
    return b->bar * BAR_SAMPLES + b->beat * BEAT_SAMPLES + b->step * STEP_SAMPLES;
}

unsigned song_board_count(void) { return BOARDS; }
const board_t *song_board_at(unsigned index) { return &k_boards[index < BOARDS ? index : BOARDS - 1]; }

const board_t *song_board(uint32_t sample, const board_t **previous, uint32_t *since)
{
    if (sample < board_sample(&k_boards[0])) {
        if (previous) *previous = NULL;
        if (since) *since = 0;
        return NULL;
    }
    unsigned i = 0;
    while (i + 1 < BOARDS && sample >= board_sample(&k_boards[i + 1])) i++;
    if (previous) *previous = i ? &k_boards[i - 1] : NULL;
    if (since) *since = sample - board_sample(&k_boards[i]);
    return &k_boards[i];
}

/* ================================================================= chords */

static const int8_t k_tones[CH_COUNT][4] = {
    [CH_Em]    = { 64, 67, 71, 76 },
    [CH_C]     = { 60, 64, 67, 72 },
    [CH_G]     = { 55, 59, 62, 67 },
    [CH_D]     = { 62, 66, 69, 74 },
    [CH_Bm]    = { 59, 62, 66, 71 },
    [CH_Am]    = { 57, 60, 64, 67 },
    [CH_Cmaj7] = { 60, 64, 67, 71 },
    [CH_Em9]   = { 64, 67, 71, 78 },
    [CH_Gmaj7] = { 55, 59, 62, 66 },
    [CH_Dsus]  = { 62, 67, 69, 74 },
    [CH_B7]    = { 59, 63, 66, 69 },
};
static const int8_t k_root[CH_COUNT] = {
    [CH_Em] = 40, [CH_C] = 36, [CH_G] = 43, [CH_D] = 38, [CH_Bm] = 35, [CH_Am] = 33,
    [CH_Cmaj7] = 36, [CH_Em9] = 40, [CH_Gmaj7] = 43, [CH_Dsus] = 38, [CH_B7] = 35,
};
int song_chord_root(int chord) { return k_root[chord < CH_COUNT ? chord : 0]; }
const int8_t *song_chord_tones(int chord) { return k_tones[chord < CH_COUNT ? chord : 0]; }

/* Four-bar progressions. */
enum { PR_NIGHT, PR_NIGHT2, PR_B, PR_DAWN, PR_DAWN2, PR_STATION, PR_DREAM, PR_END, PR_SUS, PR_COUNT };
static const uint8_t k_prog[PR_COUNT][4] = {
    [PR_NIGHT]   = { CH_Em,  CH_C,     CH_G,     CH_D    },
    [PR_NIGHT2]  = { CH_Em,  CH_C,     CH_D,     CH_Bm   },
    [PR_B]       = { CH_C,   CH_G,     CH_D,     CH_Em   },
    [PR_DAWN]    = { CH_G,   CH_D,     CH_Em,    CH_C    },
    [PR_DAWN2]   = { CH_G,   CH_D,     CH_C,     CH_G    },
    [PR_STATION] = { CH_Em9, CH_Cmaj7, CH_Gmaj7, CH_Dsus },
    [PR_DREAM]   = { CH_Cmaj7, CH_Em9, CH_Am,    CH_Bm   },
    [PR_END]     = { CH_C,   CH_D,     CH_G,     CH_G    },
    [PR_SUS]     = { CH_Em9, CH_Em9,   CH_Cmaj7, CH_Cmaj7 },
};

/* ============================================================ arrangement */

enum { DP_NONE, DP_HALF, DP_HALF_SOFT, DP_STEP, DP_RIDE, DP_STEP_B, DP_HATS, DP_ROLL, DP_KICKS, DP_COUNT };
enum { BP_NONE, BP_LONG, BP_DRIVE, BP_HOLD, BP_OCT, BP_PULSE, BP_COUNT };
enum { LP_NONE, LP_A, LP_A_END, LP_A_VAR, LP_A_DAWN, LP_TAIL, LP_COUNT };
enum { PP_NONE, PP_ONE, PP_COMP, PP_SKANK, PP_DREAM, PP_ANSWER, PP_END, PP_COUNT };
enum { CP_NONE, CP_DREAM, CP_B_DAWN, CP_B_NIGHT, CP_COUNT };

/* A row of the arrangement: bars [from, to), and what plays. */
typedef struct {
    uint8_t from, to;
    uint8_t prog, prog2;           /* progressions for bars 0-3 and 4-7 of each phrase */
    uint8_t drums, bass, lead, piano, pad, choir, filter, space, clack, energy;
} row_t;
static const row_t k_rows[] = {
    /* from to  prog        prog2       drums        bass      lead       piano      pad  choir filt space clack energy */
    {   0,   4, PR_SUS,     PR_SUS,     DP_NONE,     BP_NONE,  LP_NONE,   PP_NONE,   40,  0,   200, 200, 0,    20  },
    {   4,   8, PR_STATION, PR_STATION, DP_NONE,     BP_NONE,  LP_NONE,   PP_ONE,    60,  0,   220, 200, 160,  40  },
    {   8,  12, PR_NIGHT,   PR_NIGHT2,  DP_NONE,     BP_LONG,  LP_NONE,   PP_ONE,    80,  0,   230, 180, 200,  70  },
    {  12,  16, PR_NIGHT,   PR_NIGHT2,  DP_HALF,     BP_LONG,  LP_NONE,   PP_COMP,   90,  0,   240, 170, 220,  110 },
    {  16,  24, PR_NIGHT,   PR_NIGHT2,  DP_STEP,     BP_DRIVE, LP_A,      PP_COMP,   100, 0,   255, 160, 255,  170 },
    {  24,  28, PR_NIGHT,   PR_NIGHT2,  DP_HATS,     BP_PULSE, LP_NONE,   PP_NONE,   120, 0,   60,  60,  200,  120 },
    {  28,  32, PR_NIGHT,   PR_NIGHT2,  DP_KICKS,    BP_PULSE, LP_NONE,   PP_SKANK,  140, 0,   90,  80,  220,  150 },
    {  32,  40, PR_NIGHT,   PR_NIGHT2,  DP_RIDE,     BP_DRIVE, LP_A_END,  PP_COMP,   120, 0,   255, 170, 255,  255 },
    {  40,  48, PR_B,       PR_B,       DP_HALF,     BP_LONG,  LP_NONE,   PP_ANSWER, 200, 110, 255, 220, 200,  150 },
    {  48,  56, PR_NIGHT,   PR_NIGHT2,  DP_STEP_B,   BP_DRIVE, LP_A_VAR,  PP_SKANK,  110, 0,   255, 170, 255,  220 },
    {  56,  58, PR_NIGHT,   PR_NIGHT,   DP_STEP,     BP_DRIVE, LP_TAIL,   PP_COMP,   110, 0,   255, 180, 255,  200 },
    {  58,  60, PR_STATION, PR_STATION, DP_HALF_SOFT,BP_HOLD,  LP_NONE,   PP_ONE,    140, 0,   230, 220, 200,  120 },
    {  60,  64, PR_STATION, PR_STATION, DP_NONE,     BP_NONE,  LP_NONE,   PP_ONE,    160, 0,   220, 240, 120,  60  },
    {  64,  72, PR_DREAM,   PR_DREAM,   DP_NONE,     BP_NONE,  LP_NONE,   PP_DREAM,  200, 120, 200, 255, 60,   50  },
    {  72,  76, PR_DREAM,   PR_DREAM,   DP_HATS,     BP_HOLD,  LP_NONE,   PP_DREAM,  200, 160, 160, 240, 120,  90  },
    {  76,  78, PR_NIGHT,   PR_NIGHT,   DP_KICKS,    BP_PULSE, LP_NONE,   PP_SKANK,  160, 120, 200, 200, 200,  150 },
    {  78,  80, PR_NIGHT,   PR_NIGHT,   DP_ROLL,     BP_PULSE, LP_NONE,   PP_SKANK,  160, 100, 255, 160, 255,  210 },
    {  80,  88, PR_NIGHT,   PR_NIGHT2,  DP_RIDE,     BP_DRIVE, LP_A,      PP_COMP,   130, 0,   255, 170, 255,  255 },
    {  88,  92, PR_B,       PR_B,       DP_RIDE,     BP_DRIVE, LP_A_END,  PP_COMP,   180, 120, 255, 170, 255,  255 },
    {  92,  96, PR_NIGHT2,  PR_NIGHT2,  DP_STEP,     BP_DRIVE, LP_TAIL,   PP_COMP,   150, 60,  255, 180, 255,  220 },
    {  96, 104, PR_DAWN,    PR_DAWN,    DP_HALF,     BP_LONG,  LP_NONE,   PP_ANSWER, 220, 255, 255, 230, 200,  160 },
    { 104, 112, PR_DAWN,    PR_DAWN2,   DP_STEP,     BP_DRIVE, LP_A_DAWN, PP_COMP,   180, 200, 255, 200, 255,  240 },
    { 112, 114, PR_DAWN,    PR_DAWN,    DP_STEP,     BP_DRIVE, LP_TAIL,   PP_COMP,   180, 160, 255, 200, 255,  200 },
    { 114, 116, PR_END,     PR_END,     DP_HALF_SOFT,BP_HOLD,  LP_NONE,   PP_ONE,    200, 120, 240, 220, 200,  120 },
    { 116, 120, PR_END,     PR_END,     DP_NONE,     BP_HOLD,  LP_NONE,   PP_ONE,    200, 80,  230, 240, 120,  60  },
    { 120, 126, PR_END,     PR_END,     DP_NONE,     BP_NONE,  LP_NONE,   PP_END,    120, 0,   220, 255, 0,    30  },
    { 126, 128, PR_END,     PR_END,     DP_NONE,     BP_NONE,  LP_NONE,   PP_NONE,   60,  0,   200, 255, 0,    10  },
};
#define ROWS ((int)(sizeof k_rows / sizeof k_rows[0]))

static const row_t *row_of(uint32_t bar)
{
    for (int i = 0; i < ROWS; i++)
        if (bar >= k_rows[i].from && bar < k_rows[i].to) return &k_rows[i];
    return &k_rows[ROWS - 1];
}

/* Light: night until 86, first blue from 87, blue hour 96-103, dawn 104+. */
static uint8_t light_of(uint32_t bar)
{
    if (bar < 87) return 0;
    if (bar < 96) return (uint8_t)((bar - 86) * 4);           /* 4 .. 36  */
    if (bar < 104) return (uint8_t)(40 + (bar - 96) * 12);    /* 40 .. 124 */
    if (bar < 112) return (uint8_t)(130 + (bar - 104) * 15);  /* 130 .. 235 */
    return 255;
}

/* Both cores call this continuously and they are not on the same bar: core 1
 * synthesises up to 41 ms ahead of the sample core 0 draws. One shared cache
 * was a race (Overscan found it: two of five device runs diverged from the
 * host at a bar line). Four slots indexed by the bar: cores asking for
 * different bars write different slots, and cores asking for the same bar
 * write identical bytes. They never differ by four bars. */
static bar_t g_bar_cache[4];
const bar_t *song_bar(uint32_t bar)
{
    if (bar >= SONG_BARS) bar = SONG_BARS - 1;
    const row_t *r = row_of(bar);
    const uint8_t *prog = k_prog[(bar & 4) ? r->prog2 : r->prog];
    bar_t b;
    b.chord  = prog[bar & 3];
    b.light  = light_of(bar);
    b.energy = r->energy;
    b.drums  = r->drums;
    b.bass   = r->bass;
    b.lead   = r->lead;
    b.piano  = r->piano;
    b.pad    = r->pad;
    b.choir  = r->choir;
    b.filter = r->filter;
    b.space  = r->space;
    b.clack  = r->clack;
    bar_t *slot = &g_bar_cache[bar & 3];
    *slot = b;
    return slot;
}

/* ================================================================== drums */

/* 16 steps a bar; bits are EV_*. */
#define K EV_KICK
#define S EV_SNARE
#define Gh EV_GHOST
#define H EV_HAT
#define O EV_OPEN
#define R EV_RIDE
static const uint16_t k_drums[DP_COUNT][16] = {
    [DP_NONE]      = { 0 },
    [DP_HALF]      = { K|H, 0, H, 0,  H, 0, H|Gh, 0,  S|H, 0, H, 0,  H, 0, O, Gh },
    [DP_HALF_SOFT] = { K|H, 0, H, 0,  H, 0, H, 0,  S|H, 0, H, 0,  H, 0, H, 0 },
    [DP_STEP]      = { K|H, 0, H, 0,  S|H, 0, H, Gh,  H, 0, K|H, 0,  S|H, 0, O, Gh },
    [DP_RIDE]      = { K|H|R, 0, H, 0,  S|H|R, 0, H, Gh,  H|R, 0, K|H, 0,  S|H|R, 0, O, Gh },
    [DP_STEP_B]    = { K|H, 0, H, Gh,  S|H, 0, K|H, 0,  H, 0, K|H, Gh,  S|H, 0, O, K },
    [DP_HATS]      = { H, 0, H, 0,  H, 0, H, 0,  H, 0, H, 0,  H, 0, O, 0 },
    [DP_ROLL]      = { K|H, 0, H, 0,  S|H, 0, H, 0,  H, 0, K|H, 0,  S|H, 0, H, 0 },
    [DP_KICKS]     = { K|H, 0, H, 0,  H, 0, K|H, 0,  H, 0, H, 0,  K|H, 0, O, 0 },
};
#undef K
#undef S
#undef Gh
#undef H
#undef O
#undef R

unsigned song_events(uint32_t bar, unsigned step)
{
    if (bar >= SONG_BARS) return 0;
    step &= 15;
    const bar_t *b = song_bar(bar);
    unsigned e = k_drums[b->drums][step];
    /* The riser: steps of the snare roll thicken over bars 78-79. */
    if (bar == 78 && (step & 3) == 0) e |= EV_ROLL;
    if (bar == 78 && step >= 8 && (step & 1) == 0) e |= EV_ROLL;
    if (bar == 79) e |= EV_ROLL;
    /* crashes on the arrivals */
    if (step == 0 && (bar == 32 || bar == 80 || bar == 104 || bar == 16 || bar == 48 || bar == 88))
        e |= EV_CRASH | EV_FLASH;
    /* the timetable's own events */
    if (bar == 4 && step == 0) e |= EV_BELL;
    if ((bar == 5 && step == 8) || (bar == 62 && step == 8) || (bar == 119 && step == 8)) e |= EV_CHIME;
    if ((bar == 35 && step == 12) || (bar == 84 && step == 12)) e |= EV_HORN;
    if ((bar == 58 || bar == 114) && step == 0) e |= EV_BRAKE;
    if ((bar == 56 || bar == 112) && step == 0) e |= EV_POINTS;
    if (bar == 12 && (step & 3) == 0) e |= EV_XING;
    return e;
}

/* ================================================================= melody */

/* Phrases: 8 bars x 16 steps. 0 = hold, N_OFF = release, else MIDI; bit 7
 * marks a slide into the note. */
#define SL 0x80
#define _  0
#define X  N_OFF
static const uint8_t k_lead[LP_COUNT][8][16] = {
    [LP_A] = {
        { 76,_,_,_, _,_,_,_, _,_,74,_, 71,_,_,_ },
        { 67,_,_,_, _,_,69,_, 71,_,_,_, _,_,X,_ },
        { 76,_,_,_, _,_,_,_, _,_,74,_, 71,_,_,_ },
        { 69,_,_,_, 67,_,_,_, 66,_,_,_, 64,_,_,_ },
        { 79,_,_,_, _,_,_,_, _,_,78,_, 76,_,_,_ },
        { 74,_,_,_, _,_,76,_, 78,_,_,_, _,_,X,_ },
        { 79,_,_,_, _,_,_,_, _,_,78,_, 74,_,_,_ },
        { 71,_,_,_, _,_,_,_, _,_,_,_, _,_,X,_ },
    },
    [LP_A_END] = {
        { 76,_,_,_, _,_,_,_, _,_,74,_, 71,_,_,_ },
        { 67,_,_,_, _,_,69,_, 71,_,_,_, _,_,X,_ },
        { 76,_,_,_, _,_,_,_, _,_,74,_, 71,_,_,_ },
        { 69,_,_,_, 67,_,_,_, 66,_,_,_, 64,_,_,_ },
        { 79,_,_,_, _,_,_,_, _,_,78,_, 76,_,_,_ },
        { 74,_,_,_, _,_,76,_, 78,_,_,_, _,_,X,_ },
        { 79,_,_,_, _,_,_,_, _,_,81,_, 79|SL,_,_,_ },
        { 76,_,_,_, _,_,_,_, _,_,_,_, _,_,X,_ },
    },
    [LP_A_VAR] = {
        { 76,_,_,_, _,_,78,_, 76,_,74,_, 71,_,_,_ },
        { 67,_,69,_, _,_,71,_, 72,_,71,_, _,_,X,_ },
        { 76,_,_,_, _,_,79,_, _,_,74,_, 71,_,_,_ },
        { 69,_,_,_, 67,_,66,_, 64,_,_,_, 62,_,64,_ },
        { 79,_,_,_, _,_,81,_, 79,_,78,_, 76,_,_,_ },
        { 74,_,_,_, 76,_,_,_, 78,_,79,_, _,_,X,_ },
        { 81,_,_,_, _,_,_,_, 79,_,78,_, 74,_,_,_ },
        { 71,_,_,_, _,_,_,_, 76,_,_,_, _,_,X,_ },
    },
    /* G major: the same shape a third up in the key, over G D Em C. */
    [LP_A_DAWN] = {
        { 79,_,_,_, _,_,_,_, _,_,78,_, 74,_,_,_ },
        { 69,_,_,_, _,_,71,_, 74,_,_,_, _,_,X,_ },
        { 79,_,_,_, _,_,_,_, _,_,78,_, 74,_,_,_ },
        { 72,_,_,_, 71,_,_,_, 69,_,_,_, 67,_,_,_ },
        { 83,_,_,_, _,_,_,_, _,_,81,_, 79,_,_,_ },
        { 78,_,_,_, _,_,79,_, 81,_,_,_, _,_,X,_ },
        { 83,_,_,_, _,_,_,_, _,_,84,_, 83|SL,_,_,_ },
        { 79,_,_,_, _,_,_,_, _,_,_,_, _,_,X,_ },
    },
    /* Two bars before an arrival: the tail of the phrase, rising. */
    [LP_TAIL] = {
        { 71,_,_,_, _,_,74,_, 76,_,_,_, 78,_,_,_ },
        { 79,_,_,_, _,_,_,_, _,_,_,_, X,_,_,_ },
        { 71,_,_,_, _,_,74,_, 76,_,_,_, 78,_,_,_ },
        { 79,_,_,_, _,_,_,_, _,_,_,_, X,_,_,_ },
        { 0 }, { 0 }, { 0 }, { 0 },
    },
};

static int lead_cell(uint32_t bar, unsigned step)
{
    if (bar >= SONG_BARS) return 0;
    const bar_t *b = song_bar(bar);
    if (!b->lead) return (step & 15) == 0 ? N_OFF : 0;
    const row_t *r = row_of(bar);
    return k_lead[b->lead][(bar - r->from) & 7][step & 15];
}
int song_lead(uint32_t bar, unsigned step)       { return lead_cell(bar, step) & 0x7f; }
int song_lead_slide(uint32_t bar, unsigned step) { return (lead_cell(bar, step) & SL) != 0; }

/* ================================================================== bass */

/* Steps at which the Reese plays: 0 = hold, X = release, R = root, R8 = root
 * an octave up, R5 = the fifth. (N_OFF is 1, so the codes start at 4.) */
#define R  4
#define R8 5
#define R5 6
static const uint8_t k_bass[BP_COUNT][16] = {
    [BP_NONE]  = { 0 },
    [BP_LONG]  = { R,_,_,_, _,_,_,_, _,_,X,_, R,_,_,_ },
    [BP_DRIVE] = { R,_,_,_, _,_,R,_, X,_,R,_, R,_,_,X },
    [BP_HOLD]  = { R,_,_,_, _,_,_,_, _,_,_,_, _,_,_,_ },
    [BP_OCT]   = { R,_,_,_, R8,_,_,_, R,_,_,_, R5,_,_,_ },
    [BP_PULSE] = { R,_,X,_, R,_,X,_, R,_,X,_, R,_,X,_ },
};
#undef R
#undef R8
#undef R5
#undef _
#undef X

int song_bass(uint32_t bar, unsigned step)
{
    if (bar >= SONG_BARS) return 0;
    const bar_t *b = song_bar(bar);
    const unsigned c = k_bass[b->bass][step & 15];
    if (b->bass == BP_NONE) return (step & 15) == 0 ? N_OFF : 0;   /* a row without a bass releases it */
    if (c == 0) return 0;
    if (c == N_OFF) return N_OFF;
    const int root = song_chord_root(b->chord);
    return c == 4 ? root : c == 5 ? root + 12 : root + 7;
}

/* ================================================================== piano */

/* The dream: the tune upside down and slow, one voice. Bars 64-75. */
static const uint8_t k_dream[12][16] = {
    { 64,0,0,0, 0,0,0,0, 66,0,0,0, 67,0,0,0 },
    { 71,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { 64,0,0,0, 0,0,0,0, 66,0,0,0, 67,0,0,0 },
    { 69,0,0,0, 71,0,0,0, 72,0,0,0, 0,0,0,0 },
    { 62,0,0,0, 0,0,0,0, 64,0,0,0, 66,0,0,0 },
    { 67,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    { 62,0,0,0, 0,0,0,0, 64,0,0,0, 66,0,0,0 },
    { 64,0,0,0, 0,0,0,0, 0,0,0,0, N_OFF,0,0,0 },
    { 64,0,0,0, 0,0,66,0, 67,0,0,0, 71,0,0,0 },
    { 72,0,0,0, 0,0,0,0, 71,0,0,0, 67,0,0,0 },
    { 64,0,0,0, 0,0,66,0, 67,0,0,0, 71,0,0,0 },
    { 74,0,0,0, 0,0,0,0, 0,0,0,0, N_OFF,0,0,0 },
};

int song_piano(uint32_t bar, unsigned step, int voice)
{
    if (bar >= SONG_BARS) return 0;
    const bar_t *b = song_bar(bar);
    step &= 15;
    const int8_t *t = song_chord_tones(b->chord);
    switch (b->piano) {
    case PP_NONE:
        return step == 0 ? N_OFF : 0;
    case PP_ONE:
        return step == 0 ? t[voice & 3] : step == 14 ? N_OFF : 0;
    case PP_COMP:
        if (step == 0 || step == 6 || step == 12) return t[voice & 3] + (step == 12 ? 12 : 0) * (voice == 0);
        if (step == 3 || step == 9 || step == 15) return N_OFF;
        return 0;
    case PP_SKANK:
        if ((step & 3) == 2) return t[voice & 3];
        if ((step & 3) == 0 && step) return N_OFF;
        return 0;
    case PP_DREAM:
        if (voice) return 0;
        return k_dream[(bar - 64) % 12][step];
    case PP_ANSWER:
        /* the piano answers the long notes on the second half of the bar */
        if (step == 8) return t[voice & 3] + 12;
        if (step == 10 && voice == 0) return t[2] + 12;
        if (step == 12 && voice == 0) return t[3] + 12;
        if (step == 15) return N_OFF;
        return 0;
    case PP_END:
        if (step == 0 && (bar & 1) == 0) return t[voice & 3];
        if (step == 8 && (bar & 1) == 1 && voice == 0) return t[3];
        return 0;
    default:
        return 0;
    }
}

/* ==================================================================== pad */

int song_pad(uint32_t bar, int voice)
{
    if (bar >= SONG_BARS) return 0;
    const bar_t *b = song_bar(bar);
    if (!b->pad) return 0;
    const int8_t *t = song_chord_tones(b->chord);
    return t[voice & 3] - 12;
}

/* ================================================================== choir */

/* Theme B: long notes, one a bar. Night (over C G D Em) and dawn (over G D
 * Em C). The dream hums the third and fifth. */
static const uint8_t k_choir_b_night[8] = { 76, 79, 81, 83, 81, 83, 86, 83 };
static const uint8_t k_choir_b_dawn[8]  = { 79, 81, 83, 84, 86, 86, 83, 84 };
static const uint8_t k_choir_dream[8]   = { 64, 67, 64, 67, 62, 64, 62, 64 };

int song_choir(uint32_t bar, unsigned step)
{
    if (bar >= SONG_BARS) return 0;
    const bar_t *b = song_bar(bar);
    if (!b->choir) return step == 0 ? N_OFF : 0;
    if (step != 0) return (step == 15 && bar == 71) ? N_OFF : 0;
    const row_t *r = row_of(bar);
    const unsigned i = (bar - r->from) & 7;
    if (bar >= 64 && bar < 76) return k_choir_dream[(bar - 64) & 7];
    if (bar >= 96 && bar < 104) return k_choir_b_dawn[i];
    if (bar >= 104 && bar < 120) return k_choir_b_dawn[i] - 12;
    return k_choir_b_night[i] - 12;
}
