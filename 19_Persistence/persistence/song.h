/* The score, readable by both the synth and the picture.
 *
 * Music and visuals share one clock (persistence.h) and one event table. A
 * scene that wants to pulse on the kick asks song_drums(step); the synth asks
 * the same function to decide what to trigger. Nothing is aligned by hand.
 *
 * Everything here is a table lookup on an absolute 16th-step or bar index, so
 * it can be asked for any moment in any order -- the picture seeks, and the
 * synth is pull-model (synth.h). No accessor has state.
 *
 * The song itself is described at the top of song.c.
 */

#ifndef PV_SONG_H
#define PV_SONG_H

#include <stdint.h>

#define DR_KICK   1u
#define DR_SNARE  2u
#define DR_HAT    4u
#define DR_OHAT   8u
#define DR_CRASH  16u

uint8_t  song_drums(uint32_t step);        /* bitmask at a 16th step */
int      song_section(uint32_t bar);       /* 0 intro .. 9 power-off, see song.c */

/* Pitched rows. A row event is 0 = hold (nothing happens), SONG_OFF = note
 * off, anything else a MIDI note to trigger. Already transposed for the key
 * change, so callers never need to know there is one. */
#define SONG_OFF  1

int      song_bass(uint32_t step);         /* row event                       */
int      song_arp(uint32_t step);          /* row event; pan alternates L/R   */
int      song_lead(uint32_t step);         /* row event; theme A / riser      */
int      song_lead2(uint32_t step);        /* row event; theme B / counter    */

/* Per-bar arrangement. Levels are 0..255. */
int      song_transpose(uint32_t bar);     /* semitones added from bar 72     */
void     song_pad_chord(uint32_t bar, uint8_t out[4]); /* 0,0,0,0 = pad off  */
int      song_lead_level(uint32_t bar);
int      song_lead_cut(uint32_t bar);      /* 0..255 -> filter cutoff        */
int      song_lead2_level(uint32_t bar);
int      song_pad_level(uint32_t bar);
int      song_riser(uint32_t bar);         /* 0..255, noise sweep amount     */
int      song_energy(uint32_t bar);        /* 0..255, how busy it is         */

/* Which voices the arrangement has switched on this bar, for visuals/tools. */
#define SV_KICK   1u
#define SV_SNARE  2u
#define SV_HAT    4u
#define SV_BASS   8u
#define SV_ARP    16u
#define SV_LEAD   32u
#define SV_LEAD2  64u
#define SV_PAD    128u
#define SV_RISER  256u
uint32_t song_voices(uint32_t bar);

#endif
