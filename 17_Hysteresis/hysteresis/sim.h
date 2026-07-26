/* HYSTERESIS — the driver.
 *
 * There is no scene runner here in the usual sense. The demo is one system
 * being stepped, and this file decides what forcing it receives on each step.
 *
 * THE CLOCK IS THE FRAME INDEX, not milliseconds. That is not a convenience;
 * it is what makes the referee possible. A feedback system's state depends on
 * HOW MANY steps it has taken, so "the state at t = 90 s" is only well defined
 * if the step count to get there is fixed. Driving the sim from a wall clock
 * would make the picture depend on machine speed, which is the same thing as
 * saying it has no defined value at all.
 *
 * The consequence, stated plainly: if the device drops a frame, the demo
 * diverges from the master render and stays diverged. That is why the frame
 * budget is a correctness property (PLANNING.md section 3) and why the referee
 * fails a build that overruns it.
 */

#ifndef HYST_SIM_H
#define HYST_SIM_H

#include <stdint.h>
#include "field.h"
#include "rd.h"

/* Restart the system. `variant` selects the initial condition:
 *   0 — the demo's actual seed: a single lit cell.
 *   1 — the same, plus ONE extra cell lit elsewhere.
 * Variant 1 exists for referee test 2 (PLANNING.md section 7). If the two
 * variants converge, this demo has no memory and should not be made. */
void sim_reset(int variant);

/* Advance exactly one step and present. */
void sim_tick(void);

/* The two halves of sim_tick. The device must page-flip inside the vertical
 * blanking interval, so it computes, waits for vblank, then presents. */
void sim_step(void);
void sim_present(void);

/* Pin parameters for a sweep, bypassing the arc. Probe/tuning only. */
void sim_set_fixed(const field_params_t *p);

/* Debug: render the reaction-diffusion layer alone. */
void sim_set_rd_only(int on);

/* Override the Gray-Scott coefficients (sweeping). */
void sim_set_rd_params(const rd_params_t *p);

/* Pin the RD amplitude (negative = inhibitory). Sweeping only. */
void sim_set_rd_amp(int16_t a);

/* Pin the convolution kernel blend. Sweeping only. */
void sim_set_kern(int w);

/* Steps taken since sim_reset. */
uint32_t sim_frame(void);

/* Total length of the demo in frames. */
uint32_t sim_total_frames(void);

/* Field energy of the frame just presented — telemetry, and the equilibrium
 * check at the end of the arc. */
uint32_t sim_energy(void);

#endif
