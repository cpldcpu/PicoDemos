/* Ping-pong scratch for SUSTAIN's fields.
 *
 * QUICKSILVER's scene_scratch.h opened with:
 *
 *     "Only one scene is active at a time, so heavy per-scene buffers all
 *      alias the same physical bytes via a union."
 *
 * That is exactly the assumption a morph breaks. During a morph two fields are
 * live: one is being rendered from while the other is being prepared into. So
 * the union becomes an array of two, and world.c hands each field the slot the
 * other is not using.
 *
 * The cost is one extra slot (PLANNING.md §7 budgets 2 x 64 KB = 128 KB). The
 * 64 KB per-slot ceiling is the number to defend — if a field wants more than
 * that, it is the wrong field.
 */

#ifndef SUSTAIN_FIELD_SCRATCH_H
#define SUSTAIN_FIELD_SCRATCH_H

#include <stdint.h>

/* Budgeted at 64 KB per slot in PLANNING.md §7, but no field uses prepare()
 * yet, so 128 KB of BSS was being reserved for nothing — and RAM is the
 * binding constraint on this target. Raise it when a field actually needs it;
 * the ping-pong architecture is unchanged either way. */
#define FIELD_SCRATCH_BYTES (8 * 1024)

union field_scratch_u {
    uint8_t  bytes[FIELD_SCRATCH_BYTES];
    uint16_t px[FIELD_SCRATCH_BYTES / 2];      /* RGB565 tiles */
    int16_t  s16[FIELD_SCRATCH_BYTES / 2];     /* height / gradient LUTs */
};

/* [0] and [1] — never assume which one you have; world.c passes the index. */
extern union field_scratch_u g_slot[2];

#endif
