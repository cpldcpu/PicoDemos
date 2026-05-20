/* SOTA backend.h implementation for RP2040 + Pimoroni Pico VGA Demo Base.
 *
 * The board has built-in resistor DAC ladders for 5 bits per channel
 * (32K colors) wired to the standard RPi Foundation VGA pinout:
 *   GP0..GP4   = Red   (LSB..MSB)
 *   GP6..GP10  = Green (GP5 unused / skipped)
 *   GP11..GP15 = Blue
 *   GP16       = HSYNC
 *   GP17       = VSYNC
 * That's exactly pico_scanvideo's default pinout — we use it as-is.
 *
 * Mode is vga_mode_320x240_60 (320x240@60 Hz). Engine renders 6 bitplanes
 * on core 0; core 1 owns the scanline callback, walking bitplanes ->
 * palette -> RGB-555 directly into a scanvideo scanline buffer.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "backend.h"
#include "minmax.h"
#include "iff-font.h"
#include "wad.h"
#include "choreography.h"
#include "choreography_commands.h"
#include "sound.h"

#define WIDTH  320
#define HEIGHT 240
#define MS_PER_FRAME 20

/* -------- bitplane pool & engine plumbing -------------------------------- */

#define BITPLANE_POOL_BYTES (10 * (WIDTH / 8) * HEIGHT)   /* 96000 */
static uint8_t bitplane_pool[BITPLANE_POOL_BYTES] __attribute__((aligned(4)));
static uint8_t *bitplane_pool_next = bitplane_pool;
static uint8_t * const bitplane_pool_end = bitplane_pool + BITPLANE_POOL_BYTES;

#define RESERVE_POOL_BYTES (8 * 1024)
static uint8_t reserve_pool[RESERVE_POOL_BYTES] __attribute__((aligned(4)));
static uint8_t *reserve_pool_next = reserve_pool;

int window_width, window_height;
struct Bitplane backend_bitplane[6];

static uint32_t palette[64];

/* Pre-packed 5:5:5 RGB per palette index in pico_scanvideo's bit layout
 * (R in bits 0..4, G in bits 6..10, B in bits 11..15 — RSHIFT/GSHIFT/BSHIFT
 * defaults). 16-bit value ready to drop straight into a scanline buffer. */
static uint16_t palette_rgb555[64];

/* Per-plane-byte → 8-palette-index-contribution lookup. For plane K and
 * input byte b, plane_contrib[K][b] is two uint32_t holding the 8
 * pre-shifted (1<<K) bit contributions, packed one per byte. To render
 * 8 pixels we OR six of these (one per plane), giving palette indices
 * sitting in the bytes of two 32-bit words. */
static uint32_t plane_contrib[6][256][2];

static void init_plane_contrib_table(void)
{
    for (int K = 0; K < 6; K++) {
        for (int b = 0; b < 256; b++) {
            uint32_t lo = 0, hi = 0;
            for (int i = 0; i < 4; i++) {
                uint32_t bit_lo = ((b >> (7 - i)) & 1u) << K;
                lo |= bit_lo << (i * 8);
                uint32_t bit_hi = ((b >> (3 - i)) & 1u) << K;
                hi |= bit_hi << (i * 8);
            }
            plane_contrib[K][b][0] = lo;
            plane_contrib[K][b][1] = hi;
        }
    }
}


static int loaded_font_idx = -1;
static struct Bitplane font_bitplane[6];

static void (*copper_func)(int x, int y, uint32_t *palette);

extern const uint8_t _sota_wad_start[];

/* -------- palette -> 2:2:2 lookup ---------------------------------------- */

static void recompute_palette_rgb555(void)
{
    for (int i = 0; i < 64; i++) {
        uint32_t c = palette[i];
        uint32_t r8 = (c >> 16) & 0xffu;
        uint32_t g8 = (c >>  8) & 0xffu;
        uint32_t b8 =  c        & 0xffu;
        palette_rgb555[i] = (uint16_t)PICO_SCANVIDEO_PIXEL_FROM_RGB8(r8, g8, b8);
    }
}

/* -------- timing -------- */

uint64_t backend_get_time_ms(void)
{
    return (uint64_t)to_ms_since_boot(get_absolute_time());
}

bool backend_should_display_next_frame(int64_t time_remaining_this_frame)
{
    /* Sleep most of the slack budget, then block until the next vblank.
     * That way the engine's next-frame writes to backend_bitplane[]
     * begin during vertical blanking instead of mid-scanout, which is
     * what was producing the visible horizontal-stripe tearing. With an
     * 8-buffer scanline pool the worst-case visible tear band collapses
     * to a few scanlines near the top instead of streaks across the
     * frame. Still single-buffered — a full double-buffer would need
     * ~73 KB more SRAM than we currently have headroom for. */
    if (time_remaining_this_frame > 1)
        sleep_us((uint32_t)((time_remaining_this_frame - 1) * 1000));
    scanvideo_wait_for_vblank();
    return true;
}

/* -------- bitplane allocator (identical to other backends) --------------- */

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
    int padded_width = (width + 63) & ~63;
    int stride = padded_width / 8;

    backend_bitplane[idx].data_start = backend_bitplane[idx].data = bitplane_pool_next;
    bitplane_pool_next += (size_t)height * stride;
    if (bitplane_pool_next > bitplane_pool_end) {
        printf("bitplane pool overflow (idx=%d w=%d h=%d)\n", idx, padded_width, height);
        for (;;) tight_loop_contents();
    }

    backend_bitplane[idx].idx    = idx;
    backend_bitplane[idx].width  = padded_width;
    backend_bitplane[idx].height = height;
    backend_bitplane[idx].stride = stride;
    memset(backend_bitplane[idx].data_start, 0, (size_t)height * stride);
    return &backend_bitplane[idx];
}

void backend_allocate_standard_bitplanes(void)
{
    backend_set_new_scene();
    for (int i = 0; i < 6; i++)
        backend_allocate_bitplane(i, window_width, window_height);
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
    for (int i = 0; i < 32; i++) {
        uint32_t c = palette[i];
        palette[i + 32] = 0xff000000u
            | (((c & 0x00ff0000u) >> 17) << 16)
            | (((c & 0x0000ff00u) >>  9) <<  8)
            | (((c & 0x000000ffu) >>  1)      );
    }
    recompute_palette_rgb555();
}

void     backend_set_palette_element(int idx, uint32_t element) { palette[idx] = element; recompute_palette_rgb555(); }
uint32_t backend_get_palette_element(int idx)                   { return palette[idx]; }

void backend_register_copper_func(void (*func)(int x, int y, uint32_t *palette_arg))
{
    copper_func = func;
}

/* -------- scanline rendering on core 1 ---------------------------------- */

static void render_scanline(struct scanvideo_scanline_buffer *buffer)
{
    int y = scanvideo_scanline_number(buffer->scanline_id);
    uint16_t *out16 = (uint16_t *)buffer->data;

    int engine_y = y;                   /* 0..239 */

    /* The copper callback can mutate palette[] per scanline (iris fades,
     * SOTA's palette-animation tricks). palette_rgb555[] is otherwise
     * only refreshed inside backend_set_palette*, so we re-derive it
     * after the copper runs. */
    if (copper_func) {
        copper_func(0, engine_y, palette);
        recompute_palette_rgb555();
    }

    const uint8_t *p0 = backend_bitplane[0].data;
    const uint8_t *p1 = backend_bitplane[1].data;
    const uint8_t *p2 = backend_bitplane[2].data;
    const uint8_t *p3 = backend_bitplane[3].data;
    const uint8_t *p4 = backend_bitplane[4].data;
    const uint8_t *p5 = backend_bitplane[5].data;

    /* NULL plane = transient scene-transition gap. Substitute a zero row
     * so each bit goes to palette index 0 (whatever the scene declares
     * as background). */
    static const uint8_t zero_row[WIDTH / 8] = {0};
    if (!p0) p0 = zero_row; else p0 += engine_y * backend_bitplane[0].stride;
    if (!p1) p1 = zero_row; else p1 += engine_y * backend_bitplane[1].stride;
    if (!p2) p2 = zero_row; else p2 += engine_y * backend_bitplane[2].stride;
    if (!p3) p3 = zero_row; else p3 += engine_y * backend_bitplane[3].stride;
    if (!p4) p4 = zero_row; else p4 += engine_y * backend_bitplane[4].stride;
    if (!p5) p5 = zero_row; else p5 += engine_y * backend_bitplane[5].stride;

    /* 324 half-words: RAW_RUN + 1st pixel + count + 319 pixels + padding
     * + EOL_ALIGN. Engine 320 pixels emitted as 321 (1 extra into front
     * porch) so EOL_ALIGN lands at the high half of word 161. */
    static uint16_t pixels[WIDTH];
    const uint16_t *pal = palette_rgb555;
    int x = 0;
    for (int bx = 0; bx < WIDTH / 8; bx++) {
        uint32_t b0 = p0[bx], b1 = p1[bx], b2 = p2[bx],
                 b3 = p3[bx], b4 = p4[bx], b5 = p5[bx];
        uint32_t lo = plane_contrib[0][b0][0] | plane_contrib[1][b1][0]
                    | plane_contrib[2][b2][0] | plane_contrib[3][b3][0]
                    | plane_contrib[4][b4][0] | plane_contrib[5][b5][0];
        uint32_t hi = plane_contrib[0][b0][1] | plane_contrib[1][b1][1]
                    | plane_contrib[2][b2][1] | plane_contrib[3][b3][1]
                    | plane_contrib[4][b4][1] | plane_contrib[5][b5][1];
        pixels[x++] = pal[ lo        & 0xFFu];
        pixels[x++] = pal[(lo >>  8) & 0xFFu];
        pixels[x++] = pal[(lo >> 16) & 0xFFu];
        pixels[x++] = pal[(lo >> 24) & 0xFFu];
        pixels[x++] = pal[ hi        & 0xFFu];
        pixels[x++] = pal[(hi >>  8) & 0xFFu];
        pixels[x++] = pal[(hi >> 16) & 0xFFu];
        pixels[x++] = pal[(hi >> 24) & 0xFFu];
    }
    out16[0] = COMPOSABLE_RAW_RUN;
    out16[2] = WIDTH + 1 - 3;
    out16[1] = pixels[0];
    for (int i = 1; i < WIDTH; i++) out16[2 + i] = pixels[i];
    out16[322] = 0;
    out16[323] = COMPOSABLE_EOL_ALIGN;
    buffer->data_used = 162;
    buffer->status = SCANLINE_OK;
}

static void core1_main(void)
{
    scanvideo_setup(&vga_mode_320x240_60);
    scanvideo_timing_enable(true);

    while (true) {
        struct scanvideo_scanline_buffer *buf = scanvideo_begin_scanline_generation(true);
        render_scanline(buf);
        scanvideo_end_scanline_generation(buf);
    }
}

/* backend_render is a no-op here — core 1 is already streaming the bitplanes
 * out continuously. We accept a little tearing when the engine rewrites a
 * bitplane mid-frame; for SOTA it's basically invisible. */
void backend_render(void) { /* no-op */ }

/* -------- font ----------------------------------------------------------- */

/* fontmap.iff's BMHD says width=320, height=256, nPlanes=4 (16-colour
 * font). iff_display writes all 4 source planes and uses:
 *     font.scale = min(plane.width/iff.w, plane.height/iff.h)
 * If plane.height < iff.h that gives 0, and the font renders to a 0x0
 * region — i.e. invisibly. So the font bitplanes have to be 256 tall,
 * NOT the screen's 240. Allocate 4 planes × (320/8 × 256) = 40 KB. We
 * lazy-alloc on first font_load and reuse forever (no free). */
#define FONT_BITPLANE_WIDTH   320
#define FONT_BITPLANE_HEIGHT  256
#define FONT_BITPLANE_STRIDE  (FONT_BITPLANE_WIDTH / 8)
#define FONT_BITPLANE_BYTES   (FONT_BITPLANE_STRIDE * FONT_BITPLANE_HEIGHT)
#define FONT_BITPLANE_COUNT   4

bool backend_font_load(int file_idx, uint32_t startchar, uint32_t numchars, uint16_t *positions)
{
    for (int i = 0; i < FONT_BITPLANE_COUNT; i++) {
        font_bitplane[i].width  = FONT_BITPLANE_WIDTH;
        font_bitplane[i].height = FONT_BITPLANE_HEIGHT;
        font_bitplane[i].stride = FONT_BITPLANE_STRIDE;
        if (!font_bitplane[i].data_start) {
            font_bitplane[i].data_start = malloc(FONT_BITPLANE_BYTES);
            if (!font_bitplane[i].data_start) {
                printf("font_load: malloc %d failed\n", i);
                return false;
            }
        }
        font_bitplane[i].data = font_bitplane[i].data_start;
    }
    /* Make sure higher planes are zero/NULL so any iff_display path that
     * iterates beyond the active depth doesn't dereference garbage. */
    for (int i = FONT_BITPLANE_COUNT; i < 6; i++) {
        font_bitplane[i].data = font_bitplane[i].data_start = NULL;
        font_bitplane[i].width = font_bitplane[i].height = font_bitplane[i].stride = 0;
    }
    bool ok = ifffont_load(file_idx, startchar, numchars, positions, font_bitplane);
    if (ok) loaded_font_idx = file_idx;
    return ok;
}

void backend_font_unload(void)
{
    /* Don't free — keep the buffers around for the next font_load. */
    ifffont_unload();
}

void backend_font_draw(int numchars, char *text, int x, int y)
{
    if (x == -1) ifffont_centre(numchars, text, y, backend_bitplane);
    else         ifffont_draw  (numchars, text, x, y, backend_bitplane);
}

int backend_font_get_height(void) { return ifffont_get_height(); }

/* -------- WAD ------------------------------------------------------------ */

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

void backend_wad_unload_file(void *data) { (void)data; }

/* -------- misc ----------------------------------------------------------- */

void *backend_reserve_memory(size_t amt)
{
    amt = (amt + 3u) & ~(size_t)3u;
    if (reserve_pool_next + amt > reserve_pool + RESERVE_POOL_BYTES) {
        printf("backend_reserve_memory oom\n");
        for (;;) tight_loop_contents();
    }
    void *p = reserve_pool_next;
    reserve_pool_next += amt;
    return p;
}

int  backend_random(void) { return rand(); }

void backend_debug(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); putchar('\n');
}

/* -------- lifecycle ------------------------------------------------------ */

bool backend_init(int width, int height, bool fullscreen, const void *wad_name)
{
    (void)width; (void)height; (void)fullscreen; (void)wad_name;

    /* (sys_clk overclock and vreg bump now live in main.c, BEFORE
     * stdio_init_all, so USB CDC enumerates at the final 250 MHz from
     * the start.) */

    /* stdio_init_all() in main.c is shared with the UART-using backends.
     * It may have grabbed GP0/GP1 for UART_TX/RX (the SDK default UART
     * pins). On VGA those are red bits 0/1 — claim them back for PIO0
     * before scanvideo runs. */
    for (uint i = 0; i <= 17; i++) gpio_set_function(i, GPIO_FUNC_PIO0);

    window_width  = WIDTH;
    window_height = HEIGHT;
    copper_func   = NULL;
    loaded_font_idx = -1;

    /* Initialise palette to black so first-render colors don't read garbage. */
    for (int i = 0; i < 64; i++) palette[i] = 0xff000000;
    recompute_palette_rgb555();
    init_plane_contrib_table();

    backend_allocate_standard_bitplanes();

    multicore_launch_core1(core1_main);

    return true;
}

void backend_shutdown(void) { }

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
