#ifndef PV_VGA_H
#define PV_VGA_H

#include <stdint.h>

void     vga_init(void);                         /* launch core 1, wait for it   */
void     vga_publish(uint32_t f);                /* frame f's tables are ready   */
int      vga_wait_latched(uint32_t f, uint32_t timeout_us);
uint32_t vga_latched(void);
uint32_t vga_worst_cycles(void);                 /* worst line of the last frame */
uint32_t vga_mean_cycles(void);                  /* mean line of the last frame  */
/* Lines the beam was shown before core 1 had written them. This is the demo's
 * deadline referee: it must be 0 for the whole run. */
uint32_t vga_slips(void);
uint32_t vga_late_lines(void);
uint32_t vga_lod_events(void);
uint32_t vga_frames_done(void);

#endif
