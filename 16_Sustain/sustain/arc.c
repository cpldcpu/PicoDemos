/* SUSTAIN — the arc.
 *
 * Replaces QUICKSILVER's timeline.c. The difference is the whole demo:
 * timeline.c listed scenes with start_ms/end_ms plus a transition style to
 * paint over each seam. This file lists the times at which the world has
 * FINISHED becoming something, and how long it took. Nothing ends, and there
 * is no transition table, because there are no transitions to style.
 *
 * AUTHORED TO THE TRACK, not retrofitted to it (PLANNING.md §6.6).
 * tools/analyze_music.py measured "Sustained Bass Drone" (4:49.8) as:
 *
 *   0.0 - 14.6    quiet intro          sparse, wide
 *   14.6 - 124.8  sustained drive      energy jumps at 14.6, peak at 47.2
 *   124.8 - 180   breakdown            energy drops away
 *   180 - 266     second high
 *   266 - 289.8   outro
 *
 * with structural boundaries at 14.6, 61.6, 68.7, 124.8, 169.1, 255.0, 284.3.
 * The morphs below land on those boundaries so the world finishes becoming
 * something at the moment the music does.
 *
 * One measured hazard: the track's ONLY bass gap long enough to hear as a stop
 * is at 144.6 s (1.39 s). No morph goes near it — the camera carries the
 * continuity through while the music thins.
 *
 * THIS COVERS 0 -> 124.8 s, the terrain half. The remaining arc (the cave at
 * 124.8, the solid at 169.1, the return at 255.0) needs the particle and mesh
 * families, which are the next build steps.
 */

#include "world.h"

extern const field_t f_sea;
extern const field_t f_canyon;
extern const field_t f_tunnel;
extern const field_t f_tunnel_deep;
extern const field_t f_cave;
extern const field_t f_sea_rise;
extern const field_t f_canyon_wide;
extern const field_t f_canyon_deep;
extern const field_t f_chamber;
extern const field_t f_monolith;
extern const field_t f_monolith_cool;
extern const field_t f_sea_return;

/* t_ms is when the morph INTO this field completes; the morph occupies the
 * morph_ms immediately before it. */
const arc_node_t arc[] = {
    {      0, &f_sea,             0 },  /* open water, still and wide          */
    {   9000, &f_sea_rise,     7000 },  /*  2.0- 9.0  the swell begins to build*/
    {  14600, &f_canyon,       5600 },  /*  9.0-14.6  crests rise into walls   */
    {  27000, &f_canyon_wide, 11000 },  /* 16.0-27.0  the chasm opens out      */
    {  44000, &f_canyon_deep, 14000 },  /* 30.0-44.0  narrowing, darkening     */
    {  61600, &f_tunnel,      15000 },  /* 46.6-61.6  the roof closes over     */
    {  78000, &f_chamber,     12000 },  /* 66.0-78.0  it opens into a chamber  */
    {  96000, &f_tunnel_deep, 14000 },  /* 82.0-96.0  and constricts, hotter   */
    { 112000, &f_chamber,     11000 },  /* 101 -112   breathes open once more  */
    { 124800, &f_tunnel_deep,  9000 },  /* 115.8-124.8 tightens for the drop   */

    /* CROSS-FAMILY MORPH (1). The wall stops being continuous and breaks into
     * drifting lumps. Costs ~2x because both families are evaluated, so it is
     * placed exactly on the track's drop into the breakdown at 124.8 s — the
     * quietest, lowest-detail window in the whole piece. Completing at 136.9
     * lands it on the next measured structural boundary. */
    { 136900, &f_cave,       12000 },  /* 124.9-136.9  the wall comes apart   */
    { 152000, &f_cave,           0 },  /* hold: let the cave be looked at    */
    /* CROSS-FAMILY MORPH (2). The cave condenses into polished masses — a
     * change of MATERIAL as much as shape, which is why it is a separate
     * family: the monolith is matcap-shaded, a lighting model no amount of
     * rock-parameter interpolation can reach. Lands on the 169.1 s boundary
     * where the track climbs out of the breakdown. */
    { 169100, &f_monolith,    13000 },  /* 156.1-169.1 the cave condenses     */
    { 196000, &f_monolith,        0 },  /* hold: fly among them                */
    { 214000, &f_monolith_cool, 15000 },/* 199-214    cooling, opening out     */

    /* THE RETURN. The same sea field the demo opened on, approached on the
     * opposite heading in the cold palette. The whole arc is departure ->
     * transformation -> return, and this is the rhyme that makes it read as
     * having gone somewhere and come back rather than merely stopped. */
    { 255000, &f_sea_return,  22000 },  /* 233-255    back to open water       */
    { 289000, &f_sea_return,      0 },  /* hold to the end of the track        */
};
const int arc_count = (int)(sizeof(arc) / sizeof(arc[0]));

/* Camera. y is HEIGHT ABOVE THE LOCAL FLOOR, not absolute altitude (world.c
 * adds the terrain height), so the same path skims a sea of amplitude 3 and
 * threads a tunnel of amplitude 13 without editing keys when the world morphs
 * underneath it.
 *
 * Speed is implied by key spacing, and speed is this demo's pacing instrument
 * in place of cuts (PLANNING.md §6.5). It tracks the track's energy: ~4 u/s
 * through the quiet intro, accelerating to ~15 u/s by the end of the drive.
 *
 * The keys deliberately do NOT line up with the arc nodes above. If the camera
 * changed behaviour exactly when the world did, the two would reinforce into
 * something the eye reads as a cut even though nothing cut. Letting them drift
 * out of phase is what makes it feel continuous rather than sectioned.
 */
const cam_key_t cam_keys[] = {
    /*   t_ms      x      y        z       yaw     pitch */
    {       0,  0.0f,  4.4f,     0.0f,   0.00f,  -0.03f },
    {    7000,  1.4f,  4.0f,    70.0f,   0.05f,  -0.04f },
    {   14000, -0.8f,  4.2f,    155.0f,  -0.04f,  -0.02f },
    {   22000,  1.8f,  4.8f,    262.0f,   0.04f,   0.00f },
    {   30000, -1.6f,  6.2f,   382.0f,  -0.03f,   0.01f },
    {   38000,  2.0f,  6.8f,   515.0f,   0.03f,   0.00f },
    {   46000, -1.2f,  7.2f,   660.0f,  -0.02f,  -0.01f },
    {   54000,  1.0f,  7.8f,   818.0f,   0.02f,   0.00f },
    {   61600, -0.6f,  8.2f,   988.0f,  -0.01f,   0.01f },
    {   70000,  1.2f,  8.6f,   1178.0f,   0.03f,   0.00f },
    {   79000, -1.4f,  8.6f,   1386.0f,  -0.03f,  -0.01f },
    {   88000,  1.6f,  8.6f,   1612.0f,   0.02f,   0.01f },
    {   96000, -1.0f,  8.4f,   1822.0f,  -0.02f,   0.00f },
    {  105000,  1.3f,  8.2f,   2072.0f,   0.03f,  -0.01f },
    {  115000, -1.1f,  8.0f,  2362.0f,  -0.02f,   0.01f },
    {  124800,  0.4f,  7.8f,  2650.0f,   0.00f,   0.00f },
    /* Through the breakdown the camera eases off — the music thins, so the
     * pacing instrument thins with it. Note the 144.6 s bass gap (the track's
     * one audible stop) falls inside this stretch: no morph goes near it and
     * the camera simply keeps moving through. */
    {  134000, -0.9f,  8.0f,  2928.0f,  -0.02f,   0.01f },
    {  145000,  1.1f,  8.2f,  3262.0f,   0.02f,   0.00f },
    {  157000, -0.7f,  8.4f,  3636.0f,  -0.01f,  -0.01f },
    {  169100,  0.3f,  8.6f,  4020.0f,   0.00f,   0.00f },

    /* Among the monoliths: higher, and pitched up to look at them. The track's
     * second high runs 180-266, so the camera is at its fastest here. */
    {  180000,  1.6f, 10.5f,  4384.0f,   0.04f,   0.03f },
    {  196000, -1.8f, 12.0f,  4906.0f,  -0.05f,   0.04f },
    {  214000,  1.4f, 11.0f,  5452.0f,   0.04f,   0.02f },

    /* Decelerating into the return. Speed is the pacing instrument, so it eases
     * off with the music rather than stopping when the arc does. */
    {  232000, -1.0f,  8.6f,  5950.0f,  -0.03f,   0.00f },
    {  250000,  0.8f,  6.4f,  6380.0f,   0.02f,  -0.01f },
    {  266000, -0.5f,  5.4f,  6722.0f,  -0.01f,   0.00f },
    {  278000,  0.4f,  5.0f,  6952.0f,   0.01f,   0.01f },
    {  289000,  0.0f,  4.8f,  7132.0f,   0.00f,   0.02f },
};
const int cam_key_count = (int)(sizeof(cam_keys) / sizeof(cam_keys[0]));
