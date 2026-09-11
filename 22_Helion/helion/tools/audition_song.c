/* HELION -- the instrument audition, as a second implementation of song.h.
 *
 * Phase asked to hear the palette before the film: each patch named, the
 * same simple phrase (theme A's question) through every pitched voice with
 * its attack, a held note and its release, mostly dry so the hall cannot
 * hide the character; then the percussion and the tam-tam; then a short
 * ensemble sketch, first dry, then with the space, then the tunnel drive
 * resolving to D major. Linked in place of song.c by song_audition.py.
 *
 *   bars  0-3    the electric reed        (0:00)
 *   bars  4-7    the bowed metal          (0:08)   unison D4, A4, Bb4, then the Dm chord
 *   bars  8-11   the struck bronze        (0:16)
 *   bars 12-15   the rubbery bass         (0:24)   two octaves down, a slide into the held note
 *   bars 16-19   the solo bowed harmonic  (0:32)
 *   bars 20-22   frame drum, rim, brushed metal (0:40)
 *   bar  23      the tam-tam, alone       (0:46)
 *   bars 24-27   ensemble, dry            (0:48)   Dm Bb F C, theme A's question
 *   bars 28-31   ensemble, with the space (0:56)   theme A's answer
 *   bars 32-35   the tunnel drive         (1:04)   Dm Bb Gm A, tam-tam
 *   bars 36-37   D major, the resolution  (1:12)
 */

#include "song.h"

#include <string.h>

#define AUD_BARS 38

enum {
    C2 = 36, D2 = 38, E2 = 40, F2 = 41, A2 = 45, Bb2 = 46,
    D3 = 50, F3 = 53, A3 = 57, Bb3 = 58, C4 = 60, D4 = 62, E4 = 64, F4 = 65, Fs4 = 66, A4 = 69, Bb4 = 70,
    C5 = 72, D5 = 74, E5 = 76, F5 = 77, A5 = 81, Bb5 = 82
};
#define __ 0
#define XX SONG_OFF
#define ON SONG_BOW_ON

/* theme A's question, and its answer, as absolute steps */
static const uint8_t question[4][16] = {
    { D5, __, __, __, __, __, A5, __, __, __, __, __, __, __, __, __ },
    { Bb5,__, __, __, A5, __, __, __, F5, __, __, __, __, __, __, __ },
    { E5, __, __, __, __, __, D5, __, __, __, __, __, C5, __, __, __ },
    { D5, __, __, __, __, __, __, __, __, __, __, __, XX, __, __, __ },
};
static const uint8_t answer[4][16] = {
    { D5, __, __, __, __, __, A5, __, __, __, __, __, __, __, __, __ },
    { Bb5,__, __, __, 84, __, __, __, 86, __, __, __, __, __, __, __ },
    { 84, __, __, __, __, __, A5, __, __, __, __, __, 79, __, __, __ },
    { A5, __, __, __, __, __, __, __, __, __, __, __, XX, __, __, __ },
};
/* the same question for the bass, two octaves down, short notes, a slide into the held one */
static const uint8_t bass_q[4][16] = {
    { D2, __, XX, __, __, __, A2, __, XX, __, __, __, __, __, __, __ },
    { Bb2,__, XX, __, A2, __, XX, __, F2, __, XX, __, __, __, __, __ },
    { E2, __, XX, __, __, __, D2, __, XX, __, __, __, C2, __, __, __ },
    { D2 | SONG_SLIDE, __, __, __, __, __, __, __, __, __, __, __, XX, __, __, __ },
};
/* the harmonic: slow */
static const uint8_t harm_q[4][16] = {
    { D5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    { A5, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __ },
    { Bb5,__, __, __, __, __, __, __, A5, __, __, __, __, __, __, __ },
    { F5, __, __, __, __, __, __, __, __, __, __, __, XX, __, __, __ },
};

#define D DR_DRUM
#define G DR_GHOST
#define R DR_RIM
#define S DR_SCRAPE
#define L DR_SCRAPEL
static const uint8_t drums_solo[3][16] = {
    { D,0,0,0, 0,0,0,G, D,0,0,0, R,0,0,0 },
    { D,0,0,S, R,0,0,G, D,0,S,0, R,0,S,L },
    { D,0,S,0, R,0,D,0, D,S,S,0, R,0,D|S,L },
};
static const uint8_t drums_flight[16] = { D,0,0,0, R,0,0,G, D,0,S,0, R,0,0,G };
static const uint8_t drums_tunnel[16] = { D,0,S,S, R,0,D,0, D,0,S,S, R,S,D,S };
#undef D
#undef G
#undef R
#undef S
#undef L

/* ensemble bars: chord index into the small chord set below */
enum { CH_DM, CH_BB, CH_F, CH_C, CH_GM, CH_A, CH_D };
static const uint8_t root[7]      = { 38, 34, 41, 36, 43, 33, 38 };
static const uint8_t bow[7][4]    = { { 50, 57, 62, 65 }, { 50, 53, 58, 65 }, { 48, 57, 60, 65 }, { 48, 55, 60, 64 },
                                      { 50, 55, 58, 62 }, { 52, 57, 61, 64 }, { 50, 57, 62, 66 } };
static const uint8_t bronze[7][6] = { { 57, 62, 65, 69, 74, 77 }, { 53, 58, 62, 65, 70, 74 }, { 57, 60, 65, 69, 72, 77 },
                                      { 55, 60, 64, 67, 72, 76 }, { 55, 58, 62, 67, 70, 74 }, { 57, 61, 64, 69, 73, 76 },
                                      { 57, 62, 66, 69, 74, 78 } };
static const uint8_t ens_chord[14]  = { CH_DM, CH_BB, CH_F, CH_C, CH_DM, CH_BB, CH_F, CH_C, CH_DM, CH_BB, CH_GM, CH_A, CH_D, CH_D };
static const uint8_t bass_pulse[16] = { 8,0,1,0, 0,0,8,0, 1,0,8,0, 1,0,15,0 };           /* offset + 8, as song.c */
static const uint8_t bass_drive[16] = { 8,1,8,1, 8,1,8,1, 8,1,15,1, 8,1,8,1 };
static const uint8_t bronze_ans[16] = { 0,0,0,0, 0,0,4,0, 0,0,6,0, 0,5,0,0 };
static const uint8_t bronze_riff[16] = { 5,0,0,5, 0,0,5,0, 0,7,0,0, 5,0,6,0 };
static const uint8_t bow_phrase[16] = { ON,0,0,0, 0,0,0,0, 0,0,0,0, XX,0,0,0 };
static const uint8_t bow_stab[16]   = { ON,0,0,XX, 0,0,0,0, 0,0,ON,0, 0,XX,0,0 };
static const uint8_t bow_hold[16]   = { ON,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
static const uint8_t bow_rel[16]    = { 0,0,0,0, 0,0,0,0, 0,0,0,0, XX,0,0,0 };

static int section_of(uint32_t bar)
{
    if (bar < 4)  return 0;   /* reed      */
    if (bar < 8)  return 1;   /* bow       */
    if (bar < 12) return 2;   /* bronze    */
    if (bar < 16) return 3;   /* bass      */
    if (bar < 20) return 4;   /* harmonic  */
    if (bar < 24) return 5;   /* drums, gong */
    return 6;                 /* ensemble  */
}

static const char *const names[] = {
    "electric reed", "bowed metal", "struck bronze", "rubbery bass", "bowed harmonic", "percussion", "ensemble",
};

int song_section(uint32_t bar) { return bar < AUD_BARS ? section_of(bar) : 6; }
const char *song_section_name(int s) { return (s >= 0 && s < 7) ? names[s] : "?"; }

static int first_step_off(uint32_t s) { return s == 0 ? SONG_OFF : 0; }

uint8_t song_drums(uint32_t step)
{
    const uint32_t bar = step / 16, s = step % 16;
    if (bar >= 20 && bar < 23) return drums_solo[bar - 20][s];
    if (bar == 23) return s == 0 ? DR_GONG : 0;
    if (bar >= 24 && bar < 32) return drums_flight[s];
    if (bar >= 32 && bar < 36) return (uint8_t)(drums_tunnel[s] | (bar == 32 && s == 0 ? DR_GONG : 0));
    if (bar == 36) return s == 0 ? DR_GONGSOFT : 0;
    return 0;
}

int song_lead(uint32_t step)
{
    const uint32_t bar = step / 16, s = step % 16;
    if (bar < 4)                return question[bar][s];
    if (bar >= 24 && bar < 28)  return question[bar - 24][s];
    if (bar >= 28 && bar < 32)  return answer[bar - 28][s];
    if (bar >= 32 && bar < 36)  return question[bar - 32][s];
    if (bar == 36)              return s == 0 ? D5 : 0;
    if (bar == 37)              return s == 8 ? SONG_OFF : 0;
    return first_step_off(s);
}

int song_harm(uint32_t step)
{
    const uint32_t bar = step / 16, s = step % 16;
    if (bar >= 16 && bar < 20) return harm_q[bar - 16][s];
    return first_step_off(s);
}

int song_bronze(uint32_t step)
{
    const uint32_t bar = step / 16, s = step % 16;
    if (bar >= 8 && bar < 12) { const uint8_t e = question[bar - 8][s]; return e >= 2 ? e : 0; }
    if (bar >= 24 && bar < 36) {
        const uint8_t e = (bar >= 32 ? bronze_riff : bronze_ans)[s];
        return e >= 2 ? bronze[ens_chord[bar - 24]][e - 2] : 0;
    }
    return 0;
}

int song_bass(uint32_t step)
{
    const uint32_t bar = step / 16, s = step % 16;
    if (bar >= 12 && bar < 16) return bass_q[bar - 12][s];
    if (bar >= 24 && bar < 36) {
        const uint8_t e = (bar >= 32 ? bass_drive : bass_pulse)[s];
        return e < 2 ? e : root[ens_chord[bar - 24]] + (int)e - 8;
    }
    if (bar == 36) return s == 0 ? 38 : (s == 12 ? SONG_OFF : 0);
    return first_step_off(s);
}

void song_bow_chord(uint32_t bar, uint8_t out[4])
{
    memset(out, 0, 4);
    if (bar >= 4 && bar < 7) { const uint8_t n = bar == 4 ? D4 : bar == 5 ? A4 : Bb4; out[0] = out[1] = out[2] = out[3] = n; return; }
    if (bar == 7) { memcpy(out, bow[CH_DM], 4); return; }
    if (bar >= 24 && bar < 38) memcpy(out, bow[ens_chord[bar - 24]], 4);
}

int song_bow_gate(uint32_t step)
{
    const uint32_t bar = step / 16, s = step % 16;
    if (bar >= 4 && bar < 8)   return bow_phrase[s];
    if (bar >= 24 && bar < 32) return bow_phrase[s];
    if (bar >= 32 && bar < 36) return bow_stab[s];
    if (bar == 36)             return bow_hold[s];
    if (bar == 37)             return bow_rel[s];
    return first_step_off(s);
}

int song_lead_level(uint32_t bar)   { return bar < AUD_BARS ? 230 : 0; }
int song_lead_push(uint32_t bar)    { return bar < 4 ? 120 : bar >= 32 ? 220 : 150; }
int song_bow_level(uint32_t bar)    { return bar < AUD_BARS ? (bar >= 36 ? 220 : 190) : 0; }
int song_bow_open(uint32_t bar)     { return bar >= 36 ? 220 : 140; }
int song_bronze_level(uint32_t bar) { return bar < AUD_BARS ? 210 : 0; }
int song_bronze_decay(uint32_t bar) { return bar >= 32 ? 100 : 160; }
int song_bass_level(uint32_t bar)   { return bar < AUD_BARS ? 230 : 0; }
int song_bass_bite(uint32_t bar)    { return bar >= 32 ? 220 : 170; }
int song_harm_level(uint32_t bar)   { return bar < AUD_BARS ? 230 : 0; }
int song_air_level(uint32_t bar)    { (void)bar; return 0; }
int song_space(uint32_t bar)        { return bar >= 28 ? (bar >= 32 && bar < 36 ? 120 : 200) : 40; }
int song_energy(uint32_t bar)       { return bar < AUD_BARS ? 128 : 0; }

uint32_t song_voices(uint32_t bar)
{
    switch (song_section(bar)) {
    case 0: return SV_REED;
    case 1: return SV_BOW;
    case 2: return SV_BRONZE;
    case 3: return SV_BASS;
    case 4: return SV_HARM;
    case 5: return SV_DRUM | SV_RIM | SV_SCRAPE | SV_GONG;
    default: return bar < AUD_BARS ? (SV_DRUM | SV_RIM | SV_SCRAPE | SV_BASS | SV_BRONZE | SV_REED | SV_BOW) : 0;
    }
}
