/* render.c -- the dispatcher, the textures and the frame context.
 *
 * demo_render() reads the timetable -- song_cut(), song_bar(), song_speed(),
 * song_distance() -- builds the frame context F, draws the shot in force,
 * then the board if the shot is the board. There is no state between frames
 * and no `t` that is not the sample: F is a pure function of it, and every
 * system below reads only F. That is what lets check.c call the renderer for
 * 4,801 samples in any order and get the same page each time.
 *
 * PLANNING §7's budget is 4.5 M cycles a frame at 300 MHz with the score on
 * core 1. demo_stats() carries the split -- prepare, world, lights, board,
 * other, and the light and span counts -- so a shot that goes over is a
 * named shot and not a number.  -- Overscan
 */
#include "render.h"
#include "accelerator.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef PICO_BUILD
#include "pico/time.h"
#include "hardware/structs/systick.h"
static uint32_t stamp(void) { return time_us_32(); }
static void prof_init(void) { systick_hw->rvr = 0x00FFFFFFu; systick_hw->cvr = 0; systick_hw->csr = 0x5u; }
#else
static uint32_t stamp(void) { return 0; }
static void prof_init(void) { }
#endif

/* ---------------------------------------------------------------- textures */

uint8_t  tex_rail[TEX_SIZE * TEX_SIZE];
uint8_t  tex_ring[TEX_SIZE * TEX_SIZE];
uint8_t  tex_rail_avg[TEX_SIZE];
uint16_t tex_pal[16][256];
uint16_t tex_pal_warm[16][256];

void textures_init(void)
{
    /* The track bed: ballast, and a sleeper every four texels -- 32 to a
     * beat, which is the timetable's own number. The two rails are NOT here;
     * they are analytic spans in world_ahead.c, because a rail head one
     * texel wide is the first thing to alias and Phase's trap is exactly
     * that. What is here is the part that benefits from filtering by
     * averaging rather than by sampling. */
    for (int v = 0; v < TEX_SIZE; v++) {
        const int on_sleeper = (v & 3) < 2;
        for (int u = 0; u < TEX_SIZE; u++) {
            const uint32_t h = rhash((uint32_t)(v * 137 + u * 71));
            int c;
            if (u < 10 || u > TEX_SIZE - 11) {
                c = 26 + (int)(h % 18u);                    /* the shoulder */
            } else if (on_sleeper && u > 20 && u < TEX_SIZE - 21) {
                c = 116 + (int)(h % 26u) + (v & 1 ? 10 : 0); /* the sleeper */
            } else {
                c = 44 + (int)(h % 34u);                     /* the ballast */
            }
            tex_rail[v * TEX_SIZE + u] = (uint8_t)c;
        }
    }
    for (int u = 0; u < TEX_SIZE; u++) {
        unsigned s = 0;
        for (int v = 0; v < TEX_SIZE; v++) s += tex_rail[v * TEX_SIZE + u];
        tex_rail_avg[u] = (uint8_t)(s / TEX_SIZE);
    }
    /* The tunnel lining: concrete with a ring joint every sixteen texels of
     * depth and bolt marks down the segments. */
    for (int v = 0; v < TEX_SIZE; v++)
        for (int u = 0; u < TEX_SIZE; u++) {
            const uint32_t h = rhash((uint32_t)(v * 313 + u * 977) ^ 0x51EDu);
            int c = 62 + (int)(h % 30u);
            if ((v & 15) < 2) c = 22 + (int)(h % 12u);       /* the joint   */
            if ((u & 31) == 0) c = c * 3 / 4;                /* a segment   */
            if (((u & 31) == 16) && ((v & 15) == 8)) c = 150; /* a bolt     */
            tex_ring[v * TEX_SIZE + u] = (uint8_t)c;
        }
    /* Sixteen depth shades of one cool concrete ramp, and the same ramp in
     * sodium for the dream. The compression matters more than the hue: at
     * full range a lit sleeper lands on five-bit (13,13,15) and the track
     * bed becomes the brightest thing in a frame whose whole rule is that
     * one lamp wins. Compressed, ballast sits at (2,2,2) and a sleeper at
     * (4,4,6) -- a ladder you can read, under the lamps rather than over
     * them, with the rail heads at (11,12,13) the brightest thing on the
     * ground. Built once; the tunnel and the plane are grey in every shot
     * they appear in and the dream is warm in all of its. */
    for (int p = 0; p < 16; p++) {
        const int f = p + 1;
        for (int i = 0; i < 256; i++) {
            tex_pal[p][i]      = rgb(i * 28 / 100 * f / 16, i * 30 / 100 * f / 16, i * 38 / 100 * f / 16);
            tex_pal_warm[p][i] = rgb(i * 46 / 100 * f / 16, i * 28 / 100 * f / 16, i * 16 / 100 * f / 16);
        }
    }
}

/* ---------------------------------------------------------------- dispatch */

static unsigned g_cut_index;
unsigned demo_cut_index(void) { return g_cut_index; }

void demo_init(void)
{
    accelerator_init();
    if (accelerator_selftest()) {
#ifdef PICO_BUILD
        panic("SLEEPER SIO self-test failed");
#else
        abort();
#endif
    }
    prof_init();
    lights_init();
    song_init();
    textures_init();
    tunnel_grid_init();
}

void demo_stats(demo_stats_t *out) { *out = g_stats; }

static void build_context(uint32_t sample)
{
    F.sample = sample;
    F.cut = song_cut(sample, &F.since);
    F.bar = sample / BAR_SAMPLES;
    F.bar_pos = sample % BAR_SAMPLES;
    F.step = F.bar_pos / STEP_SAMPLES;
    F.b = song_bar(F.bar);
    F.speed = song_speed(sample);
    F.dist = (float)(song_distance(sample) >> 8);
    F.metres = F.dist * METRES_PER_UNIT;
    /* The projected near-layer motion over one field. PLANNING §3: the
     * streak is a fact about the frame, so it is the speed at this sample
     * times the field time, and nothing is accumulated between frames --
     * 160 BPM at 60 Hz is 22.5 fields a beat and an accumulator would drift
     * by half a field every beat. */
    F.ppf = ((float)F.speed * (1.f / 65536.f)) * NEAR_PX_PER_UNIT * FIELD_SAMPLES;
    F.lightlv = F.b->light;
    F.dawn = (float)F.lightlv * (1.f / 255.f);
    F.shot = F.cut->shot;
    F.world = F.cut->world;
    F.flags = F.cut->flags;
    F.variant = F.cut->variant;
    /* which cut this is, for the dispatch log referee 1 is paired with */
    g_cut_index = 0;
    for (unsigned i = 0; i < song_cut_count(); i++)
        if (song_cut_at(i) == F.cut) { g_cut_index = i; break; }
}

void HOT(demo_render)(uint16_t *page, uint32_t sample)
{
    g_fb = page;
    memset(&g_stats, 0, sizeof g_stats);
    g_lights_cy = 0;
    if (sample >= DURATION_SAMPLES) { memset(page, 0, WIDTH * HEIGHT * 2); return; }

    const uint32_t t_start = stamp();
    build_context(sample);
    sky_palette();
    g_stats.prepare = stamp() - t_start;
    g_stats.shot = (uint8_t)F.shot;
    g_stats.world_id = (uint8_t)F.world;
    g_stats.section = (uint8_t)song_section(F.bar);

    const uint32_t t_world = stamp();
    switch (F.shot) {
    case SHOT_SIDE:    world_side(); break;
    case SHOT_AHEAD:   world_ahead(); break;
    case SHOT_UNDER:   world_under(); break;
    case SHOT_UP:      world_up(); break;
    case SHOT_TUNNEL:  world_tunnel_draw(); break;
    case SHOT_RAILS:
    case SHOT_WHEEL:
    case SHOT_KALEIDO: world_dream(F.shot); break;
    case SHOT_BOARD:
        /* At 76 the board is inside the dream and the riser is running under
         * it; at 0 and 120 it is alone on black. */
        if (F.world == WORLD_DREAM) world_dream(SHOT_RAILS);
        else memset(page, 0, WIDTH * HEIGHT * 2);
        break;
    default:           memset(page, 0, WIDTH * HEIGHT * 2); break;
    }
    /* the windscreen gets the same droplets as the window */
    if ((F.flags & CUT_RAIN) && (F.shot == SHOT_AHEAD || F.shot == SHOT_UP)) rain_draw();
    g_stats.world = stamp() - t_world;

    const uint32_t t_board = stamp();
    if (F.shot == SHOT_BOARD) board_frame();
    g_stats.board_us = stamp() - t_board;

    g_stats.lights_us = g_lights_cy / 300u;      /* core 0 SysTick, at 300 MHz */
    g_stats.other = stamp() - t_start - g_stats.prepare - g_stats.world - g_stats.board_us;
}
