#include <string.h>

#include "chunky.h"

#include "pico/scanvideo.h"

/* Two static backing buffers, ping-pong. 40 KB total — fits comfortably
 * in the 264 KB SRAM budget and eliminates the mid-frame tearing that
 * single-buffer rendering produces (most visible during the texture-
 * mapped torus scenes). */
static uint8_t chunky_buf_a[SCREEN_BYTES] __attribute__((aligned(4)));
static uint8_t chunky_buf_b[SCREEN_BYTES] __attribute__((aligned(4)));

uint8_t *chunky = chunky_buf_a;                /* engine writes here */
uint8_t * volatile chunky_scanout = chunky_buf_b;  /* core-1 reads here */

void chunky_present(void)
{
    /* Wait for vertical blank before publishing the new frame. The
     * scanline reader on core 1 picks up `chunky_scanout` each scanline,
     * so swapping mid-frame would tear; swapping in vblank confines any
     * residual mismatch to the few pre-filled scanline-pool buffers at
     * the top of the picture (typically <8 lines). */
    scanvideo_wait_for_vblank();

    uint8_t *new_scanout = chunky;             /* finished frame */
    uint8_t *new_back = (uint8_t *)chunky_scanout;
    /* SEQ_CST so the scanline reader on the other core sees the swap
     * atomically — the volatile alone isn't enough on multicore. */
    __atomic_store_n(&chunky_scanout, new_scanout, __ATOMIC_SEQ_CST);
    chunky = new_back;
}

void chunky_clear(uint8_t idx)
{
    if (idx == 0) {
        memset(chunky, 0, SCREEN_BYTES);
    } else {
        memset(chunky, idx & 0x3F, SCREEN_BYTES);
    }
}

/* Compute the blur-pair output for a 16-bit word (two 8-bit byte halves).
 *
 * Original: dawn_final.s:60-76 builds a 16 KB LUT of all input pairs. We
 * inline the same math — the table was an 020-era optimization that doesn't
 * pay off on RP2040, and 16 KB is significant SRAM. */
static inline uint16_t blur_pair(uint16_t sum)
{
    uint16_t v = sum & 0x3F3F;
    v = (uint16_t)(v - 0x0202);
    if (v & 0x8000) v &= 0x00FF;
    if (v & 0x0080) v &= 0xFF00;
    return v;
}

void chunky_blur_vertical(void)
{
    /* Word width = SCREEN_W / 2 (two pixels per uint16_t). The original
     * relies on Amiga big-endian byte order so the per-byte averaging works
     * unchanged; on little-endian we still get the correct *pair*-wise
     * average since both lanes are added in the same word and clamped
     * independently. */
    const int words = SCREEN_W / 2;
    uint16_t *w = (uint16_t *)chunky;
    for (int row = 0; row < SCREEN_H - 1; row++) {
        uint16_t *top = w + row * words;
        uint16_t *bot = top + words;
        for (int col = 0; col < words; col++) {
            uint16_t sum = (uint16_t)((top[col] + bot[col]) >> 1);
            top[col] = blur_pair(sum);
        }
    }
    /* Clear the bottom row (it's never overwritten otherwise). */
    memset(chunky + (SCREEN_H - 1) * SCREEN_W, 0, SCREEN_W);
}
