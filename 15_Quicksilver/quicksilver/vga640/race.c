/* race.c — see race.h. The interpolator POP inner loop is the whole point:
 * `dst[x] = roto[ pop_full(interp0) ]` with the accumulators self-stepping,
 * reading the texture from SRAM, with the function pinned in SRAM on device. */

#include "race.h"
#include "../interp_compat.h"
#include "../rgb565.h"
#include "../effects/qs_fx.h"
#include "../assets/_packed/assets.h"

#ifdef PICO_BUILD
#include "pico/platform.h"
#define QS_FAST(name) __not_in_flash_func(name)
#else
#define QS_FAST(name) name
#endif

#define TW 256
#define TH 256

/* SRAM copy of the roto texture — beam-racing can't afford XIP cache misses. */
static uint16_t s_tex[TW * TH] __attribute__((aligned(4)));
static int s_ready = 0;

void qs_race_setup(void)
{
    qs_texmap_setup(interp0, 1, 8, 8);     /* roto: RGB565 256x256, POP mode */
    if (!s_ready) {
        const uint16_t *src = (const uint16_t *)asset_roto_data;
        for (int i = 0; i < TW * TH; i++) s_tex[i] = src[i];
        s_ready = 1;
    }
}

void QS_FAST(qs_race_scanline)(uint16_t *dst, int y, int W, int H, const qs_race_params *p)
{
    const uint8_t *base = (const uint8_t *)s_tex;
    float fl = p->flex ? p->flex[y] : 1.0f;
    float ca = p->ca0 * fl, sa = p->sa0 * fl;

    float dys = (float)(y - H / 2);
    float u0 = p->cu + (float)(-(W / 2)) * ca - dys * sa;
    float v0 = p->cv + (float)(-(W / 2)) * sa + dys * ca;

    interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)(u0 * 65536.0f));
    interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)(v0 * 65536.0f));
    qs_texmap_step(interp0, (uint32_t)(int32_t)(ca * 65536.0f),
                            (uint32_t)(int32_t)(sa * 65536.0f));

    /* the full-VGA hot loop: 1 POP (offset + auto-advance) + 1 SRAM load + store */
    for (int x = 0; x < W; x++)
        dst[x] = qs_tap_point(interp0, base);
}
