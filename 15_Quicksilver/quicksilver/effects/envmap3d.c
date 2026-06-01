/* envmap3d.c — see envmap3d.h. Transform → project → painter-sort → matcap
 * scanline fill (interpolator affine address-gen + bilinear in the span loop).
 *
 * Per-frame working set is file-static (kept modest: int16 screen coords,
 * 16.16 fixed matcap UVs) so it doesn't blow the SRAM budget. The hot span
 * loop is the same interp tap the other scenes use. On device the loop and the
 * transform should be placed in SRAM (__not_in_flash_func) once we cycle-count.
 */

#include "envmap3d.h"
#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "qs_fx.h"

#include <math.h>

#define MAXV 2048                  /* covers the largest object (TORUS 1920) */
#define MAXT 4096                  /* covers the most triangles (TORUS 3840) */
#define ENV_BYTES (256 * 256 * 2)
#define EMASK     (ENV_BYTES - 1)

/* per-vertex projected state */
static int16_t  s_x[MAXV], s_y[MAXV];
static int32_t  s_u[MAXV], s_v[MAXV];     /* matcap UV, 16.16 texel fixed */
static uint8_t  s_vis[MAXV];              /* 1 if projected (z>near)      */
/* triangle painter order */
static uint16_t s_order[MAXT];
static int32_t  s_key[MAXT];

void qs_env_default(qs_env_params *p)
{
    p->yaw = p->pitch = p->roll = 0.f;
    p->scale = 1.f;
    p->ox = 0.f; p->oy = 0.f; p->oz = 4.0f;
    p->focal = 280.f;
    p->env = NULL; p->envW = 256; p->envH = 256;
    p->log2bpp = 1; p->log2w = 8; p->log2h = 8;
}

/* fill one triangle (screen ints + 16.16 matcap UVs) via the interpolator. */
static void fill_tri(const uint8_t *env, int texw,
                     int ax,int ay,int32_t au,int32_t av,
                     int bx,int by,int32_t bu,int32_t bv,
                     int cx,int cy,int32_t cu,int32_t cv)
{
    /* sort by y: a (top) .. c (bottom) */
    if (ay > by){int t; t=ax;ax=bx;bx=t; t=ay;ay=by;by=t; int32_t u; u=au;au=bu;bu=u; u=av;av=bv;bv=u;}
    if (ay > cy){int t; t=ax;ax=cx;cx=t; t=ay;ay=cy;cy=t; int32_t u; u=au;au=cu;cu=u; u=av;av=cv;cv=u;}
    if (by > cy){int t; t=bx;bx=cx;cx=t; t=by;by=cy;cy=t; int32_t u; u=bu;bu=cu;cu=u; u=bv;bv=cv;cv=u;}
    if (cy == ay) return;

    uint16_t *fb = vga_hires_back_buffer();
    float fay=(float)ay, fby=(float)by, fcy=(float)cy;
    float invAC = 1.f/(fcy-fay);
    float invAB = (by>ay) ? 1.f/(fby-fay) : 0.f;
    float invBC = (cy>by) ? 1.f/(fcy-fby) : 0.f;

    int y0 = ay < 0 ? 0 : ay;
    int y1 = cy > VGA_HIRES_H ? VGA_HIRES_H : cy;
    for (int y = y0; y < y1; y++) {
        float tac = (y - fay) * invAC;                 /* along long edge a->c */
        float xL = ax + (cx - ax) * tac;
        float uL = au + (cu - au) * tac;
        float vL = av + (cv - av) * tac;
        float xS, uS, vS;
        if (y < by) { float t=(y-fay)*invAB; xS=ax+(bx-ax)*t; uS=au+(bu-au)*t; vS=av+(bv-av)*t; }
        else        { float t=(y-fby)*invBC; xS=bx+(cx-bx)*t; uS=bu+(cu-bu)*t; vS=bv+(cv-bv)*t; }

        float xl=xL, xr=xS, ul=uL, ur=uS, vl=vL, vr=vS;
        if (xl > xr){ float t; t=xl;xl=xr;xr=t; t=ul;ul=ur;ur=t; t=vl;vl=vr;vr=t; }
        int ixl=(int)(xl+0.5f), ixr=(int)(xr+0.5f);
        if (ixr <= ixl) continue;
        float w = (float)(ixr - ixl);
        float du = (ur - ul)/w, dv = (vr - vl)/w;

        int cxl = ixl, cxr = ixr;
        float u = ul, v = vl;
        if (cxl < 0){ u += du*(-cxl); v += dv*(-cxl); cxl = 0; }
        if (cxr > VGA_HIRES_W) cxr = VGA_HIRES_W;
        if (cxr <= cxl) continue;

        interp_set_accumulator(interp0, 0, (uint32_t)(int32_t)u);
        interp_set_accumulator(interp0, 1, (uint32_t)(int32_t)v);
        qs_texmap_step(interp0, (uint32_t)(int32_t)du, (uint32_t)(int32_t)dv);

        uint32_t bytemask = (uint32_t)(texw * texw * 2 - 1);   /* square matcap */
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = cxl; x < cxr; x++)
            row[x] = qs_tap_bilerp(interp0, env, texw, bytemask);  /* SRAM bilinear */
    }
}

void qs_envmap_render(const qs_mesh *m, const qs_env_params *p)
{
    qs_texmap_setup(interp0, p->log2bpp, p->log2w, p->log2h);
    const uint8_t *env = p->env;

    /* rotation matrix (yaw=Y, pitch=X, roll=Z) */
    float cy=cosf(p->yaw),  sy=sinf(p->yaw);
    float cx=cosf(p->pitch),sx=sinf(p->pitch);
    float cz=cosf(p->roll), sz=sinf(p->roll);
    /* R = Rz * Rx * Ry */
    float m00= cz*cy + sz*sx*sy, m01= -sz*cx, m02= cz*(-sy)+sz*sx*cy;
    float m10= sz*cy - cz*sx*sy, m11=  cz*cx, m12= sz*(-sy)-cz*sx*cy;
    float m20= cx*sy,            m21= -sx,    m22= cx*cy;

    float F = p->focal, sc = p->scale;
    float cxs = VGA_HIRES_W * 0.5f, cys = VGA_HIRES_H * 0.5f;
    float halfW = (p->envW * 0.5f), halfH = (p->envH * 0.5f);

    int nv = m->nv;
    for (int i = 0; i < nv; i++) {
        qs_mvec vv = m->v[i], nn = m->n[i];
        /* rotate position */
        float rx = m00*vv.x + m01*vv.y + m02*vv.z;
        float ry = m10*vv.x + m11*vv.y + m12*vv.z;
        float rz = m20*vv.x + m21*vv.y + m22*vv.z;
        float vx = rx*sc + p->ox, vy = ry*sc + p->oy, vz = rz*sc + p->oz;
        if (vz < 0.25f) { s_vis[i] = 0; continue; }
        s_vis[i] = 1;
        s_x[i] = (int16_t)(cxs + vx * F / vz);
        s_y[i] = (int16_t)(cys - vy * F / vz);
        /* rotate normal -> view space; matcap UV from its x,y */
        float nx = m00*nn.x + m01*nn.y + m02*nn.z;
        float ny = m10*nn.x + m11*nn.y + m12*nn.z;
        float u = (nx * 0.5f + 0.5f) * p->envW;
        float v = (0.5f - ny * 0.5f) * p->envH;   /* flip y for image space */
        (void)halfW; (void)halfH;
        s_u[i] = (int32_t)(u * 65536.0f);
        s_v[i] = (int32_t)(v * 65536.0f);
    }

    /* painter sort: triangles far (large vz proxy) first. Use sum of screen-y?
     * better: average rotated z. Recompute cheaply from positions. */
    int nt = m->nt, ndraw = 0;
    for (int f = 0; f < nt; f++) {
        int a = m->tri[f*3], b = m->tri[f*3+1], c = m->tri[f*3+2];
        if (!s_vis[a] || !s_vis[b] || !s_vis[c]) continue;
        /* backface cull by screen winding (front faces CCW->positive area) */
        int ax=s_x[a],ay=s_y[a],bx=s_x[b],by=s_y[b],cx2=s_x[c],cy2=s_y[c];
        long area = (long)(bx-ax)*(cy2-ay) - (long)(cx2-ax)*(by-ay);
        if (area <= 0) continue;     /* cull back faces by screen winding */
        s_order[ndraw++] = (uint16_t)f;
    }

    /* depth keys = rotated z of each visible triangle's centroid (far first) */
    for (int k = 0; k < ndraw; k++) {
        int f = s_order[k];
        int a = m->tri[f*3], b = m->tri[f*3+1], c = m->tri[f*3+2];
        qs_mvec va=m->v[a], vb=m->v[b], vc=m->v[c];
        float zx = (va.x+vb.x+vc.x)/3.f, zy=(va.y+vb.y+vc.y)/3.f, zz=(va.z+vb.z+vc.z)/3.f;
        float rz = m20*zx + m21*zy + m22*zz;
        s_key[k] = (int32_t)((rz*sc + p->oz) * 1024.f);   /* larger = farther */
    }

    /* O(n) counting sort by depth, far-first (insertion sort was O(n^2) and
     * tanked the high-poly objects). 512 depth buckets; intra-bucket order is
     * arbitrary (those triangles are at ~the same depth). */
    #define NB 512
    static uint16_t s_order2[MAXT];
    static int cnt[NB], off[NB];
    int32_t kmin = 0x7fffffff, kmax = -0x7fffffff;
    for (int k = 0; k < ndraw; k++) { if (s_key[k] < kmin) kmin = s_key[k]; if (s_key[k] > kmax) kmax = s_key[k]; }
    int range = kmax - kmin; if (range < 1) range = 1;
    for (int b = 0; b < NB; b++) cnt[b] = 0;
    for (int k = 0; k < ndraw; k++) cnt[(int)(((int64_t)(s_key[k]-kmin) * (NB-1)) / range)]++;
    int acc = 0;
    for (int b = NB - 1; b >= 0; b--) { off[b] = acc; acc += cnt[b]; }  /* high bucket first */
    for (int k = 0; k < ndraw; k++) {
        int b = (int)(((int64_t)(s_key[k]-kmin) * (NB-1)) / range);
        s_order2[off[b]++] = s_order[k];
    }

    for (int k = 0; k < ndraw; k++) {
        int f = s_order2[k];
        int a = m->tri[f*3], b = m->tri[f*3+1], c = m->tri[f*3+2];
        fill_tri(env, p->envW,
                 s_x[a],s_y[a],s_u[a],s_v[a],
                 s_x[b],s_y[b],s_u[b],s_v[b],
                 s_x[c],s_y[c],s_u[c],s_v[c]);
    }
}
