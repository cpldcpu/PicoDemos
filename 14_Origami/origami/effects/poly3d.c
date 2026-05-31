/* poly3d — flat-shaded filled-polygon 3D renderer (truecolor, antialiased).
 * See poly3d.h. Renders into the MODE_HIRES RGB565 back buffer. */

#include "poly3d.h"
#include "vga.h"
#include "rgb565.h"
#include <math.h>
#include <string.h>

#define W 320
#define H 240
#define P3_NEAR 6.0f

/* ---- file-static working set (BSS) ----------------------------------- */
static p3_vec3 g_world[P3_MAX_VERTS];
static p3_vec3 g_view [P3_MAX_VERTS];
static float   g_sx[P3_MAX_VERTS], g_sy[P3_MAX_VERTS];
static uint8_t g_vis[P3_MAX_VERTS];
static uint8_t g_fr[P3_MAX_FACES], g_fg[P3_MAX_FACES], g_fb[P3_MAX_FACES];
static struct { float key; uint16_t idx; } g_sort[P3_MAX_FACES];
static p3_vec3 g_folded[P3_MAX_VERTS];

/* material lit RGB (set per scene by p3_set_material) */
static uint8_t g_mat[P3_MAT_MAX][3];

/* ---- small vec / mat helpers ----------------------------------------- */

static inline void m3_euler(float yaw, float pitch, float roll, float m[9])
{
    float cy = cosf(yaw),   sy = sinf(yaw);
    float cx = cosf(pitch), sx = sinf(pitch);
    float cz = cosf(roll),  sz = sinf(roll);
    float rx[9] = { 1,0,0,  0,cx,-sx,  0,sx,cx };
    float ry[9] = { cy,0,sy, 0,1,0,  -sy,0,cy };
    float rz[9] = { cz,-sz,0, sz,cz,0, 0,0,1 };
    float t[9];
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
        t[r*3+c] = rx[r*3+0]*ry[0*3+c] + rx[r*3+1]*ry[1*3+c] + rx[r*3+2]*ry[2*3+c];
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
        m[r*3+c] = rz[r*3+0]*t[0*3+c] + rz[r*3+1]*t[1*3+c] + rz[r*3+2]*t[2*3+c];
}

static inline p3_vec3 m3_apply(const float m[9], p3_vec3 v)
{
    p3_vec3 o;
    o.x = m[0]*v.x + m[1]*v.y + m[2]*v.z;
    o.y = m[3]*v.x + m[4]*v.y + m[5]*v.z;
    o.z = m[6]*v.x + m[7]*v.y + m[8]*v.z;
    return o;
}

/* ---- folding (Rodrigues hinge) --------------------------------------- */

static void fold_model(const p3_model *mo, const p3_render_params *rp, p3_vec3 *Wv)
{
    memcpy(Wv, mo->verts, mo->nverts * sizeof(p3_vec3));
    if (!mo->creases) return;
    for (int ci = 0; ci < mo->ncreases; ci++) {
        const p3_crease *c = &mo->creases[ci];
        float ang = rp->fold_angle[c->angle_src];
        if (ang == 0.0f) continue;
        p3_vec3 A = Wv[c->a], B = Wv[c->b];
        float kx = B.x-A.x, ky = B.y-A.y, kz = B.z-A.z;
        float len = sqrtf(kx*kx+ky*ky+kz*kz);
        if (len < 1e-6f) continue;
        float inv = 1.0f/len; kx*=inv; ky*=inv; kz*=inv;
        float ca = cosf(ang), sa = sinf(ang), omc = 1.0f-ca;
        for (int j = 0; j < c->nmoves; j++) {
            uint16_t vi = c->moves[j];
            p3_vec3 p = { Wv[vi].x-A.x, Wv[vi].y-A.y, Wv[vi].z-A.z };
            float cxp = ky*p.z - kz*p.y;
            float cyp = kz*p.x - kx*p.z;
            float czp = kx*p.y - ky*p.x;
            float kd  = kx*p.x + ky*p.y + kz*p.z;
            Wv[vi].x = A.x + p.x*ca + cxp*sa + kx*kd*omc;
            Wv[vi].y = A.y + p.y*ca + cyp*sa + ky*kd*omc;
            Wv[vi].z = A.z + p.z*ca + czp*sa + kz*kd*omc;
        }
    }
}

/* ---- span helpers (return left/right x for scanline yc) -------------- */

static inline int span_x(const float *px, const float *py, int n, float yc,
                         float *xl, float *xr)
{
    float L = 1e9f, R = -1e9f;
    for (int i = 0, j = n-1; i < n; j = i++) {
        float ya = py[j], yb = py[i];
        if ((ya <= yc) == (yb <= yc)) continue;
        float t = (yc - ya) / (yb - ya);
        float x = px[j] + t * (px[i] - px[j]);
        if (x < L) L = x;
        if (x > R) R = x;
    }
    *xl = L; *xr = R;
    return R >= L;
}

/* Push a convex polygon's vertices outward from its centroid by `pad` px.
 * Adjacent facets that share an edge then overlap by ~pad on each side, so
 * one facet's solid interior covers the shared boundary — no AA seam/gap
 * between tiled faces — while the outer silhouette still antialiases. */
static inline void dilate_poly(const float *px, const float *py, int n, float pad,
                               float *ox, float *oy)
{
    float cx = 0, cy = 0;
    for (int i = 0; i < n; i++) { cx += px[i]; cy += py[i]; }
    cx /= n; cy /= n;
    for (int i = 0; i < n; i++) {
        float dx = px[i]-cx, dy = py[i]-cy;
        float l = sqrtf(dx*dx+dy*dy); if (l < 1e-3f) l = 1.0f;
        float s = (l + pad) / l;
        ox[i] = cx + dx*s; oy[i] = cy + dy*s;
    }
}

static inline uint16_t blend565(uint16_t cur, int r, int g, int b, int cov)
{
    int cr = rgb565_r8(cur), cg = rgb565_g8(cur), cb = rgb565_b8(cur);
    return rgb565_pack(cr + (((r-cr)*cov)>>8),
                       cg + (((g-cg)*cov)>>8),
                       cb + (((b-cb)*cov)>>8));
}

/* Antialiased solid fill (horizontal-edge coverage). */
void p3_fill_convex(const float *px, const float *py, int n, int r, int g, int b)
{
    uint16_t *fb = vga_hires_back_buffer();
    uint16_t packed = rgb565_pack(r, g, b);
    float fymin = 1e9f, fymax = -1e9f;
    for (int i = 0; i < n; i++) { if (py[i]<fymin) fymin=py[i]; if (py[i]>fymax) fymax=py[i]; }
    int y0 = (int)ceilf(fymin);  if (y0 < 0) y0 = 0;
    int y1 = (int)floorf(fymax); if (y1 > H-1) y1 = H-1;
    for (int y = y0; y <= y1; y++) {
        float xl, xr;
        if (!span_x(px, py, n, y+0.5f, &xl, &xr)) continue;
        if (xl < 0) xl = 0;
        if (xr > W) xr = W;
        if (xr <= xl) continue;
        uint16_t *row = &fb[y*W];
        int ixl = (int)floorf(xl), ixr = (int)floorf(xr - 1e-4f);
        if (ixl == ixr) {
            int cov = (int)((xr - xl) * 256.0f);
            if ((unsigned)ixl < W) row[ixl] = blend565(row[ixl], r,g,b, cov);
            continue;
        }
        int lcov = (int)((1.0f - (xl - ixl)) * 256.0f);
        int rcov = (int)((xr - ixr) * 256.0f);
        if ((unsigned)ixl < W) row[ixl] = blend565(row[ixl], r,g,b, lcov);
        for (int x = ixl+1; x < ixr; x++) if ((unsigned)x < W) row[x] = packed;
        if ((unsigned)ixr < W) row[ixr] = blend565(row[ixr], r,g,b, rcov);
    }
}

/* ---- transform + project --------------------------------------------- */

static void project_all(const p3_model *mo, const float M[9], const p3_vec3 t,
                        const p3_render_params *rp)
{
    float fcx = rp->cx, fcy = rp->cy, focal = rp->focal;
    for (int i = 0; i < mo->nverts; i++) {
        p3_vec3 w = g_world[i], v;
        v.x = M[0]*w.x + M[1]*w.y + M[2]*w.z + t.x;
        v.y = M[3]*w.x + M[4]*w.y + M[5]*w.z + t.y;
        v.z = M[6]*w.x + M[7]*w.y + M[8]*w.z + t.z;
        g_view[i] = v;
        if (v.z < P3_NEAR) { g_vis[i] = 0; continue; }
        float inv = focal / v.z;
        g_sx[i] = fcx + v.x * inv;
        g_sy[i] = fcy - v.y * inv;
        g_vis[i] = 1;
    }
}

/* ---- main render ----------------------------------------------------- */

void p3_render(const p3_model *mo, const p3_render_params *rp)
{
    if (mo->nverts > P3_MAX_VERTS || mo->nfaces > P3_MAX_FACES) return;

    fold_model(mo, rp, g_folded);
    float Rm[9]; m3_euler(rp->yaw, rp->pitch, rp->roll, Rm);
    for (int i = 0; i < mo->nverts; i++) {
        p3_vec3 v = m3_apply(Rm, g_folded[i]);
        g_world[i].x = v.x + rp->ox;
        g_world[i].y = v.y + rp->oy;
        g_world[i].z = v.z + rp->oz;
    }

    float Rc[9]; m3_euler(rp->cam_yaw, rp->cam_pitch, rp->cam_roll, Rc);
    p3_vec3 camrel = { -rp->cam_x, -rp->cam_y, -rp->cam_z };
    p3_vec3 t = m3_apply(Rc, camrel);
    project_all(mo, Rc, t, rp);

    p3_vec3 lv = m3_apply(Rc, rp->light);
    float ll = sqrtf(lv.x*lv.x + lv.y*lv.y + lv.z*lv.z); if (ll < 1e-6f) ll = 1.0f;
    lv.x/=ll; lv.y/=ll; lv.z/=ll;
    float amb = rp->ambient, span = 1.0f - amb;

    int nvis = 0;
    for (int f = 0; f < mo->nfaces; f++) {
        const p3_face *fc = &mo->faces[f];
        int n = (fc->i3 == P3_NO_VERT) ? 3 : 4;
        uint16_t vi[4] = { fc->i0, fc->i1, fc->i2, fc->i3 };
        int ok = 1;
        for (int k = 0; k < n; k++) if (!g_vis[vi[k]]) { ok = 0; break; }
        if (!ok) continue;

        p3_vec3 a = g_view[vi[0]], b = g_view[vi[1]], c = g_view[vi[2]];
        float e1x=b.x-a.x, e1y=b.y-a.y, e1z=b.z-a.z;
        float e2x=c.x-a.x, e2y=c.y-a.y, e2z=c.z-a.z;
        float nx = e1y*e2z - e1z*e2y;
        float ny = e1z*e2x - e1x*e2z;
        float nz = e1x*e2y - e1y*e2x;

        /* orient/cull by whether the normal faces the camera: the view-space
         * face centroid IS the direction from the camera (at origin) to the
         * face, so dot(n,centroid) > 0 means the normal points away. Works for
         * any orientation (incl. near-horizontal ground planes). */
        float ccx = (a.x+b.x+c.x)*0.3333f, ccy = (a.y+b.y+c.y)*0.3333f, ccz = (a.z+b.z+c.z)*0.3333f;
        float facing = nx*ccx + ny*ccy + nz*ccz;
        int dbl = fc->flags & P3_FACE_DOUBLE_SIDED;
        if (rp->backface_cull && !dbl && facing > 0.0f) continue;
        if (facing > 0.0f) { nx=-nx; ny=-ny; nz=-nz; }
        float nl = sqrtf(nx*nx+ny*ny+nz*nz); if (nl < 1e-6f) nl = 1.0f;
        float d = (nx*lv.x + ny*lv.y + nz*lv.z) / nl;
        if (d < 0.0f) d = 0.0f;
        float br = amb + span * d;

        const uint8_t *mc = g_mat[fc->material];
        int rr = (int)(mc[0]*br), gg = (int)(mc[1]*br), bb = (int)(mc[2]*br);
        if (rr>255)rr=255;
        if (gg>255)gg=255;
        if (bb>255)bb=255;
        g_fr[f]=(uint8_t)rr; g_fg[f]=(uint8_t)gg; g_fb[f]=(uint8_t)bb;

        float keyz = (a.z+b.z+c.z + (n==4 ? g_view[vi[3]].z : 0.0f)) / (float)n;
        g_sort[nvis].key = keyz; g_sort[nvis].idx = (uint16_t)f; nvis++;
    }

    for (int i = 1; i < nvis; i++) {
        float k = g_sort[i].key; uint16_t id = g_sort[i].idx;
        int j = i-1;
        while (j >= 0 && g_sort[j].key < k) { g_sort[j+1] = g_sort[j]; j--; }
        g_sort[j+1].key = k; g_sort[j+1].idx = id;
    }

    for (int s = 0; s < nvis; s++) {
        int f = g_sort[s].idx;
        const p3_face *fc = &mo->faces[f];
        int n = (fc->i3 == P3_NO_VERT) ? 3 : 4;
        uint16_t vi[4] = { fc->i0, fc->i1, fc->i2, fc->i3 };
        float px[4], py[4], dx[4], dy[4];
        for (int k = 0; k < n; k++) { px[k]=g_sx[vi[k]]; py[k]=g_sy[vi[k]]; }
        dilate_poly(px, py, n, 0.75f, dx, dy);     /* close shared-edge seams */
        p3_fill_convex(dx, dy, n, g_fr[f], g_fg[f], g_fb[f]);
    }
}

/* ---- soft drop shadow (alpha-darken the ground, union silhouette) ---- */

/* Rasterise a convex polygon into the coverage mask as a UNION (each pixel
 * keeps the MAX coverage), so overlapping facets don't compound. */
static void mask_convex(uint8_t *mask, const float *px, const float *py, int n)
{
    float fymin = 1e9f, fymax = -1e9f;
    for (int i = 0; i < n; i++) { if (py[i]<fymin) fymin=py[i]; if (py[i]>fymax) fymax=py[i]; }
    int y0 = (int)ceilf(fymin);  if (y0 < 0) y0 = 0;
    int y1 = (int)floorf(fymax); if (y1 > H-1) y1 = H-1;
    for (int y = y0; y <= y1; y++) {
        float xl, xr;
        if (!span_x(px, py, n, y+0.5f, &xl, &xr)) continue;
        if (xl < 0) xl = 0;
        if (xr > W) xr = W;
        if (xr <= xl) continue;
        uint8_t *row = &mask[y*W];
        int ixl = (int)floorf(xl), ixr = (int)floorf(xr - 1e-4f);
        if (ixl == ixr) {
            int cov = (int)((xr-xl)*255.0f);
            if ((unsigned)ixl < W && cov > row[ixl]) row[ixl] = (uint8_t)cov;
            continue;
        }
        int lcov = (int)((1.0f-(xl-ixl))*255.0f);
        int rcov = (int)((xr-ixr)*255.0f);
        if ((unsigned)ixl < W && lcov > row[ixl]) row[ixl] = (uint8_t)lcov;
        for (int x = ixl+1; x < ixr; x++) if ((unsigned)x < W) row[x] = 255;
        if ((unsigned)ixr < W && rcov > row[ixr]) row[ixr] = (uint8_t)rcov;
    }
}

void p3_render_shadow(const p3_model *mo, const p3_render_params *rp,
                      float ground_y, int darkness, uint8_t *mask)
{
    if (mo->nverts > P3_MAX_VERTS || !mask) return;
    fold_model(mo, rp, g_folded);
    float Rm[9]; m3_euler(rp->yaw, rp->pitch, rp->roll, Rm);

    float lx = rp->light.x, ly = rp->light.y, lz = rp->light.z;
    float ln = sqrtf(lx*lx+ly*ly+lz*lz); if (ln < 1e-6f) ln = 1.0f;
    lx/=ln; ly/=ln; lz/=ln;
    if (ly < 0.2f) ly = 0.2f;

    for (int i = 0; i < mo->nverts; i++) {
        p3_vec3 v = m3_apply(Rm, g_folded[i]);
        float wx = v.x+rp->ox, wy = v.y+rp->oy, wz = v.z+rp->oz;
        float s = (wy - ground_y) / ly;
        g_world[i].x = wx - s*lx;
        g_world[i].y = ground_y + 0.05f;
        g_world[i].z = wz - s*lz;
    }
    float Rc[9]; m3_euler(rp->cam_yaw, rp->cam_pitch, rp->cam_roll, Rc);
    p3_vec3 camrel = { -rp->cam_x, -rp->cam_y, -rp->cam_z };
    p3_vec3 t = m3_apply(Rc, camrel);
    project_all(mo, Rc, t, rp);

    /* screen bbox of the (visible) projected silhouette */
    int bx0 = W, by0 = H, bx1 = -1, by1 = -1;
    for (int i = 0; i < mo->nverts; i++) {
        if (!g_vis[i]) continue;
        int sx = (int)floorf(g_sx[i]), sy = (int)floorf(g_sy[i]);
        if (sx < bx0) bx0 = sx; if (sx > bx1) bx1 = sx;
        if (sy < by0) by0 = sy; if (sy > by1) by1 = sy;
    }
    if (bx1 < bx0 || by1 < by0) return;
    if (bx0 < 0) bx0 = 0; if (by0 < 0) by0 = 0;
    if (bx1 > W-1) bx1 = W-1; if (by1 > H-1) by1 = H-1;

    /* clear mask bbox, rasterise all facets as a union */
    for (int y = by0; y <= by1; y++) memset(&mask[y*W + bx0], 0, (size_t)(bx1-bx0+1));
    for (int f = 0; f < mo->nfaces; f++) {
        const p3_face *fc = &mo->faces[f];
        int n = (fc->i3 == P3_NO_VERT) ? 3 : 4;
        uint16_t vi[4] = { fc->i0, fc->i1, fc->i2, fc->i3 };
        int ok = 1;
        for (int k = 0; k < n; k++) if (!g_vis[vi[k]]) { ok = 0; break; }
        if (!ok) continue;
        float px[4], py[4], dx[4], dy[4];
        for (int k = 0; k < n; k++){ px[k]=g_sx[vi[k]]; py[k]=g_sy[vi[k]]; }
        dilate_poly(px, py, n, 0.75f, dx, dy);     /* union with no inner seams */
        mask_convex(mask, dx, dy, n);
    }

    /* darken the ground ONCE per covered pixel (uniform, AA outer edge) */
    if (darkness < 0) darkness = 0; else if (darkness > 255) darkness = 255;
    int keep0 = 256 - (darkness * 5 / 8);     /* full-coverage darkening */
    uint16_t *fb = vga_hires_back_buffer();
    for (int y = by0; y <= by1; y++) {
        const uint8_t *mr = &mask[y*W];
        uint16_t *fr = &fb[y*W];
        for (int x = bx0; x <= bx1; x++) {
            int m = mr[x];
            if (!m) continue;
            int keep = 256 - (((256-keep0) * m) >> 8);   /* scale by coverage */
            uint16_t c = fr[x];
            fr[x] = rgb565_pack((rgb565_r8(c)*keep)>>8, (rgb565_g8(c)*keep)>>8, (rgb565_b8(c)*keep)>>8);
        }
    }
}

/* ---- params + materials ---------------------------------------------- */

void p3_params_default(p3_render_params *rp)
{
    memset(rp, 0, sizeof *rp);
    rp->focal = 300.0f;
    rp->cx = 160.0f; rp->cy = 120.0f;
    rp->light.x = -0.4f; rp->light.y = 0.78f; rp->light.z = -0.48f;
    rp->ambient = 0.42f;
    rp->backface_cull = 1;
}

void p3_set_material(int m, int r, int g, int b)
{
    if ((unsigned)m >= P3_MAT_MAX) return;
    g_mat[m][0] = (uint8_t)r; g_mat[m][1] = (uint8_t)g; g_mat[m][2] = (uint8_t)b;
}
