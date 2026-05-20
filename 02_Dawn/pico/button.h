/* Pimoroni Pico VGA Demo Base — user button A.
 *
 * Default wired to GP22 (active-low, internal pull-up).
 * If your board uses a different pin, override at build time with
 *   cmake -DDAWN_BUTTON_A_PIN=21 ..
 *
 * Pin can't collide with the VGA bitfields:
 *   GP0..4 = R, GP6..10 = G, GP11..15 = B, GP16 = HSYNC, GP17 = VSYNC.
 * Safe candidates on a typical VGA Base layout: GP21, GP22, GP26.
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

#ifndef DAWN_BUTTON_A_PIN
#define DAWN_BUTTON_A_PIN 22
#endif

void button_init(void);

/* Returns true exactly once per press (rising edge of the "pressed"
 * state — i.e. when the button goes from released to pressed).
 * Call once per frame. */
bool button_a_just_pressed(void);

#endif
