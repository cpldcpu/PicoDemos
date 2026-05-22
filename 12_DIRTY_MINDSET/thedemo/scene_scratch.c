#include "scene_scratch.h"

/* Allocate the shared scratchpad union in the BSS segment.
 * Aligned to 4 bytes for optimal 32-bit access in rendering loops. */
union scene_scratch_u g_scratch __attribute__((aligned(4)));
