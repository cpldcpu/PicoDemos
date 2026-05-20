/* Sound stub for Pico bring-up. Real audio (QOA over PWM-DMA) lands in
 * Milestone D. Until then every API in sound.h is a no-op so the engine's
 * choreography commands that issue music/sample play don't have to know
 * the difference. */
#include "sound.h"

bool sound_init(bool nosound)         { (void)nosound; return true; }
bool sound_deinit(void)               { return true; }
bool sound_mod_play(int mod)          { (void)mod; return true; }
bool sound_mod_stop(void)             { return true; }
bool sound_mp3_play(int mp3)          { (void)mp3; return true; }
bool sound_mp3_stop(void)             { return true; }
bool sound_sample_play(int sample_idx){ (void)sample_idx; return true; }
void sound_update(void)               { }
