#include "tables.h"
#include <math.h>

int8_t  pv_sin8_tab[2048];
uint8_t pv_usin8_tab[2048];
int16_t pv_sin16_tab[2048];

void pv_tables_init(void)
{
    for (int i = 0; i < 1024; i++) {
        double s = sin(i * (2.0 * 3.14159265358979323846 / 1024.0));
        pv_sin8_tab[i]  = pv_sin8_tab[i + 1024]  = (int8_t)lrint(s * 127.0);
        pv_usin8_tab[i] = pv_usin8_tab[i + 1024] = (uint8_t)(128 + (int)lrint(s * 127.0));
        pv_sin16_tab[i] = pv_sin16_tab[i + 1024] = (int16_t)lrint(s * 32767.0);
    }
}
