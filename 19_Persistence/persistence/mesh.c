#include "mesh.h"
#include "arena.h"

#include <math.h>
#include <string.h>

static uint8_t *s_pool;
static uint32_t s_used;

void mesh_pool_reset(void)
{
    s_pool = (uint8_t *)ARENA(ARENA_MESH_OFF);
    s_used = 0;
}

uint32_t mesh_pool_used(void) { return s_used; }

static void *alloc(uint32_t bytes)
{
    bytes = (bytes + 3u) & ~3u;
    if (s_used + bytes > ARENA_MESH_SIZE) return 0;
    void *p = s_pool + s_used;
    s_used += bytes;
    return p;
}

static int reserve(mesh_t *m, int nv, int nt)
{
    m->nv = nv; m->nt = nt;
    m->v = (float *)alloc((uint32_t)nv * 3u * sizeof(float));
    m->t = (uint16_t *)alloc((uint32_t)nt * 3u * sizeof(uint16_t));
    return m->v && m->t;
}

/* ------------------------------------------------------------ icosphere -- */

static int find_or_add(float *v, int *nv, float x, float y, float z)
{
    for (int i = 0; i < *nv; i++) {
        const float dx = v[i * 3] - x, dy = v[i * 3 + 1] - y, dz = v[i * 3 + 2] - z;
        if (dx * dx + dy * dy + dz * dz < 1e-6f) return i;
    }
    v[*nv * 3] = x; v[*nv * 3 + 1] = y; v[*nv * 3 + 2] = z;
    return (*nv)++;
}

int mesh_icosphere(mesh_t *m, int subdiv)
{
    const float t = (1.0f + sqrtf(5.0f)) * 0.5f;
    const float base_v[12][3] = {
        {-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0},
        {0, -1, t}, {0, 1, t}, {0, -1, -t}, {0, 1, -t},
        {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1} };
    static const uint16_t base_t[20][3] = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1} };

    int nt = 20; for (int s = 0; s < subdiv; s++) nt *= 4;
    const int nv_max = nt / 2 + 2;

    /* The triangle array must hold the OLD list and the NEW one at once while
     * a level is being subdivided: cur + 4*cur entries at the last step, which
     * is nt + nt/4. Sizing it at nt exactly overran into the next mesh's
     * vertices -- silently, because the result is compacted back afterwards. */
    if (!reserve(m, nv_max, nt + nt / 4 + 4)) return 0;
    m->nt = nt;

    int nv = 0;
    for (int i = 0; i < 12; i++) {
        const float l = sqrtf(base_v[i][0] * base_v[i][0] + base_v[i][1] * base_v[i][1] + base_v[i][2] * base_v[i][2]);
        find_or_add(m->v, &nv, base_v[i][0] / l, base_v[i][1] / l, base_v[i][2] / l);
    }
    int cur = 20;
    memcpy(m->t, base_t, sizeof base_t);

    for (int s = 0; s < subdiv; s++) {
        int next = 0;
        uint16_t *out = m->t + cur * 3;
        for (int i = 0; i < cur; i++) {
            const int a = m->t[i * 3], b = m->t[i * 3 + 1], c = m->t[i * 3 + 2];
            float mx, my, mz, l;
            mx = (m->v[a * 3] + m->v[b * 3]) * 0.5f; my = (m->v[a * 3 + 1] + m->v[b * 3 + 1]) * 0.5f; mz = (m->v[a * 3 + 2] + m->v[b * 3 + 2]) * 0.5f;
            l = sqrtf(mx * mx + my * my + mz * mz); const int ab = find_or_add(m->v, &nv, mx / l, my / l, mz / l);
            mx = (m->v[b * 3] + m->v[c * 3]) * 0.5f; my = (m->v[b * 3 + 1] + m->v[c * 3 + 1]) * 0.5f; mz = (m->v[b * 3 + 2] + m->v[c * 3 + 2]) * 0.5f;
            l = sqrtf(mx * mx + my * my + mz * mz); const int bc = find_or_add(m->v, &nv, mx / l, my / l, mz / l);
            mx = (m->v[c * 3] + m->v[a * 3]) * 0.5f; my = (m->v[c * 3 + 1] + m->v[a * 3 + 1]) * 0.5f; mz = (m->v[c * 3 + 2] + m->v[a * 3 + 2]) * 0.5f;
            l = sqrtf(mx * mx + my * my + mz * mz); const int ca = find_or_add(m->v, &nv, mx / l, my / l, mz / l);
            out[next * 3] = (uint16_t)a;  out[next * 3 + 1] = (uint16_t)ab; out[next * 3 + 2] = (uint16_t)ca; next++;
            out[next * 3] = (uint16_t)b;  out[next * 3 + 1] = (uint16_t)bc; out[next * 3 + 2] = (uint16_t)ab; next++;
            out[next * 3] = (uint16_t)c;  out[next * 3 + 1] = (uint16_t)ca; out[next * 3 + 2] = (uint16_t)bc; next++;
            out[next * 3] = (uint16_t)ab; out[next * 3 + 1] = (uint16_t)bc; out[next * 3 + 2] = (uint16_t)ca; next++;
        }
        memmove(m->t, out, (size_t)next * 3 * sizeof(uint16_t));
        cur = next;
    }
    m->nv = nv; m->nt = cur;
    return 1;
}

/* ---------------------------------------------------------------- torus --
 *
 * Winding: d/di is the major direction and d/dj the minor one, and
 * cross(d/di, d/dj) points INWARD (checked by hand at the outer equator), so
 * the outward-facing triangles are (a, c, b) and (a, d, c). Getting this
 * backwards does not produce an invisible object -- it produces one whose
 * near surface is culled and whose far inner surface is drawn, which reads as
 * a torn mesh rather than as an obvious error. */

int mesh_torus(mesh_t *m, int nu, int nv, float R, float r)
{
    if (!reserve(m, nu * nv, nu * nv * 2)) return 0;
    for (int i = 0; i < nu; i++) {
        const float a = (float)i * 6.2831853f / nu, ca = cosf(a), sa = sinf(a);
        for (int j = 0; j < nv; j++) {
            const float b = (float)j * 6.2831853f / nv, cb = cosf(b), sb = sinf(b);
            float *v = m->v + (i * nv + j) * 3;
            v[0] = (R + r * cb) * ca; v[1] = r * sb; v[2] = (R + r * cb) * sa;
        }
    }
    int t = 0;
    for (int i = 0; i < nu; i++) for (int j = 0; j < nv; j++) {
        const int i1 = (i + 1) % nu, j1 = (j + 1) % nv;
        const uint16_t a = (uint16_t)(i * nv + j), b = (uint16_t)(i1 * nv + j);
        const uint16_t c = (uint16_t)(i1 * nv + j1), d = (uint16_t)(i * nv + j1);
        m->t[t * 3] = a; m->t[t * 3 + 1] = c; m->t[t * 3 + 2] = b; t++;
        m->t[t * 3] = a; m->t[t * 3 + 1] = d; m->t[t * 3 + 2] = c; t++;
    }
    return 1;
}

/* ----------------------------------------------------------------- knot --
 *
 * A (p,q) torus knot, tubed. The frame around the tube is NOT a Frenet frame
 * and not a fixed-reference cross product: both flip where the tangent swings
 * past the reference, which twists the tube and tears it at the seam.
 *
 * The curve lies on a torus, so the frame comes from the geometry for free:
 * the vector from the ambient torus's central circle to the curve point is a
 * unit vector, smooth, and periodic with the curve. That closes the tube with
 * no holonomy mismatch, which no propagated frame does on a closed loop. */

int mesh_knot(mesh_t *m, int p, int q, int segs, int tube, float R, float r)
{
    if (!reserve(m, segs * tube, segs * tube * 2)) return 0;
    for (int i = 0; i < segs; i++) {
        const float u  = (float)i * 6.2831853f / segs;
        const float u2 = u + 0.01f;
        const float cq = cosf(q * u), sq = sinf(q * u), cp = cosf(p * u), sp = sinf(p * u);
        const float cx = (2.0f + cq) * cp, cy = sq, cz = (2.0f + cq) * sp;
        const float nx2 = (2.0f + cosf(q * u2)) * cosf(p * u2), ny2 = sinf(q * u2), nz2 = (2.0f + cosf(q * u2)) * sinf(p * u2);

        float tx = nx2 - cx, ty = ny2 - cy, tz = nz2 - cz;
        const float tl = sqrtf(tx * tx + ty * ty + tz * tz);
        tx /= tl; ty /= tl; tz /= tl;

        /* radial: curve point minus the ambient torus's central circle (2*cp, 0, 2*sp) */
        float bx = cq * cp, by = sq, bz = cq * sp;
        /* orthogonalise against the tangent, then complete the frame */
        const float dot = bx * tx + by * ty + bz * tz;
        bx -= dot * tx; by -= dot * ty; bz -= dot * tz;
        const float bl = sqrtf(bx * bx + by * by + bz * bz);
        bx /= bl; by /= bl; bz /= bl;
        const float ex = ty * bz - tz * by, ey = tz * bx - tx * bz, ez = tx * by - ty * bx;

        for (int j = 0; j < tube; j++) {
            const float a = (float)j * 6.2831853f / tube, ca = cosf(a), sa = sinf(a);
            float *v = m->v + (i * tube + j) * 3;
            v[0] = cx * R * 0.5f + r * (bx * ca + ex * sa);
            v[1] = cy * R * 0.5f + r * (by * ca + ey * sa);
            v[2] = cz * R * 0.5f + r * (bz * ca + ez * sa);
        }
    }
    /* Same handedness question as the torus: d/di along the curve crossed with
     * d/dj around the tube points at the curve, i.e. inward. So (a, c, b). */
    int t = 0;
    for (int i = 0; i < segs; i++) for (int j = 0; j < tube; j++) {
        const int i1 = (i + 1) % segs, j1 = (j + 1) % tube;
        const uint16_t a = (uint16_t)(i * tube + j), b = (uint16_t)(i1 * tube + j);
        const uint16_t c = (uint16_t)(i1 * tube + j1), d = (uint16_t)(i * tube + j1);
        m->t[t * 3] = a; m->t[t * 3 + 1] = c; m->t[t * 3 + 2] = b; t++;
        m->t[t * 3] = a; m->t[t * 3 + 1] = d; m->t[t * 3 + 2] = c; t++;
    }
    return 1;
}

/* ------------------------------------------------------------------ gem -- */

int mesh_gem(mesh_t *m, int sides)
{
    if (!reserve(m, sides * 2 + 2, sides * 4)) return 0;
    for (int i = 0; i < sides; i++) {
        const float a = (float)i * 6.2831853f / sides, ca = cosf(a), sa = sinf(a);
        const float a2 = a + 3.1415926f / sides, cb = cosf(a2), sb = sinf(a2);
        m->v[i * 3] = ca;                  m->v[i * 3 + 1] = 0.25f;             m->v[i * 3 + 2] = sa;
        m->v[(sides + i) * 3] = cb * 0.8f; m->v[(sides + i) * 3 + 1] = -0.35f;  m->v[(sides + i) * 3 + 2] = sb * 0.8f;
    }
    const int top = sides * 2, bot = sides * 2 + 1;
    m->v[top * 3] = 0; m->v[top * 3 + 1] = 0.55f; m->v[top * 3 + 2] = 0;
    m->v[bot * 3] = 0; m->v[bot * 3 + 1] = -1.3f; m->v[bot * 3 + 2] = 0;
    int t = 0;
    for (int i = 0; i < sides; i++) {
        const uint16_t a = (uint16_t)i, b = (uint16_t)((i + 1) % sides);
        const uint16_t c = (uint16_t)(sides + i), d = (uint16_t)(sides + (i + 1) % sides);
        m->t[t * 3] = (uint16_t)top; m->t[t * 3 + 1] = a; m->t[t * 3 + 2] = b; t++;    /* crown    */
        m->t[t * 3] = a;             m->t[t * 3 + 1] = c; m->t[t * 3 + 2] = b; t++;    /* girdle   */
        m->t[t * 3] = b;             m->t[t * 3 + 1] = c; m->t[t * 3 + 2] = d; t++;
        m->t[t * 3] = c;             m->t[t * 3 + 1] = (uint16_t)bot; m->t[t * 3 + 2] = d; t++; /* pavilion */
    }
    return 1;
}

/* ------------------------------------------------------------------ bar -- */

int mesh_box(mesh_t *m, float hx, float hy, float hz)
{
    if (!reserve(m, 8, 12)) return 0;
    const float vs[8][3] = { {-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1} };
    for (int i = 0; i < 8; i++) { m->v[i*3] = vs[i][0]*hx; m->v[i*3+1] = vs[i][1]*hy; m->v[i*3+2] = vs[i][2]*hz; }
    static const uint16_t ts[12][3] = {
        {0,2,1},{0,3,2},  {4,5,6},{4,6,7},
        {0,1,5},{0,5,4},  {2,3,7},{2,7,6},
        {1,2,6},{1,6,5},  {0,4,7},{0,7,3} };
    memcpy(m->t, ts, sizeof ts);
    return 1;
}
