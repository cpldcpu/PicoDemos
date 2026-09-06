/* Solid 3D as visible-boundary lists (an S-buffer). See s3d.h.
 *
 * Each row holds a sorted list of boundaries {x, colour}: from x onwards the
 * row shows colour, until the next boundary. S3D_EMPTY means "nothing drawn
 * here" and the floor (or whatever is under the object) shows through. A
 * face's span is inserted by clipping whatever it covers -- faces arrive far
 * to near, so the incoming span always wins -- and the list only ever holds
 * what will actually be visible. Twenty-four boundaries a row is enough for
 * every mesh here; when it is not, the incoming span is dropped and counted.
 *
 * MEASURED FIRST: a plain span list capped at 12 spans a row tore the torus
 * and the knot to shreds, because a row through a 600-face object crosses
 * twenty front faces and the ones dropped were the nearest. Storing only what
 * survives occlusion is both smaller and correct.
 */

#include "s3d.h"
#include "arena.h"
#include "persistence.h"
#include "rgb565.h"

#include <math.h>
#include <string.h>

#define S3D_EMPTY 0xFFFFu     /* bit 5 is never set by rgb565_pack, so this is not a colour */

static seg_t   *s_segs[2];                  /* [480][S3D_MAX_SEGS]              */
static uint8_t  s_count[2][PV_H];
static uint32_t s_overflow, s_faces;
/* Rows that lost a sliver at least once this frame. The raw event count is a
 * bad metric: once a row is full, EVERY further insert into it merges, so a
 * single busy row can contribute dozens and the number says more about how
 * many faces cross that row than about how much of the picture is affected.
 * What matters is how many of the 480 rows are touched at all. */
static uint8_t  s_row_dirty[PV_H];
static uint32_t s_rows_merged;

/* per-object scratch (core 0 only) */
#define MAXV 1024
#define MAXT 2048
static float    s_vx[MAXV], s_vy[MAXV], s_vz[MAXV];   /* camera space         */
static float    s_sx[MAXV], s_sy[MAXV];               /* screen               */
static uint16_t s_bucket_head[256];
static uint16_t s_bucket_next[MAXT];
static uint16_t s_col[MAXT];

void s3d_init(void)
{
    s_segs[0] = (seg_t *)ARENA(ARENA_SPANS_OFF);
    s_segs[1] = s_segs[0] + PV_H * S3D_MAX_SEGS;
    memset(s_count, 0, sizeof s_count);
}

void s3d_begin(uint32_t parity)
{
    memset(s_count[parity & 1], 0, PV_H);
    memset(s_row_dirty, 0, sizeof s_row_dirty);
    s_rows_merged = 0;
}

uint32_t s3d_rows_merged(void) { return s_rows_merged; }

uint32_t s3d_merges(void)     { return s_overflow; }
uint32_t s3d_last_faces(void) { return s_faces; }

void s3d_rot(float m[3][3], float ax, float ay, float az)
{
    const float cx = cosf(ax), sx = sinf(ax), cy = cosf(ay), sy = sinf(ay), cz = cosf(az), sz = sinf(az);
    m[0][0] = cz * cy;  m[0][1] = cz * sy * sx - sz * cx;  m[0][2] = cz * sy * cx + sz * sx;
    m[1][0] = sz * cy;  m[1][1] = sz * sy * sx + cz * cx;  m[1][2] = sz * sy * cx - cz * sx;
    m[2][0] = -sy;      m[2][1] = cy * sx;                 m[2][2] = cy * cx;
}

/* Insert [a, b) with colour c into row y's boundary list. */
static void insert_span(seg_t *row, uint8_t *cnt, int a, int b, uint16_t c, int y)
{
    if (a < 0) a = 0;
    if (b > PV_W) b = PV_W;
    if (a >= b) return;
    int n = *cnt;

    /* i0: first boundary with x >= a;  i1: first boundary with x >= b */
    int i0 = 0; while (i0 < n && row[i0].x < a) i0++;
    int i1 = i0; while (i1 < n && row[i1].x < b) i1++;

    /* colour in effect just before b (what continues after our span) */
    uint16_t after = (i1 > 0) ? row[i1 - 1].c : (uint16_t)S3D_EMPTY;
    const int b_exists = (i1 < n && row[i1].x == b);
    const int need = 1 + (b_exists ? 0 : 1);

    int newn = n - (i1 - i0) + need;
    if (newn > S3D_MAX_SEGS) {
        /* The row is full. Faces arrive far to near, so the span being
         * inserted is the NEAREST thing on this row -- dropping it punches a
         * hole straight through a solid object and shows the floor. Instead,
         * throw away the narrowest run already in the list and let its
         * left-hand neighbour cover the gap. That costs a few pixels of the
         * wrong colour on the thinnest sliver in the row, which is invisible,
         * and it keeps the near surface, which is not.
         *
         * Doing it this way is also what makes the limit a tuning number
         * rather than a correctness one: the picture stays right as the count
         * is lowered, it just loses fine slivers first. */
        s_overflow++;
        if (!s_row_dirty[y]) { s_row_dirty[y] = 1; s_rows_merged++; }
        int victim = 1, best = PV_W + 1;
        for (int k = 1; k + 1 < n; k++) {
            const int w = row[k + 1].x - row[k].x;
            if (w < best) { best = w; victim = k; }
        }
        if (n < 3) return;                       /* nothing safe to give up */
        memmove(row + victim, row + victim + 1, (size_t)(n - victim - 1) * sizeof(seg_t));
        n--;
        *cnt = (uint8_t)n;
        /* recompute the insertion point against the shortened list */
        i0 = 0; while (i0 < n && row[i0].x < a) i0++;
        i1 = i0; while (i1 < n && row[i1].x < b) i1++;
        after = (i1 > 0) ? row[i1 - 1].c : (uint16_t)S3D_EMPTY;
        const int be = (i1 < n && row[i1].x == b);
        const int nd = 1 + (be ? 0 : 1);
        newn = n - (i1 - i0) + nd;
        if (newn > S3D_MAX_SEGS) return;         /* still full: give up quietly */
        if (i1 - i0 != nd) memmove(row + i0 + nd, row + i1, (size_t)(n - i1) * sizeof(seg_t));
        row[i0].x = (uint16_t)a; row[i0].c = c;
        if (!be) { row[i0 + 1].x = (uint16_t)b; row[i0 + 1].c = after; }
        *cnt = (uint8_t)newn;
        return;
    }

    /* close the gap: move the tail to make room for `need` entries at i0 */
    if (i1 - i0 != need) memmove(row + i0 + need, row + i1, (size_t)(n - i1) * sizeof(seg_t));
    row[i0].x = (uint16_t)a; row[i0].c = c;
    if (!b_exists) { row[i0 + 1].x = (uint16_t)b; row[i0 + 1].c = after; }
    *cnt = (uint8_t)newn;
}

/* Scan-convert a screen triangle into row spans of colour c. */
static void raster_tri(uint32_t parity, float x0, float y0, float x1, float y1, float x2, float y2,
                       uint16_t c, int ymin_ok, int ymax_ok)
{
    float t;
    if (y1 < y0) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 < y0) { t = x0; x0 = x2; x2 = t; t = y0; y0 = y2; y2 = t; }
    if (y2 < y1) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }

    int ya = (int)ceilf(y0 - 0.5f), yb = (int)ceilf(y2 - 0.5f);      /* rows whose centre is inside */
    if (ya < ymin_ok) ya = ymin_ok;
    if (yb > ymax_ok) yb = ymax_ok;
    if (ya >= yb) return;

    const float h02 = y2 - y0; if (h02 < 1e-4f) return;
    const float s02 = (x2 - x0) / h02;
    const float h01 = y1 - y0, s01 = h01 > 1e-4f ? (x1 - x0) / h01 : 0.0f;
    const float h12 = y2 - y1, s12 = h12 > 1e-4f ? (x2 - x1) / h12 : 0.0f;

    seg_t   *rows = s_segs[parity & 1];
    uint8_t *cnt  = s_count[parity & 1];

    for (int y = ya; y < yb; y++) {
        const float yc = (float)y + 0.5f;
        const float xl = x0 + (yc - y0) * s02;
        float xs;
        if (yc < y1) { if (h01 < 1e-4f) continue; xs = x0 + (yc - y0) * s01; }
        else         { if (h12 < 1e-4f) continue; xs = x1 + (yc - y1) * s12; }
        int a, b;
        if (xl < xs) { a = (int)ceilf(xl - 0.5f); b = (int)ceilf(xs - 0.5f); }
        else         { a = (int)ceilf(xs - 0.5f); b = (int)ceilf(xl - 0.5f); }
        insert_span(rows + y * S3D_MAX_SEGS, &cnt[y], a, b, c, y);
    }
}

void s3d_object(uint32_t parity, const mesh_t *m, const s3d_xform_t *xf,
                const s3d_material_t *mat, const s3d_view_t *view)
{
    const int nv = m->nv < MAXV ? m->nv : MAXV;
    const int nt = m->nt < MAXT ? m->nt : MAXT;

    for (int i = 0; i < nv; i++) {
        const float x = m->v[i * 3], y = m->v[i * 3 + 1], z = m->v[i * 3 + 2];
        const float cx = (xf->m[0][0] * x + xf->m[0][1] * y + xf->m[0][2] * z) * xf->scale + xf->tx;
        const float cy = (xf->m[1][0] * x + xf->m[1][1] * y + xf->m[1][2] * z) * xf->scale + xf->ty;
        const float cz = (xf->m[2][0] * x + xf->m[2][1] * y + xf->m[2][2] * z) * xf->scale + xf->tz;
        s_vx[i] = cx; s_vy[i] = cy; s_vz[i] = cz;
        const float iz = cz > 1.0f ? view->F / cz : view->F;
        float sx = (float)view->cx + cx * iz, sy = (float)view->cy + cy * iz;
        if (sx < -4096.0f) sx = -4096.0f;
        if (sx > 4096.0f) sx = 4096.0f;
        if (sy < -4096.0f) sy = -4096.0f;
        if (sy > 4096.0f) sy = 4096.0f;
        s_sx[i] = sx; s_sy[i] = sy;
    }

    float zmin = 1e9f, zmax = -1e9f;
    for (int i = 0; i < nv; i++) { if (s_vz[i] < zmin) zmin = s_vz[i]; if (s_vz[i] > zmax) zmax = s_vz[i]; }
    const float zscale = (zmax > zmin) ? 255.0f / (zmax - zmin) : 0.0f;
    for (int b = 0; b < 256; b++) s_bucket_head[b] = 0xFFFF;

    /* half vector between the light and the viewer (0,0,-1), once */
    float hx = mat->lx, hy = mat->ly, hz = mat->lz - 1.0f;
    const float hl = sqrtf(hx * hx + hy * hy + hz * hz);
    if (hl > 0.0f) { hx /= hl; hy /= hl; hz /= hl; }
    const float amb = mat->amb / 255.0f, spk = mat->spec / 255.0f, dim = mat->dim / 256.0f;

    int visible = 0;
    for (int t = 0; t < nt; t++) {
        const int i0 = m->t[t * 3], i1 = m->t[t * 3 + 1], i2 = m->t[t * 3 + 2];
        if (s_vz[i0] < 1.0f || s_vz[i1] < 1.0f || s_vz[i2] < 1.0f) continue;
        const float ax = s_sx[i1] - s_sx[i0], ay = s_sy[i1] - s_sy[i0];
        const float bx = s_sx[i2] - s_sx[i0], by = s_sy[i2] - s_sy[i0];
        if (ax * by - ay * bx >= 0.0f) continue;          /* back face (y-down screen) */

        const float ex = s_vx[i1] - s_vx[i0], ey = s_vy[i1] - s_vy[i0], ez = s_vz[i1] - s_vz[i0];
        const float fx = s_vx[i2] - s_vx[i0], fy = s_vy[i2] - s_vy[i0], fz = s_vz[i2] - s_vz[i0];
        float nx = ey * fz - ez * fy, ny = ez * fx - ex * fz, nz = ex * fy - ey * fx;
        const float nl = sqrtf(nx * nx + ny * ny + nz * nz);
        if (nl > 0.0f) { nx /= nl; ny /= nl; nz /= nl; }
        /* the face normal must point at the viewer; flip if the winding says otherwise */
        const float cxm = s_vx[i0] + s_vx[i1] + s_vx[i2], cym = s_vy[i0] + s_vy[i1] + s_vy[i2], czm = s_vz[i0] + s_vz[i1] + s_vz[i2];
        if (nx * cxm + ny * cym + nz * czm > 0.0f) { nx = -nx; ny = -ny; nz = -nz; }

        float ndl = nx * mat->lx + ny * mat->ly + nz * mat->lz; if (ndl < 0.0f) ndl = 0.0f;
        float ndh = nx * hx + ny * hy + nz * hz;                if (ndh < 0.0f) ndh = 0.0f;
        float spec = ndh * ndh; spec *= spec; spec *= spec; spec *= spec;            /* ^16 */
        const float lum = (amb + (1.0f - amb) * ndl) * dim, sp = spec * spk * 255.0f * dim;
        s_col[t] = rgb565_pack((int)(mat->r * lum + sp), (int)(mat->g * lum + sp), (int)(mat->b * lum + sp));

        int bk = (int)((czm * (1.0f / 3.0f) - zmin) * zscale);
        if (bk < 0) bk = 0;
        if (bk > 255) bk = 255;
        s_bucket_next[t] = s_bucket_head[bk];
        s_bucket_head[bk] = (uint16_t)t;
        visible++;
    }
    s_faces = (uint32_t)visible;

    for (int bk = 255; bk >= 0; bk--) {
        for (uint16_t t = s_bucket_head[bk]; t != 0xFFFF; t = s_bucket_next[t]) {
            const int i0 = m->t[t * 3], i1 = m->t[t * 3 + 1], i2 = m->t[t * 3 + 2];
            raster_tri(parity, s_sx[i0], s_sy[i0], s_sx[i1], s_sy[i1], s_sx[i2], s_sy[i2],
                       s_col[t], view->y_min, view->y_max);
        }
    }
}

void PV_HOT(s3d_line)(uint32_t parity, uint16_t *px, int y)
{
    const int n = s_count[parity & 1][y];
    if (!n) return;
    const seg_t *s = s_segs[parity & 1] + y * S3D_MAX_SEGS;
    for (int i = 0; i + 1 < n; i++) {
        const uint16_t c = s[i].c;
        if (c == S3D_EMPTY) continue;
        int x0 = s[i].x;
        const int x1 = s[i + 1].x;
        if (x0 & 1) { px[x0] = c; x0++; }
        const uint32_t cc = (uint32_t)c | ((uint32_t)c << 16);
        uint32_t *w = (uint32_t *)(px + x0);
        int nw = (x1 - x0) >> 1;
        while (nw >= 4) { w[0] = cc; w[1] = cc; w[2] = cc; w[3] = cc; w += 4; nw -= 4; }
        while (nw-- > 0) *w++ = cc;
        if ((x1 - x0) & 1) px[x1 - 1] = c;
    }
}
