/* The sky, shared by every family.
 *
 * This exists so the sky CANNOT crossfade. A cross-family morph evaluates both
 * sides and lerps the results, so if two families computed their skies even
 * slightly differently, the background would dissolve from one to the other
 * while the geometry morphed — a visible crossfade in the largest area of the
 * frame, which is the one thing this demo may not do.
 *
 * Every family calls this. There is only one sky, and it is a function of view
 * direction and a single grade parameter, so blending two families' skies is
 * mathematically identical to blending their grade values. Nothing dissolves.
 */

#ifndef SUSTAIN_SKY_COMMON_H
#define SUSTAIN_SKY_COMMON_H

/* u: view azimuth 0..1 around the compass. v: 0 at horizon, 1 at top of frame.
 * grade: 0 = cold dawn, 1 = the hot mid-arc palette (a runtime grade of the
 * same asset, never a second sky — the closing vista has to be recognisably
 * the opening one). */
void sustain_sky(float grade, float u, float v, int *r, int *g, int *b);

#endif
