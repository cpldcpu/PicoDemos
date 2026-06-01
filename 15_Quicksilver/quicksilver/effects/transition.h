/* transition.h — a small set of distinct liquid-chrome scene transitions,
 * applied by the scene runner after every MODE_HIRES frame(). Each boundary
 * picks its own style (themed to the scenes it joins). The outgoing scene is
 * progressively covered toward chrome-white and the incoming scene is revealed
 * back out of white with the same style, so adjacent scenes meet on white. */

#ifndef QS_TRANSITION_H
#define QS_TRANSITION_H
#include <stdint.h>

enum {
    QS_TR_MELT = 0,    /* chrome floods down from the top, drippy edge */
    QS_TR_WIPE,        /* bright chrome bar sweeps left -> right        */
    QS_TR_DISSOLVE,    /* quicksilver speckle dissolve                  */
    QS_TR_IRIS,        /* chrome disc grows from the centre             */
    QS_TR_BLINDS,      /* horizontal venetian blinds close              */
    QS_TR_COUNT
};

/* Apply the active in/out transition to the hires back buffer. `out_style` is
 * used near the scene's end, `in_style` near its start. If `suppress_out` is
 * set (final scene) the out phase is skipped (the scene fades to black itself). */
void qs_transition_apply(uint32_t t_global, uint32_t scene_start, uint32_t scene_end,
                         int suppress_out, int out_style, int in_style);

/* Scalar chrome-white glint amount (0..256) near a scene's start/end — used by
 * beam-raced (MODE_RACE) scenes, which have no framebuffer for the pattern
 * transitions, so they blend their generated line toward white by this. */
int qs_trans_white(uint32_t t_global, uint32_t scene_start, uint32_t scene_end, int suppress_out);

#endif
