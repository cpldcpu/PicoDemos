/* VOLTAGE demoscene - Scene 3: Ray-Volt (Raymarching)
 *
 * Mode: MODE_160 (160x120 direct RGB565 truecolor, pixel-doubled to 320x240)
 * Visuals: Real-time 3D raymarching of a neon cyber-forest of infinite metallic
 *          corrugated columns on a glowing grid floor. Fast specular highlights,
 *          distance fog, and high-voltage beat-synced flashes.
 */

#include "../scene.h"
#include "../vga.h"
#include "../scene_scratch.h"
#include "../rgb565.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define W 160
#define H 120
#define MAX_STEPS 16
#define FAR_CLIP  18.0f

static inline float map_cylinders(float px, float py, float pz, float t_sec)
{
    /* Fast space folding without fmodf! */
    float tx = px + 1.5f - (int)((px + 1.5f) * 0.333333f) * 3.0f;
    if (tx < 0.0f) tx += 3.0f;
    float cx = tx - 1.5f;

    float tz = pz + 1.5f - (int)((pz + 1.5f) * 0.333333f) * 3.0f;
    if (tz < 0.0f) tz += 3.0f;
    float cz = tz - 1.5f;

    /* Bhaskara / Fast triangle wave cylinder radius modulation (0 sinf calls!) */
    float arg = py * 3.5f + t_sec * 3.0f;
    float p = arg * 0.1591549f; /* arg / (2 * PI) */
    float fract = p - (int)p;
    if (fract < 0.0f) fract += 1.0f;
    float tri = fabsf(fract - 0.5f) * 4.0f - 1.0f;
    float r = 0.35f + 0.08f * tri;

    return sqrtf(cx*cx + cz*cz) - r;
}

static inline void get_analytic_normal(float px, float py, float pz, int is_floor, float *nx, float *ny, float *nz)
{
    if (is_floor) {
        *nx = 0.0f;
        *ny = 1.0f;
        *nz = 0.0f;
    } else {
        /* Folded column coordinates */
        float tx = px + 1.5f - (int)((px + 1.5f) * 0.333333f) * 3.0f;
        if (tx < 0.0f) tx += 3.0f;
        float cx = tx - 1.5f;
        
        float tz = pz + 1.5f - (int)((pz + 1.5f) * 0.333333f) * 3.0f;
        if (tz < 0.0f) tz += 3.0f;
        float cz = tz - 1.5f;
        
        float len = sqrtf(cx*cx + cz*cz) + 0.0001f;
        *nx = cx / len;
        *ny = 0.0f;
        *nz = cz / len;
    }
}

void fx_ray_volt_init(void)
{
}

static inline uint16_t rgb565_add_saturate(uint16_t c1, uint16_t c2)
{
    int r1 = (c1 >> 11) & 0x1F;
    int g1 = (c1 >> 5) & 0x3F;
    int b1 = c1 & 0x1F;
    
    int r2 = (c2 >> 11) & 0x1F;
    int g2 = (c2 >> 5) & 0x3F;
    int b2 = c2 & 0x1F;
    
    int r = r1 + r2; if (r > 0x1F) r = 0x1F;
    int g = g1 + g2; if (g > 0x3F) g = 0x3F;
    int b = b1 + b2; if (b > 0x1F) b = 0x1F;
    
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void fx_ray_volt_frame(uint32_t t_ms, uint32_t t_global)
{
    uint16_t *fb = vga_160_back_buffer();
    float t_sec = t_ms * 0.001f;
    uint32_t scene_duration = 15000;

    /* Beat pulse for flashing lights */
    uint32_t beat_interval = 923; 
    uint32_t beat_time = t_ms % beat_interval;
    float beat_pulse = expf(-((float)beat_time) * 0.006f);

    /* Camera and Speed tuning for slow flight */
    float cam_x = 1.5f + sinf(t_sec * 0.4f) * 1.0f;
    float cam_y = 0.2f + sinf(t_sec * 0.7f) * 0.3f;
    float cam_z = t_sec * 1.5f; /* Smooth constant forward motion */

    /* Target look-at point ahead of camera */
    float tar_x = cam_x + sinf(t_sec * 0.4f + 0.5f) * 0.6f;
    float tar_y = cam_y - 0.2f;
    float tar_z = cam_z + 3.0f;

    /* Camera coordinate system vectors */
    float forward_x = tar_x - cam_x;
    float forward_y = tar_y - cam_y;
    float forward_z = tar_z - cam_z;
    float flen = sqrtf(forward_x*forward_x + forward_y*forward_y + forward_z*forward_z);
    forward_x /= flen; forward_y /= flen; forward_z /= flen;

    /* Right vector (cross product of forward and world up [0,1,0]) */
    float right_x = forward_z;
    float right_y = 0.0f;
    float right_z = -forward_x;
    float rlen = sqrtf(right_x*right_x + right_z*right_z);
    right_x /= rlen; right_z /= rlen;

    /* Up vector (cross product of right and forward) */
    float up_x = -right_z * forward_y;
    float up_y = right_z * forward_x - right_x * forward_z;
    float up_z = right_x * forward_y;

    /* Neon flying light source */
    float l_x = cam_x + sinf(t_sec * 2.0f) * 1.5f;
    float l_y = 0.3f + cosf(t_sec * 1.5f) * 0.5f;
    float l_z = cam_z + 2.0f + cosf(t_sec * 2.0f) * 1.0f;

    /* Raymarch! */
    float fov_scale = 0.8f;
    for (int y = 0; y < H; y += 2) {
        float screen_y = ((y + 0.5f) - H / 2.0f) / (H / 2.0f) * fov_scale;
        int row_offset0 = y * W;
        int row_offset1 = (y + 1) * W;
        
        for (int x = 0; x < W; x += 2) {
            float screen_x = ((x + 0.5f) - W / 2.0f) / (W / 2.0f) * fov_scale * (W / (float)H);

            /* Ray direction */
            float rx = right_x * screen_x + up_x * screen_y + forward_x;
            float ry = right_y * screen_x + up_y * screen_y + forward_y;
            float rz = right_z * screen_x + up_z * screen_y + forward_z;
            float rlen = sqrtf(rx*rx + ry*ry + rz*rz);
            rx /= rlen; ry /= rlen; rz /= rlen;

            /* Analytic floor intersection */
            float t_floor = 1e6f;
            if (ry < 0.0f) {
                t_floor = (-1.0f - cam_y) / ry;
            }

            /* Raymarch cylinders only */
            float t = 0.05f;
            int hit_cyl = 0;
            float px = cam_x, py = cam_y, pz = cam_z;

            for (int step = 0; step < MAX_STEPS; step++) {
                px = cam_x + rx * t;
                py = cam_y + ry * t;
                pz = cam_z + rz * t;

                /* Stop if we cross the floor or exceed far clip */
                if (t >= t_floor || t >= FAR_CLIP) break;

                float d = map_cylinders(px, py, pz, t_sec);
                if (d < 0.005f) {
                    hit_cyl = 1;
                    break;
                }
                t += d;
            }

            /* Decide which was hit first */
            int hit = 0;
            float hit_t = FAR_CLIP;
            int is_floor = 0;

            if (hit_cyl && t < t_floor && t < FAR_CLIP) {
                hit = 1;
                hit_t = t;
                is_floor = 0;
            } else if (t_floor < FAR_CLIP) {
                hit = 1;
                hit_t = t_floor;
                is_floor = 1;
                /* Project point exactly onto the floor plane */
                px = cam_x + rx * t_floor;
                py = -1.0f;
                pz = cam_z + rz * t_floor;
            }

            /* Shade pixel */
            uint16_t pixel_color;
            if (hit) {
                /* Hit! Compute normal */
                float nx, ny, nz;
                get_analytic_normal(px, py, pz, is_floor, &nx, &ny, &nz);

                /* Light vector */
                float lx = l_x - px;
                float ly = l_y - py;
                float lz = l_z - pz;
                float ldist = sqrtf(lx*lx + ly*ly + lz*lz) + 0.001f;
                lx /= ldist; ly /= ldist; lz /= ldist;

                /* Specular vector (Reflected ray) */
                float dot_nl = nx*lx + ny*ly + nz*lz;
                if (dot_nl < 0.0f) dot_nl = 0.0f;

                float rx_refl = 2.0f * dot_nl * nx - lx;
                float ry_refl = 2.0f * dot_nl * ny - ly;
                float rz_refl = 2.0f * dot_nl * nz - lz;

                /* Specular highlight (Phong) */
                float dot_refl_view = -(rx_refl*rx + ry_refl*ry + rz_refl*rz);
                if (dot_refl_view < 0.0f) dot_refl_view = 0.0f;
                float specular = powf(dot_refl_view, 8.0f) * 180.0f;

                /* High-voltage color styling */
                int r_base = 20, g_base = 10, b_base = 40; /* Cyber Void base */

                if (is_floor) {
                    /* Floor plane grid texture */
                    float fx = fmodf(fabsf(px), 1.0f);
                    float fz = fmodf(fabsf(pz), 1.0f);
                    if (fx < 0.05f || fz < 0.05f) {
                        /* Glowing Neon Cyan lines */
                        r_base = 0; g_base = 220; b_base = 255;
                    }
                } else {
                    /* Cylinder columns: custom hot stripes */
                    float strip = sinf(py * 12.0f + pz * 1.5f);
                    if (strip > 0.7f) {
                        /* Hot Laser Magenta stripes */
                        r_base = 255; g_base = 0; b_base = 180;
                    } else {
                        /* Standard metal: Shimmering purple-blue */
                        r_base = 60  + (int)(nx * 40.0f);
                        g_base = 40  + (int)(ny * 30.0f);
                        b_base = 100 + (int)(nz * 50.0f);
                    }
                }

                /* Shading calculation */
                float atten = 1.5f / (1.0f + ldist * 0.4f + ldist * ldist * 0.1f);
                float diffuse = dot_nl * 1.2f + 0.15f; /* Ambient baseline */

                /* Dynamic strobe light pulse from music beat */
                diffuse += beat_pulse * 0.5f;

                int r = (int)((r_base * diffuse + specular) * atten);
                int g = (int)((g_base * diffuse + specular) * atten);
                int b = (int)((b_base * diffuse + specular) * atten);

                /* Depth-Fog: Vanishes completely at FAR_CLIP */
                float fog = 1.0f - expf(-0.22f * hit_t);
                if (fog > 1.0f) fog = 1.0f;
                int r_fog = 10;
                int g_fog = 8;
                int b_fog = 20;
                r = (int)(r * (1.0f - fog) + r_fog * fog);
                g = (int)(g * (1.0f - fog) + g_fog * fog);
                b = (int)(b * (1.0f - fog) + b_fog * fog);

                pixel_color = rgb565_pack(r, g, b);
            } else {
                /* Sky void: Deep neon space horizon */
                float sky_grad = (screen_y + 0.5f) * 0.8f;
                if (sky_grad < 0.0f) sky_grad = 0.0f;
                if (sky_grad > 1.0f) sky_grad = 1.0f;

                /* Synced strobe flash in the sky background */
                int sky_r_tgt = 40 + beat_pulse * 80;
                int sky_g_tgt = 0  + beat_pulse * 30;
                int sky_b_tgt = 100 + beat_pulse * 100;

                int r_sky = (int)(10 * (1.0f - sky_grad) + sky_r_tgt * sky_grad);
                int g_sky = (int)(8  * (1.0f - sky_grad) + sky_g_tgt * sky_grad);
                int b_sky = (int)(20 * (1.0f - sky_grad) + sky_b_tgt * sky_grad);

                pixel_color = rgb565_pack(r_sky, g_sky, b_sky);
            }

            /* Paint 2x2 block */
            fb[row_offset0 + x] = pixel_color;
            fb[row_offset0 + x + 1] = pixel_color;
            fb[row_offset1 + x] = pixel_color;
            fb[row_offset1 + x + 1] = pixel_color;
        }
    }

    /* 6. ADVANCED THEMED TRANSITIONS */
    float fade = 1.0f;
    uint32_t fade_duration = 1500;

    if (t_ms < fade_duration) {
        fade = t_ms / (float)fade_duration;
        /* Chorus 1: Thermal Cool-down from Plasma Core's explosive white-hot flash! */
        float intensity = 1.0f - fade;
        uint16_t cooldown_col = rgb565_pack((int)(255 * intensity), (int)(240 * intensity), (int)(220 * intensity));
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                fb[y * W + x] = rgb565_add_saturate(fb[y * W + x], cooldown_col);
            }
        }
    } else if (t_ms > (scene_duration - fade_duration)) {
        fade = (scene_duration - t_ms) / (float)fade_duration;
        if (fade < 0.0f) fade = 0.0f;

        /* Chorus 1 Fade-Out: Horizontal Scanline Blind Wipe! */
        int blind_height = 8;
        int step = (int)(fade * blind_height);
        for (int y = 0; y < H; y++) {
            if ((y % blind_height) >= step) {
                for (int x = 0; x < W; x++) {
                    fb[y * W + x] = 0x0842;
                }
            }
        }
    }
}

void fx_ray_volt_done(void)
{
}

const effect_t fx_ray_volt = {
    "Ray-Volt",
    MODE_160,
    fx_ray_volt_init,
    fx_ray_volt_frame,
    fx_ray_volt_done
};
