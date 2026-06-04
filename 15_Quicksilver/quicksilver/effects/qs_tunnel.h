/* qs_tunnel.h — fast chrome TUNNEL shared by the mid-demo conduit and the
 * credits finale. Ported from 10_TheDemo's tunnel: instead of a precomputed
 * angle/depth LUT with per-pixel bilinear (full-res, RGB565 multiply — too slow
 * on hardware), we RAYCAST a breathing elliptical tube at 320x120 and LINE-DOUBLE
 * to 240 (half the pixels), POINT-sample a small wall texture, and fog by a cheap
 * depth multiply. The camera flies forward, drifts in x/y and banks (the ellipse
 * mouth rolls past) — so it stays interesting without a static centre. ~90 cy/px
 * over 38 400 px fits the 60 fps budget, same as demo 10.
 *
 * The texture is power-of-two (TUW x TVH) and lives in g_scratch SRAM (the caller
 * downsamples it there at init). No LUT, no permanent static. */

#ifndef QS_TUNNEL_H
#define QS_TUNNEL_H

#include "../vga.h"
#include "../rgb565.h"
#include "qs_fx.h"
#include <math.h>

#ifdef PICO_BUILD
#include "pico/platform.h"
#define QS_TUN_FAST(n) __not_in_flash_func(n)
#else
#define QS_TUN_FAST(n) n
#endif

#define QT_INT_W  VGA_HIRES_W          /* 320 — full horizontal                  */
#define QT_CELL   4                    /* raycast every 4th px; interpolate between */
#define QT_NCO    (QT_INT_W / QT_CELL + 1)   /* 81 coarse samples per row          */

typedef struct {
    float fwd;        /* forward fly speed (cam_z = t * fwd)               */
    float ell_amp;    /* ellipse breathing amplitude (0 = round)          */
    float cam_k;      /* camera x/y drift as a fraction of the tube radius */
    float twist;      /* extra angular swirl per second                   */
    float fog_range;  /* depth at which the wall fogs fully to black       */
    int   bright;     /* brightness boost added after fog (0..80)          */
    int   pulse_amp;  /* rhythmic brightness pulse amplitude (0 = none)    */
    float pulse_hz;   /* pulse frequency                                   */

    /* --- bump + moving-light extension (qs_tunnel_render_lit only) ---   */
    float bump;        /* relief strength (0 = flat shading)               */
    float light_speed; /* world depth units/sec the light rings travel     */
    float light_span;  /* depth spacing between successive light rings      */
    float light_range; /* ring falloff radius (world depth units)          */
    float light_amb;   /* ambient floor 0..1 (unlit-wall brightness)       */
    float key_amp;     /* steady rotating azimuthal key-light strength 0..1 */
    float bump_tile;   /* bump sampled this many x the colour freq (finer   */
                       /* relief without more SRAM; 0/1 = same as colour)   */
    float spec;        /* specular gloss gain (0 = matte diffuse; ~500 =     */
                       /* sharp white highlights where a bump faces a light) */
    float bump_lerp;   /* >0.5 = bilinear-sample the gradient (smooth relief, */
                       /* no blocky texels when magnified); 0 = point-sample  */
    float disp;        /* relief raymarch: radial displacement amplitude      */
                       /* (fraction of tube radius the wall bumps inward)     */
    float disp_tile;   /* relief: height sampled this many x around/along the  */
                       /* tube (>1 = smaller, more numerous bumps; 0/1 = 1x)  */
} qs_tun_params;

/* Fast atan2 (~0.0015 rad error — invisible as a texture U coord, ~10x cheaper
 * than libm atan2f). Drives the per-pixel cost down enough to make full QVGA
 * worth measuring on hardware. */
static inline float qs_fast_atan2(float y, float x)
{
    float ax = fabsf(x), ay = fabsf(y);
    float d = (ax > ay) ? ax : (ay + 1e-12f);
    float a = ((ax > ay) ? ay : ax) / d;          /* [0,1] */
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0)   r = 3.14159274f - r;
    if (y < 0)   r = -r;
    return r;
}

/* Render the tube into the MODE_HIRES back buffer at full 320x240. `tex` is
 * TUW x TVH RGB565 (both powers of two). `t` is scene time in seconds.
 *
 * SPEED: the costly per-pixel work is the ray-vs-ellipse intersection (sqrt +
 * divide) and the azimuth atan2. We do those only on a COARSE horizontal grid
 * (every QT_CELL px) and linearly interpolate the texture U/V (kept continuous,
 * so spans away from the centre are smooth) and the fog brightness across each
 * span — the cheap texture-sample + shade runs per full-res pixel. Net ~2 M
 * cy/frame at 320x240 — comfortably 60 fps at 300 MHz, and crisp vertically
 * (no line-doubling). The only interp artifacts are a thin smear at the dark
 * centre and the one radial atan2 wrap line, both hidden. */
/* `cs` is caller-provided scratch of at least 3*QT_NCO floats (≈ 1 KB) — pass a
 * slice of g_scratch past the texture so this adds no BSS and no deep stack. */
static inline void qs_tunnel_render(const uint16_t *tex, int tuw, int tvh,
                                    float t, const qs_tun_params *P, float *cs)
{
    uint16_t *fb = vga_hires_back_buffer();
    const int tum = tuw - 1, tvm = tvh - 1;
    float *cu = cs, *cv = cs + QT_NCO, *cb = cs + 2 * QT_NCO;

    /* breathing ellipse axes + flying / banking camera (demo-10 motion) */
    float a = 1.00f + P->ell_amp * sinf(t * 0.40f);
    float b = 0.70f + (P->ell_amp * 0.73f) * cosf(t * 0.27f);
    float min_axis = (a < b) ? a : b;
    float cam_amp  = min_axis * P->cam_k;
    float cam_x = cam_amp * sinf(t * 0.31f + 0.7f);
    float cam_y = cam_amp * cosf(t * 0.23f);
    float cam_z = t * P->fwd;
    float rot = 0.55f * sinf(t * 0.21f) + 0.28f * sinf(t * 0.39f + 1.3f)
              + 0.14f * sinf(t * 0.73f + 0.7f) + 0.40f * sinf(t * 0.13f) + t * P->twist;
    float rcos = cosf(rot), rsin = sinf(rot);
    float inva = 1.0f / a, invb = 1.0f / b;          /* reciprocals: no per-px divide */
    float inv_a2 = inva * inva, inv_b2 = invb * invb;
    float cam_xe =  rcos * cam_x + rsin * cam_y;
    float cam_ye = -rsin * cam_x + rcos * cam_y;
    float C_q = cam_xe * cam_xe * inv_a2 + cam_ye * cam_ye * inv_b2 - 1.0f;

    const float focal   = (float)(QT_INT_W / 2);
    const float fx_step = 1.0f / focal, fy_step = 1.0f / focal;
    const float u_scale = (float)tuw / 6.2831853f;
    const float v_scale = (float)tvh * 0.25f;
    const float fog_range = P->fog_range, inv_fog = 1.0f / fog_range;
    const float fcell = 1.0f / QT_CELL;
    int pulse = P->pulse_amp ? (int)(P->pulse_amp * (0.5f + 0.5f * sinf(t * P->pulse_hz))) : 0;
    float bbias = (float)(P->bright + pulse);

    for (int iy = 0; iy < VGA_HIRES_H; iy++) {
        float fy_scr = (iy - VGA_HIRES_H * 0.5f) * fy_step;

        /* --- coarse pass: raycast every QT_CELL px (continuous U/V) --- */
        for (int c = 0; c < QT_NCO; c++) {
            int cxp = c * QT_CELL; if (cxp > QT_INT_W) cxp = QT_INT_W;
            float fx_scr = (cxp - QT_INT_W * 0.5f) * fx_step;
            float Dx = rcos * fx_scr + rsin * fy_scr;
            float Dy = -rsin * fx_scr + rcos * fy_scr;
            float A_q = Dx * Dx * inv_a2 + Dy * Dy * inv_b2;
            float B_q = cam_xe * Dx * inv_a2 + cam_ye * Dy * inv_b2;
            float disc = B_q * B_q - A_q * C_q;
            float t_hit;
            if (A_q < 1e-5f || disc < 0.0f) t_hit = fog_range;
            else {
                t_hit = (-B_q + sqrtf(disc)) / A_q;
                if (t_hit < 0.0f) t_hit = 0.0f;
                if (t_hit > fog_range) t_hit = fog_range;
            }
            float hx = cam_xe + Dx * t_hit;
            float hy = cam_ye + Dy * t_hit;
            float hz = cam_z + t_hit;
            cu[c] = qs_fast_atan2(hy * invb, hx * inva) * u_scale + 128.0f;
            cv[c] = hz * v_scale;
            cb[c] = (1.0f - t_hit * inv_fog) * 255.0f + bbias;
        }

        /* --- fine pass: interpolate U/V/brightness across each span --- */
        uint16_t *row = fb + iy * VGA_HIRES_W;
        for (int c = 0; c < QT_NCO - 1; c++) {
            int cx0 = c * QT_CELL;
            float u = cu[c], v = cv[c], bf = cb[c];
            float du = (cu[c + 1] - u) * fcell;
            float dv = (cv[c + 1] - v) * fcell;
            float db = (cb[c + 1] - bf) * fcell;
            for (int k = 0; k < QT_CELL; k++) {
                int ix = cx0 + k;
                int iu = ((int)u) & tum, iv = ((int)v) & tvm;
                uint16_t texel = tex[iv * tuw + iu];
                int br = (int)bf; if (br > 255) br = 255; if (br < 0) br = 0;
                int d = qs_dither(ix, iy);
                row[ix] = rgb565_pack((rgb565_r8(texel) * br >> 8) + d,
                                      (rgb565_g8(texel) * br >> 8) + d,
                                      (rgb565_b8(texel) * br >> 8) + d);
                u += du; v += dv; bf += db;
            }
        }
    }
}

/* ---- bump-mapped variant: moving light rings + per-pixel surface relief ----
 *
 * Same coarse-raycast/interpolate skeleton as qs_tunnel_render, plus lighting.
 * The trick that keeps it cheap: the LIGHT is low-frequency, so the three light
 * coefficients (base / azimuthal / axial) are evaluated only at the coarse grid
 * and interpolated; the BUMP is high-frequency, so per pixel we read one packed
 * gradient and add two multiplies. Net ~+1.5 M cy over the flat tube.
 *
 * Lighting model, all derived from data the raycast already produced:
 *   - the hit lies ON the breathing ellipse, so (hx/a, hy/b) is the unit
 *     azimuthal basis (cos phi, sin phi) for free — no normalize.
 *   - moving "lights" are bright rings periodic in world depth hz, sweeping at
 *     light_speed; dz = signed depth offset to the nearest ring drives both the
 *     attenuation and the side the relief is lit from (the sign of gy*Iv flips
 *     as a ring rolls past -> ridges flash).
 *   - a slowly rotating azimuthal key light gives vertical ridges steady form.
 *
 * `grad` is the tuw x tvh packed central-difference height gradient from
 * qs_tunnel_build_grad (int16: (gx<<8)|(gy&0xff), each signed [-127,127]).
 * `cs` needs 5*QT_NCO floats (cu,cv + 3 light coefs) — a slice of g_scratch. */

static inline int qs_luma565(uint16_t t)
{
    return (rgb565_r8(t) + 2 * rgb565_g8(t) + rgb565_b8(t)) >> 2;   /* 0..255 */
}

/* Build the packed luma-gradient (height = luma) bump map. Wraps on both axes
 * to match the tube's seamless texture. int16 output, lives in g_scratch. */
static inline void qs_tunnel_build_grad(const uint16_t *tex, int tuw, int tvh,
                                        int16_t *grad)
{
    const int tum = tuw - 1, tvm = tvh - 1;
    for (int v = 0; v < tvh; v++) {
        for (int u = 0; u < tuw; u++) {
            int hl = qs_luma565(tex[v * tuw + ((u - 1) & tum)]);
            int hr = qs_luma565(tex[v * tuw + ((u + 1) & tum)]);
            int hb = qs_luma565(tex[((v - 1) & tvm) * tuw + u]);
            int ht = qs_luma565(tex[((v + 1) & tvm) * tuw + u]);
            int gx = (hr - hl) >> 1, gy = (ht - hb) >> 1;   /* [-127..127] */
            if (gx >  127) gx =  127;
            if (gx < -127) gx = -127;
            if (gy >  127) gy =  127;
            if (gy < -127) gy = -127;
            grad[v * tuw + u] = (int16_t)((gx << 8) | (gy & 0xff));
        }
    }
}

/* Box-averaged height sample at source texel (cx,cy), radius sr, wrapping (sw,sh
 * must be powers of two). Low-passes the source so sharp creases become smooth
 * rounded mounds — turns "crumpled foil" into "flowing mercury". */
static inline int qs_h_box(const uint8_t *src, int swm, int shm, int sw,
                           int cx, int cy, int sr)
{
    int sum = 0, n = 0;
    for (int dy = -sr; dy <= sr; dy++) {
        int y = (cy + dy) & shm;
        for (int dx = -sr; dx <= sr; dx++) {
            sum += src[y * sw + ((cx + dx) & swm)];
            n++;
        }
    }
    return sum / n;
}

/* Build the bump map from a DEDICATED grayscale height texture. The source may be
 * higher-res than the gradient grid (e.g. 256 -> 128). `smooth` is a box-blur
 * radius (in source texels) applied before differencing: 0 = crisp (grooves,
 * rivets), larger = smooth rounded relief (liquid mercury). `baseline` is the
 * central-difference half-span in source texels (wider = broader, softer slopes).
 * Same packed int16 output as qs_tunnel_build_grad. Wraps both axes. */
static inline void qs_tunnel_build_grad_g8(const uint8_t *src, int sw, int sh,
                                           int dw, int dh, int16_t *grad,
                                           int smooth, int baseline)
{
    int us = sw / dw, vs = sh / dh;
    if (us < 1) us = 1;
    if (vs < 1) vs = 1;
    if (baseline < 1) baseline = 1;
    const int swm = sw - 1, shm = sh - 1;   /* pow2 wrap masks */
    for (int v = 0; v < dh; v++) {
        int y0 = v * vs;
        for (int u = 0; u < dw; u++) {
            int x0 = u * us;
            int hl = qs_h_box(src, swm, shm, sw, x0 - baseline, y0, smooth);
            int hr = qs_h_box(src, swm, shm, sw, x0 + baseline, y0, smooth);
            int hb = qs_h_box(src, swm, shm, sw, x0, y0 - baseline, smooth);
            int ht = qs_h_box(src, swm, shm, sw, x0, y0 + baseline, smooth);
            int gx = (hr - hl) >> 1, gy = (ht - hb) >> 1;
            if (gx >  127) gx =  127;
            if (gx < -127) gx = -127;
            if (gy >  127) gy =  127;
            if (gy < -127) gy = -127;
            grad[v * dw + u] = (int16_t)((gx << 8) | (gy & 0xff));
        }
    }
}

static inline void qs_tunnel_render_lit(const uint16_t *tex, const int16_t *grad,
                                        int tuw, int tvh, float t,
                                        const qs_tun_params *P, float *cs)
{
    uint16_t *fb = vga_hires_back_buffer();
    const int tum = tuw - 1, tvm = tvh - 1;
    float *cu = cs, *cv = cs + QT_NCO, *cn = cs + 2 * QT_NCO,
          *clu = cs + 3 * QT_NCO, *clv = cs + 4 * QT_NCO;

    /* breathing ellipse axes + flying / banking camera (demo-10 motion) */
    float a = 1.00f + P->ell_amp * sinf(t * 0.40f);
    float b = 0.70f + (P->ell_amp * 0.73f) * cosf(t * 0.27f);
    float min_axis = (a < b) ? a : b;
    float cam_amp  = min_axis * P->cam_k;
    float cam_x = cam_amp * sinf(t * 0.31f + 0.7f);
    float cam_y = cam_amp * cosf(t * 0.23f);
    float cam_z = t * P->fwd;
    float rot = 0.55f * sinf(t * 0.21f) + 0.28f * sinf(t * 0.39f + 1.3f)
              + 0.14f * sinf(t * 0.73f + 0.7f) + 0.40f * sinf(t * 0.13f) + t * P->twist;
    float rcos = cosf(rot), rsin = sinf(rot);
    float inva = 1.0f / a, invb = 1.0f / b;
    float inv_a2 = inva * inva, inv_b2 = invb * invb;
    float cam_xe =  rcos * cam_x + rsin * cam_y;
    float cam_ye = -rsin * cam_x + rcos * cam_y;
    float C_q = cam_xe * cam_xe * inv_a2 + cam_ye * cam_ye * inv_b2 - 1.0f;

    const float focal   = (float)(QT_INT_W / 2);
    const float fx_step = 1.0f / focal, fy_step = 1.0f / focal;
    const float u_scale = (float)tuw / 6.2831853f;
    const float v_scale = (float)tvh * 0.25f;
    const float fog_range = P->fog_range, inv_fog = 1.0f / fog_range;
    const float fcell = 1.0f / QT_CELL;

    /* lighting setup (per frame) */
    float gain = 1.0f + (P->pulse_amp ? (P->pulse_amp / 255.0f) *
                         (0.5f + 0.5f * sinf(t * P->pulse_hz)) : 0.0f);
    gain *= 1.0f + P->bright / 255.0f;
    float lph     = t * P->light_speed;
    float inv_span = (P->light_span > 1e-4f) ? 1.0f / P->light_span : 0.0f;
    float inv_lr2  = 1.0f / (P->light_range * P->light_range + 1e-6f);
    float gscale   = P->bump * (1.0f / 127.0f);   /* int8 grad -> response   */
    float keyang   = t * 0.5f;                    /* slow key-light rotation */
    float kx = cosf(keyang), ky = sinf(keyang);
    float amb = P->light_amb;
    float spec = P->spec;

    for (int iy = 0; iy < VGA_HIRES_H; iy++) {
        float fy_scr = (iy - VGA_HIRES_H * 0.5f) * fy_step;

        /* --- coarse pass: raycast + light coefficients every QT_CELL px --- */
        for (int c = 0; c < QT_NCO; c++) {
            int cxp = c * QT_CELL; if (cxp > QT_INT_W) cxp = QT_INT_W;
            float fx_scr = (cxp - QT_INT_W * 0.5f) * fx_step;
            float Dx = rcos * fx_scr + rsin * fy_scr;
            float Dy = -rsin * fx_scr + rcos * fy_scr;
            float A_q = Dx * Dx * inv_a2 + Dy * Dy * inv_b2;
            float B_q = cam_xe * Dx * inv_a2 + cam_ye * Dy * inv_b2;
            float disc = B_q * B_q - A_q * C_q;
            float t_hit;
            if (A_q < 1e-5f || disc < 0.0f) t_hit = fog_range;
            else {
                t_hit = (-B_q + sqrtf(disc)) / A_q;
                if (t_hit < 0.0f) t_hit = 0.0f;
                if (t_hit > fog_range) t_hit = fog_range;
            }
            float hx = cam_xe + Dx * t_hit;
            float hy = cam_ye + Dy * t_hit;
            float hz = cam_z + t_hit;
            cu[c] = qs_fast_atan2(hy * invb, hx * inva) * u_scale + 128.0f;
            cv[c] = hz * v_scale;

            float fog = 1.0f - t_hit * inv_fog; if (fog < 0.0f) fog = 0.0f;
            float fg  = fog * gain;
            float cphi = hx * inva, sphi = hy * invb;     /* unit on ellipse  */
            float rel = (hz - lph) * inv_span;
            float fr  = rel - floorf(rel + 0.5f);         /* [-0.5,0.5] ring  */
            float dzw = fr * P->light_span;               /* world axial off  */
            float atten = 1.0f / (1.0f + dzw * dzw * inv_lr2);
            float keyproj = (-sphi) * kx + cphi * ky;     /* azimuthal [-1,1] */
            cn[c]  = (amb + atten) * fg;                  /* base lit         */
            clu[c] = (P->key_amp * keyproj) * fg * gscale;/* * gx (vert ridge)*/
            clv[c] = (atten * fr * 2.0f) * fg * gscale;   /* * gy (ring sweep)*/
        }

        /* --- fine pass: interpolate tex coords + light, bump per pixel --- */
        /* bump samples at bt x the colour frequency (finer relief, same map) */
        float bt = (P->bump_tile > 1.0f) ? P->bump_tile : 1.0f;
        int bilin = (P->bump_lerp > 0.5f);
        uint16_t *row = fb + iy * VGA_HIRES_W;
        for (int c = 0; c < QT_NCO - 1; c++) {
            int cx0 = c * QT_CELL;
            float u = cu[c], v = cv[c], n = cn[c], lu = clu[c], lv = clv[c];
            float du = (cu[c + 1] - u) * fcell;
            float dv = (cv[c + 1] - v) * fcell;
            float dn = (cn[c + 1] - n) * fcell;
            float dlu = (clu[c + 1] - lu) * fcell;
            float dlv = (clv[c + 1] - lv) * fcell;
            float bu = u * bt, bv = v * bt;              /* bump coords       */
            float dbu = du * bt, dbv = dv * bt;
            for (int k = 0; k < QT_CELL; k++) {
                int ix = cx0 + k;
                int iu = ((int)u) & tum, iv = ((int)v) & tvm;
                uint16_t texel = tex[iv * tuw + iu];
                float gx, gy;
                if (bilin) {
                    /* bilinear-sample the gradient -> continuous (smooth) relief,
                     * no blocky texels however much the wall is magnified */
                    int iu0 = (int)bu, iv0 = (int)bv;
                    float fx = bu - iu0, fy = bv - iv0;
                    int u0 = iu0 & tum, u1 = (iu0 + 1) & tum;
                    int r0 = (iv0 & tvm) * tuw, r1 = ((iv0 + 1) & tvm) * tuw;
                    int g00 = grad[r0 + u0], g10 = grad[r0 + u1];
                    int g01 = grad[r1 + u0], g11 = grad[r1 + u1];
                    float w00 = (1.0f - fx) * (1.0f - fy), w10 = fx * (1.0f - fy);
                    float w01 = (1.0f - fx) * fy,          w11 = fx * fy;
                    gx = (g00 >> 8) * w00 + (g10 >> 8) * w10
                       + (g01 >> 8) * w01 + (g11 >> 8) * w11;
                    gy = (float)((int8_t)(g00 & 0xff)) * w00
                       + (float)((int8_t)(g10 & 0xff)) * w10
                       + (float)((int8_t)(g01 & 0xff)) * w01
                       + (float)((int8_t)(g11 & 0xff)) * w11;
                } else {
                    int ibu = ((int)bu) & tum, ibv = ((int)bv) & tvm;
                    int g = grad[ibv * tuw + ibu];
                    gx = (float)(g >> 8);
                    gy = (float)((int8_t)(g & 0xff));
                }
                float lit = n + gx * lu + gy * lv;
                int br = (int)(lit * 255.0f);
                if (br > 255) br = 255;
                if (br < 0) br = 0;
                /* specular gloss: a sharp white add where the lit surface is
                 * driven past unity (a bump facing a light) -> mercury hot spot */
                int sp = 0;
                if (spec > 0.0f && lit > 1.0f) {
                    float e = lit - 1.0f;
                    sp = (int)(e * e * spec);
                    if (sp > 255) sp = 255;
                }
                int d = qs_dither(ix, iy);
                int cr = (rgb565_r8(texel) * br >> 8) + sp + d;
                int cg = (rgb565_g8(texel) * br >> 8) + sp + d;
                int cb = (rgb565_b8(texel) * br >> 8) + sp + d;
                if (cr > 255) cr = 255;
                if (cg > 255) cg = 255;
                if (cb > 255) cb = 255;
                row[ix] = rgb565_pack(cr, cg, cb);
                u += du; v += dv; n += dn; lu += dlu; lv += dlv;
                bu += dbu; bv += dbv;
            }
        }
    }
}

/* ---- relief raymarch: REAL 3D wall displacement (parallax + occlusion) ----
 *
 * Bump mapping only perturbs shading, so down a foreshortened tunnel it reads
 * flat. This instead marches each view ray through a height field draped on the
 * tube: bumps physically protrude inward, parallax-shift with the view, and
 * occlude what's behind them — a genuine 3D surface. The hit point then samples
 * a grayscale height field and is shaded by moving coloured light rings.
 *
 * Cost: the per-pixel march is expensive, so the hot path was hand-optimized
 * (Codex / GPT-5.5): renderer pinned in SRAM (__not_in_flash_func); per-pixel
 * atan2 replaced by a 96x96 angle->U table + an analytic angular derivative;
 * sqrt/divide replaced by fast rsqrt/reciprocal bit-hacks; the ray quadratic and
 * rho^2 march stepped by finite differences; (1-amp*h)^2 and ring attenuation via
 * 256-entry LUTs. Renders 320x120 line-doubled, horizontally subsampled by QT_RXS.
 * QT_RELIEF_STEPS / QT_RXS / row count are the knobs if it underruns on hardware.
 *
 * `hgt` is a tuw x tvh grayscale height field (0..255). `grad` is the matching
 * packed central-difference map: int16 (gx<<8)|(gy&0xff), signed 8-bit lanes. */

#define QT_RW            VGA_HIRES_W      /* 320 screen width */
#define QT_RH            (VGA_HIRES_H/2)  /* 120, line-doubled to 240 */
#define QT_RXS           8                /* ADAPTIVE: coarse-march 1 of every N    */
                                          /* columns; smooth spans are lerp-filled, */
                                          /* but spans where the hit DEPTH jumps     */
                                          /* (silhouette/occlusion edges) are        */
                                          /* re-marched per column -> sharp edges    */
                                          /* where it matters, cheap fills elsewhere */
#define QT_REFINE_DT     0.20f            /* hit-depth jump (world u) that triggers  */
                                          /* a full per-column re-march of a span    */
#define QT_RELIEF_STEPS  4                /* view-ray march samples per pixel */
#define QT_ANGLE_LUT_N   96               /* 96x96 int16 U-coord LUT = 18 KB */

static inline uint8_t qs_h_pt(const uint8_t *h, int tuw, int tum, int tvm,
                              int iu, int iv)
{
    return h[(iv & tvm) * tuw + (iu & tum)];
}

/* Lerp two rgb565 colours, f8 in 0..256. Used to fill the columns between marched
 * samples in the relief tunnel — a smooth horizontal upscale of the half-rate
 * march, far cheaper than re-shading and near-lossless on smooth mercury. */
static inline uint16_t qs_tun_rgb565_lerp(uint16_t a, uint16_t b, int f8)
{
    int ar = rgb565_r8(a), ag = rgb565_g8(a), ab = rgb565_b8(a);
    return rgb565_pack(ar + (((rgb565_r8(b) - ar) * f8) >> 8),
                       ag + (((rgb565_g8(b) - ag) * f8) >> 8),
                       ab + (((rgb565_b8(b) - ab) * f8) >> 8));
}

static inline float qs_tun_fast_rsqrt_pos(float x)
{
    union { float f; uint32_t u; } v = { x };
    v.u = 0x5f3759dfu - (v.u >> 1);
    float y = v.f;
    return y * (1.5f - 0.5f * x * y * y);
}

static inline float qs_tun_fast_rcp_pos(float x)
{
    union { float f; uint32_t u; } v = { x };
    v.u = 0x7ef311c3u - v.u;
    float y = v.f;
    return y * (2.0f - x * y);
}

/* Build an angle->texture-U table for points on the normalized tube section
 * (x,y in [-1,1]). Values are U texels in signed 8.8 fixed, with the caller's
 * relief tiling baked in. Lives in g_scratch, not BSS. */
static inline void qs_tunnel_build_angle_u_lut(int16_t *dst, int n,
                                               int tuw, float htile)
{
    float u_scale = (float)tuw / 6.2831853f * htile * 256.0f;
    float inv = 2.0f / (float)(n - 1);
    for (int y = 0; y < n; y++) {
        float fy = y * inv - 1.0f;
        for (int x = 0; x < n; x++) {
            float fx = x * inv - 1.0f;
            float qf = atan2f(fy, fx) * u_scale;
            int q = (int)(qf + ((qf >= 0.0f) ? 0.5f : -0.5f));
            if (q >  32767) q =  32767;
            if (q < -32768) q = -32768;
            dst[y * n + x] = (int16_t)q;
        }
    }
}

/* Downsample a grayscale height source (e.g. 256 -> 128) into the SRAM height
 * field for the relief raymarch. `smooth` box-blurs (source must be pow2). */
static inline void qs_tunnel_build_height_g8(const uint8_t *src, int sw, int sh,
                                             int dw, int dh, uint8_t *dst,
                                             int smooth)
{
    int us = sw / dw, vs = sh / dh;
    if (us < 1) us = 1;
    if (vs < 1) vs = 1;
    const int swm = sw - 1, shm = sh - 1;
    for (int v = 0; v < dh; v++)
        for (int u = 0; u < dw; u++)
            dst[v * dw + u] =
                (uint8_t)qs_h_box(src, swm, shm, sw, u * us, v * vs, smooth);
}

/* Per-frame constants shared by the coarse + refined sample paths. */
typedef struct {
    const uint8_t *hgt; const int16_t *grad; const int16_t *angle_u;
    const float *surf2_lut; const uint8_t *atten_lut;
    int tuw, tum, tvm;
    float cam_xe, cam_ye, cam_z, inva, invb, inv_a2, inv_b2, Cc, C_q;
    float fog_range, inv_fog, u_scale, v_scale, UOFF, angle_idx_scale;
    float amp, near_scale, inv_steps, lph, inv_span;
    float kx, ky, amb, spec, gsc, key_amp, gain;
} qs_rctx;

/* March + shade ONE screen column. Returns the packed colour; reports the hit
 * depth via *out_hitT so the adaptive pass can decide interpolate-vs-refine.
 * (Invalid/black columns report a far sentinel so they always force a refine.) */
/* Split into MARCH (find the hit — the expensive coarse-sampled part) and SHADE
 * (colour from a hit — cheap, run per output pixel). The row loop interpolates
 * the hit COORDINATES between coarse marches and shades every pixel, so highlights
 * and the light ring stay crisp (interpolating colours smeared them) and the
 * angle-wrap is handled by unwrapping U, not by blending across the seam.
 *
 * In FLASH (not __not_in_flash_func): RAM clones blew the malloc heap below the
 * scanvideo pool and risked no-boot. Small + hot in XIP cache, so flash is cheap.
 * noinline,noclone => one copy each. */
static int __attribute__((noinline, noclone))
qs_relief_march(const qs_rctx *X, float Dx, float Dy,
                float *hT, float *hU, float *hV, int *hH)
{
    const int tuw = X->tuw, tum = X->tum, tvm = X->tvm;
    float A_q = Dx * Dx * X->inv_a2 + Dy * Dy * X->inv_b2;
    float B_q = X->cam_xe * Dx * X->inv_a2 + X->cam_ye * Dy * X->inv_b2;
    float discs = B_q * B_q - A_q * X->C_q;
    if (A_q < 1e-6f || discs < 0.0f) { *hT = 1e9f; *hU = 0; *hV = 0; *hH = 0; return 0; }

    float B2_q = 2.0f * B_q;
    float inv_root = qs_tun_fast_rsqrt_pos(discs);
    float t_s = (-B_q + discs * inv_root) * qs_tun_fast_rcp_pos(A_q);
    float t_n = t_s - X->near_scale * inv_root;
    if (t_n < 0.0f) t_n = 0.0f;
    if (t_s > X->fog_range) t_s = X->fog_range;

    float xs = (X->cam_xe + Dx * t_s) * X->inva;
    float ys = (X->cam_ye + Dy * t_s) * X->invb;
    int axi = (int)((xs + 1.0f) * X->angle_idx_scale);
    int ayi = (int)((ys + 1.0f) * X->angle_idx_scale);
    if (axi < 0) axi = 0; else if (axi >= QT_ANGLE_LUT_N) axi = QT_ANGLE_LUT_N - 1;
    if (ayi < 0) ayi = 0; else if (ayi >= QT_ANGLE_LUT_N) ayi = QT_ANGLE_LUT_N - 1;
    float dtheta_dt = xs * (Dy * X->invb) - ys * (Dx * X->inva);
    float Us = X->angle_u[ayi * QT_ANGLE_LUT_N + axi] * (1.0f / 256.0f) + X->UOFF;
    float Un = Us - dtheta_dt * (t_s - t_n) * X->u_scale;
    float Vn = (X->cam_z + t_n) * X->v_scale, Vs = (X->cam_z + t_s) * X->v_scale;
    float dt = (t_s - t_n) * X->inv_steps;
    float dU = (Us - Un) * X->inv_steps, dV = (Vs - Vn) * X->inv_steps;

    float ti = t_n, U = Un, V = Vn;
    float prevdiff = -1.0f, prevU = Un, prevV = Vn, prevT = t_n;
    float hitU = Us, hitV = Vs, hitT = t_s;
    int hitH = 0;
    float rho2 = A_q * t_n * t_n + B2_q * t_n + X->Cc;
    float drho = (2.0f * A_q * t_n + B2_q) * dt + A_q * dt * dt;
    float ddrho = 2.0f * A_q * dt * dt;
    for (int s = 0; s <= QT_RELIEF_STEPS; s++) {
        int hb = qs_h_pt(X->hgt, tuw, tum, tvm, (int)U, (int)V);
        float diff = rho2 - X->surf2_lut[hb];
        if (diff >= 0.0f) {
            float den = diff - prevdiff;
            float fr = (den > 1e-8f) ? (-prevdiff * qs_tun_fast_rcp_pos(den)) : 0.0f;
            if (fr < 0.0f) fr = 0.0f;
            if (fr > 1.0f) fr = 1.0f;
            hitU = prevU + (U - prevU) * fr;
            hitV = prevV + (V - prevV) * fr;
            hitT = prevT + (ti - prevT) * fr;
            hitH = hb;
            break;
        }
        prevdiff = diff; prevU = U; prevV = V; prevT = ti;
        ti += dt; U += dU; V += dV;
        rho2 += drho; drho += ddrho;
    }
    if (hitT > X->fog_range) hitT = X->fog_range;
    *hT = hitT; *hU = hitU; *hV = hitV; *hH = hitH;
    return 1;
}

static uint16_t __attribute__((noinline, noclone))
qs_relief_shade(const qs_rctx *X, int ix, int ry2, float Dx, float Dy,
                float hitT, float hitU, float hitV, int hitH)
{
    if (hitT >= 1e8f) return rgb565_pack(0, 0, 0);   /* invalid sentinel */
    const int tuw = X->tuw, tum = X->tum, tvm = X->tvm;
    int iu = ((int)hitU) & tum, iv = ((int)hitV) & tvm;
    int gp = X->grad[iv * tuw + iu];
    float gx = (float)(gp >> 8);
    float gy = (float)((int8_t)(gp & 0xff));
    float hz = X->cam_z + hitT;
    float fog = 1.0f - hitT * X->inv_fog; if (fog < 0.0f) fog = 0.0f;
    float fg = fog * X->gain;
    float Px = X->cam_xe + Dx * hitT, Py = X->cam_ye + Dy * hitT;
    float cphi = Px * X->inva, sphi = Py * X->invb;
    float rel = (hz - X->lph) * X->inv_span;
    int ring = (int)(rel + 0.5f);
    float frac = rel - (float)ring;
    int ai = (int)((frac + 0.5f) * 255.0f);
    if (ai < 0) ai = 0; else if (ai > 255) ai = 255;
    float atten = X->atten_lut[ai] * (1.0f / 255.0f);
    float keyproj = (-sphi) * X->kx + cphi * X->ky;
    float hh = hitH * (1.0f / 255.0f);
    float ao = 0.55f + 0.45f * hh;
    float baseL = (X->amb * fg
                 + gx * (X->key_amp * keyproj) * fg * X->gsc
                 + gy * (atten * frac * 2.0f) * fg * X->gsc) * ao;
    float ringL = (atten * fg) * ao;
    if (baseL < 0.0f) baseL = 0.0f;
    if (ringL < 0.0f) ringL = 0.0f;
    float lit = baseL + ringL;
    int sp = 0;
    if (X->spec > 0.0f && lit > 1.0f) {
        float e = lit - 1.0f; sp = (int)(e * e * X->spec);
        if (sp > 255) sp = 255;
    }
    /* dither y MUST be the row index, not the (always-even) scanline ry2: with an
     * even y the 4x4 Bayer degenerates to a dark-even/light-odd COLUMN stripe. */
    int d = qs_dither(ix, ry2 >> 1);
    int cr = (int)((0.82f * baseL + 1.45f * ringL) * 160.0f) + sp + d;
    int cg = (int)((0.94f * baseL + 1.00f * ringL) * 160.0f) + sp + d;
    int cb = (int)((1.18f * baseL + 0.45f * ringL) * 160.0f) + sp + d;
    if (cr > 255) cr = 255;
    if (cr < 0) cr = 0;
    if (cg > 255) cg = 255;
    if (cg < 0) cg = 0;
    if (cb > 255) cb = 255;
    if (cb < 0) cb = 0;
    return rgb565_pack(cr, cg, cb);
}

/* `unused` silences the warning in credits.c (includes this header but doesn't
 * call it); noinline keeps it ONE flash function instead of inlining into the
 * RAM-pinned tunnel_frame (which would refill the heap hole). */
static __attribute__((noinline, unused))
void qs_tunnel_render_relief(const uint8_t *hgt,
                             const int16_t *grad,
                             const int16_t *angle_u,
                             int tuw, int tvh,
                             float t,
                             const qs_tun_params *P)
{
    uint16_t *fb = vga_hires_back_buffer();
    const int tum = tuw - 1, tvm = tvh - 1;

    /* breathing ellipse + flying / banking camera (same motion as the others) */
    float a = 1.00f + P->ell_amp * sinf(t * 0.40f);
    float b = 0.70f + (P->ell_amp * 0.73f) * cosf(t * 0.27f);
    float min_axis = (a < b) ? a : b;
    float cam_amp  = min_axis * P->cam_k;
    float cam_x = cam_amp * sinf(t * 0.31f + 0.7f);
    float cam_y = cam_amp * cosf(t * 0.23f);
    float cam_z = t * P->fwd;
    float rot = 0.55f * sinf(t * 0.21f) + 0.28f * sinf(t * 0.39f + 1.3f)
              + 0.14f * sinf(t * 0.73f + 0.7f) + 0.40f * sinf(t * 0.13f) + t * P->twist;
    float rcos = cosf(rot), rsin = sinf(rot);
    float inva = 1.0f / a, invb = 1.0f / b;
    float inv_a2 = inva * inva, inv_b2 = invb * invb;
    float cam_xe =  rcos * cam_x + rsin * cam_y;
    float cam_ye = -rsin * cam_x + rcos * cam_y;
    float Cc  = cam_xe * cam_xe * inv_a2 + cam_ye * cam_ye * inv_b2;  /* rho^2 at cam */
    float C_q = Cc - 1.0f;

    const float focal   = (float)(QT_RW / 2);
    const float fx_step = 1.0f / focal, fy_step = 1.0f / focal;
    float htile = (P->disp_tile > 1.0f) ? P->disp_tile : 1.0f;
    const float u_scale = (float)tuw / 6.2831853f * htile;  /* bumps around tube */
    const float v_scale = (float)tvh * 0.25f * htile;       /* bumps along depth */
    const float UOFF = 4096.0f;                             /* keep U >0 pre-wrap */
    const float angle_idx_scale = (float)(QT_ANGLE_LUT_N - 1) * 0.5f;
    const float fog_range = P->fog_range, inv_fog = 1.0f / fog_range;

    float amp = (P->disp > 0.0f) ? P->disp : 0.15f;
    float r_near2 = (1.0f - amp) * (1.0f - amp);
    float near_drop = 1.0f - r_near2;
    float near_scale = near_drop * 0.55f;
    const float inv_steps = 1.0f / QT_RELIEF_STEPS;
    float surf2_lut[256];
    for (int i = 0; i < 256; i++) {
        float h = i * (1.0f / 255.0f);
        float surf = 1.0f - amp * h;
        surf2_lut[i] = surf * surf;
    }

    /* lighting (per frame) */
    float gain = 1.0f + (P->pulse_amp ? (P->pulse_amp / 255.0f) *
                         (0.5f + 0.5f * sinf(t * P->pulse_hz)) : 0.0f);
    gain *= 1.0f + P->bright / 255.0f;
    float lph     = t * P->light_speed;
    float inv_span = (P->light_span > 1e-4f) ? 1.0f / P->light_span : 0.0f;
    float inv_lr2  = 1.0f / (P->light_range * P->light_range + 1e-6f);
    uint8_t atten_lut[256];
    for (int i = 0; i < 256; i++) {
        float frac = i * (1.0f / 255.0f) - 0.5f;
        float dzw = frac * P->light_span;
        int a8 = (int)(255.0f / (1.0f + dzw * dzw * inv_lr2) + 0.5f);
        if (a8 > 255) a8 = 255;
        if (a8 < 0) a8 = 0;
        atten_lut[i] = (uint8_t)a8;
    }
    float keyang   = t * 0.5f;
    float kx = cosf(keyang), ky = sinf(keyang);
    float amb = P->light_amb, spec = P->spec;
    float gsc = ((P->bump > 0.0f) ? P->bump : 1.0f) * (1.0f / 127.0f);

    qs_rctx X = {
        hgt, grad, angle_u, surf2_lut, atten_lut,
        tuw, tum, tvm,
        cam_xe, cam_ye, cam_z, inva, invb, inv_a2, inv_b2, Cc, C_q,
        fog_range, inv_fog, u_scale, v_scale, UOFF, angle_idx_scale,
        amp, near_scale, inv_steps, lph, inv_span,
        kx, ky, amb, spec, gsc, P->key_amp, gain,
    };
    const float HALF = QT_RW * 0.5f;
    const float wrapHalf = (float)tuw * htile * 0.5f;  /* |dU| this big => angle wrap */

    uint16_t *prevRow = (uint16_t *)0;
    for (int ry = 0; ry < QT_RH; ry++) {
        float fy_scr = (ry * 2 - VGA_HIRES_H * 0.5f) * fy_step;
        int ry2 = ry * 2;
        uint16_t *row0 = fb + ry2 * VGA_HIRES_W;   /* even scanline */

        /* coarse column 0: march for hit coords, shade for colour */
        float fx0 = (0 - HALF) * fx_step;
        float pDx = rcos * fx0 + rsin * fy_scr, pDy = -rsin * fx0 + rcos * fy_scr;
        float pT, pU, pV; int pH;
        qs_relief_march(&X, pDx, pDy, &pT, &pU, &pV, &pH);
        row0[0] = qs_relief_shade(&X, 0, ry2, pDx, pDy, pT, pU, pV, pH);
        int pcx = 0;

        for (int cx = QT_RXS; ; cx += QT_RXS) {
            int last = 0;
            if (cx >= QT_RW) { cx = QT_RW - 1; last = 1; }
            float fxc = (cx - HALF) * fx_step;
            float cDx = rcos * fxc + rsin * fy_scr, cDy = -rsin * fxc + rcos * fy_scr;
            float cT, cU, cV; int cH;
            qs_relief_march(&X, cDx, cDy, &cT, &cU, &cV, &cH);

            float dT = cT - pT; if (dT < 0.0f) dT = -dT;
            float dU = cU - pU; if (dU < 0.0f) dU = -dU;
            if (dT > QT_REFINE_DT || dU > wrapHalf) {  /* depth edge / wrap -> march each col */
                for (int ox = pcx + 1; ox <= cx; ox++) {
                    float fxx = (ox - HALF) * fx_step;
                    float oDx = rcos * fxx + rsin * fy_scr, oDy = -rsin * fxx + rcos * fy_scr;
                    float oT, oU, oV; int oH;
                    qs_relief_march(&X, oDx, oDy, &oT, &oU, &oV, &oH);
                    row0[ox] = qs_relief_shade(&X, ox, ry2, oDx, oDy, oT, oU, oV, oH);
                }
            } else {                          /* smooth -> interpolate COORDS, shade each px */
                float invspan = 1.0f / (float)(cx - pcx);
                for (int ox = pcx + 1; ox <= cx; ox++) {
                    float f = (float)(ox - pcx) * invspan;
                    float hT = pT + (cT - pT) * f;
                    float hU = pU + (cU - pU) * f;
                    float hV = pV + (cV - pV) * f;
                    int   hH = pH + (int)((float)(cH - pH) * f);
                    float fxx = (ox - HALF) * fx_step;
                    float oDx = rcos * fxx + rsin * fy_scr, oDy = -rsin * fxx + rcos * fy_scr;
                    row0[ox] = qs_relief_shade(&X, ox, ry2, oDx, oDy, hT, hU, hV, hH);
                }
            }
            pcx = cx; pT = cT; pU = cU; pV = cV; pH = cH;
            if (last) break;
        }

        /* vertical interpolation: the odd scanline between the previous even row
         * and this one is the midpoint blend, not a copy -> no obvious doubling */
        if (prevRow) {
            uint16_t *odd = prevRow + VGA_HIRES_W;
            for (int x = 0; x < VGA_HIRES_W; x++)
                odd[x] = qs_tun_rgb565_lerp(prevRow[x], row0[x], 128);
        }
        prevRow = row0;
    }
    if (prevRow) {                  /* last odd scanline: no row below, replicate */
        uint16_t *odd = prevRow + VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) odd[x] = prevRow[x];
    }
}

#endif
