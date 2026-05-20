/* SOTA backend.h implementation for the RP2040 + composite/S-Video Y-only
 * 1-bit-with-Bayer-dither output target. Mirrors pico_st7789_backend.c
 * in shape but emits a pre-encoded composite framebuffer driven by PIO+DMA
 * instead of streaming RGB-565 to a panel.
 *
 * Native resolution: 320x256 @ 50 Hz PAL — matches the original Amiga,
 * no aspect squashing.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"

#include "backend.h"
#include "minmax.h"
#include "iff-font.h"
#include "wad.h"
#include "choreography.h"
#include "choreography_commands.h"
#include "sound.h"

#include "composite.pio.h"

/* -------- timing constants ----------------------------------------------- */
/* PIO at 6.25 MHz pixel rate (sysclk/20). 32+32+320+16 = 400 px = 64 µs/line.
 * 4 bits per pixel (3 video LSB..MSB, sync) drive a 3-bit binary-weighted DAC
 * for 8 actual grey levels; combined with a 4x4 Bayer matrix that doubles
 * the effective level count to ~128 perceived greys per row. */
#define LINE_PX            400
#define HSYNC_PX           32
#define BACK_PORCH_PX      32
#define ACTIVE_PX          320
#define FRONT_PORCH_PX     16

#define LINES_VSYNC        3
#define LINES_VBL_TOP      22
#define LINES_ACTIVE       256
#define LINES_VBL_BOT      31
#define LINES_TOTAL        (LINES_VSYNC + LINES_VBL_TOP + LINES_ACTIVE + LINES_VBL_BOT)
_Static_assert(LINES_TOTAL == 312, "PAL line count");

/* 8 pixels per FIFO word (4 bits each, all 32 bits used). */
#define PIXELS_PER_WORD       8
#define WORDS_PER_LINE        (LINE_PX / PIXELS_PER_WORD)              /* 50 */
#define ACTIVE_WORD_OFFSET    ((HSYNC_PX + BACK_PORCH_PX) / PIXELS_PER_WORD)  /* 8 */
#define ACTIVE_WORDS_PER_LINE (ACTIVE_PX / PIXELS_PER_WORD)            /* 40 */
_Static_assert((HSYNC_PX + BACK_PORCH_PX) % PIXELS_PER_WORD == 0,
               "active video must start on an 8-pixel word boundary");
_Static_assert(ACTIVE_PX % PIXELS_PER_WORD == 0,
               "active video width must be an 8-pixel multiple");

#define PIN_BASE 12                /* GP12/13/14/15 = vidL / vid1 / vidM / sync */
#define PIN_VIDEO_L  (PIN_BASE + 0)   /* through 2 kΩ   — LSB              */
#define PIN_VIDEO_1  (PIN_BASE + 1)   /* through 1 kΩ                       */
#define PIN_VIDEO_M  (PIN_BASE + 2)   /* through 430 Ω  — MSB              */
#define PIN_SYNC     (PIN_BASE + 3)   /* through 1 kΩ                       */

/* 4-bit pixel encodings, bit layout matches PIN_BASE:
 *   bit 0 = vidL (GP12), bit 1 = vid1 (GP13), bit 2 = vidM (GP14), bit 3 = sync */
#define PIX_SYNC_LOW   0x0    /* sync tip — all low                       */
#define PIX_LEVEL0     0x8    /* sync hi, video 000 — BLACK               */
#define PIX_LEVEL1     0x9
#define PIX_LEVEL2     0xA
#define PIX_LEVEL3     0xB
#define PIX_LEVEL4     0xC
#define PIX_LEVEL5     0xD
#define PIX_LEVEL6     0xE
#define PIX_LEVEL7     0xF    /* sync hi, video 111 — WHITE               */
#define PIX_BLACK      PIX_LEVEL0   /* alias for sync/blanking init        */

/* Pack 8 identical 4-bit pixels into one 32-bit FIFO word. */
#define WORD_OF(p)     (((uint32_t)(p)) * 0x11111111u)

#define WIDTH  ACTIVE_PX
#define HEIGHT LINES_ACTIVE

#define MS_PER_FRAME 20

/* -------- big buffers (BSS) ----------------------------------------------- */

static uint32_t framebuf[LINES_TOTAL][WORDS_PER_LINE];

/* Bitplane pool: 10x one-plane scratch. With native 320x256 a single plane
 * is 40 * 256 = 10240 B, so 10x = 102400 B. Holds the 1*2X2 + 5*1X1 peak
 * (90 KB) of jump-1 / iris-vs-glitchy / static-dancers. */
#define BITPLANE_POOL_BYTES (10 * 40 * 256)
static uint8_t bitplane_pool[BITPLANE_POOL_BYTES] __attribute__((aligned(4)));
static uint8_t *bitplane_pool_next = bitplane_pool;
static uint8_t * const bitplane_pool_end = bitplane_pool + BITPLANE_POOL_BYTES;

#define RESERVE_POOL_BYTES (8 * 1024)
static uint8_t reserve_pool[RESERVE_POOL_BYTES] __attribute__((aligned(4)));
static uint8_t *reserve_pool_next = reserve_pool;

/* External engine state (made non-extern in backend.h). */
int window_width, window_height;
struct Bitplane backend_bitplane[6];

/* 32 + 32 EHB. Same format as desktop / ST7789 backend. */
static uint32_t palette[64];
static uint8_t  palette_luma[64];   /* 0..255 grey level, recomputed on palette change */

static int loaded_font_idx = -1;
static struct Bitplane font_bitplane[6];

static void (*copper_func)(int x, int y, uint32_t *palette);

extern const uint8_t _sota_wad_start[];

/* -------- PIO + DMA ------------------------------------------------------- */

static int dma_chan;

static void __isr composite_dma_isr(void) {
    dma_hw->ints0 = 1u << dma_chan;
    dma_channel_set_trans_count(dma_chan, LINES_TOTAL * WORDS_PER_LINE, false);
    dma_channel_set_read_addr(dma_chan, framebuf, true);
}

static void composite_pio_init(void) {
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &composite_video_4bit_program);

    float clkdiv = (float)clock_get_hz(clk_sys) / 6250000.0f;
    composite_video_4bit_init(pio, sm, offset, PIN_BASE, clkdiv);

    dma_chan = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(dma_chan, &c, &pio->txf[sm],
                          framebuf, LINES_TOTAL * WORDS_PER_LINE, false);

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, composite_dma_isr);
    irq_set_enabled(DMA_IRQ_0, true);

    dma_channel_start(dma_chan);
}

/* Fill the framebuffer with valid sync regions; active rows start as
 * solid black (PIX_BLACK encoded everywhere in the active span).
 * Run once at boot. backend_render rewrites only the active words. */
static void framebuf_init(void) {
    /* Pre-built constant words for blanking regions. */
    const uint32_t W_SYNC  = WORD_OF(PIX_SYNC_LOW);
    const uint32_t W_BLACK = WORD_OF(PIX_BLACK);

    for (int line = 0; line < LINES_TOTAL; line++) {
        bool is_vsync = (line < LINES_VSYNC);
        for (int w = 0; w < WORDS_PER_LINE; w++) {
            int px = w * PIXELS_PER_WORD;
            if (is_vsync) {
                framebuf[line][w] = W_SYNC;            /* whole-line sync low */
            } else if (px < HSYNC_PX) {
                framebuf[line][w] = W_SYNC;            /* HSYNC pulse */
            } else if (px >= LINE_PX - FRONT_PORCH_PX) {
                framebuf[line][w] = W_BLACK;           /* front porch */
            } else if (px < HSYNC_PX + BACK_PORCH_PX) {
                framebuf[line][w] = W_BLACK;           /* back porch */
            } else {
                framebuf[line][w] = W_BLACK;           /* active region: black until first render */
            }
        }
    }
}

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
    return true;
}

/* -------- bitplane allocator -------- */

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

    /* Pad bitplane width to a multiple of 64 — see the matching comment
     * in pico_st7789_backend.c. Without this, Cortex-M0+ unaligned 32-bit
     * accesses inside graphics.c will hard-fault. */
    int padded_width = (width + 63) & ~63;
    int stride = padded_width / 8;

    backend_bitplane[idx].data_start = backend_bitplane[idx].data = bitplane_pool_next;
    bitplane_pool_next += (size_t)height * stride;

    if (bitplane_pool_next > bitplane_pool_end) {
        printf("Bitplane alloc overflow (idx=%d w=%d h=%d)\n",
               idx, padded_width, height);
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

static void recompute_luma(void)
{
    /* Y = 0.299 R + 0.587 G + 0.114 B, integer approximation. */
    for (int i = 0; i < 64; i++) {
        uint32_t c = palette[i];
        uint32_t r = (c >> 16) & 0xff;
        uint32_t g = (c >>  8) & 0xff;
        uint32_t b =  c        & 0xff;
        uint32_t y = (77 * r + 150 * g + 29 * b) >> 8;
        palette_luma[i] = (uint8_t)y;
    }
}

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
    recompute_luma();
}

void     backend_set_palette_element(int idx, uint32_t element) { palette[idx] = element; recompute_luma(); }
uint32_t backend_get_palette_element(int idx)                   { return palette[idx]; }

void backend_register_copper_func(void (*func)(int x, int y, uint32_t *palette_arg))
{
    copper_func = func;
}

/* -------- render: bitplanes -> 1-bit dither -> active-region words ------- */

/* Standard 4x4 Bayer matrix, values 0..15. Added to 7-bit-scaled luma
 * and shifted >> 4 to quantise to 8 grey levels with ordered dithering. */
static const uint8_t bayer4_15[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

void backend_render(void)
{
    const uint8_t *row[6];
    int            stride[6];
    for (int k = 0; k < 6; k++) {
        row[k]    = backend_bitplane[k].data;
        stride[k] = backend_bitplane[k].stride;
    }

    for (int y = 0; y < HEIGHT; y++) {
        if (copper_func) copper_func(0, y, palette);

        const uint8_t *p0 = row[0], *p1 = row[1], *p2 = row[2];
        const uint8_t *p3 = row[3], *p4 = row[4], *p5 = row[5];

        const uint8_t *bayer_row = bayer4_15[y & 3];

        uint32_t *fb_words = &framebuf[LINES_VSYNC + LINES_VBL_TOP + y][ACTIVE_WORD_OFFSET];

        /* One bitplane byte = 8 logical pixels = 1 output word (32 bits,
         * 4 bits/pixel: bit 3 = sync hi, bits 0..2 = video level). */
        for (int byte_x = 0; byte_x < ACTIVE_WORDS_PER_LINE; byte_x++) {
            uint8_t b0 = p0 ? p0[byte_x] : 0;
            uint8_t b1 = p1 ? p1[byte_x] : 0;
            uint8_t b2 = p2 ? p2[byte_x] : 0;
            uint8_t b3 = p3 ? p3[byte_x] : 0;
            uint8_t b4 = p4 ? p4[byte_x] : 0;
            uint8_t b5 = p5 ? p5[byte_x] : 0;

            uint32_t out = 0;
            uint8_t  bit_mask = 0x80;
            int      x_base   = byte_x * 8;

            for (int b = 0; b < 8; b++) {
                uint32_t idx =
                      ((b0 & bit_mask) ? 1u  : 0)
                    | ((b1 & bit_mask) ? 2u  : 0)
                    | ((b2 & bit_mask) ? 4u  : 0)
                    | ((b3 & bit_mask) ? 8u  : 0)
                    | ((b4 & bit_mask) ? 16u : 0)
                    | ((b5 & bit_mask) ? 32u : 0);
                bit_mask >>= 1;

                /* luma 0..255 -> scaled 0..127 -> add bayer 0..15 -> >> 4 */
                uint8_t luma7 = palette_luma[idx] >> 1;          /* 0..127 */
                uint8_t bayer = bayer_row[(x_base + b) & 3];     /* 0..15  */
                uint32_t level = (luma7 + bayer) >> 4;            /* 0..8   */
                if (level > 7) level = 7;

                /* PIX_LEVEL0..7 = 0x8..0xF = 8 + level. */
                uint32_t pix = 8u + level;
                out |= pix << (b * 4);
            }
            fb_words[byte_x] = out;
        }

        for (int k = 0; k < 6; k++) row[k] += stride[k];
    }
}

/* -------- font (forwards to iff-font, malloc'd from newlib heap) -------- */

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

void backend_wad_unload_file(void *data) { (void)data; }

/* -------- misc -------- */

void *backend_reserve_memory(size_t amt)
{
    amt = (amt + 3u) & ~(size_t)3u;
    if (reserve_pool_next + amt > reserve_pool + RESERVE_POOL_BYTES) {
        printf("backend_reserve_memory: oom\n");
        for (;;) tight_loop_contents();
    }
    void *p = reserve_pool_next;
    reserve_pool_next += amt;
    return p;
}

int backend_random(void) { return rand(); }

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
    (void)width; (void)height; (void)fullscreen; (void)wad_name;

    window_width  = WIDTH;     /* 320 */
    window_height = HEIGHT;    /* 256 */

    copper_func = NULL;
    loaded_font_idx = -1;

    framebuf_init();
    composite_pio_init();

    backend_allocate_standard_bitplanes();
    return true;
}

void backend_shutdown(void) {}

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
