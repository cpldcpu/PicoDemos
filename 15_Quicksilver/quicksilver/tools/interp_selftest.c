/* interp_selftest.c — validates the host interpolator emulator against
 * hand-computed expected values for every datapath feature QUICKSILVER uses.
 * Build & run on host:
 *     gcc -std=gnu11 -O2 -Wall -DHOST_BUILD=1 -I.. tools/interp_selftest.c \
 *         interp_emu.c -o interp_selftest && ./interp_selftest
 * (The same vectors can be replayed on real RP2350 hardware to confirm the
 *  emulator matches the silicon.) */

#include <stdio.h>
#include "../interp_compat.h"

static int g_fail = 0;
static void check(const char *what, uint32_t got, uint32_t want) {
    if (got != want) { printf("  FAIL %-34s got=0x%08x want=0x%08x\n", what, got, want); g_fail++; }
    else             { printf("  ok   %-34s = 0x%08x\n", what, got); }
}

int main(void)
{
    /* 1. plain shift(rotate)+mask, no base ------------------------------- */
    {
        interp_config c = interp_default_config();
        interp_config_set_shift(&c, 4);
        interp_config_set_mask(&c, 0, 7);     /* keep 8 bits after rotate */
        interp_set_config(interp0, 0, &c);
        interp_set_base(interp0, 0, 0);
        interp_set_accumulator(interp0, 0, 0x000003F0u);     /* >>4 -> 0x3F */
        check("shiftmask 0x3F0>>4 & 0xFF", interp_peek_lane_result(interp0, 0), 0x3F);
    }

    /* 2. base added to lane result --------------------------------------- */
    {
        interp_config c = interp_default_config();
        interp_config_set_shift(&c, 0);
        interp_set_config(interp0, 0, &c);
        interp_set_base(interp0, 0, 1000);
        interp_set_accumulator(interp0, 0, 23);
        check("base+accum", interp_peek_lane_result(interp0, 0), 1023);
    }

    /* 3. sign extension from MASK_MSB ------------------------------------ */
    {
        interp_config c = interp_default_config();
        interp_config_set_shift(&c, 0);
        interp_config_set_mask(&c, 0, 7);
        interp_config_set_signed(&c, true);
        interp_set_config(interp0, 0, &c);
        interp_set_base(interp0, 0, 0);
        interp_set_accumulator(interp0, 0, 0xFF);   /* bit7 set -> -1 */
        check("signext 0xFF[7:0]", interp_peek_lane_result(interp0, 0), 0xFFFFFFFFu);
    }

    /* 4. affine texture-OFFSET generation (the headline use) ------------- */
    {
        /* 256x256 RGB565: log2bpp=1, log2w=8, log2h=8 */
        qs_texmap_setup(interp0, 1, 8, 8);
        interp_set_accumulator(interp0, 0, 3u << 16);   /* u col 3   */
        interp_set_accumulator(interp0, 1, 5u << 16);   /* v row 5   */
        check("texoff (5*256+3)*2", interp_peek_full_result(interp0), (5*256 + 3) * 2);

        /* fractional bits ignored for the integer offset */
        interp_set_accumulator(interp0, 0, (3u << 16) | 0x8000u);
        check("texoff frac ignored", interp_peek_full_result(interp0), (5*256 + 3) * 2);

        /* integer part wraps mod texture size for free */
        interp_set_accumulator(interp0, 0, 259u << 16);  /* 259 & 255 = 3 */
        check("texoff wrap 259->3", interp_peek_full_result(interp0), (5*256 + 3) * 2);

        /* stepping via add_accumulator */
        interp_set_accumulator(interp0, 0, 0);
        interp_set_accumulator(interp0, 1, 0);
        interp_add_accumulator(interp0, 0, 0x8000);   /* +0.5 */
        interp_add_accumulator(interp0, 0, 0x8000);   /* +0.5 -> col 1 */
        check("texoff after 2x +0.5 step", interp_peek_full_result(interp0), 1 * 2);
    }

    /* 5. BLEND mode (interp0): linear lerp in hardware ------------------- */
    {
        interp_config c0 = interp_default_config();
        interp_config_set_blend(&c0, true);
        interp_set_config(interp0, 0, &c0);
        interp_config c1 = interp_default_config();   /* lane1 carries alpha */
        interp_set_config(interp0, 1, &c1);
        interp_set_base(interp0, 0, 100);
        interp_set_base(interp0, 1, 200);
        interp_set_base(interp0, 2, 0);
        interp_set_accumulator(interp0, 1, 128);       /* alpha = 128/256 */
        check("blend lane1 100..200 @128", interp_peek_lane_result(interp0, 1), 150);
        check("blend lane0 = alpha",       interp_peek_lane_result(interp0, 0), 128);
        interp_set_accumulator(interp0, 0, 7);
        check("blend FULL = base2+lane0sm", interp_peek_full_result(interp0), 7);
    }

    /* 6. CLAMP mode (interp1 lane0) -------------------------------------- */
    {
        interp_config c = interp_default_config();
        interp_config_set_clamp(&c, true);
        interp_set_config(interp1, 0, &c);
        interp_set_base(interp1, 0, 10);   /* lo */
        interp_set_base(interp1, 1, 20);   /* hi */
        interp_set_accumulator(interp1, 0, 5);
        check("clamp 5 ->10", interp_peek_lane_result(interp1, 0), 10);
        interp_set_accumulator(interp1, 0, 25);
        check("clamp 25->20", interp_peek_lane_result(interp1, 0), 20);
        interp_set_accumulator(interp1, 0, 15);
        check("clamp 15->15", interp_peek_lane_result(interp1, 0), 15);
    }

    /* 7. POP updates both accumulators (cross_result off) ---------------- */
    {
        interp_config c = interp_default_config();
        interp_config_set_shift(&c, 0);
        interp_set_config(interp0, 0, &c);
        interp_set_config(interp0, 1, &c);
        interp_set_base(interp0, 0, 1);
        interp_set_base(interp0, 1, 2);
        interp_set_base(interp0, 2, 0);
        interp_set_accumulator(interp0, 0, 10);
        interp_set_accumulator(interp0, 1, 20);
        uint32_t full = interp_pop_full_result(interp0);   /* 0+10+20 = 30 */
        check("pop full result", full, 30);
        check("pop wrote accum0 = base0+10", interp_get_accumulator(interp0, 0), 11);
        check("pop wrote accum1 = base1+20", interp_get_accumulator(interp0, 1), 22);
    }

    /* 8. POP self-stepping affine (the production inner-loop config) --------
     * add_raw makes each lane result = base+accum (the step), while FULL still
     * uses shift+mask for the texel offset. One pop_full = offset + advance. */
    {
        qs_texmap_setup(interp0, 1, 8, 8);          /* 256x256 RGB565, add_raw on */
        interp_set_accumulator(interp0, 0, 3u << 16);
        interp_set_accumulator(interp0, 1, 5u << 16);
        qs_texmap_step(interp0, 1u << 16, 0);       /* du=1 texel, dv=0 */
        uint32_t o0 = interp_pop_full_result(interp0);   /* (5*256+3)*2, then u++ */
        uint32_t o1 = interp_pop_full_result(interp0);   /* (5*256+4)*2, then u++ */
        uint32_t o2 = interp_pop_full_result(interp0);   /* (5*256+5)*2 */
        check("pop affine offset 0", o0, (5*256 + 3) * 2);
        check("pop affine offset 1", o1, (5*256 + 4) * 2);
        check("pop affine offset 2", o2, (5*256 + 5) * 2);
        check("pop affine accum advanced", interp_get_accumulator(interp0,0) >> 16, 6);
    }

    printf(g_fail ? "\nSELFTEST: %d FAILURE(S)\n" : "\nSELFTEST: ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
