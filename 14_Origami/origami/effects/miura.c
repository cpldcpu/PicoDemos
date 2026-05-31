/* Scene 4 — MIURA-ORI TESSELLATION WAVE (1:17.11–1:53.01, MODE_320).
 *
 * The climax. A field of Miura-fold parallelograms (NX*NY cells on an
 * animated vertex grid) ripples in a travelling fold wave. Each flat facet
 * catches the light by its own tilt, so as the wave passes, whole bands
 * flip between lit and shadowed — a shimmering corrugated sheet. Amplitude
 * pulses on the beat. The densest scene; still pure memset span fill.
 */

#include "scene.h"
#include "vga.h"
#include "poly3d.h"
#include "origami_fx.h"
#include "scene_scratch.h"
#include <math.h>

#define SCENE_LEN_MS 35900
#define CELL   34.0f
#define SHEAR  (CELL * 0.30f)     /* Miura zigzag offset on odd rows */

#define VID(i,j)  ((j)*(MIURA_NX+1) + (i))

static p3_face miura_faces[MIURA_NFACE];
static p3_model miura_model;

static void miura_init(void)
{
    og_materials();
    /* a complementary two-tone field: coral + cream, with sky behind */
    int n = 0;
    for (int j = 0; j < MIURA_NY; j++) {
        for (int i = 0; i < MIURA_NX; i++) {
            uint8_t mat = ((i + j) & 1) ? P3_MAT_CORAL : P3_MAT_CREAM;
            miura_faces[n++] = (p3_face){
                (uint16_t)VID(i,  j  ), (uint16_t)VID(i+1,j  ),
                (uint16_t)VID(i+1,j+1), (uint16_t)VID(i,  j+1),
                mat, P3_FACE_DOUBLE_SIDED };
        }
    }
    miura_model = (p3_model){ g_scratch.miura.v, MIURA_NVERT, miura_faces, MIURA_NFACE, 0, 0 };
}

static void miura_frame(uint32_t t_into, uint32_t t_global)
{
    float beat = og_beat_pulse(t_global);
    float amp  = 26.0f * (1.0f + 0.55f * beat);
    float tt   = t_into * 0.001f;

    /* build the animated world-space grid */
    p3_vec3 *V = g_scratch.miura.v;
    for (int j = 0; j <= MIURA_NY; j++) {
        float zc = (j - MIURA_NY * 0.5f) * CELL;
        float shear = (j & 1) ? SHEAR : 0.0f;
        for (int i = 0; i <= MIURA_NX; i++) {
            float xc = (i - MIURA_NX * 0.5f) * CELL + shear;
            /* accordion ridges across columns, swelling as the wave travels
             * down the rows -> facet bands flip light/shadow on the beat */
            float fold   = sinf(i * 1.05f);
            float travel = 0.5f + 0.5f * sinf(j * 0.55f - tt * 2.6f);
            float y = amp * fold * travel;
            V[VID(i,j)] = (p3_vec3){ xc, y, zc };
        }
    }

    /* deep sky, then the rippling field tilted away from a high camera */
    og_sky_grad(120,178,222, 196,222,236);

    p3_render_params rp; p3_params_default(&rp);
    rp.focal = 300.0f;
    rp.oz = 300.0f;
    rp.oy = -6.0f;
    rp.pitch = -0.92f;                    /* tilt the field toward a level cam */
    rp.roll  = 0.05f * sinf(t_into * 0.00035f);
    rp.yaw   = 0.18f * sinf(t_into * 0.00028f);
    rp.ambient = 0.34f;
    rp.light.x = -0.46f; rp.light.y = 0.72f; rp.light.z = -0.52f;
    rp.backface_cull = 0;                 /* double-sided sheet, see both */

    p3_render(&miura_model, &rp);

    if (t_into > 2500 && t_into < 9000)
        og_text_centred("MIURA-ORI", 16, 2);
}

static void miura_done(void) {}

const effect_t fx_miura_real = {
    .name = "miura", .mode = MODE_HIRES,
    .init = miura_init, .frame = miura_frame, .done = miura_done,
};
