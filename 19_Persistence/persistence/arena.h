/* The arena: the SRAM that not having a framebuffer buys.
 *
 * Every scene carves what it needs out of one static block at fixed offsets.
 * Two scenes may only be on screen together (a beam wipe) if their regions do
 * not overlap; the table below is the whole allocation, so that question is
 * answered here rather than discovered on the monitor.
 *
 *   region      offset    size      used by
 *   TEXTURE     0         131,072   256x256 RGB565: the floor, and the tunnel wall
 *   SPANS       131,072   122,880   solid-3D boundary lists, 2 x 480 x 32 x 4 B
 *   MESH        253,952    40,960   procedural meshes for the span renderer
 *   SMALL       294,912    16,384   kefrens line, plasma tables
 *   total       311,296
 *
 * The tunnel and the plane both want TEXTURE, so they are joined by a blind
 * rather than a wipe -- a blind blacks the outgoing scene out before the
 * incoming one is entered, so the two are never resident at once (timeline.c).
 *
 * ------------------------------------------------------- what is NOT here --
 *
 * There was a 76,800-byte TUNNEL region for a per-pixel (angle, depth) lookup
 * table, planned before the tunnel was written. The scene ended up computing
 * those coordinates exactly every sixteen pixels and letting the interpolator
 * walk between them, so the table never existed -- but the reservation did,
 * for another two days, until the firmware panicked with "Out of memory" on
 * the first build that included the finished synthesiser.
 *
 * That is worth recording because of HOW it failed. The link succeeded: the
 * linker only checks static sections, and 499 KB of static data fits in 520 KB
 * of SRAM. What did not fit was pico_scanvideo's runtime malloc of its
 * scanline buffers, 21 KB that no build-time tool accounts for. 15_Quicksilver
 * hit the same wall and wrote it down; this is what it looks like when you
 * inherit the note and still walk into it, because the thing eating the heap
 * was not a new allocation but an old one nobody had deleted.
 *
 * Budget (RP2350, 520 KB): arena 304 KB + per-frame scene tables ~78 KB
 * + synth ~18 KB + audio rings 16 KB + code/other statics ~37 KB
 * = ~453 KB of static data, leaving ~79 KB of heap for scanvideo's 21 KB.
 *
 * The span list was briefly 40 boundaries a row instead of 32. That is 30 KB
 * more, it linked, and the board panicked with "Out of memory" again -- the
 * same failure as above, from the other direction, thirty minutes after
 * writing the paragraph about it. 79 KB of heap boots; 48 KB does not. There
 * is no build-time check that would have caught either, which is why the
 * number is written down here rather than left to be rediscovered.
 */

#ifndef PV_ARENA_H
#define PV_ARENA_H

#include <stdint.h>

#define ARENA_TEXTURE_OFF  0u
#define ARENA_TEXTURE_SIZE (256u * 256u * 2u)
#define ARENA_SPANS_OFF    (ARENA_TEXTURE_OFF + ARENA_TEXTURE_SIZE)
#define ARENA_SPANS_SIZE   (2u * 480u * 32u * 4u)
#define ARENA_MESH_OFF     (ARENA_SPANS_OFF + ARENA_SPANS_SIZE)
#define ARENA_MESH_SIZE    40960u
#define ARENA_SMALL_OFF    (ARENA_MESH_OFF + ARENA_MESH_SIZE)
#define ARENA_SMALL_SIZE   16384u
#define ARENA_BYTES        (ARENA_SMALL_OFF + ARENA_SMALL_SIZE)

extern uint8_t g_arena[ARENA_BYTES];

#define ARENA(off) ((void *)(g_arena + (off)))

/* SMALL sub-allocation (offsets within ARENA_SMALL). */
#define SMALL_KEFRENS_LINE  0u        /* 640 x 2 = 1,280 B  (+ spare)          */
#define SMALL_PLASMA_XT     2048u     /* 2 x 640 B x-tables                    */
#define SMALL_END           4096u

#endif
