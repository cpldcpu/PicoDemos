/* credits.c — QUICKSILVER finale. A chrome TUNNEL (not a 3D object) carries the
 * scrolling credits + wordmark, then fades to black.
 *
 * The tunnel is a LUT effect: at init we precompute, per screen pixel, the
 * angle around the shaft and the depth into it (one atan2/sqrt per pixel, once).
 * Each frame is then just "rotate + fly forward + bilinearly sample a small
 * chrome texture", no transform/sort/flash, so it holds full framerate. Both the
 * 128x64 wall texture and the 160x120 LUT live in g_scratch SRAM (no permanent
 * static — see the carve note below). */

#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "../scene_scratch.h"
#include "assets.h"
#include "qs_text.h"
#include "qs_fx.h"
#include "qs_tunnel.h"

#include <math.h>

#define TUW 128                         /* tex width  = around the tube (angle)   */
#define TVH 64                          /* tex height = depth                      */

/* The 128x64 chrome flute wall lives in g_scratch; the tube is the shared fast
 * raycast renderer (qs_tunnel.h), so the finale runs full framerate with no
 * permanent static. */
static uint16_t *s_tex;

static const char *lines[] = {
    /* Demoscene order: the PRODUCTION first (what this is + the tech), then the
     * crew, greets, and finally the GROUP sting — which hands off to the LATENT
     * logo end card. (Leading with the group read like an ad before the content.)
     * Every glyph is in the 8x8 charset (A-Z 0-9 space - . ! :) — no parens. */
    "QUICKSILVER",
    "",
    "AFFINE . BLEND . CLAMP . POP",
    "ROTOZOOM . TUNNEL . MODE-7 . CHROME",
    "BEAM-RACED FULL VGA. NO BUFFER.",
    "",
    "",
    "- CREDITS -",
    "",
    "BEAM",
    "CLAUDE OPUS 4.8",
    "CODE AND DIRECTION",
    "",
    "CODEX GPT-5.5",
    "TUNNEL OPTIMIZATION",
    "",
    "AZURE",
    "HUMAN CRITIC AND PRODUCER",
    "",
    "ANTIGRAVITY",
    "GEMINI 3.5 FLASH",
    "AND NANO BANANA 2",
    "VISUALS",
    "",
    "SUNO 5.5",
    "MUSIC",
    "",
    "",
    "GREETINGS TO THE SCENE",
    "AND TO THE SCEPTICS.",
    "",
    "",
    "A LATENT PRODUCTION",
    "",
    "LATENT IS A NEW GROUP FOR",
    "MACHINE-MADE PRODUCTIONS",
    "ON BARE-METAL SILICON",
    "",
    "FOUNDED 2026",
    "",
    NULL,
};

static void credits_init(void)
{
    s_tex = (uint16_t *)g_scratch.bg_cache;
    /* downsample the seamless chrome flute wall into the 128x64 texture */
    const uint16_t *src = (const uint16_t *)asset_tunnel_data;
    int ustep = ASSET_TUNNEL_W / TUW, vstep = ASSET_TUNNEL_W / TVH;
    for (int v = 0; v < TVH; v++)
        for (int u = 0; u < TUW; u++)
            s_tex[v * TUW + u] = src[(v * vstep) * ASSET_TUNNEL_W + u * ustep];
}

static void tunnel(uint32_t t_ms)
{
    /* calm backdrop for the text: slow forward fly, gentle breathing, no pulse */
    static const qs_tun_params P = {
        .fwd = 1.4f, .ell_amp = 0.18f, .cam_k = 0.35f, .twist = 0.10f,
        .fog_range = 6.0f, .bright = 0, .pulse_amp = 0, .pulse_hz = 0.0f,
    };
    float *cs = (float *)(s_tex + TUW * TVH);
    qs_tunnel_render(s_tex, TUW, TVH, t_ms * 0.001f, &P, cs);
}

/* fade ramp: 0 before `in`, 0..256 over `in..in+rise`, holds, then 256..0
 * over `out-fall..out` (out<=0 = never fade out). t in seconds. */
static int credits_ramp(float t, float in, float rise, float out, float fall)
{
    if (t < in) return 0;
    int a = 256;
    if (t < in + rise) a = (int)((t - in) / rise * 256.0f);
    if (out > 0.0f && t > out - fall) {
        int b = (int)((out - t) / fall * 256.0f);
        if (b < a) a = b;
    }
    if (a < 0) a = 0;
    if (a > 256) a = 256;
    return a;
}

static void credits_frame(uint32_t t_ms, uint32_t t_global)
{
    tunnel(t_ms);
    float t = t_ms * 0.001f;

    /* PHASE 1 — the credit lines scroll up and clear the screen. SLOW so the roll
     * is comfortably readable; this finale is ~40 s (it starts on the 2:25 melody
     * now that the victory lap was shortened), so at this pace the scroller clears
     * by ~24 s, leaving room for the two-stage end card + fade.                 */
    int scroll = (int)(t_ms * 0.032f);
    int y = VGA_HIRES_H + 20 - scroll;
    for (int i = 0; lines[i]; i++) {
        const char *s = lines[i];
        if (*s) {
            int w = qs_text_w(s, 1);
            if (y > -10 && y < VGA_HIRES_H) qs_text_chrome(s, (VGA_HIRES_W - w) / 2, y, 1, 50);
            y += 17;
        } else {
            y += 11;
        }
    }

    /* PHASE 2 — end card, two spaced stages that hand off (never crowded):
     *   stage A (25..31s): QUICKSILVER wordmark + tagline, centred.
     *   stage B (31.5s..) : LATENT group sting + year, centred. */
    int wa = credits_ramp(t, 25.0f, 1.0f, 31.0f, 1.0f);
    if (wa > 0) {
        int wy = 66;
        qs_logo_blit_a(0, wy, wa);
        const char *sub = "RACING THE RP2350 INTERPOLATOR";
        int sw = qs_text_w(sub, 1);
        qs_text_chrome_a(sub, (VGA_HIRES_W - sw) / 2, wy + QS_LOGO_H + 12, 1, 50, wa);
    }
    /* the group sting rises ~31.5 s in — the two logos hand off cleanly instead
     * of colliding mid-screen. The global fade (last 5 s) ends it. */
    int ga = credits_ramp(t, 31.5f, 1.0f, 0.0f, 0.0f);
    if (ga > 0) {
        int gy = 92;
        qs_latent_blit_a(gy, ga);
        const char *m = "2026"; int w = qs_text_w(m, 1);
        qs_text_chrome_a(m, (VGA_HIRES_W - w) / 2, gy + ASSET_LATENT_LOGO_H + 16, 1, 40, ga);
    }

    /* fade to black over the last 5 s — a clear ending */
    uint32_t end = scene_cur_end_ms();
    uint32_t left = end > t_global ? end - t_global : 0;
    if (left < 5000) {
        int k = 256 - (int)(left * 256 / 5000);
        uint16_t *fb = vga_hires_back_buffer();
        int n = VGA_HIRES_W * VGA_HIRES_H;
        for (int i = 0; i < n; i++) {
            uint16_t c = fb[i];
            fb[i] = rgb565_pack(rgb565_r8(c) * (256 - k) >> 8,
                                rgb565_g8(c) * (256 - k) >> 8,
                                rgb565_b8(c) * (256 - k) >> 8);
        }
    }
}

const effect_t fx_credits = {
    .name  = "credits",
    .mode  = MODE_HIRES,
    .init  = credits_init,
    .frame = credits_frame,
    .done  = NULL,
};
