#include "texmap.h"
#include "chunky.h"

env_mode_t env_mode = ENV_SPHERE;
bool tex_blur_mode = false;
int tex_light_u_offset = 0;
int tex_light_v_offset = 0;

/* Span buffers — one entry per scanline. UV stored as integer; we don't
 * need 16.16 fractional accuracy at 320 wide. */
static int16_t span_lx[SCREEN_H];
static int16_t span_rx[SCREEN_H];
static int16_t span_lu[SCREEN_H];
static int16_t span_ru[SCREEN_H];
static int16_t span_lv[SCREEN_H];
static int16_t span_rv[SCREEN_H];

/* Env map: compute one texel inline.
 *   dx, dy = signed offset from center (128, 128) → range ~[-128, 127]
 *   distSq >> 4, clamp to 0xFC, complement → fade to dark at edges.
 * dawn_final.s:917-959; matches web_port textureMap.ts:33-74. */
static inline uint8_t env_sample(int u, int v)
{
    u = (u + tex_light_u_offset) & 0xFF;
    v = (v + tex_light_v_offset) & 0xFF;
    const int dx = u - 128;
    const int dy = v - 128;
    int distSq = (dx*dx + dy*dy) >> 4;
    int color;
    if (distSq > 0xFC) color = 0xFE;
    else color = distSq;
    color = (~color) & 0xFF;
    if (env_mode == ENV_SPHERE) {
        color >>= 2;
    } else {
        /* Checker: scale by 14500/65536 ≈ 0.22, then bump brighter half. */
        color = ((color * 14500) >> 16) & 0xFF;
        if (((u ^ v) & 0x20) == 0) {
            color += 7;
            if (color > 63) color = 63;
        }
    }
    return (uint8_t)(color & 0x3F);
}

/* Shortest wrap delta for UV stepping (handles 256-wraparound seam). */
static inline int short_delta(int a, int b)
{
    int d = (b - a) & 0xFF;
    if (d >= 128) d -= 256;
    return d;
}

void texmap_draw_polygon(const screen_pt_t *pts,
                         const uint8_t *us,
                         const uint8_t *vs,
                         int n)
{
    if (n < 3) return;

    int min_y = SCREEN_H - 1, max_y = 0;
    for (int i = 0; i < n; i++) {
        if (pts[i].sy < min_y) min_y = pts[i].sy;
        if (pts[i].sy > max_y) max_y = pts[i].sy;
    }
    if (max_y < 0 || min_y >= SCREEN_H) return;
    if (min_y < 0) min_y = 0;
    if (max_y >= SCREEN_H) max_y = SCREEN_H - 1;

    for (int y = min_y; y <= max_y; y++) {
        span_lx[y] =  INT16_MAX;
        span_rx[y] = -INT16_MAX;
    }

    for (int i = 0; i < n; i++) {
        const int j = (i + 1) % n;
        int x0 = pts[i].sx, y0 = pts[i].sy;
        int x1 = pts[j].sx, y1 = pts[j].sy;
        int u0 = us[i], v0 = vs[i];
        int u1 = us[j], v1 = vs[j];

        if (y0 == y1) continue;
        if (y0 > y1) {
            int t;
            t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
            t = u0; u0 = u1; u1 = t;
            t = v0; v0 = v1; v1 = t;
        }

        const int dy = y1 - y0;
        /* Scaled deltas in 8.8 fixed point — keeps the inner loop integer. */
        const int dx_q = ((x1 - x0) << 8) / dy;
        const int du_q = (short_delta(u0, u1) << 8) / dy;
        const int dv_q = (short_delta(v0, v1) << 8) / dy;

        int y_start = (y0 < min_y) ? min_y : y0;
        int y_end   = (y1 - 1 > max_y) ? max_y : (y1 - 1);

        int x_q = (x0 << 8) + dx_q * (y_start - y0);
        int u_q = (u0 << 8) + du_q * (y_start - y0);
        int v_q = (v0 << 8) + dv_q * (y_start - y0);

        for (int y = y_start; y <= y_end; y++) {
            const int xi = x_q >> 8;
            if (xi < span_lx[y]) {
                span_lx[y] = (int16_t)xi;
                span_lu[y] = (int16_t)((u_q >> 8) & 0xFF);
                span_lv[y] = (int16_t)((v_q >> 8) & 0xFF);
            }
            if (xi > span_rx[y]) {
                span_rx[y] = (int16_t)xi;
                span_ru[y] = (int16_t)((u_q >> 8) & 0xFF);
                span_rv[y] = (int16_t)((v_q >> 8) & 0xFF);
            }
            x_q += dx_q;
            u_q += du_q;
            v_q += dv_q;
        }
    }

    for (int y = min_y; y <= max_y; y++) {
        int lx = span_lx[y];
        int rx = span_rx[y];
        if (lx >= rx) continue;
        if (lx < 0) lx = 0;
        if (rx >= SCREEN_W) rx = SCREEN_W - 1;
        if (lx >= rx) continue;

        const int span_w = rx - lx;
        if (span_w <= 0) continue;

        const int ul = span_lu[y];
        const int vl = span_lv[y];
        const int du = short_delta(ul, span_ru[y]);
        const int dv = short_delta(vl, span_rv[y]);
        const int du_q = (du << 8) / span_w;
        const int dv_q = (dv << 8) / span_w;

        int u_q = ul << 8;
        int v_q = vl << 8;

        uint8_t *out = chunky + y * SCREEN_W + lx;
        for (int x = lx; x <= rx; x++) {
            const uint8_t tex = env_sample(u_q >> 8, v_q >> 8);
            if (tex_blur_mode) {
                /* MAX blend: only brighten existing pixels. */
                if (tex > *out) *out = tex;
            } else {
                *out = tex;
            }
            out++;
            u_q += du_q;
            v_q += dv_q;
        }
    }
}
