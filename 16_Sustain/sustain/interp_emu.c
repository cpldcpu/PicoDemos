/* interp_emu.c — software datapath for the host interpolator emulator.
 * Compiled ONLY by host/Makefile (never by CMake). See interp_emu.h. */

#include "interp_emu.h"

interp_hw_t qs_interp0 = { .is_interp1 = 0 };
interp_hw_t qs_interp1 = { .is_interp1 = 1 };

/* right-rotate (SHIFT semantics on RP2350) */
static inline uint32_t rotr32(uint32_t v, uint s) {
    s &= 31u;
    return s ? ((v >> s) | (v << (32 - s))) : v;
}

/* the cross-muxed input accumulator for a lane */
static inline uint32_t cross_in(const interp_hw_t *I, int L) {
    return (I->ctrl[L] & QS_CTRL_CROSS_INPUT_BITS) ? I->accum[1 - L] : I->accum[L];
}

/* shift(rotate)+mask of an accumulator value per a lane's ctrl, sign-extended
 * from MASK_MSB if SIGNED. This is the value used for FULL composition and as
 * the additive term for a normal lane result. */
static uint32_t shiftmask(uint32_t ctrl, uint32_t acc) {
    uint sh   = (ctrl & QS_CTRL_SHIFT_BITS) >> QS_CTRL_SHIFT_LSB;
    uint mlsb = (ctrl & QS_CTRL_MASK_LSB_BITS) >> QS_CTRL_MASK_LSB_LSB;
    uint mmsb = (ctrl & QS_CTRL_MASK_MSB_BITS) >> QS_CTRL_MASK_MSB_LSB;
    uint32_t r = rotr32(acc, sh);
    uint32_t width = (mmsb >= mlsb) ? (mmsb - mlsb + 1) : 0;
    uint32_t mask = (width >= 32) ? 0xFFFFFFFFu : (((1u << width) - 1u) << mlsb);
    uint32_t v = r & mask;
    if (ctrl & QS_CTRL_SIGNED_BITS) {
        if (mmsb < 31 && (v & (1u << mmsb)))
            v |= (0xFFFFFFFFu << (mmsb + 1));   /* sign-extend from MASK_MSB */
    }
    return v;
}

/* lane result WITHOUT the FORCE_MSB OR (the value committed to accumulators on
 * POP — FORCE only affects what the processor reads, not the datapath). */
static uint32_t lane_internal(const interp_hw_t *I, int L) {
    uint32_t ctrl0 = I->ctrl[0];

    /* BLEND mode (interp0, controlled by lane0 ctrl). */
    if (!I->is_interp1 && (ctrl0 & QS_CTRL_BLEND_BITS)) {
        uint32_t alpha = shiftmask(I->ctrl[1], cross_in(I, 1)) & 0xFFu;
        if (L == 0) return alpha;                 /* lane0 = alpha only */
        if (I->ctrl[1] & QS_CTRL_SIGNED_BITS) {
            int32_t b0 = (int32_t)I->base[0], b1 = (int32_t)I->base[1];
            return (uint32_t)(b0 + (((b1 - b0) * (int32_t)alpha) >> 8));
        } else {
            int32_t b0 = (int32_t)I->base[0], b1 = (int32_t)I->base[1];
            return (uint32_t)(b0 + (((b1 - b0) * (int32_t)alpha) >> 8));
        }
    }

    /* CLAMP mode (interp1 lane0 only). */
    if (I->is_interp1 && L == 0 && (ctrl0 & QS_CTRL_CLAMP_BITS)) {
        uint32_t v = shiftmask(I->ctrl[0], cross_in(I, 0));
        if (I->ctrl[0] & QS_CTRL_SIGNED_BITS) {
            int32_t sv = (int32_t)v, lo = (int32_t)I->base[0], hi = (int32_t)I->base[1];
            if (sv < lo) sv = lo;
            if (sv > hi) sv = hi;
            return (uint32_t)sv;
        } else {
            uint32_t lo = I->base[0], hi = I->base[1];
            if (v < lo) v = lo;
            if (v > hi) v = hi;
            return v;
        }
    }

    /* Normal: base + (add_raw ? raw cross-muxed accum : shift+mask). */
    uint32_t add = (I->ctrl[L] & QS_CTRL_ADD_RAW_BITS)
                       ? cross_in(I, L)
                       : shiftmask(I->ctrl[L], cross_in(I, L));
    return I->base[L] + add;
}

/* lane result AS READ BY THE PROCESSOR (adds FORCE_MSB into bits 29:28). */
static uint32_t lane_presented(const interp_hw_t *I, int L) {
    uint32_t res = lane_internal(I, L);
    /* In BLEND mode lane0 yields only alpha and FORCE doesn't apply there. */
    uint32_t force = (I->ctrl[L] & QS_CTRL_FORCE_MSB_BITS) >> QS_CTRL_FORCE_MSB_LSB;
    return res | (force << 28);
}

static uint32_t full_result(const interp_hw_t *I) {
    if (!I->is_interp1 && (I->ctrl[0] & QS_CTRL_BLEND_BITS))
        return I->base[2] + shiftmask(I->ctrl[0], cross_in(I, 0)); /* lane1 omitted */
    uint32_t l0 = shiftmask(I->ctrl[0], cross_in(I, 0));
    uint32_t l1 = shiftmask(I->ctrl[1], cross_in(I, 1));
    return I->base[2] + l0 + l1;
}

static void commit_pop(interp_hw_t *I) {
    uint32_t l0 = lane_internal(I, 0);
    uint32_t l1 = lane_internal(I, 1);
    uint32_t n0 = (I->ctrl[0] & QS_CTRL_CROSS_RESULT_BITS) ? l1 : l0;
    uint32_t n1 = (I->ctrl[1] & QS_CTRL_CROSS_RESULT_BITS) ? l0 : l1;
    I->accum[0] = n0;
    I->accum[1] = n1;
}

/* ---- public API ---------------------------------------------------------- */

void interp_set_config(interp_hw_t *I, uint lane, interp_config *c) { I->ctrl[lane] = c->ctrl; }
void interp_set_base(interp_hw_t *I, uint lane, uint32_t v)         { I->base[lane] = v; }
uint32_t interp_get_base(interp_hw_t *I, uint lane)                 { return I->base[lane]; }
void interp_set_accumulator(interp_hw_t *I, uint lane, uint32_t v)  { I->accum[lane] = v; }
uint32_t interp_get_accumulator(interp_hw_t *I, uint lane)          { return I->accum[lane]; }
void interp_add_accumulator(interp_hw_t *I, uint lane, uint32_t v)  { I->accum[lane] += v; }
uint32_t interp_get_raw(interp_hw_t *I, uint lane)                  { return shiftmask(I->ctrl[lane], cross_in(I, lane)); }

void interp_set_base_both(interp_hw_t *I, uint32_t val) {
    uint32_t lo = val & 0xFFFFu, hi = (val >> 16) & 0xFFFFu;
    if (I->ctrl[0] & QS_CTRL_SIGNED_BITS) lo = (uint32_t)(int32_t)(int16_t)lo;
    if (I->ctrl[1] & QS_CTRL_SIGNED_BITS) hi = (uint32_t)(int32_t)(int16_t)hi;
    I->base[0] = lo;
    I->base[1] = hi;
}

uint32_t interp_peek_lane_result(interp_hw_t *I, uint lane) { return lane_presented(I, lane); }
uint32_t interp_peek_full_result(interp_hw_t *I)            { return full_result(I); }

uint32_t interp_pop_lane_result(interp_hw_t *I, uint lane) {
    uint32_t r = lane_presented(I, lane);
    commit_pop(I);
    return r;
}
uint32_t interp_pop_full_result(interp_hw_t *I) {
    uint32_t r = full_result(I);
    commit_pop(I);
    return r;
}
