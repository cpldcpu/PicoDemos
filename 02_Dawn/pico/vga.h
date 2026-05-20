/* VGA scanvideo backend — Pimoroni Pico VGA Demo Base.
 *
 * Mode: vga_mode_320x240_60 (320x240 @ 60 Hz, RPi Foundation pinout).
 *   GP0..GP4   = Red    (LSB..MSB)
 *   GP6..GP10  = Green  (GP5 skipped)
 *   GP11..GP15 = Blue
 *   GP16/17    = HSYNC/VSYNC
 *
 * Core 1 owns scanout: it serves each scanline by reading the 160-wide
 * chunky buffer, looking up palette_rgb565[], and writing two consecutive
 * RGB-565 words per chunky pixel (2× horizontal scale).
 *
 * Vertical mapping: VGA scanlines 0..239 → chunky rows
 *   (VPORT_TOP + scanline/2)
 * VPORT_TOP=4 means we show rows 4..123 of the 128-row chunky buffer
 * (4-row trim top and bottom — minimal loss given the demo's centered
 * content).
 */

#ifndef VGA_H
#define VGA_H

void vga_start(void);   /* boots scanvideo on core 1, returns immediately */

#endif
