/* The runner. Decides who owns each row of each frame.
 *
 * Shared verbatim by both targets. On the device beam_frame() runs on core 0
 * and beam_line() on core 1; on the host they run back to back in the present
 * loop. Nothing in here touches a pixel except the blind, which is a fill.
 */

#include "beam.h"
#include "timeline.h"
#include "arena.h"

#include <string.h>

uint8_t g_arena[ARENA_BYTES] __attribute__((aligned(4)));

beam_state_t g_beam[2];
volatile uint8_t g_lod = 0;

static uint8_t s_entered[TL_MAX_CUES];
static int     s_last_cue = -1;

void beam_init(void)
{
    memset(g_beam, 0, sizeof g_beam);
    beam_reset();
}

void beam_reset(void)
{
    memset(s_entered, 0, sizeof s_entered);
    s_last_cue = -1;
}

int beam_cue_count(void) { return tl_cue_count(); }
uint32_t beam_cue_start(int ci) { return tl_cues()[ci].start; }

int beam_cue_index(uint32_t f)
{
    const cue_t *c = tl_cues();
    int n = tl_cue_count(), ci = 0;
    for (int i = 0; i < n; i++) if (c[i].start <= f) ci = i;
    return ci;
}

const char *beam_scene_name(uint32_t f)
{
    return tl_cues()[beam_cue_index(f)].scene->name;
}

static void enter_cue(int ci)
{
    if (s_entered[ci]) return;
    const scene_t *s = tl_cues()[ci].scene;
    if (s->enter) s->enter();
    s_entered[ci] = 1;
}

void beam_frame(uint32_t f)
{
    const cue_t *cues = tl_cues();
    const int n  = tl_cue_count();
    const int ci = beam_cue_index(f);
    const cue_t *cue  = &cues[ci];
    const cue_t *next = (ci + 1 < n) ? &cues[ci + 1] : NULL;
    const cue_t *prev = (ci > 0) ? &cues[ci - 1] : NULL;
    const uint32_t local = f - cue->start;

    beam_state_t *st = &g_beam[f & 1];
    st->f = f;
    st->a = cue->scene;
    st->b = NULL;
    st->split = 0;
    st->blind = 0;
    st->fade = 0;

    /* A wipe keeps the previous scene alive below the split, so it has to be
     * entered too (after a seek it may not have been). Enter it first: if the
     * two overlapped in the arena they could not be wiped, so order is only
     * about which one's enter() ran last, and the current cue's should. */
    if (cue->trans == TR_WIPE && prev && local < cue->n) enter_cue(ci - 1);
    enter_cue(ci);
    s_last_cue = ci;

    cue->scene->frame(f, local);

    if (cue->trans == TR_WIPE && prev && local < cue->n) {
        st->b = cue->scene;
        st->a = prev->scene;
        st->split = (int16_t)((PV_H * (local + 1)) / cue->n);
        prev->scene->frame(f, f - prev->start);
    } else if (cue->trans == TR_BLIND && local < cue->n / 2) {
        /* The reveal: 7 -> 0 over the first half of n. Seven, not eight:
         * `(y & 7) < 8` is every row, so a blind that reaches 8 is a black
         * frame, and the audit counts black frames as a defect because that
         * is exactly what a scene that failed to draw looks like. At 7 one row
         * in eight still carries the incoming picture. */
        st->blind = (uint8_t)(7 - (7 * local) / (cue->n / 2));
    }

    /* The tail of this cue if the next one blinds in: 0 -> 8 over n/2. */
    if (next && next->trans == TR_BLIND) {
        const uint32_t half = next->n / 2;
        if (f + half >= next->start) {
            const uint32_t k = f + half - next->start;   /* 0 .. half-1 */
            uint8_t b = (uint8_t)((7 * (k + 1)) / half);
            if (b > st->blind) st->blind = b;
        }
    }
}

/* Called by the drawing core at the top of every frame. Runs a scene's
 * setup1() the first time that scene is about to draw. Tracks two scenes
 * because a wipe draws two. */
static const scene_t *s_cfg_a, *s_cfg_b;
void PV_HOT(beam_line_setup)(uint32_t f)
{
    const beam_state_t *st = &g_beam[f & 1];
    if (st->a != s_cfg_a) { s_cfg_a = st->a; if (st->a && st->a->setup1) st->a->setup1(); }
    if (st->b != s_cfg_b) { s_cfg_b = st->b; if (st->b && st->b->setup1) st->b->setup1(); }
    if (st->a && st->a->line0) st->a->line0(f);
    if (st->b && st->b->line0) st->b->line0(f);
}

void PV_HOT(beam_line)(uint32_t f, uint16_t *px, int y)
{
    const beam_state_t *st = &g_beam[f & 1];
    /* Core 1 starts asking for lines before core 0 has prepared frame 0. */
    if (!st->a || (st->blind && (y & 7) < st->blind)) { pv_black(px); return; }
    const scene_t *s = (st->b && y < st->split) ? st->b : st->a;
    s->line(f, px, y);
}
