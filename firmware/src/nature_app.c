/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Gorgonzola777 */
/* NATURE, the app: the FM-1 as a generative nature machine, nothing else. It takes the screen, the buttons, the
 * knobs and the LEDs from Felucca's UI (main.c runs nui_* instead of ui_input / ui_leds / ui_draw); Felucca's sound
 * (the NATURE engine on track 1, its reverb), USB, MIDI and the update path stay as they are. OCT- + OCT+ held 5 s
 * still enters UBOOT (main.c), for a reinstall whatever happens here.
 *
 *   WHITE KEYS  F3 SHORE  G3 STORM  A3 FOREST  B3 CAMPFIRE  C4 RAIN  D4 NIGHT  E4 DRIFT     (places)
 *               F4 WIND  G4 SEA  A4 RAIN  B4 FIRE  C5 BIRDS                                (layers on / off)
 *               D5 MEADOW  E5 MONSOON  F5 WINTER  G5 DAWN                                   (more places)
 *   BLACK KEYS  events (eng_nature.c): THUNDER WAVE GUST DOWNPOUR CRACKLE BIRD CHORUS CALM STORM CRICKETS SHUFFLE;
 *               the key's pressure plays the event harder
 *   KNOB 1..4   the page's four: MAIN  MOOD (calm .. storm)  LIFE (how much it changes by itself)  TONE  VOL
 *               FX (again: FX 2)  REVERB ECHO ECHO-TIME REPEATS / CHORUS DRIVE ROOM TYPE
 *               LFO  TARGET RATE DEPTH SHAPE      ENV  FADE-IN FADE-OUT GLIDE (place to place) EVENT (length)
 *               EDIT (again: MIX 2)  WIND SEA RAIN FIRE / BIRDS CRICKETS THUNDER SENS (the keys' pressure)
 *   SELECT      SPACE (reverb)        ALGORITHM  TIME (day .. night)       PRESETS  the places, one by one
 *   PLAY        pause / play (fades)  HOME  back to MAIN; on MAIN the place as it was (every change undone)
 *   OCT- / OCT+ previous / next place
 *   SCL GLO SAVE ARP SEQ REC: HELP (the map on the screen); any button or key: back */
#include "nf_font.h"

#define NUI_BG NC(0x151213u)
#define NUI_PANEL NC(0x201b22u)
#define NUI_TEXT NC(0xf4f1e8u)
#define NUI_DIM NC(0x8a8494u)
#define NUI_OFF NC(0x3a3340u)
#define NUI_ACC NC(0xff8f5au)
#define NUI_SUN NC(0xffd86bu)
#define NUI_TOP_H 124u                                 /* the scene's strip (header + scene), then the HUD's */
#define NUI_BOT_H 116u

/* {name, WIND SEA RAIN FIRE BIRD, MOOD LIFE TONE, TIME (night), SPACE (reverb)} */
typedef struct { const char *name; uint8_t lv[NL_COUNT], mood, life, tone, night, space; } nui_place_t;
static const nui_place_t NUI_PLACES[] = {
    {"SHORE", {45, 110, 0, 0, 45}, 30, 60, 64, 0, 50},
    {"STORM", {110, 70, 110, 0, 0}, 118, 45, 56, 10, 40},
    {"FOREST", {40, 0, 0, 0, 105}, 15, 70, 70, 0, 70},
    {"CAMPFIRE", {30, 0, 0, 110, 25}, 20, 50, 60, 70, 35},
    {"RAIN", {30, 0, 100, 0, 15}, 60, 50, 64, 0, 45},
    {"NIGHT", {25, 60, 0, 0, 10}, 10, 50, 58, 127, 60},
    {"DRIFT", {70, 85, 60, 45, 60}, 50, 127, 64, 30, 60},
    {"MEADOW", {55, 0, 0, 0, 90}, 25, 60, 72, 0, 55},
    {"MONSOON", {80, 90, 120, 0, 0}, 100, 40, 56, 20, 45},
    {"WINTER", {115, 0, 0, 30, 0}, 70, 60, 48, 60, 50},
    {"DAWN", {20, 70, 0, 0, 120}, 5, 60, 76, 0, 65},
};
#define NUI_NPLACE ((uint32_t)(sizeof NUI_PLACES / sizeof NUI_PLACES[0]))
static const char *const NUI_LAYER[NL_COUNT] = {"WIND", "SEA", "RAIN", "FIRE", "BIRDS"};
static const char *const NUI_EVENT[EV_COUNT] = {"THUNDER", "BIG WAVE", "GUST", "DOWNPOUR", "CRACKLE", "BIRD",
                                                "BIRD CHORUS", "CALM", "STORM", "CRICKETS", "SHUFFLE"};
/* the 27 keys from F3: 0..10 a place, 20 + l a layer, 40 + e an event (nui_keymap) */
static uint8_t nui_key[27];
static const char *const NUI_KNAME[27] = {"F3", "F#3", "G3", "G#3", "A3", "A#3", "B3", "C4", "C#4", "D4", "D#4", "E4",
                                          "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4", "C5", "C#5", "D5", "D#5", "E5",
                                          "F5", "F#5", "G5"};

static void nui_keymap(void)
{
    /* white keys: F3 G3 A3 B3 C4 D4 E4 places 0..6, F4 G4 A4 B4 C5 layers, D5 E5 F5 G5 places 7..10; black: events */
    static const uint8_t WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    static const uint8_t BLACK[11] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
    uint32_t i;
    for (i = 0; i < 16u; i++)
        nui_key[WHITE[i]] = (uint8_t)(i < 7u ? i : i < 12u ? 20u + (i - 7u) : 7u + (i - 12u));
    for (i = 0; i < 11u; i++)
        nui_key[BLACK[i]] = (uint8_t)(40u + i);
}

/* the values the knobs set (0..127 each, LIST ones an index) */
enum {
    K_MOOD, K_LIFE, K_TONE, K_VOL, K_SPACE, K_TIME,
    K_ECHO, K_ETIME, K_EFDBK, K_CHORUS, K_DRIVE, K_RSIZE, K_RTYPE,
    K_LTGT, K_LRATE, K_LDEPTH, K_LSHAPE,
    K_FIN, K_FOUT, K_GLIDE, K_EVLEN,
    K_WIND, K_SEA, K_RAIN, K_FIRE, K_BIRDS, K_CRICK, K_THUN, K_SENS,
    K_N
};
enum { KT_PCT, KT_LIST, KT_HZ, KT_SEC };
typedef struct { const char *label; uint8_t kind, max, def; const char *const *names; } nui_knob_t;
static const char *const N_ETIME[] = {"60MS", "125MS", "250MS", "375MS", "500MS", "750MS", "1S", "2S"};
static const uint8_t ETIME_DIV[8] = {3, 2, 1, 10, 0, 12, 6, 7};   /* G_DTIME at 120 BPM: 1/32 .. 1/1 */
static const char *const N_RTYPE2[] = {"ROOM", "SPRING", "HALL"};
static const char *const N_LTGT[] = {"MOOD", "TONE", "WIND", "SEA", "RAIN", "FIRE", "BIRDS", "VOL", "SPACE", "ECHO"};
static const char *const N_LSHAPE[] = {"SINE", "TRI", "SQUARE", "RANDOM"};
static const uint8_t LTGT_K[10] = {K_MOOD, K_TONE, K_WIND, K_SEA, K_RAIN, K_FIRE, K_BIRDS, K_VOL, K_SPACE, K_ECHO};
static const nui_knob_t NUI_K[K_N] = {
    [K_MOOD] = {"MOOD", KT_PCT, 127, 30, 0},       [K_LIFE] = {"LIFE", KT_PCT, 127, 60, 0},
    [K_TONE] = {"TONE", KT_PCT, 127, 64, 0},       [K_VOL] = {"VOL", KT_PCT, 127, 104, 0},
    [K_SPACE] = {"REVERB", KT_PCT, 127, 50, 0},    [K_TIME] = {"TIME", KT_PCT, 127, 0, 0},
    [K_ECHO] = {"ECHO", KT_PCT, 127, 0, 0},        [K_ETIME] = {"TIME", KT_LIST, 7, 4, N_ETIME},
    [K_EFDBK] = {"REPEAT", KT_PCT, 127, 50, 0},    [K_CHORUS] = {"CHORUS", KT_PCT, 127, 0, 0},
    [K_DRIVE] = {"DRIVE", KT_PCT, 127, 0, 0},      [K_RSIZE] = {"ROOM", KT_PCT, 127, 90, 0},
    [K_RTYPE] = {"TYPE", KT_LIST, 2, 2, N_RTYPE2},
    [K_LTGT] = {"TARGET", KT_LIST, 9, 0, N_LTGT},  [K_LRATE] = {"RATE", KT_HZ, 127, 30, 0},
    [K_LDEPTH] = {"DEPTH", KT_PCT, 127, 0, 0},     [K_LSHAPE] = {"SHAPE", KT_LIST, 3, 0, N_LSHAPE},
    [K_FIN] = {"FADE IN", KT_SEC, 127, 45, 0},     [K_FOUT] = {"FADE OUT", KT_SEC, 127, 70, 0},
    [K_GLIDE] = {"GLIDE", KT_SEC, 127, 45, 0},     [K_EVLEN] = {"EVENT", KT_SEC, 127, 64, 0},
    [K_WIND] = {"WIND", KT_PCT, 127, 0, 0},        [K_SEA] = {"SEA", KT_PCT, 127, 0, 0},
    [K_RAIN] = {"RAIN", KT_PCT, 127, 0, 0},        [K_FIRE] = {"FIRE", KT_PCT, 127, 0, 0},
    [K_BIRDS] = {"BIRDS", KT_PCT, 127, 0, 0},      [K_CRICK] = {"CRICKET", KT_PCT, 127, 64, 0},
    [K_THUN] = {"THUNDER", KT_PCT, 127, 64, 0},    [K_SENS] = {"SENS", KT_LIST, 9, 4, 0},
};
/* the pages: what KNOB 1..4 set */
enum { PG_MAIN, PG_FX1, PG_FX2, PG_LFO, PG_ENV, PG_MIX1, PG_MIX2, PG_N };
static const struct { const char *title; uint8_t k[4]; } NUI_PG[PG_N] = {
    [PG_MAIN] = {"MAIN", {K_MOOD, K_LIFE, K_TONE, K_VOL}},
    [PG_FX1] = {"FX 1/2: SPACE + ECHO", {K_SPACE, K_ECHO, K_ETIME, K_EFDBK}},
    [PG_FX2] = {"FX 2/2: COLOR", {K_CHORUS, K_DRIVE, K_RSIZE, K_RTYPE}},
    [PG_LFO] = {"LFO: SLOW MOTION", {K_LTGT, K_LRATE, K_LDEPTH, K_LSHAPE}},
    [PG_ENV] = {"ENV: FADES + SPEED", {K_FIN, K_FOUT, K_GLIDE, K_EVLEN}},
    [PG_MIX1] = {"MIX 1/2: LEVELS", {K_WIND, K_SEA, K_RAIN, K_FIRE}},
    [PG_MIX2] = {"MIX 2/2", {K_BIRDS, K_CRICK, K_THUN, K_SENS}},
};

static struct {
    uint8_t place, on[NL_COUNT], k[K_N], paused, help, page, hot, init;
    uint32_t frame, toast_t, hot_t, bot_sig, lfo_ph;
    int32_t lfo_v, lfo_rnd, lfo_rnd_tg;                /* the LFO's value (Q15, -32767..32767), its random's */
    char toast[24];
} nui;

/* ------------------------------------------------------------ text --- */
static int32_t nf_w(const char *s, int32_t sc)
{
    int32_t n = 0;
    while (s[n])
        n++;
    return n ? n * 6 * sc - sc : 0;
}
static void nf_text(int32_t x, int32_t y, const char *s, uint16_t c, int32_t sc)
{
    for (; *s; s++, x += 6 * sc) {
        uint32_t ch = (uint8_t)*s, r, b;
        const uint8_t *g;
        if (ch < 32u || ch > 127u)
            ch = '?';
        g = NF_GLYPH[ch - 32u];
        for (r = 0; r < 7u; r++)
            for (b = 0; b < 5u; b++)
                if ((g[r] >> (4u - b)) & 1u)
                    cv_rect(x + (int32_t)b * sc, y + (int32_t)r * sc, sc, sc, c);
    }
}
static void nf_center(int32_t cx, int32_t y, const char *s, uint16_t c, int32_t sc) { nf_text(cx - nf_w(s, sc) / 2, y, s, c, sc); }
static void nui_num(char *b, uint32_t v)                /* 0..999 */
{
    if (v >= 100u)
        *b++ = (char)('0' + v / 100u);
    if (v >= 10u)
        *b++ = (char)('0' + v / 10u % 10u);
    *b++ = (char)('0' + v % 10u);
    *b = 0;
}
static void nui_say(const char *a, const char *b)       /* a toast: a, then b if given */
{
    uint32_t i = 0;
    while (*a && i < sizeof nui.toast - 1u)
        nui.toast[i++] = *a++;
    if (b) {
        if (i < sizeof nui.toast - 1u)
            nui.toast[i++] = ' ';
        while (*b && i < sizeof nui.toast - 1u)
            nui.toast[i++] = *b++;
    }
    nui.toast[i] = 0;
    nui.toast_t = fm1_ms + 1400u;
}

/* ------------------------------------------------------------ state --- */
static int32_t nui_lfo_mod(uint32_t k)                 /* the LFO's offset on value k (in its 0..127 units) */
{
    if (!nui.k[K_LDEPTH] || LTGT_K[nui.k[K_LTGT] % 10u] != k)
        return 0;
    return (nui.lfo_v * nui.k[K_LDEPTH]) >> 15;         /* +-depth */
}
static uint32_t nui_val(uint32_t k) { return (uint32_t)clamp((int32_t)nui.k[k] + nui_lfo_mod(k), 0, 127); }

static void nui_lfo_tick(void)                          /* once a frame (~60 / s) */
{
    int32_t r = nui.k[K_LRATE];
    uint32_t mhz = (10u * (15u + (uint32_t)r % 15u) << ((uint32_t)r / 15u)) / 15u;   /* 0.01 .. 3.7 Hz */
    uint32_t ph0 = nui.lfo_ph;
    nui.lfo_ph += mhz * 71582u;                         /* 2^32 / 60 frames / 1000 mHz */
    switch (nui.k[K_LSHAPE]) {
    case 0:
        nui.lfo_v = sine_i(nui.lfo_ph);
        break;
    case 1: {
        int32_t x = (int32_t)(nui.lfo_ph >> 16);       /* 0..65535 */
        nui.lfo_v = x < 32768 ? x * 2 - 32767 : 98303 - x * 2;
        break;
    }
    case 2:
        nui.lfo_v = nui.lfo_ph < 0x80000000u ? 32767 : -32767;
        break;
    default:                                            /* RANDOM: a new target each cycle, glided to */
        if (nui.lfo_ph < ph0)
            nui.lfo_rnd_tg = (int32_t)(fm1_ms * 2654435761u >> 16) - 32768;
        nui.lfo_rnd += (nui.lfo_rnd_tg - nui.lfo_rnd) >> 4;
        nui.lfo_v = nui.lfo_rnd;
        break;
    }
}

static void nui_write(void)                             /* the app's values -> track 1, the effects, the engine */
{
    track_t *t = &trk[0];
    nat_t *s = NAT_ST(t);
    uint32_t k;
    for (k = 0; k < NL_COUNT; k++)
        t->p[P_E0 + k] = (int16_t)(nui.on[k] ? nui_val(K_WIND + k) : 0);
    t->p[P_E5] = (int16_t)nui_val(K_MOOD);
    t->p[P_E6] = (int16_t)nui_val(K_LIFE);
    t->p[P_E7] = (int16_t)nui_val(K_TONE);
    t->p[P_LEVEL] = (int16_t)nui_val(K_VOL);
    t->p[P_REV] = (int16_t)nui_val(K_SPACE);
    t->p[P_DLY] = (int16_t)nui_val(K_ECHO);
    t->p[P_CHOR] = nui.k[K_CHORUS];
    t->p[P_DIST] = nui.k[K_DRIVE];
    t->p[P_ATK] = nui.k[K_FIN];
    t->p[P_REL] = nui.k[K_FOUT];
    t->p[P_ATSNS] = (int16_t)(1 + nui.k[K_SENS] % 10u);
    song.g[G_DTIME] = ETIME_DIV[nui.k[K_ETIME] % 8u];
    song.g[G_DFDBK] = (int16_t)(nui.k[K_EFDBK] * 120 / 127);
    song.g[G_RSIZE] = nui.k[K_RSIZE];
    song.g[G_RTYPE] = nui.k[K_RTYPE];
    if (eng_mem_owner[0] == ENGI_NATURE + 1u) {
        s->night_tg = nui.k[K_TIME] * 258;
        s->glide_sh = (uint8_t)(6 + nui.k[K_GLIDE] * 7 / 127);
        s->ev_sh = (uint8_t)(11 + nui.k[K_EVLEN] * 4 / 127);
        s->cr_amt = (uint8_t)(nui.k[K_CRICK] ? nui.k[K_CRICK] : 1);
        s->th_amt = (uint8_t)(nui.k[K_THUN] ? nui.k[K_THUN] : 1);
    }
}

static void nui_place(uint32_t i)                       /* go to place i, as it is (its own values; the effects stay) */
{
    const nui_place_t *pl = &NUI_PLACES[i % NUI_NPLACE];
    uint32_t k;
    nui.place = (uint8_t)(i % NUI_NPLACE);
    for (k = 0; k < NL_COUNT; k++) {
        nui.k[K_WIND + k] = pl->lv[k] ? pl->lv[k] : 90u;   /* (a layer the place has not: its level when switched on) */
        nui.on[k] = pl->lv[k] != 0u;
    }
    nui.k[K_MOOD] = pl->mood;
    nui.k[K_LIFE] = pl->life;
    nui.k[K_TONE] = pl->tone;
    nui.k[K_SPACE] = pl->space;
    nui.k[K_TIME] = pl->night;
    nui_write();
}

static void nui_play(void)                              /* the scene on (a note: the engine's voice, if it has none) */
{
    nat_t *s = NAT_ST(&trk[0]);
    fm1_irq_off();
    trk_note_on(&trk[0], 53u, 100u);                    /* (no note-off: during the engine switch at power-on it waits
                                                         * in the track's queue, and a note-off would drop it; the
                                                         * engine's own envelope keeps the voice: ownenv) */
    fm1_irq_on();
    if (eng_mem_owner[0] == ENGI_NATURE + 1u)
        s->playing = 1;
    nui.paused = 0;
}

static void nui_boot(void)
{
    uint32_t k;
    nui_keymap();
    for (k = 0; k < K_N; k++)
        nui.k[k] = NUI_K[k].def;
    song.sel = 0;
    song.playing = 0;
    song.g[G_BPM] = 120;                                /* (the echo's times are notes at 120 BPM) */
    for (k = 1; k < NTRK; k++)                          /* the other tracks: silent */
        trk[k].p[P_MUTE] = 1;
    set_engine_of(&trk[0], ENGI_NATURE);
    trk[0].p[P_MUTE] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    nui_place(0);
    nui_play();
    nui.init = 1;
    nui_say("NATURE", 0);
}

/* ------------------------------------------------------------ input --- */
static void nui_page(uint32_t pg)
{
    nui.page = (uint8_t)pg;
    nui.help = 0;
    nui_say(pg == PG_MAIN ? "MAIN" : NUI_PG[pg].title, 0);
}

static void nui_turn(uint32_t k, int32_t d)
{
    const nui_knob_t *n = &NUI_K[k];
    int32_t step = n->kind == KT_LIST ? 1 : 2;
    nui.k[k] = (uint8_t)clamp((int32_t)nui.k[k] + d * step, 0, n->max);
    if (k >= K_WIND && k <= K_BIRDS && !nui.on[k - K_WIND]) {   /* a level turned: its layer comes on */
        nui.on[k - K_WIND] = 1;
        nui_say(NUI_LAYER[k - K_WIND], "ON");
    }
    nui_write();
}

static void nui_input(void)
{
    uint32_t pressed = fm1_input_edges(0), notes = fm1_input_note_edges(), b, k;
    int32_t d;
    nat_t *s = NAT_ST(&trk[0]);
    int own = eng_mem_owner[0] == ENGI_NATURE + 1u;
    if (!nui.init)
        nui_boot();
    for (b = 0; b < NB; b++) {
        if (!((pressed >> panel.btn[b]) & 1u))
            continue;
        if (nui.help) {                                 /* help up: any button closes it */
            nui.help = 0;
            continue;
        }
        switch (b) {
        case B_PLAY:
            if (!nui.paused && own && s->playing) {
                s->playing = 0;
                nui.paused = 1;
                nui_say("PAUSE", 0);
            } else {
                nui_play();
                nui_say("PLAY", 0);
            }
            break;
        case B_HOME:
            if (nui.page != PG_MAIN) {
                nui_page(PG_MAIN);
            } else {
                nui_place(nui.place);
                nui_say("RESET", NUI_PLACES[nui.place].name);
            }
            break;
        case B_OCTDN:
        case B_OCTUP:
            nui_place(nui.place + (b == B_OCTUP ? 1u : NUI_NPLACE - 1u));
            nui_say(NUI_PLACES[nui.place].name, 0);
            if (nui.paused)
                nui_play();
            break;
        case B_FX:
            nui_page(nui.page == PG_FX1 ? PG_FX2 : nui.page == PG_FX2 ? PG_MAIN : PG_FX1);
            break;
        case B_EDIT:
            nui_page(nui.page == PG_MIX1 ? PG_MIX2 : nui.page == PG_MIX2 ? PG_MAIN : PG_MIX1);
            break;
        case B_LFO:
            nui_page(nui.page == PG_LFO ? PG_MAIN : PG_LFO);
            break;
        case B_ENV:
            nui_page(nui.page == PG_ENV ? PG_MAIN : PG_ENV);
            break;
        default:
            nui.help = 1;
            break;
        }
    }
    if (notes) {
        nui.help = 0;
        for (k = 0; k < 27u; k++) {
            uint32_t a;
            if (!((notes >> k) & 1u))
                continue;
            a = nui_key[k];
            if (a < 20u) {                              /* a place (the key's note resumes a pause: eng_nature.c) */
                nui_place(a);
                nui.paused = 0;
                nui_say(NUI_PLACES[a].name, 0);
            } else if (a < 40u) {                       /* a layer on / off */
                a -= 20u;
                nui.on[a] = (uint8_t)!nui.on[a];
                nui_write();
                nui_say(NUI_LAYER[a], nui.on[a] ? "ON" : "OFF");
            } else {                                    /* an event (the engine plays it) */
                nui.paused = 0;
                nui_say(NUI_EVENT[a - 40u], 0);
            }
        }
    }
    for (k = 0; k < 4u; k++)
        if ((d = panel_enc(EN_K1 + k)) != 0) {
            nui_turn(NUI_PG[nui.page].k[k], d);
            nui.hot = (uint8_t)(k + 1u);
            nui.hot_t = fm1_ms + 1500u;
        }
    if ((d = panel_enc(EN_SELECT)) != 0) {
        nui_turn(K_SPACE, d);
        nui.hot = 5;
        nui.hot_t = fm1_ms + 1500u;
    }
    if ((d = panel_enc(EN_ALGO)) != 0) {
        nui_turn(K_TIME, d * 2);
        nui.hot = 6;
        nui.hot_t = fm1_ms + 1500u;
    }
    if ((d = panel_enc(EN_PRESET)) != 0) {
        nui_place(nui.place + (d > 0 ? 1u : NUI_NPLACE - 1u));
        nui_say(NUI_PLACES[nui.place].name, 0);
        if (nui.paused)
            nui_play();
    }
    if (own && nui.paused && s->playing)                /* (a key resumed it) */
        nui.paused = 0;
}

/* ------------------------------------------------------------- LEDs --- */
static void nui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0};
    uint32_t k, c, held = fm1_in.notes, pb = 0;
    static uint8_t ready;
    if (fm1_led_anim_on())
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    for (k = 0; k < 27u; k++) {
        uint32_t a = nui_key[k];
        int lit = (held >> k) & 1u;
        if (a < 20u)
            lit |= a == nui.place;                      /* the place: lit */
        else if (a < 40u)
            lit |= nui.on[a - 20u];                     /* a layer: lit while on */
        led_put(lit ? nl : nd, 14u + k, 1);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    led_put(nl, panel.btn[B_HOME], 1);
    pb = nui.page == PG_FX1 || nui.page == PG_FX2 ? B_FX : nui.page == PG_LFO ? B_LFO : nui.page == PG_ENV ? B_ENV
       : nui.page == PG_MIX1 || nui.page == PG_MIX2 ? B_EDIT : NB;
    if (pb < NB)
        led_put(nl, panel.btn[pb], 1);                  /* the page's button: lit */
    if (nui.paused)
        led_put(nb, panel.btn[B_PLAY], 1);              /* paused: PLAY breathes */
    else
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(0);
    for (c = 0; c < FM1_NCOL; c++) {
        nd[c] &= (uint8_t)~(nl[c] | nb[c]);
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = nb[c];
        fm1_led_mid[c] = 0;
        fm1_led[c] = nl[c] & (uint8_t)~nb[c];
    }
}

/* ----------------------------------------------------------- screen --- */
static void nui_help(uint32_t strip)
{
    static const char *const H0[] = {"WHITE KEYS - PLACES", "F3 SHORE   G3 STORM", "A3 FOREST  B3 CAMPFIRE",
                                      "C4 RAIN    D4 NIGHT", "E4 DRIFT   D5 MEADOW", "E5 MONSOON F5 WINTER",
                                      "G5 DAWN", "LAYERS ON/OFF", "F4 WIND G4 SEA A4 RAIN", "B4 FIRE C5 BIRDS"};
    static const char *const H1[] = {"BLACK KEYS - EVENTS", "F#3 THUNDER G#3 WAVE", "A#3 GUST C#4 DOWNPOUR",
                                     "D#4 CRACKLE F#4 BIRD", "G#4 CHORUS A#4 CALM", "C#5 STORM D#5 CRICKETS",
                                     "F#5 SHUFFLE. PRESS=HARDER", "PAGES: FX LFO ENV EDIT",
                                     "PLAY PAUSE HOME BACK/RESET", "OCT-/+ PLACE  ANY: BACK"};
    const char *const *h = strip ? H1 : H0;
    uint32_t i;
    for (i = 0; i < 10u; i++)
        nf_text(8, 6 + (int32_t)i * 11, h[i], i == 0 || (!strip && i == 7) || (strip && i == 7) ? NUI_ACC : NUI_TEXT, 1);
}

static void nui_draw_top(void)
{
    cv_begin(240, NUI_TOP_H, NUI_BG);
    cv_oy = 0;
    if (nui.help) {
        nui_help(0);
        cv_blit(0, 0);
        return;
    }
    nf_text(10, 3, NUI_PLACES[nui.place].name, NUI_SUN, 2);
    if (nui.paused) {
        nf_text(240 - 10 - nf_w("PAUSED", 1), 7, "PAUSED", NUI_ACC, 1);
    } else {
        uint32_t k = (nui.frame / 8u) % 3u;             /* playing: a dot steps along three places */
        cv_rect(214 + 6 * (int32_t)k, 9, 4, 4, NUI_ACC);
        if (nui.k[K_LDEPTH])                            /* the LFO on: its value as a bar beside */
            cv_rect(196, 13 - (nui.lfo_v * 5 >> 15), 4, 2, NUI_DIM);
    }
    ns_y0 = 20;
    if (eng_mem_owner[0] == ENGI_NATURE + 1u)
        graph_nature(&trk[0]);
    else
        cv_rect(10, 20, 220, 100, NUI_PANEL);
    if (nui.toast[0] && (int32_t)(nui.toast_t - fm1_ms) > 0) {
        int32_t sc = nf_w(nui.toast, 2) + 16 <= 220 ? 2 : 1, w = nf_w(nui.toast, sc) + 16;
        cv_rect(120 - w / 2, 92, w, 22, NUI_BG);
        nf_center(120, sc == 2 ? 96 : 99, nui.toast, NUI_TEXT, sc);
    }
    cv_blit(0, 0);
}

static void nui_bar(int32_t x, int32_t y, int32_t w, int32_t v, int32_t max, uint16_t c)
{
    cv_rect(x, y, w, 3, NUI_OFF);
    cv_rect(x, y, max ? w * v / max : 0, 3, c);
}

static void nui_value(char *b, uint32_t k)              /* the value's text */
{
    const nui_knob_t *n = &NUI_K[k];
    uint32_t v = nui.k[k], i = 0;
    switch (n->kind) {
    case KT_LIST:
        if (n->names) {
            const char *s = n->names[v % ((uint32_t)n->max + 1u)];
            while (*s && i < 7u)
                b[i++] = *s++;
            b[i] = 0;
        } else {
            nui_num(b, v + 1u);                         /* (SENS 1..10) */
        }
        return;
    case KT_HZ: {
        uint32_t mhz = (10u * (15u + v % 15u) << (v / 15u)) / 15u;
        if (mhz < 1000u) {
            b[0] = '.';
            b[1] = (char)('0' + mhz / 100u);
            b[2] = (char)('0' + mhz / 10u % 10u);
            b[3] = 0;
        } else {
            b[0] = (char)('0' + mhz / 1000u);
            b[1] = '.';
            b[2] = (char)('0' + mhz / 100u % 10u);
            b[3] = 0;
        }
        return;
    }
    case KT_SEC: {                                      /* tenths of a second, by what the value does */
        uint32_t ds;
        if (k == K_GLIDE)
            ds = (30u << (6u + v * 7u / 127u)) / 1378u;  /* ~3 time constants (eng_nature.c the glide) */
        else if (k == K_EVLEN)
            ds = (15u << (11u + v * 4u / 127u)) / 1378u;
        else
            ds = v * v * 7u / 8u * 10u / 1378u;         /* FADE IN / OUT (eng_nature.c the fade) */
        if (ds >= 100u) {
            nui_num(b, ds / 10u);
            for (i = 0; b[i]; i++)
                ;
            b[i++] = 'S';
            b[i] = 0;
        } else {
            b[0] = (char)('0' + ds / 10u);
            b[1] = '.';
            b[2] = (char)('0' + ds % 10u);
            b[3] = 'S';
            b[4] = 0;
        }
        return;
    }
    default:
        nui_num(b, v * 100u / 127u);
        return;
    }
}

static void nui_draw_bot(void)
{
    nat_t *s = NAT_ST(&trk[0]);
    uint32_t k, sig = 2166136261u;
    char b[12];
    int own = eng_mem_owner[0] == ENGI_NATURE + 1u;
    int hot = (int32_t)(nui.hot_t - fm1_ms) > 0 ? nui.hot : 0;
    /* redraw only on a change (the levels move: in steps of 1/8) */
    for (k = 0; k < NL_COUNT; k++)
        sig = (sig ^ (nui.on[k] + 2u * (own ? (uint32_t)(s->v_lvl[k] >> 12) : 0u))) * 16777619u;
    for (k = 0; k < K_N; k++)
        sig = (sig ^ nui.k[k]) * 16777619u;
    sig = (sig ^ (nui.page + 131u * (uint32_t)hot + 104729u * nui.help)) * 16777619u;
    if (sig == nui.bot_sig)
        return;
    nui.bot_sig = sig;
    cv_begin(240, NUI_BOT_H, NUI_BG);
    cv_oy = 0;
    if (nui.help) {
        nui_help(1);
        cv_blit(0, NUI_TOP_H);
        return;
    }
    /* the layers: name, level (what sounds now), on / off; their keys under */
    for (k = 0; k < NL_COUNT; k++) {
        int32_t x = 6 + (int32_t)k * 46, lv = own ? s->v_lvl[k] : 0, seg = 0, i;
        uint16_t c = nui.on[k] ? NUI_TEXT : NUI_OFF;
        nf_text(x, 3, NUI_LAYER[k], c, 1);
        while (seg < 8 && lv > (seg * 32767) / 9)
            seg++;
        if (lv > 0 && seg == 0)
            seg = 1;
        for (i = 0; i < 8; i++)
            cv_rect(x + i * 5, 12, 4, 4, nui.on[k] && i < seg ? NUI_ACC : NUI_OFF);
        nf_text(x, 19, NUI_KNAME[12 + (k < 4u ? 2 * k : 7)], NUI_OFF, 1);
    }
    /* the page: its title, its four knobs */
    cv_rect(0, 30, 240, 11, nui.page == PG_MAIN ? NUI_BG : NUI_PANEL);
    nf_text(6, 32, NUI_PG[nui.page].title, nui.page == PG_MAIN ? NUI_DIM : NUI_ACC, 1);
    if (nui.page != PG_MAIN)
        nf_text(240 - 6 - nf_w("HOME:BACK", 1), 32, "HOME:BACK", NUI_DIM, 1);
    for (k = 0; k < 4u; k++) {
        uint32_t kk = NUI_PG[nui.page].k[k];
        int32_t x = 6 + (int32_t)k * 58, on = hot == (int)(k + 1u);
        nf_text(x, 45, NUI_K[kk].label, on ? NUI_ACC : NUI_DIM, 1);
        nui_value(b, kk);
        if (kk >= K_WIND && kk <= K_BIRDS && !nui.on[kk - K_WIND])
            b[0] = 'O', b[1] = 'F', b[2] = 'F', b[3] = 0;   /* (the layer is off: a turn switches it on) */
        nf_text(x, 55, b, on ? NUI_ACC : NUI_TEXT, nf_w(b, 2) <= 54 ? 2 : 1);
        nui_bar(x, 71, 52, nui.k[kk], NUI_K[kk].max, on ? NUI_ACC : NUI_DIM);
        if (nui.k[K_LDEPTH] && LTGT_K[nui.k[K_LTGT] % 10u] == kk)
            nf_text(x + 40, 45, "~", NUI_ACC, 1);       /* the LFO moves this one */
    }
    cv_rect(6, 79, 228, 1, NUI_PANEL);
    /* SELECT and ALGORITHM, the hint */
    nf_text(6, 84, "REVERB", hot == 5 ? NUI_ACC : NUI_DIM, 1);
    nui_bar(48, 86, 56, nui.k[K_SPACE], 127, hot == 5 ? NUI_ACC : NUI_DIM);
    nf_text(118, 84, "TIME", hot == 6 ? NUI_ACC : NUI_DIM, 1);
    nf_text(146, 84, nui.k[K_TIME] < 32u ? "DAY" : nui.k[K_TIME] < 96u ? "DUSK" : "NIGHT", hot == 6 ? NUI_ACC : NUI_TEXT, 1);
    nui_bar(184, 86, 50, nui.k[K_TIME], 127, hot == 6 ? NUI_ACC : NUI_DIM);
    nf_center(120, 102, "FX LFO ENV EDIT: PAGES. ELSE: HELP", NUI_OFF, 1);
    cv_blit(0, NUI_TOP_H);
}

static void nui_frame(void)
{
    static uint32_t kick;
    nat_t *s = NAT_ST(&trk[0]);
    nui_input();
    nui_lfo_tick();
    nui_write();                                        /* (every frame: the LFO; the engine's memory is zeroed when it
                                                         * claims it after power-on's switch, its settings with it) */
    if (eng_mem_owner[0] == ENGI_NATURE + 1u && !nui.paused && !s->playing && s->fade <= 0 &&
        nui.frame - kick > 60u) {                       /* not paused but silent (a start that went astray): start */
        kick = nui.frame;
        nui_play();
    }
    if (ui.uboot) {                                     /* OCT- + OCT+ held (main.c): the update mode's countdown */
        char d[2] = {(char)('0' + ui.uboot % 10u), 0};
        nui_say("UPDATE MODE IN", d);
    }
    if ((nui.frame % 120u) == 0)
        nui.bot_sig = 0;                                /* (the HUD again now and then: after an update's screens) */
    nui_leds();
    if (!(nui.frame++ & 1u))                            /* the scene: every other frame (~30 a second) */
        nui_draw_top();
    nui_draw_bot();
}
