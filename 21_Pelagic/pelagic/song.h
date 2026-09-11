/* PELAGIC -- the score, readable by the synth and by tools.
 *
 * Everything here is a table lookup on an absolute 16th-step or bar index
 * (pelagic.h fixes the clock: 125 BPM, a 16th = 2,880 samples, a bar =
 * 46,080, eighty bars). No accessor has state, so the synth can be pull-model
 * and a tool can ask for any moment in any order.
 *
 * The song itself is described at the top of song.c.
 */

#ifndef PELAGIC_SONG_H
#define PELAGIC_SONG_H

#include <stdint.h>

#define SONG_BARS          80u
#define SONG_STEPS_PER_BAR 16u
#define SONG_STEP_SAMPLES  2880u
#define SONG_BAR_SAMPLES   46080u

/* Drum bits at a 16th step. */
#define DR_KICK   1u      /* the soft thud                  */
#define DR_BRUSH  2u      /* brushed snare                  */
#define DR_SHAKER 4u      /* closed shaker tick             */
#define DR_OSHAKER 8u     /* open shaker                    */
#define DR_BOOM   16u     /* the 48 Hz swell                */
#define DR_WHOOSH 32u     /* the water whoosh, with a boom  */

uint8_t     song_drums(uint32_t step);
int         song_section(uint32_t bar);      /* 0..6, Phase's chapters */
const char *song_section_name(int section);

/* Pitched rows. A row event is 0 = hold, SONG_OFF = note off, anything else
 * a MIDI note to trigger. Already transposed for the key change. */
#define SONG_OFF  1

int  song_bass(uint32_t step);
int  song_pluck(uint32_t step);              /* the droplets; note-offs are ignored by the voice */
int  song_lead(uint32_t step);               /* the ray's theme (A) and the long notes (B) */
int  song_lead2(uint32_t step);              /* the hollow voice: the hint, the descent */

/* Per-bar arrangement. Levels are 0..255. */
int  song_transpose(uint32_t bar);           /* +2 from the ascent on */
void song_pad_chord(uint32_t bar, uint8_t out[4]);     /* 0,0,0,0 = pad off */
void song_glass_chord(uint32_t bar, uint8_t out[5]);   /* root an octave up + the pad voicing */
void song_choir_chord(uint32_t bar, uint8_t out[4]);   /* the pad voicing an octave up */
int  song_lead_level(uint32_t bar);
int  song_lead_cut(uint32_t bar);
int  song_lead_octave(uint32_t bar);         /* how much of the octave saw */
int  song_lead2_level(uint32_t bar);
int  song_pad_level(uint32_t bar);
int  song_pad_cut(uint32_t bar);
int  song_drone_level(uint32_t bar);
int  song_drone_note(uint32_t bar);          /* MIDI, transposed; 0 = keep the last */
int  song_shimmer(uint32_t bar);
int  song_glass_level(uint32_t bar);
int  song_choir_level(uint32_t bar);
int  song_pluck_level(uint32_t bar);
int  song_pluck_decay(uint32_t bar);         /* 0..255 -> short .. bell */
int  song_energy(uint32_t bar);

/* Which voices the arrangement has switched on this bar, for tools. */
#define SV_KICK    1u
#define SV_BRUSH   2u
#define SV_SHAKER  4u
#define SV_BASS    8u
#define SV_PLUCK   16u
#define SV_LEAD    32u
#define SV_LEAD2   64u
#define SV_PAD     128u
#define SV_SHIMMER 256u
#define SV_DRONE   512u
#define SV_GLASS   1024u
#define SV_CHOIR   2048u
uint32_t song_voices(uint32_t bar);

#endif
