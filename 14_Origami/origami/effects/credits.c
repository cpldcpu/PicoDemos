/* Scene 5 — CONFETTI CREDITS (1:53.01–2:14.96, MODE_320).
 *
 * The world bursts into a gentle rain of tumbling paper confetti (each a
 * free quad, painter-sorted by depth, flat-shaded as it spins) over a warm
 * paper endcard, while the credits scroll. A quiet, tactile outro.
 */

#include "scene.h"
#include "vga.h"
#include "poly3d.h"
#include "origami_fx.h"
#include "scene_scratch.h"
#include <math.h>

#define SCENE_LEN_MS 21950

static uint32_t rng = 0x1234abcdu;
static inline uint32_t xr(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static inline float frand(float a, float b){ return a + (b-a)*((xr()>>8)/16777216.0f); }

static const char *CREDITS[] = {
    "ORIGAMI", "",
    "A FOLDED PAPER WORLD", "FOR THE RP2350", "",
    "FLAT-SHADED POLYGON 3D", "FOLD - LAMBERT - PAINTER", "",
    "CODE AND DIRECTION", "CLAUDE OPUS 4.8", "",
    "MUSIC", "MARIMBA SEEDBOX", "SUNO 4.5 - PROMPTED BY CLAUDE", "",
    "GREETINGS TO THE SCENE", "", "2026",
};
#define NCREDITS ((int)(sizeof(CREDITS)/sizeof(CREDITS[0])))

static void seed_piece(int b, int from_top)
{
    g_scratch.confetti.c[b].px = frand(-230, 230);
    g_scratch.confetti.c[b].py = from_top ? frand(180, 320) : frand(-170, 200);
    g_scratch.confetti.c[b].pz = frand(220, 520);
    g_scratch.confetti.c[b].vx = frand(-22, 22);
    g_scratch.confetti.c[b].vy = frand(-30, 40);
    g_scratch.confetti.c[b].vz = frand(-12, 12);
    float ax = frand(-1,1), ay = frand(-1,1), az = frand(-1,1);
    float l = sqrtf(ax*ax+ay*ay+az*az); if (l<1e-3f){ax=0;ay=1;az=0;l=1;}
    g_scratch.confetti.c[b].ax = ax/l; g_scratch.confetti.c[b].ay = ay/l; g_scratch.confetti.c[b].az = az/l;
    g_scratch.confetti.c[b].ang  = frand(0, 6.28f);
    g_scratch.confetti.c[b].angv = frand(-3.0f, 3.0f);
    g_scratch.confetti.c[b].half = frand(6, 13);
    g_scratch.confetti.c[b].mat  = (uint8_t)(xr() % 6);   /* any paper colour */
}

static void credits_init(void)
{
    og_materials();
    rng = 0x1234abcdu;
    for (int b = 0; b < CONFETTI_N; b++) {
        seed_piece(b, 0);
        g_scratch.confetti.f[b] = (p3_face){
            (uint16_t)(b*4+0),(uint16_t)(b*4+1),(uint16_t)(b*4+2),(uint16_t)(b*4+3),
            g_scratch.confetti.c[b].mat, P3_FACE_DOUBLE_SIDED };
    }
}

/* rotate local corner about unit axis (ax,ay,az) by ang (Rodrigues) */
static inline p3_vec3 tumble(float lx, float ly, float ax, float ay, float az,
                             float ca, float sa)
{
    /* local corner p = (lx,ly,0) */
    float omc = 1.0f - ca;
    float kd  = ax*lx + ay*ly;                 /* k . p (pz=0) */
    float cxp = ay*0.0f - az*ly;               /* (k x p).x */
    float cyp = az*lx  - ax*0.0f;              /* (k x p).y */
    float czp = ax*ly  - ay*lx;                /* (k x p).z */
    p3_vec3 o;
    o.x = lx*ca + cxp*sa + ax*kd*omc;
    o.y = ly*ca + cyp*sa + ay*kd*omc;
    o.z = 0.0f*ca + czp*sa + az*kd*omc;
    return o;
}

static void credits_frame(uint32_t t_into, uint32_t t_global)
{
    const float dt = 1.0f/60.0f, G = 42.0f;

    /* update + build confetti world quads */
    p3_vec3 *V = g_scratch.confetti.v;
    for (int b = 0; b < CONFETTI_N; b++) {
        typeof(g_scratch.confetti.c[b]) *c = &g_scratch.confetti.c[b];
        c->vy -= G * dt;
        c->px += c->vx * dt + sinf(c->ang) * 0.5f;   /* flutter */
        c->py += c->vy * dt;
        c->pz += c->vz * dt;
        c->ang += c->angv * dt;
        if (c->py < -175.0f) seed_piece(b, 1);

        float ca = cosf(c->ang), sa = sinf(c->ang), h = c->half;
        p3_vec3 q0 = tumble(-h,-h, c->ax,c->ay,c->az, ca,sa);
        p3_vec3 q1 = tumble( h,-h, c->ax,c->ay,c->az, ca,sa);
        p3_vec3 q2 = tumble( h, h, c->ax,c->ay,c->az, ca,sa);
        p3_vec3 q3 = tumble(-h, h, c->ax,c->ay,c->az, ca,sa);
        V[b*4+0] = (p3_vec3){ c->px+q0.x, c->py+q0.y, c->pz+q0.z };
        V[b*4+1] = (p3_vec3){ c->px+q1.x, c->py+q1.y, c->pz+q1.z };
        V[b*4+2] = (p3_vec3){ c->px+q2.x, c->py+q2.y, c->pz+q2.z };
        V[b*4+3] = (p3_vec3){ c->px+q3.x, c->py+q3.y, c->pz+q3.z };
        g_scratch.confetti.f[b].material = c->mat;
    }

    /* warm paper endcard backdrop */
    og_sky_grad(232,222,200, 246,238,218);

    p3_render_params rp; p3_params_default(&rp);
    rp.focal = 300.0f;
    rp.ambient = 0.45f;
    rp.backface_cull = 0;
    p3_model cm = { g_scratch.confetti.v, CONFETTI_N*4,
                    g_scratch.confetti.f, CONFETTI_N, 0, 0 };
    p3_render(&cm, &rp);

    /* credits scroll up through the lower-middle, paced to last the outro */
    if (t_into > 800) {
        float scroll = (t_into - 800) * 0.0238f;
        float y = 252.0f - scroll;
        for (int i = 0; i < NCREDITS; i++, y += 17.0f) {
            if (!CREDITS[i][0]) continue;
            if (y < -10 || y > 240) continue;
            if (i == 0) og_logo_centred(CREDITS[i], (int)y, 30);   /* the wordmark */
            else        og_text_centred(CREDITS[i], (int)y, 1);
        }
    }
    (void)t_global;
}

static void credits_done(void) {}

const effect_t fx_credits_real = {
    .name = "credits", .mode = MODE_HIRES,
    .init = credits_init, .frame = credits_frame, .done = credits_done,
};
