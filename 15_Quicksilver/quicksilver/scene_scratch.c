/* Backing storage for the shared per-scene scratch union (see
 * scene_scratch.h). In its own .c so multiple effects don't multiply-define. */

#include "scene_scratch.h"

union scene_scratch_u g_scratch __attribute__((aligned(4)));
