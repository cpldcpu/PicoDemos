/* The VOLTAGE demo timeline. Each entry binds a time range to one effect. */

#include "scene.h"

extern const effect_t fx_spark_gap;
extern const effect_t fx_plasma_core;
extern const effect_t fx_ray_volt;
extern const effect_t fx_vector_strike;
extern const effect_t fx_spark_generator;
extern const effect_t fx_canyon_flight;
extern const effect_t fx_julia_shockwave;
extern const effect_t fx_voltage_arc;

const timeline_entry_t timeline[] = {
    {      0,  15000, &fx_spark_gap       },   /* 0:00 - 0:15  Scene 1: Spark-Gap (Intro Grid)          */
    {  15000,  30000, &fx_plasma_core      },   /* 0:15 - 0:30  Scene 2: Plasma Core (Verse 1 Fluid)     */
    {  30000,  45000, &fx_ray_volt         },   /* 0:30 - 0:45  Scene 3: Ray-Volt (Chorus 1 Slow-Flight) */
    {  45000,  60000, &fx_spark_generator  },   /* 0:45 - 1:00  Scene 4: Spark Generator (Chorus 2 Bolt) */
    {  60000,  85000, &fx_vector_strike    },   /* 1:00 - 1:25  Scene 5: Vector Strike (Bridge Scroller) */
    {  85000, 108000, &fx_canyon_flight    },   /* 1:25 - 1:48  Scene 6: Canyon Flight (New Climax!)     */
    { 108000, 125000, &fx_julia_shockwave  },   /* 1:48 - 2:05  Scene 7: Julia Shockwave (Silent Drop)   */
    { 125000, 158200, &fx_voltage_arc      },   /* 2:05 - 2:38  Scene 8: Voltage Arc (Outro Credits)     */
};
const int timeline_count = sizeof(timeline) / sizeof(timeline[0]);
