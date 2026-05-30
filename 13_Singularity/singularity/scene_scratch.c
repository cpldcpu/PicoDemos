/* Backing storage for the shared per-scene scratch buffer. See
 * scene_scratch.h for the layout. Defined in its own .c so multiple
 * effects don't end up with multiple definitions. */

#include "scene_scratch.h"

union scene_scratch_u g_scratch __attribute__((aligned(4)));
