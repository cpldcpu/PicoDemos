#ifndef TESSERA_SONG_H
#define TESSERA_SONG_H
#include "tessera.h"
enum { BASS, BRASS, CHORD1, CHORD2, CHORD3, MALLET, VOICES };
typedef struct { uint8_t note, steps, velocity; } Note;
typedef struct { Note v[VOICES]; uint8_t kick, snare, hat, rim; } ScoreStep;
void song_init(void);
int song_section(unsigned bar);
void song_step(unsigned absolute_step, ScoreStep *out);
/* Last lead onset, shared by score and tile lighting. Pure, no cross-core cache. */
unsigned song_accent(uint32_t sample);
#endif
