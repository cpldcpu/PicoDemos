/* HELION -- the score, readable by the synth and by tools.
 *
 * Everything here is a table lookup on an absolute 16th-step or bar index
 * (helion.h fixes the clock: 120 BPM, a 16th = 3,000 samples, a bar =
 * 48,000, eighty bars). No accessor has state, so the synth can be
 * pull-model and a tool can ask for any moment in any order.
 *
 * The song itself is described at the top of song.c. The same interface is
 * implemented a second time by tools/audition_song.c, the instrument
 * audition Phase asked for, so the synth cannot tell the two apart.
 */

#ifndef HELION_SONG_H
#define HELION_SONG_H

#include <stdint.h>

#define SONG_BARS          80u
#define SONG_STEPS_PER_BAR 16u
#define SONG_STEP_SAMPLES  3000u
#define SONG_BAR_SAMPLES   48000u

/* Drum bits at a 16th step. */
#define DR_DRUM     1u    /* the low frame drum                        */
#define DR_GHOST    2u    /* the same drum, a soft touch               */
#define DR_RIM      4u    /* the dry rim knock                         */
#define DR_SCRAPE   8u    /* brushed metal, short                      */
#define DR_SCRAPEL  16u   /* brushed metal, long                       */
#define DR_GONG     32u   /* the tam-tam, struck                       */
#define DR_GONGSOFT 64u   /* the tam-tam, touched                      */

uint8_t     song_drums(uint32_t step);
int         song_section(uint32_t bar);      /* 0..6, Phase's chapters */
const char *song_section_name(int section);

/* Pitched rows. A row event is 0 = hold, SONG_OFF = note off, anything else
 * a MIDI note to trigger. The bass may carry SONG_SLIDE: glide into the
 * note from wherever the bass is instead of jumping. */
#define SONG_OFF   1
#define SONG_SLIDE 128

int  song_bass(uint32_t step);               /* note | SONG_SLIDE */
int  song_bronze(uint32_t step);             /* the struck bar; note-offs are ignored */
int  song_lead(uint32_t step);               /* the electric reed: the theme */
int  song_harm(uint32_t step);               /* the solo bowed harmonic */

/* The bowed metal plays one chord a bar, gated by the step table: 0 = keep,
 * SONG_OFF = release, SONG_BOW_ON = a fresh bow stroke. A chord change
 * while the bow is held changes the pitches without a new stroke. */
#define SONG_BOW_ON 2
void song_bow_chord(uint32_t bar, uint8_t out[4]);   /* 0,0,0,0 = nothing this bar */
int  song_bow_gate(uint32_t step);

/* Per-bar arrangement. Levels are 0..255. */
int  song_lead_level(uint32_t bar);
int  song_lead_push(uint32_t bar);           /* the reed's brightness and edge */
int  song_bow_level(uint32_t bar);
int  song_bow_open(uint32_t bar);            /* how far the bowed metal's filter opens */
int  song_bronze_level(uint32_t bar);
int  song_bronze_decay(uint32_t bar);        /* 0..255 -> muted .. ringing */
int  song_bass_level(uint32_t bar);
int  song_bass_bite(uint32_t bar);           /* the resonant bite on each note */
int  song_harm_level(uint32_t bar);
int  song_air_level(uint32_t bar);           /* the solar wind, band-passed air */
int  song_space(uint32_t bar);               /* the delay and the hall, together */
int  song_energy(uint32_t bar);

/* Which voices the arrangement has switched on this bar, for tools. */
#define SV_DRUM    1u
#define SV_RIM     2u
#define SV_SCRAPE  4u
#define SV_BASS    8u
#define SV_BRONZE  16u
#define SV_REED    32u
#define SV_BOW     64u
#define SV_HARM    128u
#define SV_AIR     256u
#define SV_GONG    512u
uint32_t song_voices(uint32_t bar);

#endif
