/* Scene runner. Reads the demo clock, picks the active timeline entry,
 * handles boundary crossings (done/init), calls frame().
 */

#include "scene.h"
#include "vga.h"
#include <stdio.h>

static const effect_t *g_active   = NULL;
static int             g_active_i = -1;
static uint32_t        g_scene_start_ms = 0;

static int find_entry(uint32_t t_ms)
{
    for (int i = 0; i < timeline_count; i++) {
        if (t_ms >= timeline[i].start_ms && t_ms < timeline[i].end_ms)
            return i;
    }
    return -1;
}

int scene_runner_tick(uint32_t t_ms_global)
{
    int idx = find_entry(t_ms_global);
    if (idx < 0) {
        /* Past the end — close any active effect and signal done. */
        if (g_active && g_active->done) {
            g_active->done();
            g_active = NULL;
            g_active_i = -1;
        }
        return 0;
    }

    if (idx != g_active_i) {
        /* Boundary crossing. */
        if (g_active && g_active->done) g_active->done();
        const effect_t *next = timeline[idx].effect;
        vga_set_mode(next->mode);
        if (next->init) next->init();
        g_active       = next;
        g_active_i     = idx;
        g_scene_start_ms = timeline[idx].start_ms;
        printf("scene: -> [%d] %s (t=%lu ms)\n", idx, next->name, (unsigned long)t_ms_global);
    }

    if (g_active && g_active->frame) {
        uint32_t t_into = t_ms_global - g_scene_start_ms;
        g_active->frame(t_into, t_ms_global);
    }

    return 1;
}

uint32_t scene_next_boundary_ms(uint32_t t_ms_global)
{
    /* If t is inside an entry, return that entry's end_ms (= next scene start). */
    int idx = find_entry(t_ms_global);
    if (idx >= 0) return timeline[idx].end_ms;
    /* Before the first entry: jump to the first scene start. */
    if (t_ms_global < timeline[0].start_ms) return timeline[0].start_ms;
    /* Past the last entry: clamp to last end. */
    return timeline[timeline_count - 1].end_ms;
}

uint32_t scene_prev_boundary_ms(uint32_t t_ms_global)
{
    int idx = find_entry(t_ms_global);
    if (idx < 0) {
        /* Before first or past last: jump to last scene's start. */
        if (t_ms_global < timeline[0].start_ms) return 0;
        return timeline[timeline_count - 1].start_ms;
    }
    /* If we're more than 500 ms into the current scene, rewind to its
     * start. Otherwise jump to the previous entry's start. */
    uint32_t within = t_ms_global - timeline[idx].start_ms;
    if (within > 500 || idx == 0) return timeline[idx].start_ms;
    return timeline[idx - 1].start_ms;
}
