/* The event list. See score.h for the clock and for why the picture leads.
 *
 * Positions, radii and amplitudes are exactly the schedule the visual arc was
 * tuned to; the beat column is the old second column doubled, and the last two
 * columns are new -- what the same event does to the audio.
 *
 * PITCH. D natural minor, and only its notes: 26/38/50/62 are D, 41/53/65 are
 * F, 45/57 are A, and so on. There is no modulation anywhere in the piece. That
 * is not laziness about harmony -- a field with memory has one long shape and
 * one attractor, and a key change would be the loudest possible claim that
 * something outside the system decided to move. The pitches instead trace a
 * single arc: low and rooted while the field is still sparse, climbing through
 * the growth, then descending back to the bottom D for the last strike.
 *
 * WEIGHT rises monotonically to the largest impact (beat 232, the radius-16 hit
 * that is also the visual peak) and then eases off. It is the only dynamic
 * marking in the piece that is not a slow ramp.
 */

#include "score.h"

const score_hit_t score_hits[] = {
    /*  beat     x    y    r  amp  note weight */
    {     10, 104,  88,   7, 255,   38, 190 },   /*  5 s  D2  the first impact */
    {     16, 216, 148,   8, 250,   45, 185 },   /*  8 s  A2  */
    {     22,  86, 166,   9, 250,   41, 195 },   /* 11 s  F2  */
    {     28, 234,  74,  10, 255,   50, 200 },   /* 14 s  D3  */
    {     36, 142, 192,  10, 250,   46, 200 },   /* 18 s  Bb2 */
    {     46,  58, 102,  11, 255,   45, 205 },   /* 23 s  A2  */
    {     58, 258, 130,  11, 250,   53, 210 },   /* 29 s  F3  */
    {     72, 160,  58,  12, 255,   38, 215 },   /* 36 s  D2  */
    {     88, 108, 134,  12, 250,   48, 215 },   /* 44 s  C3  */
    {    106, 210,  98,  13, 255,   43, 220 },   /* 53 s  G2  */
    {    124,  72, 178,  13, 250,   58, 225 },   /* 62 s  Bb3 */
    {    142, 248, 186,  14, 255,   45, 230 },   /* 71 s  A2  */
    {    160, 130,  94,  14, 250,   50, 235 },   /* 80 s  D3  */
    {    178, 176, 158,  15, 255,   53, 240 },   /* 89 s  F3  */
    {    196,  94,  60,  15, 255,   60, 245 },   /* 98 s  C4  */
    {    214, 224, 128,  15, 255,   57, 250 },   /*107 s  A3  */
    {    232, 160, 120,  16, 255,   38, 255 },   /*116 s  D2  the peak */
    {    250,  68, 140,  15, 255,   43, 245 },   /*125 s  G2  */
    {    268, 252,  96,  15, 255,   41, 235 },   /*134 s  F2  */
    {    286, 120, 200,  14, 255,   50, 220 },   /*143 s  D3  */
    {    304, 200,  44,  13, 255,   26, 255 },   /*152 s  D1  the last strike */

    /* 203 s. An audio-only event (r = 0), one second before the field
     * collapses. Nothing enters the picture -- injecting energy here would
     * fight the ending, which is the field failing to hold its state under
     * pressure rather than being pushed.
     *
     * Its job is to make the collapse a release instead of a stop. The bed is
     * already down to a quarter of its level and it cuts to nothing at 204.2 s
     * with the screen; this low D is struck just before that and rings on
     * underneath the wordmark for the remaining six seconds. The brief rejects
     * takes that end with a hard stop, and so does the demo -- the last thing
     * you hear is the room the piece was played in, decaying. */
    {    406,   0,   0,   0,   0,   26, 200 },   /*203 s  D1  the collapse */
};

const unsigned score_hit_count = sizeof score_hits / sizeof score_hits[0];
