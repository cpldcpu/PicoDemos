/* make_textures.c — bake QUICKSILVER's procedural chrome textures to raw
 * RGB565 (.bin) files in PIO-native bit order (see ../rgb565.h), incbin'd into
 * flash by assets/_packed/assets.S.
 *
 * These are placeholder/fallback textures so the demo runs with no external
 * art. To use nano-banana PNGs instead, run tools/pack_assets.py to overwrite
 * the .bin files (same PIO-native packing) — see assets/PROMPTS.md.
 *
 * Build & run (from the quicksilver/ dir):
 *     gcc -O2 -lm tools/make_textures.c -o tools/make_textures.exe
 *     ./tools/make_textures.exe
 * Writes: assets/_packed/{roto,ground,envmap,sky}.bin
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

static uint16_t pack(int r, int g, int b) {
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (uint16_t)(((b & 0xF8) << 8) | ((g & 0xF8) << 3) | (r >> 3));
}

static void write_bin(const char *path, const uint16_t *px, int n) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    fwrite(px, 2, (size_t)n, f);
    fclose(f);
    printf("wrote %s (%d px, %d bytes)\n", path, n, n * 2);
}

#define CLAMP01(x) ((x) < 0.f ? 0.f : (x) > 1.f ? 1.f : (x))

/* A small synthetic environment sampled by a direction (dx,dy,dz): a
 * chrome-dusk world — violet zenith, bright horizon band with a warm sun, dark
 * mirror sea below. Shared by the spheremap and the sky panorama so the chrome
 * ball reflects the same world the sky shows. Returns 0..255 rgb. */
static void sample_env(float dx, float dy, float dz, int *r, int *g, int *b) {
    float len = sqrtf(dx*dx + dy*dy + dz*dz); if (len < 1e-6f) len = 1e-6f;
    dx /= len; dy /= len; dz /= len;
    float up = dy;                       /* -1..1, +1 = straight up */
    /* sky gradient: violet high, pale cyan at horizon */
    float t = CLAMP01(up * 0.5f + 0.5f);
    float sr = 0.10f + 0.55f * (1.f - t) + 0.30f * powf(1.f - fabsf(up), 6.f);
    float sg = 0.12f + 0.45f * (1.f - t);
    float sb = 0.28f + 0.65f * t;
    /* warm sun toward (+x slightly up) */
    float sun = dx * 0.5f + dy * 0.6f + dz * 0.6f;
    if (sun > 0.f) { float s = powf(CLAMP01(sun), 40.f); sr += s * 1.4f; sg += s * 1.1f; sb += s * 0.6f; }
    /* dark reflective sea below the horizon */
    if (up < 0.f) {
        float m = CLAMP01(-up * 2.0f);
        sr = sr * (1.f - m) + 0.04f * m;
        sg = sg * (1.f - m) + 0.06f * m;
        sb = sb * (1.f - m) + 0.10f * m;
        /* a few specular streaks on the sea */
        float streak = powf(CLAMP01(sinf(dx * 9.f) * 0.5f + 0.5f), 8.f) * m;
        sr += streak * 0.5f; sg += streak * 0.55f; sb += streak * 0.7f;
    }
    *r = (int)(CLAMP01(sr) * 255.f);
    *g = (int)(CLAMP01(sg) * 255.f);
    *b = (int)(CLAMP01(sb) * 255.f);
}

/* Chrome reflection probe (sphere map): for pixel (x,y) on the unit disk, the
 * surface normal is (nx,ny,nz); the eye looks down -z, so the reflected ray is
 * r = reflect((0,0,-1), n) = (2*nz*nx, 2*nz*ny, 2*nz*nz - 1). Sample the env in
 * that direction. Outside the disk, fade to a dark rim. */
static void gen_envmap(uint16_t *px, int W, int H) {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float nx = (x + 0.5f) / W * 2.f - 1.f;
            float ny = 1.f - (y + 0.5f) / H * 2.f;
            float rr = nx*nx + ny*ny;
            int r, g, b;
            if (rr <= 1.f) {
                float nz = sqrtf(1.f - rr);
                float rx = 2.f * nz * nx;
                float ry = 2.f * nz * ny;
                float rz = 2.f * nz * nz - 1.f;
                sample_env(rx, ry, rz, &r, &g, &b);
                /* a crisp fresnel rim brightens the silhouette */
                float fres = powf(1.f - nz, 3.f);
                r += (int)(fres * 90.f); g += (int)(fres * 100.f); b += (int)(fres * 120.f);
            } else {
                sample_env(nx, ny, 0.3f, &r, &g, &b);   /* off-sphere = open sky */
            }
            px[y*W + x] = pack(r, g, b);
        }
    }
}

/* Equirectangular sky panorama (2:1-ish): u = azimuth, v = elevation. */
static void gen_sky(uint16_t *px, int W, int H) {
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float az = (x + 0.5f) / W * 2.f * 3.14159265f;
            float el = (0.5f - (y + 0.5f) / H) * 3.14159265f;   /* +pi/2..-pi/2 */
            float dx = cosf(el) * cosf(az);
            float dy = sinf(el);
            float dz = cosf(el) * sinf(az);
            int r, g, b; sample_env(dx, dy, dz, &r, &g, &b);
            px[y*W + x] = pack(r, g, b);
        }
    }
}

/* Seamless mercury ground tile: summed sines of liquid ripples, silver/steel. */
static void gen_ground(uint16_t *px, int W, int H) {
    const float TAU = 6.2831853f;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float u = (float)x / W, v = (float)y / H;
            /* integer-harmonic sines => seamless across the tile edge */
            float h = 0.f;
            h += sinf(TAU * (1*u))            * 0.50f;
            h += sinf(TAU * (2*v + 0.25f))    * 0.35f;
            h += sinf(TAU * (3*u + 2*v))      * 0.25f;
            h += sinf(TAU * (2*u - 3*v + .5f))* 0.20f;
            float s = CLAMP01(h * 0.5f + 0.5f);
            /* metallic ramp: steel-blue shadow -> bright silver highlight */
            int r = (int)((0.30f + 0.65f * s) * 255.f);
            int g = (int)((0.34f + 0.62f * s) * 255.f);
            int b = (int)((0.42f + 0.58f * s) * 255.f);
            float spec = powf(s, 6.f);
            r += (int)(spec * 80.f); g += (int)(spec * 85.f); b += (int)(spec * 95.f);
            px[y*W + x] = pack(r, g, b);
        }
    }
}

/* Seamless ornate radial chrome filigree for the rotozoomer. */
static void gen_roto(uint16_t *px, int W, int H) {
    const float TAU = 6.2831853f;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float u = (float)x / W, v = (float)y / H;
            float cx = u - 0.5f, cy = v - 0.5f;
            float ang = atan2f(cy, cx);
            float rad = sqrtf(cx*cx + cy*cy);
            /* radial petals + concentric rings, tileable via integer harmonics */
            float petals = sinf(ang * 8.f) * 0.5f + 0.5f;
            float rings  = sinf(rad * TAU * 6.f) * 0.5f + 0.5f;
            float grid   = (sinf(TAU * 4*u) * sinf(TAU * 4*v)) * 0.5f + 0.5f;
            float m = CLAMP01(petals * 0.5f + rings * 0.3f + grid * 0.4f);
            /* iridescent chrome: cyan/magenta shift with the pattern */
            int r = (int)((0.20f + 0.70f * m) * 255.f);
            int g = (int)((0.30f + 0.55f * (1.f - m)) * 255.f);
            int b = (int)((0.45f + 0.50f * m) * 255.f);
            float edge = powf(fabsf(petals - 0.5f) * 2.f, 4.f);
            r += (int)(edge * 60.f); g += (int)(edge * 40.f); b += (int)(edge * 80.f);
            px[y*W + x] = pack(r, g, b);
        }
    }
}

int main(void) {
    static uint16_t buf[512 * 256];
    gen_roto  (buf, 256, 256); write_bin("assets/_packed/roto.bin",   buf, 256*256);
    gen_ground(buf, 256, 256); write_bin("assets/_packed/ground.bin", buf, 256*256);
    gen_envmap(buf, 256, 256); write_bin("assets/_packed/envmap.bin", buf, 256*256);
    gen_sky   (buf, 512, 128); write_bin("assets/_packed/sky.bin",    buf, 512*128);
    return 0;
}
