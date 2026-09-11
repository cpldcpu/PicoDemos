/* interp_emu.h — software emulation of the RP2350 SIO interpolator.
 *
 * QUICKSILVER's hardware hero is the RP2350 interpolator. On the device we use
 * the real peripheral (hardware/interp.h). The SDL host has no such hardware,
 * so this file reproduces the interpolator datapath BIT-EXACTLY so the same
 * effect code previews identically on PC and on the RP2350.
 *
 * The SDK's interp_peek_lane_result() reads interp->peek[lane], a side-effecting
 * memory-mapped register that recomputes the datapath live; pop[] additionally
 * writes both accumulators. A plain C struct can't model a load-with-side-
 * effects, so on host we do NOT include the SDK header — we define our own
 * interp_hw_t and our own interp_* functions that COMPUTE the result inside
 * peek/pop. interp_compat.h selects this file only when HOST_BUILD is defined;
 * interp_emu.c is never compiled into the firmware.
 *
 * Datapath reference (RP2350 datasheet / SDK regs):
 *  - SHIFT[4:0] is a RIGHT-ROTATE of the (cross-muxed) accumulator, applied
 *    before masking.
 *  - MASK keeps bits [MASK_LSB..MASK_MSB] in place; others zeroed.
 *  - SIGNED sign-extends the masked value from bit MASK_MSB.
 *  - lane result = base[L] + (ADD_RAW ? cross_accum : shift+mask); cross_input
 *    mux is BEFORE the add_raw bypass. FORCE_MSB OR's bits into result[29:28].
 *  - FULL result = base[2] + lane0_shiftmask + lane1_shiftmask (ignores
 *    ADD_RAW and FORCE_MSB; uses each lane's SIGNED).
 *  - BLEND (interp0 only): lane1 = base0 + (base1-base0)*alpha/256, alpha = 8
 *    LSBs of lane1 shift+mask; lane0 = alpha alone; FULL omits the lane1 term.
 *  - CLAMP (interp1 lane0 only): clamp(base0, shift+mask(accum0), base1).
 *  - POP returns the value, then writes both accumulators with the internal
 *    lane results (CROSS_RESULT routes the other lane's result into a lane).
 */

#ifndef QS_INTERP_EMU_H
#define QS_INTERP_EMU_H

#include <stdint.h>
#include <stdbool.h>

#ifndef QS_UINT_DEFINED
#define QS_UINT_DEFINED
typedef unsigned int uint;   /* the SDK interp API takes `uint` arguments */
#endif

/* ---- ctrl bitfields (mirror SIO_INTERP*_CTRL_LANE0_* from the SDK) ------- */
#define QS_CTRL_SHIFT_LSB         0
#define QS_CTRL_SHIFT_BITS        0x0000001fu
#define QS_CTRL_MASK_LSB_LSB      5
#define QS_CTRL_MASK_LSB_BITS     0x000003e0u
#define QS_CTRL_MASK_MSB_LSB      10
#define QS_CTRL_MASK_MSB_BITS     0x00007c00u
#define QS_CTRL_SIGNED_BITS       0x00008000u
#define QS_CTRL_CROSS_INPUT_BITS  0x00010000u
#define QS_CTRL_CROSS_RESULT_BITS 0x00020000u
#define QS_CTRL_ADD_RAW_BITS      0x00040000u
#define QS_CTRL_FORCE_MSB_LSB     19
#define QS_CTRL_FORCE_MSB_BITS    0x00180000u
#define QS_CTRL_BLEND_BITS        0x00200000u  /* interp0 lane0 only */
#define QS_CTRL_CLAMP_BITS        0x00400000u  /* interp1 lane0 only */

/* ---- config object (identical shape to the SDK's interp_config) ---------- */
typedef struct { uint32_t ctrl; } interp_config;

/* ---- emulated hardware instance ------------------------------------------ */
typedef struct {
    uint32_t accum[2];
    uint32_t base[3];
    uint32_t ctrl[2];
    int      is_interp1;   /* selects CLAMP legality + matches SDK panics */
} interp_hw_t;

extern interp_hw_t qs_interp0;
extern interp_hw_t qs_interp1;
#define interp0 (&qs_interp0)
#define interp1 (&qs_interp1)

/* ---- config builders (pure ctrl bit-math; same semantics as the SDK) ----- */
static inline void interp_config_set_shift(interp_config *c, uint shift) {
    c->ctrl = (c->ctrl & ~QS_CTRL_SHIFT_BITS) |
              ((shift << QS_CTRL_SHIFT_LSB) & QS_CTRL_SHIFT_BITS);
}
static inline void interp_config_set_mask(interp_config *c, uint mask_lsb, uint mask_msb) {
    c->ctrl = (c->ctrl & ~(QS_CTRL_MASK_LSB_BITS | QS_CTRL_MASK_MSB_BITS)) |
              ((mask_lsb << QS_CTRL_MASK_LSB_LSB) & QS_CTRL_MASK_LSB_BITS) |
              ((mask_msb << QS_CTRL_MASK_MSB_LSB) & QS_CTRL_MASK_MSB_BITS);
}
static inline void interp_config_set_cross_input(interp_config *c, bool v) {
    c->ctrl = (c->ctrl & ~QS_CTRL_CROSS_INPUT_BITS) | (v ? QS_CTRL_CROSS_INPUT_BITS : 0);
}
static inline void interp_config_set_cross_result(interp_config *c, bool v) {
    c->ctrl = (c->ctrl & ~QS_CTRL_CROSS_RESULT_BITS) | (v ? QS_CTRL_CROSS_RESULT_BITS : 0);
}
static inline void interp_config_set_signed(interp_config *c, bool v) {
    c->ctrl = (c->ctrl & ~QS_CTRL_SIGNED_BITS) | (v ? QS_CTRL_SIGNED_BITS : 0);
}
static inline void interp_config_set_add_raw(interp_config *c, bool v) {
    c->ctrl = (c->ctrl & ~QS_CTRL_ADD_RAW_BITS) | (v ? QS_CTRL_ADD_RAW_BITS : 0);
}
static inline void interp_config_set_blend(interp_config *c, bool v) {
    c->ctrl = (c->ctrl & ~QS_CTRL_BLEND_BITS) | (v ? QS_CTRL_BLEND_BITS : 0);
}
static inline void interp_config_set_clamp(interp_config *c, bool v) {
    c->ctrl = (c->ctrl & ~QS_CTRL_CLAMP_BITS) | (v ? QS_CTRL_CLAMP_BITS : 0);
}
static inline void interp_config_set_force_bits(interp_config *c, uint bits) {
    c->ctrl = (c->ctrl & ~QS_CTRL_FORCE_MSB_BITS) |
              ((bits << QS_CTRL_FORCE_MSB_LSB) & QS_CTRL_FORCE_MSB_BITS);
}
static inline interp_config interp_default_config(void) {
    interp_config c = { 0 };
    interp_config_set_mask(&c, 0, 31);
    return c;
}

/* ---- register-style accessors (implemented in interp_emu.c) -------------- */
void     interp_set_config(interp_hw_t *interp, uint lane, interp_config *config);
void     interp_set_base(interp_hw_t *interp, uint lane, uint32_t val);
uint32_t interp_get_base(interp_hw_t *interp, uint lane);
void     interp_set_base_both(interp_hw_t *interp, uint32_t val);
void     interp_set_accumulator(interp_hw_t *interp, uint lane, uint32_t val);
uint32_t interp_get_accumulator(interp_hw_t *interp, uint lane);
void     interp_add_accumulator(interp_hw_t *interp, uint lane, uint32_t val);
uint32_t interp_get_raw(interp_hw_t *interp, uint lane);
uint32_t interp_peek_lane_result(interp_hw_t *interp, uint lane);
uint32_t interp_pop_lane_result(interp_hw_t *interp, uint lane);
uint32_t interp_peek_full_result(interp_hw_t *interp);
uint32_t interp_pop_full_result(interp_hw_t *interp);

/* No-ops on host (lane claiming is a device bookkeeping concern). */
static inline void interp_claim_lane(interp_hw_t *i, uint l)        { (void)i; (void)l; }
static inline void interp_claim_lane_mask(interp_hw_t *i, uint m)   { (void)i; (void)m; }
static inline void interp_unclaim_lane(interp_hw_t *i, uint l)      { (void)i; (void)l; }
static inline void interp_unclaim_lane_mask(interp_hw_t *i, uint m) { (void)i; (void)m; }

#endif /* QS_INTERP_EMU_H */
