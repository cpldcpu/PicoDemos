/* The score: one event list, read by both the field and the synth.
 *
 * PLANNING.md section 6 argued that the sequencer and the forcing schedule
 * should be the same object, and this is that object. The alternative -- music
 * rendered separately, then envelopes extracted and lined up with the visual
 * events -- has an alignment step, and an alignment step can drift. Here there
 * is nothing to align: `sim.c` reads this table to decide where to inject
 * energy, `synth.c` reads the same table to decide when to strike a resonator,
 * and both derive their position from the same integer clock.
 *
 * THE CLOCK. Three rates, deliberately chosen so that all conversions are
 * exact integers with no accumulating remainder:
 *
 *      120 BPM  ->  1 beat = 0.5 s = 30 frames = 11025 samples
 *
 * Every event time in this file is a beat index, so a hit lands on frame
 * beat*30 and on sample beat*11025, and those two are the same instant
 * forever rather than for the first few minutes.
 *
 * WHICH SIDE LEADS. The picture does. These times are the visual impact
 * schedule, tuned against a percolation threshold in the field that is not
 * movable by taste (PLANNING.md section 8) -- so the music is written around
 * them rather than the other way round. It happens that they all fall on beats,
 * because they were all on whole seconds already.
 *
 * Events may address one side only:
 *   r == 0       audio-only. Nothing enters the field.
 *   weight == 0  field-only. Nothing is struck.
 * Two uses of one table, rather than two tables that have to agree.
 */

#ifndef HYST_SCORE_H
#define HYST_SCORE_H

#include <stdint.h>

#define SCORE_BPM               120
#define SCORE_FPS               60
#define SCORE_RATE              22050
#define SCORE_FRAMES_PER_BEAT   30
#define SCORE_SAMPLES_PER_BEAT  11025
#define SCORE_BEATS             420          /* 210 s */

typedef struct {
    uint16_t beat;      /* when, in beats from the seed */
    int16_t  x, y;      /* field: where */
    uint8_t  r;         /* field: blob radius, 0 = audio-only event */
    uint8_t  amp;       /* field: injected value */
    uint8_t  note;      /* audio: MIDI pitch of the struck resonator */
    uint8_t  weight;    /* audio: how hard, 0 = field-only event */
} score_hit_t;

extern const score_hit_t score_hits[];
extern const unsigned    score_hit_count;

#endif
