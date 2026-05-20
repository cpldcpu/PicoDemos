#include <math.h>

#include "mathtab.h"

/* sin_tab[i] = round(sin(i * 2π / 1024) * 32255). The unusual scale comes
 * from the original (line 1413-1455) — a Taylor-series build that lands at
 * ~31.5 K peak, which several callers rely on (texture mapper scaling,
 * voxel height shading). */
int16_t sin_tab[SIN_TAB_LEN];

void mathtab_init(void)
{
    for (int i = 0; i < SIN_TAB_LEN; i++) {
        double a = (double)i * (2.0 * M_PI) / (double)SIN_TAB_LEN;
        int v = (int)(sin(a) * 32255.0 + (a >= 0 ? 0.5 : -0.5));
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        sin_tab[i] = (int16_t)v;
    }
}
