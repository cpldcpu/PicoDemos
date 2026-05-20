/* Demo sequencer — owns the scene timeline and runs one frame per call.
 * Reference: dawn_final.s:159-271 and web_port sequencer.ts.
 */

#ifndef SEQUENCER_H
#define SEQUENCER_H

void sequencer_init(void);
void sequencer_step(void);          /* run one frame, update chunky buffer */

/* Immediately trigger a fade-out to the next scene. Safe to call mid-fade
 * (becomes a no-op). Used by main.c on button-A press for debug skipping. */
void sequencer_skip(void);

#endif
