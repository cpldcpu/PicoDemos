/* render.h -- what the renderer's files share. Overscan's; Phosphor owns
 * sleeper.h and song.h, and nothing here changes either.
 *
 * The rule the whole file obeys: there is no per-frame state. Every function
 * below is a pure function of the frame context F, which is a pure function
 * of the sample. A skipped frame skips; a seek lands; check.c proves it.
 */
#ifndef SLEEPER_RENDER_H
#define SLEEPER_RENDER_H
#include "sleeper.h"
#include "song.h"

/* ------------------------------------------------------------ the palette */
/* PLANNING §3 after Phase's critique, which gives the anchors in five-bit
 * RGB because that is what the DAC has. The eight-bit literals below all
 * truncate exactly onto those anchors -- silhouette (0,0,1), night top
 * (1,2,4), horizon (3,5,9), sodium (31,22,8)/(24,12,2), fluorescent
 * (27,30,31)/(16,19,24) -- and the brightest steps are reserved for small
 * cores. A macro and not rgb() because these are used in static
 * initialisers; the two agree bit for bit. */
#define RGB(r, g, b) ((uint16_t)(((r) >> 3) | (((g) & 248) << 3) | (((b) & 248) << 8)))
#define C_SKY_TOP   RGB(0x0A, 0x10, 0x20)   /* (1,2,4)                       */
#define C_SKY_HOR   RGB(0x18, 0x28, 0x48)   /* (3,5,9)                       */
#define C_LAND      RGB(0x04, 0x06, 0x08)   /* (0,0,1): only the blue lives  */
#define C_SODIUM    RGB(0xFF, 0xB0, 0x40)   /* (31,22,8)  core               */
#define C_SODIUM_H  RGB(0xC0, 0x60, 0x10)   /* (24,12,2)  halo               */
#define C_FLUO      RGB(0xD8, 0xF0, 0xFF)   /* (27,30,31) core               */
#define C_FLUO_H    RGB(0x80, 0x98, 0xC0)   /* (16,19,24) halo               */
#define C_GREEN     RGB(0x20, 0xFF, 0x60)
#define C_RED       RGB(0xFF, 0x20, 0x20)
#define C_MOON      RGB(0xE8, 0xEE, 0xF8)
#define C_DREAM     RGB(0x10, 0x18, 0x30)
#define C_TILE      RGB(0x10, 0x12, 0x1A)   /* the board's near-black tile   */
#define C_HAIR      RGB(0x00, 0x00, 0x00)   /* its hairline                  */

/* The horizon. Phase's plates put it at row 150 and everything below it is
 * black in the night plate and painted sea in the dawn plate, so every layer
 * measures from here and the plate drops in without moving anything. */
#define HORIZON 150

/* The one visual sentence (Phase): two rails, repeated crossbars, one light
 * beyond them. Repeated elements across every system -- platform tubes,
 * passing-train windows, bridge bays, catenary posts, the dream's crossbars
 * -- share this screen spacing, and the "one light" sits at the same screen
 * position in the stopped window at 64, at the dream's vanishing point from
 * 66, and back in the real window at 80. */
#define RHYTHM  74
#define MATCH_X 196.f
#define MATCH_Y 96.f

/* --------------------------------------------------------- frame context */
typedef struct {
    uint32_t     sample;
    const cut_t *cut;
    uint32_t     since;      /* samples since the cut began                 */
    uint32_t     bar;        /* absolute bar                                */
    unsigned     step;       /* 0..15 within the bar                        */
    uint32_t     bar_pos;    /* samples into the bar                        */
    const bar_t *b;
    int32_t      speed;      /* Q16 fraction of cruise                      */
    float        dist;       /* cruise-samples travelled; exact as a float  */
    float        metres;     /* the same, in metres of track                */
    float        ppf;        /* near-layer screen pixels per 1/60 s field   */
    int          lightlv;    /* 0 night .. 255 dawn                         */
    float        dawn;       /* lightlv / 255                               */
    unsigned     shot, world, flags, variant;
} frame_t;
extern frame_t F;
extern uint16_t *g_fb;
extern demo_stats_t g_stats;

/* Near-layer screen pixels per cruise-sample of distance. This is not a
 * taste knob: at cruise the train does 51.2 m/s (32 sleepers a beat at
 * 0.6 m), the near layer is about twelve metres out of the window, and the
 * focal length the ahead view uses is 300 px, so a near point crosses at
 * 51.2 * 300 / 12 = 1,280 px a second. That is 0.0533 units; 0.070 is the
 * same picture with the near layer at nine metres, which is where a lineside
 * lamp actually stands. It makes the near streak 28 px at cruise and 35 in
 * the second drop, and it is why the world's lamps are spaced in beats
 * (BEAT_PX) rather than in pixels -- the world is the sequencer. */
#define NEAR_PX_PER_UNIT 0.070f
/* One beat of near-layer travel, in screen pixels. Every lamp spacing in the
 * side world is a simple fraction or multiple of this, so lamps arrive on
 * the beat by construction and not by tuning. */
#define BEAT_PX (NEAR_PX_PER_UNIT * (float)BEAT_SAMPLES)
#define FIELD_SAMPLES    400.f    /* 24,000 Hz / 60 fields                   */
/* The track: 32 sleepers a beat at 0.6 m gives 19.2 m a beat, which is the
 * period of the rail texture's V and 184 km/h. One bar is four of them. */
#define METRES_PER_BEAT  19.2f
#define METRES_PER_UNIT  (METRES_PER_BEAT / (float)BEAT_SAMPLES)

/* ------------------------------------------------------- lights.c: pixels */
extern const uint8_t bayer4[16];
uint16_t mixc(uint16_t a, uint16_t b, int f);
void px(int x, int y, uint16_t c, int a);
void hspan(int y, int x0, int x1, uint16_t c);           /* solid, clipped   */
void vspan(int x, int y0, int y1, uint16_t c);           /* solid, clipped   */
void hspan_a(int y, int x0, int x1, uint16_t c, int a);
void line_a(int x, int y, int xx, int yy, uint16_t c, int a);
void halo(float cx, float cy, float radius, uint16_t c, int a);
void disc(float cx, float cy, float r, uint16_t c);
void rect(int x0, int y0, int x1, int y1, uint16_t c);   /* inclusive, solid */
void vgradient(int y0, int y1, int x0, int x1, uint16_t top, uint16_t bottom);
uint32_t rhash(uint32_t x);
float    rnd01(uint32_t h);
int      profile1(int wx, int cell, int amp, uint32_t seed);
float    fsin(float a);
float    fcos(float a);
void     lights_init(void);              /* builds the sine table, once     */

typedef struct {
    float    x, y;        /* screen position at the middle of the exposure  */
    float    vx, vy;      /* projected screen motion during one field, px   */
    float    radius;      /* halo radius                                    */
    uint16_t core, glow;
    int16_t  intensity;   /* halo alpha, 0..32, before the streak spreads it */
    int16_t  core_r;      /* hard core radius; 0 for none                   */
    /* How far above the top of a five-bit channel the light really is, which
     * is what decides how long a streak stays saturated before it starts to
     * dim. 0 means the default of 4, which is a lineside sodium lamp. A
     * tunnel lamp two and a half metres from the glass wants 16: it crosses
     * at a hundred pixels a field and it has to stay a bright line, and it
     * is the one case in the film where a light really does become one.   */
    int16_t  over;
} light_t;
void light_draw(const light_t *L);
/* A rectangular core, smeared along the same motion. The passing train's
 * windows and the dream's are windows, not bulbs, and a disc at 7x4 is an
 * oval whatever colour it is. Draws the halo through light_draw() and then
 * the rectangle, so the energy rule is the same one. */
void window_light(float x, float y, float vx, float vy, int w, int h,
                  float radius, uint16_t core, uint16_t glow, int intensity, int over);
void rect_a(int x0, int y0, int x1, int y1, uint16_t c, int a);
/* Daylight: PLANNING §13 -- past light 200 the sodium lamps are switched off
 * and the fluorescents drop to a quarter. Signals keep their intensity in
 * every light, because they are information and not illumination. */
int daylight_scale(uint16_t core, int intensity);
/* Scale the whole page toward black by f/32, a word at a time. The sleep
 * shot's lighting change, and the only full-page pass outside the sky. */
void page_dim(int f);

/* ------------------------------------------------------------- board.c    */
/* `cols` tiles wide, centred on the text; `shrink` 1 for the full face and
 * 2 for half. The station's own board is eleven columns at full size,
 * because at half size it is unreadable and it is the station's name. */
void board_at(int x0, int y0, int shrink, int cols, const board_t *now, const board_t *prev, uint32_t since);
void board_frame(void);                       /* the centred full board      */
void board_platform(int x0, int y0);          /* the station's own, full size */

/* ------------------------------------------------------------- the sky    */
/* Phase's indexed plates, expanded through a per-frame tinted palette. The
 * night plate serves bars 0-103; the dawn plate arrives on the cut at 104
 * and the two never blend. */
void sky_palette(void);                       /* rebuild the eight dithered ramps */
/* Screen rows [y0,y1) are written from plate rows plate_y0 + (y - y0). The
 * shift is what lets the ahead view -- whose horizon is at row 112, not 150
 * -- use the same painted sky with the same moon in it. */
void sky_expand(int y0, int y1, int plate_y0);
int  sky_is_dawn(void);

/* ------------------------------------------------------------- worlds     */
void world_side(void);
void world_ahead(void);
void world_under(void);
void world_up(void);
void world_tunnel_draw(void);
void world_dream(unsigned shot);
void far_layer(int baseline, float offset, int amp_div);
void rain_draw(void);                     /* droplets on the glass          */
void tunnel_grid_init(void);

/* ------------------------------------------------------------ profiling  */
/* Core 0's own SysTick (each core has one; video.c uses core 1's). A single
 * register read, so light_draw() can be timed without the measurement
 * costing more than the thing measured. Zero on the host. */
uint32_t prof_cyc(void);
uint32_t prof_since(uint32_t t0);
extern uint32_t g_lights_cy;
unsigned demo_cut_index(void);            /* for the dispatch log           */

/* ------------------------------------------------------------- textures  */
/* Two 128x128 indexed textures in SRAM and one shade bank. TEX_BITS is 7, so
 * INTERP1's mask is set once by texture_config(7) and every span is a POP.
 * tex_rail_avg is the rail texture averaged down V: the far rows of the
 * plane, where a screen row spans several sleepers, draw from it instead,
 * which is Phase's "merge subpixel sleepers into their average tone" and the
 * difference between a receding track and a moire. */
#define TEX_BITS 7
#define TEX_SIZE (1 << TEX_BITS)
extern uint8_t  tex_rail[TEX_SIZE * TEX_SIZE];
extern uint8_t  tex_ring[TEX_SIZE * TEX_SIZE];
extern uint8_t  tex_rail_avg[TEX_SIZE];
extern uint16_t tex_pal[16][256];          /* 16 depth shades, cool concrete */
extern uint16_t tex_pal_warm[16][256];     /* the same, sodium: the dream    */
void textures_init(void);
#endif
