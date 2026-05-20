/* SOTA backend.h implementation for the RP2040 + ST7789 240x240 target.
 *
 * Mirrors the structure of sota/native/posix_sdl2_backend.c but with:
 *   - WAD baked into flash (XIP-resident, no copy into RAM)
 *   - 6 bitplanes from a static pool, sized for 240x240
 *   - backend_render = compose 240 lines via compose_scanline_565() and
 *     stream them line-by-line to the ST7789 over DMA SPI
 *   - No event pump (no keyboard); backend_should_display_next_frame just
 *     paces the loop with sleep_us
 *   - copper callback fires once per scanline (the same granularity it
 *     does on desktop after our pixel-level "every 1/40th" was abandoned)
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "backend.h"
#include "minmax.h"
#include "iff-font.h"
#include "wad.h"
#include "choreography.h"
#include "choreography_commands.h"
#include "sound.h"

#include "st7789.h"
#include "bitplane_compose.h"

#define MS_PER_FRAME 20            /* engine pacing target */
#define WIDTH  ST7789_WIDTH
#define HEIGHT ST7789_HEIGHT
#define BYTES_PER_ROW (WIDTH / 8)  /* 30 */

/* WAD lives in flash via .incbin in sota_wad.S. */
extern const uint8_t _sota_wad_start[];
extern const uint8_t _sota_wad_end[];

int window_width, window_height;
struct Bitplane backend_bitplane[6];

/* Bitplane pool: 10x one-plane scratch with the padded stride
 * (32*240 = 7680 B per plane). Total 76800 B. The 10x sizing matches
 * desktop and is tight on jump-1 / iris-vs-glitchy / omg3d / similar
 * scenes that allocate plane 5 as BITPLANE_2X2 (480x480 = 28.8 KB) on
 * top of five 1x1 planes (38.4 KB padded) for a peak of 67.2 KB. */
#define BITPLANE_PADDED_BYTES_PER_ROW 32   /* matches the pad in alloc */
#define BITPLANE_POOL_BYTES (10 * BITPLANE_PADDED_BYTES_PER_ROW * HEIGHT)
static uint8_t bitplane_pool[BITPLANE_POOL_BYTES] __attribute__((aligned(4)));
static uint8_t *bitplane_pool_next = bitplane_pool;
static uint8_t * const bitplane_pool_end = bitplane_pool + BITPLANE_POOL_BYTES;

/* 32 active palette entries + 32 EHB doubled-bright shadows, both
 * pre-translated, exactly mirroring the desktop backend. */
static uint32_t palette[64];

/* Font bitplanes (separate, dynamically allocated like on desktop). */
static int loaded_font_idx = -1;
static struct Bitplane font_bitplane[6];

/* Reserved-memory pool used by backend_reserve_memory(). Sized for ~24 KB
 * of "init-time forever allocations". Actual usage observed by reading
 * the engine: scene structures, font glyph tables. */
#define RESERVE_POOL_BYTES (24 * 1024)
static uint8_t reserve_pool[RESERVE_POOL_BYTES] __attribute__((aligned(4)));
static uint8_t *reserve_pool_next = reserve_pool;

/* Per-scanline copper callback. */
static void (*copper_func)(int x, int y, uint32_t *palette);

/* One DMA-target line buffer per full-frame send. */
static uint16_t line_buffer[WIDTH];

/* -------- timing -------- */

uint64_t backend_get_time_ms(void)
{
    return (uint64_t)to_ms_since_boot(get_absolute_time());
}

bool backend_should_display_next_frame(int64_t time_remaining_this_frame)
{
    if (time_remaining_this_frame > 0) {
        sleep_us((uint32_t)(time_remaining_this_frame * 1000));
    }
    return true;   /* no "Esc to quit" on a microcontroller */
}

/* -------- bitplane allocation -------- */

void backend_set_new_scene(void)
{
    for (int i = 0; i < 6; i++) {
        backend_bitplane[i].data_start = backend_bitplane[i].data = NULL;
        backend_bitplane[i].width = backend_bitplane[i].height = backend_bitplane[i].stride = 0;
    }
    bitplane_pool_next = bitplane_pool;
}

struct Bitplane *backend_allocate_bitplane(int idx, int width, int height)
{
    assert(backend_bitplane[idx].data_start == NULL);

    /* Pad to a 64-pixel (8-byte) row stride.
     *
     * The minimum 32-pixel pad fixes per-row alignment for
     * planar_line_horizontal's (uint32_t *) casts. But the static2
     * effect (sota/native/scene.c::scene_static2_tick) offsets a 2x2
     * plane's data pointer by stride/2 to expose quadrants — that
     * needs *stride* to be a multiple of 4 too, i.e. the pixel width
     * must be a multiple of 64. So:
     *   240 px -> 256 px wide, stride 32 (compose stops at byte 29)
     *   480 px -> 512 px wide, stride 64 (jump-1 / iris-vs-glitchy etc) */
    int padded_width = (width + 63) & ~63;
    int stride = padded_width / 8;

    backend_bitplane[idx].data_start = backend_bitplane[idx].data = bitplane_pool_next;
    bitplane_pool_next += (size_t)height * stride;

    if (bitplane_pool_next > bitplane_pool_end) {
        printf("Bitplane alloc overflow (idx=%d w=%d h=%d)\n", idx, padded_width, height);
        for (;;) tight_loop_contents();
    }

    backend_bitplane[idx].idx    = idx;
    backend_bitplane[idx].width  = padded_width;
    backend_bitplane[idx].height = height;
    backend_bitplane[idx].stride = stride;

    /* Bitplane memory persists, but the engine *expects* it to be cleared
     * after a new-scene + allocate cycle (the desktop happens to give us
     * fresh malloc'd memory implicitly zero on first touch from the OS;
     * here we have a static pool so we zero it explicitly). */
    memset(backend_bitplane[idx].data_start, 0, (size_t)height * stride);

    return &backend_bitplane[idx];
}

void backend_allocate_standard_bitplanes(void)
{
    backend_set_new_scene();
    for (int i = 0; i < 6; i++) {
        backend_allocate_bitplane(i, window_width, window_height);
    }
}

void backend_copy_bitplane(struct Bitplane *dst, struct Bitplane *src)
{
    assert(dst->height == src->height && dst->stride == src->stride);
    memcpy(dst->data_start, src->data_start, (size_t)src->stride * src->height);
}

/* -------- palette -------- */

void backend_get_palette(int num_elements, uint32_t *elements)
{
    if (num_elements > 32) num_elements = 32;
    memcpy(elements, palette, (size_t)num_elements * sizeof(uint32_t));
}

void backend_set_palette(int num_elements, uint32_t *elements)
{
    memcpy(palette, elements, (size_t)num_elements * sizeof(uint32_t));
    for (int i = num_elements; i < 32; i++) palette[i] = 0xff000000;

    /* EHB: every entry mirrored at +32 with each channel halved. */
    for (int i = 0; i < 32; i++) {
        uint32_t c = palette[i];
        palette[i + 32] = 0xff000000u
            | (((c & 0x00ff0000u) >> 17) << 16)
            | (((c & 0x0000ff00u) >>  9) <<  8)
            | (((c & 0x000000ffu) >>  1)      );
    }
}

void     backend_set_palette_element(int idx, uint32_t element) { palette[idx] = element; }
uint32_t backend_get_palette_element(int idx)                   { return palette[idx]; }

void backend_register_copper_func(void (*func)(int x, int y, uint32_t *palette_arg))
{
    copper_func = func;
}

/* -------- render -------- */

void backend_render(void)
{
    st7789_begin_frame();

    const uint8_t *row[6];
    int            stride[6];
    for (int k = 0; k < 6; k++) {
        row[k]    = backend_bitplane[k].data;
        stride[k] = backend_bitplane[k].stride;
    }

    for (int y = 0; y < HEIGHT; y++) {
        if (copper_func) {
            /* Rough match to the desktop's "fire a few times per scanline"
             * cadence: once at the start of the scanline is plenty for
             * the gradient effects SOTA actually uses. */
            copper_func(0, y * 256 / HEIGHT, palette);
        }

        const uint8_t *planes[6] = { row[0], row[1], row[2], row[3], row[4], row[5] };
        compose_scanline_565(planes, palette, line_buffer);
        st7789_write_pixels(line_buffer, WIDTH);

        for (int k = 0; k < 6; k++) row[k] += stride[k];
    }
}

/* -------- font -------- */

bool backend_font_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions)
{
    if (loaded_font_idx >= 0) backend_font_unload();

    for (int i = 0; i < 6; i++) {
        font_bitplane[i].width  = backend_bitplane[0].width;
        font_bitplane[i].height = backend_bitplane[0].height;
        font_bitplane[i].stride = backend_bitplane[0].width / 8;
        font_bitplane[i].data_start = font_bitplane[i].data
            = malloc((size_t)font_bitplane[i].height * font_bitplane[i].stride);
        if (!font_bitplane[i].data_start) {
            printf("font_load: malloc fail\n");
            return false;
        }
    }

    bool ok = ifffont_load(file_idx, startchar, numchars, positions, font_bitplane);
    if (ok) loaded_font_idx = file_idx;
    return ok;
}

void backend_font_unload(void)
{
    for (int i = 0; i < 6; i++) {
        if (font_bitplane[i].data_start) {
            free(font_bitplane[i].data_start);
            font_bitplane[i].data = font_bitplane[i].data_start = NULL;
            font_bitplane[i].width = font_bitplane[i].height = font_bitplane[i].stride = 0;
        }
    }
    ifffont_unload();
}

void backend_font_draw(int numchars, char *text, int x, int y)
{
    if (x == -1) ifffont_centre(numchars, text, y, backend_bitplane);
    else         ifffont_draw  (numchars, text, x, y, backend_bitplane);
}

int backend_font_get_height(void) { return ifffont_get_height(); }

/* -------- WAD (XIP-resident) -------- */

void *backend_wad_load_choreography_for_scene_ms(int ms)
{
    uint8_t *wad = (uint8_t *)_sota_wad_start;
    uint8_t *choreography = wad + wad_get_choreography_offset(wad);
    return choreography + choreography_find_offset_for_scene(choreography, ms, NULL);
}

void *backend_wad_load_file(int file_idx, size_t *size_out)
{
    uint8_t *wad = (uint8_t *)_sota_wad_start;
    uint32_t off = wad_get_file_offset(wad, file_idx);
    if (size_out) *size_out = wad_get_file_size(wad, file_idx);
    return wad + off;
}

void backend_wad_unload_file(void *data) { (void)data; /* XIP: nothing to free */ }

/* -------- misc -------- */

void *backend_reserve_memory(size_t amt)
{
    /* 4-byte align */
    amt = (amt + 3u) & ~(size_t)3u;
    if (reserve_pool_next + amt > reserve_pool + RESERVE_POOL_BYTES) {
        printf("backend_reserve_memory: out of reserve pool (asked %u)\n", (unsigned)amt);
        for (;;) tight_loop_contents();
    }
    void *p = reserve_pool_next;
    reserve_pool_next += amt;
    return p;
}

int backend_random(void)        { return rand(); }

void backend_debug(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

/* -------- lifecycle -------- */

bool backend_init(int width, int height, bool fullscreen, const void *wad_name)
{
    (void)fullscreen; (void)wad_name;
    (void)width; (void)height;
    window_width  = WIDTH;
    window_height = HEIGHT;

    copper_func = NULL;
    loaded_font_idx = -1;

    st7789_init();

    backend_allocate_standard_bitplanes();
    return true;
}

void backend_shutdown(void) { /* no-op */ }

static uint64_t starttime;
static int64_t  time_remaining_this_frame;

static uint32_t scene_name_to_scene_ms(char *scene_name)
{
    uint8_t *wad = (uint8_t *)_sota_wad_start;
    uint8_t *choreography = wad + wad_get_choreography_offset(wad);
    return choreography_find_ms_for_scene_name(choreography, scene_name);
}

static void _backend_run_one(void)
{
    uint64_t frametime = backend_get_time_ms();
    int ms = (int)(frametime - starttime);

    choreography_do_frame(ms);
    time_remaining_this_frame = MS_PER_FRAME - (int64_t)(backend_get_time_ms() - frametime);

    backend_render();
    sound_update();
}

void backend_run(int ms, char *scene_name)
{
    if (scene_name) ms = (int)scene_name_to_scene_ms(scene_name);

    starttime = backend_get_time_ms() - (uint64_t)ms;

    uint8_t *choreography = backend_wad_load_choreography_for_scene_ms(ms);
    if (!choreography_prepare_to_run(choreography, ms)) return;

    while (true) {
        _backend_run_one();
        backend_should_display_next_frame(time_remaining_this_frame);
    }
}
