/* VGA backend for 20_TheDemo.
 *
 * Three modes, switched at scene boundaries via vga_set_mode(). Switching
 * stops scanvideo, reconfigures the timing+callback, and restarts. A
 * brief glitch (~1 frame) is unavoidable; effects hide it by ending and
 * starting on a black frame.
 *
 *  - MODE_320: 320x240 60 Hz, 8 bpp chunky palettized. Effect writes into
 *              vga_320_back_buffer() (320*240 bytes). The scanline callback
 *              palette-lookups each byte → RGB555 and emits a composable
 *              scanline. vga_320_present() flips the double buffer.
 *
 *  - MODE_160: 160x120 pixel-doubled both axes into a 320x240 scanout.
 *              Effect writes RGB565 directly into vga_160_back_buffer().
 *              No palette.
 *
 *  - MODE_SPLIT_160_OVER_320: scanout switches source mid-frame at
 *              vga_set_split_row(). Top rows from fb160, bottom rows
 *              from fb320 — copper-style raster split.
 */

#ifndef THEDEMO_VGA_H
#define THEDEMO_VGA_H

#include <stdint.h>
#include "scene.h"

/* Bring up core 1 + scanvideo in MODE_320 with a 256-entry default palette.
 * Idempotent. Call once from main() before scene_runner_tick. */
void vga_init(void);

/* Host backend only: returns non-zero when the user has requested exit
 * (window close, ESC). Pico backend always returns 0. Main loop checks
 * this each iteration so the same scene-runner logic works for both. */
int vga_should_quit(void);

/* Returns and clears the "scene jump" request set since last call.
 *
 *    0   no jump
 *   +1   advance to next scene boundary
 *   -1   rewind to previous scene boundary (= current scene's start)
 *   +2   restart demo from t = 0
 *
 * Host:  SPACE = +1, LEFT = -1, R = +2.
 * Pico:  GP18 = +1, GP19 = -1, GP20 = +2 (active-low, internal pull-ups). */
int vga_consume_skip_request(void);

/* Switch screenmode. No-op if already in `mode`. Blocks until the new
 * scanvideo callback is live. */
void vga_set_mode(screen_mode_t mode);

screen_mode_t vga_current_mode(void);

/* MODE_320 -------------------------------------------------------------- */

#define VGA_320_W 320
#define VGA_320_H 240

/* Pointer to the back buffer for the current frame. Effect writes one
 * byte per pixel, row-major, indexing into the palette. */
uint8_t *vga_320_back_buffer(void);

/* Set one palette entry. r/g/b are 0..255 (downsampled to RGB555 by the
 * scanline callback). Safe to call at any time, takes effect on the next
 * scanline. */
void vga_320_palette_set(int idx, uint8_t r, uint8_t g, uint8_t b);

/* Publish the back buffer to the scanline callback (page-flip). Returns
 * after the swap is visible — i.e. it's safe to start drawing into the
 * new back buffer immediately on return. */
void vga_320_present(void);

/* MODE_160 -------------------------------------------------------------- */

#define VGA_160_W 160
#define VGA_160_H 120

uint16_t *vga_160_back_buffer(void);  /* RGB565 */
void      vga_160_present(void);

/* MODE_HIRES — 320x240 RGB565 truecolor, full native resolution -----------
 *
 * Same scanvideo timing as the others; the scanline callback passes each
 * RGB565 pixel straight through (no palette, no doubling). The back buffer
 * is 320*240 uint16. Shares storage with the 8bpp / 160 buffers (exclusive
 * use), so it costs no extra SRAM beyond the largest buffer pair. */
#define VGA_HIRES_W 320
#define VGA_HIRES_H 240

uint16_t *vga_hires_back_buffer(void);  /* RGB565, 320*240 */
void      vga_hires_present(void);

/* MODE_RACE — beam-raced full VGA (640x480) -------------------------------
 *
 * The whole demo scans out at 640x480@60; framebuffer (MODE_HIRES) scenes are
 * 2x pixel/line-doubled, MODE_RACE scenes generate each 640-wide line live. A
 * race scene registers its per-scanline generator `scan(dst, y)` and an
 * optional one-time `setup()` that runs on the scanout core (to configure its
 * interpolator). Beam-raced textures are copied into vga_race_sram() (which
 * aliases the framebuffer arena — never live at the same time). */
void      vga_set_race_fn(void (*scan)(uint16_t *dst, int y), void (*setup)(void));
uint8_t  *vga_race_sram(void);
unsigned  vga_race_sram_size(void);

/* Present a MODE_RACE frame. Device: no-op (core 1 beam-races continuously).
 * Host: runs the registered generator over all 640x480 lines into the window. */
void      vga_race_present(void);

#define VGA_RACE_W 640
#define VGA_RACE_H 480

/* MODE_SPLIT_160_OVER_320 -------------------------------------------------
 *
 * Set the display row at which scanout switches from the fb160 (RGB565,
 * upper) source to the fb320 (palette, lower) source. 0..240; 0 means
 * "all fb320", 240 means "all fb160". Takes effect on the next scanline. */
void      vga_set_split_row(int display_row);

/* Flip both back buffers atomically. Use when the scene is in
 * MODE_SPLIT_160_OVER_320 and has drawn into BOTH fb160 and fb320. */
void      vga_split_present(void);

#endif
