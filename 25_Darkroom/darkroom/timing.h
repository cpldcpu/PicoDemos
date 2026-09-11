#pragma once
#include <stdint.h>

/* Original Darkroom playback is ProTracker VBlank mode. The 2.31 fixed-point
 * duration comes from pt2-clone's PAL VBlank entry (~49.920409 Hz). */
#define DARKROOM_AUDIO_RATE 24000u
#define DARKROOM_PAL_TICK_DURATION_Q31 0x029067A6u
#define DARKROOM_TICK_SCALED ((uint64_t)DARKROOM_PAL_TICK_DURATION_Q31 * DARKROOM_AUDIO_RATE)

/* The song sets speed 8 on every even row and 5 on every odd row. */
#define DARKROOM_ORDER_TICKS (32u * (8u + 5u))
#define DARKROOM_SAMPLE_AT_TICK(tick_) ((uint32_t)(((uint64_t)(tick_) * DARKROOM_TICK_SCALED) >> 31))
#define DARKROOM_ORDER_SAMPLE(order_) DARKROOM_SAMPLE_AT_TICK((uint64_t)(order_) * DARKROOM_ORDER_TICKS)

#define DARKROOM_ORDER_4_SAMPLE DARKROOM_ORDER_SAMPLE(4u) /*  799993: 33.333042 s */
#define DARKROOM_ORDER_6_SAMPLE DARKROOM_ORDER_SAMPLE(6u) /* 1199990: 49.999583 s */
#define DARKROOM_ORDER_8_SAMPLE DARKROOM_ORDER_SAMPLE(8u) /* 1599986: 66.666083 s */

/* F00 is on order 8, row 63, after 32 speed-8 and 31 speed-5 rows. */
#define DARKROOM_F00_TICK (8u * DARKROOM_ORDER_TICKS + 32u * 8u + 31u * 5u)
#define DARKROOM_F00_SAMPLE DARKROOM_SAMPLE_AT_TICK(DARKROOM_F00_TICK) /* 1797581: 74.899208 s */

/* Tick zero starts at sample zero. A boundary belongs to the new tick. */
static inline uint32_t darkroom_tick_at_sample(uint32_t sample)
{
    return (uint32_t)(((((uint64_t)sample + 1u) << 31) - 1u) / DARKROOM_TICK_SCALED);
}

