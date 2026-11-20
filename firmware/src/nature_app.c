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
 *   KNOB 1..4   MOOD (calm .. storm)  LIFE (how much it changes by itself)  TONE (dark .. bright)  VOL
 *   SELECT      SPACE (reverb)        ALGORITHM  TIME (day .. night)       PRESETS  the places, one by one
 *   PLAY        pause / play (fades)  HOME  the place as it was (undo every change)  OCT- / OCT+  previous / next place
 *   any other button: HELP (the map on the screen); again, or a key: back */
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

static struct {
    uint8_t place, on[NL_COUNT], lv[NL_COUNT], knob[4], space, night, paused, help, hot, init;
    uint32_t frame, toast_t, hot_t, bot_sig;
    char toast[24];
} nui;

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
static void nui_write(void)                             /* the app's state -> track 1's parameters, the engine */
{
    track_t *t = &trk[0];
    nat_t *s = NAT_ST(t);
    uint32_t k;
    for (k = 0; k < NL_COUNT; k++)
        t->p[P_E0 + k] = (int16_t)(nui.on[k] ? nui.lv[k] : 0);
    t->p[P_E5] = nui.knob[0];
    t->p[P_E6] = nui.knob[1];
    t->p[P_E7] = nui.knob[2];
    t->p[P_LEVEL] = nui.knob[3];
    t->p[P_REV] = nui.space;
    if (eng_mem_owner[0] == ENGI_NATURE + 1u)
        s->night_tg = nui.night * 258;
}

static void nui_place(uint32_t i)                       /* go to place i, as it is (every change undone) */
{
    const nui_place_t *pl = &NUI_PLACES[i % NUI_NPLACE];
    uint32_t k;
    nui.place = (uint8_t)(i % NUI_NPLACE);
    for (k = 0; k < NL_COUNT; k++) {
        nui.lv[k] = pl->lv[k] ? pl->lv[k] : 90u;        /* (a layer the place has not: its level when switched on) */
        nui.on[k] = pl->lv[k] != 0u;
    }
    nui.knob[0] = pl->mood;
    nui.knob[1] = pl->life;
    nui.knob[2] = pl->tone;
    nui.space = pl->space;
    nui.night = pl->night;
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
    song.sel = 0;
    song.playing = 0;
    for (k = 1; k < NTRK; k++)                          /* the other tracks: silent */
        trk[k].p[P_MUTE] = 1;
    set_engine_of(&trk[0], ENGI_NATURE);
    trk[0].p[P_MUTE] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_ATSNS] = 5;                              /* the keys' pressure: on */
    trk[0].p[P_ATK] = 70;
    trk[0].p[P_REL] = 90;
    nui.knob[3] = 104;                                  /* VOL: the track's default level (a place leaves it) */
    nui_place(0);
    nui_play();
    nui.init = 1;
    nui_say("NATURE", 0);
}

/* ------------------------------------------------------------ input --- */
static int32_t nui_step(uint32_t v, int32_t d, int32_t mul) { return clamp((int32_t)v + d * mul, 0, 127); }

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
            nui_place(nui.place);
            nui_say("RESET", NUI_PLACES[nui.place].name);
            break;
        case B_OCTDN:
        case B_OCTUP:
            nui_place(nui.place + (b == B_OCTUP ? 1u : NUI_NPLACE - 1u));
            nui_say(NUI_PLACES[nui.place].name, 0);
            if (nui.paused)
                nui_play();
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
            nui.knob[k] = (uint8_t)nui_step(nui.knob[k], d, 2);
            nui.hot = (uint8_t)(k + 1u);
            nui.hot_t = fm1_ms + 1500u;
            nui_write();
        }
    if ((d = panel_enc(EN_SELECT)) != 0) {
        nui.space = (uint8_t)nui_step(nui.space, d, 2);
        nui.hot = 5;
        nui.hot_t = fm1_ms + 1500u;
        nui_write();
    }
    if ((d = panel_enc(EN_ALGO)) != 0) {
        nui.night = (uint8_t)nui_step(nui.night, d, 4);
        nui.hot = 6;
        nui.hot_t = fm1_ms + 1500u;
        nui_write();
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
    uint32_t k, c, held = fm1_in.notes;
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
    if (nui.paused)
        led_put(nb, panel.btn[B_PLAY], 1);              /* paused: PLAY breathes */
    else
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    if (nui.help)
        for (k = 0; k < NB; k++)
            led_put(nb, panel.btn[k], k != B_PLAY && k != B_HOME && k != B_OCTDN && k != B_OCTUP);
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
    static const char *const H1[] = {"BLACK KEYS - EVENTS", "PRESS HARDER: STRONGER", "F#3 THUNDER G#3 WAVE",
                                     "A#3 GUST C#4 DOWNPOUR", "D#4 CRACKLE F#4 BIRD", "G#4 CHORUS A#4 CALM",
                                     "C#5 STORM D#5 CRICKETS", "F#5 SHUFFLE",
                                     "PLAY PAUSE  HOME RESET", "OCT-/+ PLACE  ANY: BACK"};
    const char *const *h = strip ? H1 : H0;
    uint32_t i;
    for (i = 0; i < 10u; i++)
        nf_text(8, 6 + (int32_t)i * 11, h[i], i == 0 || (strip && i == 1) || (!strip && i == 7) ? NUI_ACC : NUI_TEXT, 1);
}

static void nui_draw_top(void)
{
    nat_t *s = NAT_ST(&trk[0]);
    cv_begin(240, NUI_TOP_H, NUI_BG);
    cv_oy = 0;
    if (nui.help) {
        nui_help(0);
        cv_blit(0, 0);
        return;
    }
    nf_text(10, 3, NUI_PLACES[nui.place].name, NUI_SUN, 2);
    if (nui.paused)
        nf_text(240 - 10 - nf_w("PAUSED", 1), 7, "PAUSED", NUI_ACC, 1);
    else {
        uint32_t k = (nui.frame / 8u) % 3u;             /* playing: a dot steps along three places */
        cv_rect(214 + 6 * (int32_t)k, 9, 4, 4, NUI_ACC);
    }
    ns_y0 = 20;
    if (eng_mem_owner[0] == ENGI_NATURE + 1u)
        graph_nature(&trk[0]);
    else
        cv_rect(10, 20, 220, 100, NUI_PANEL);
    (void)s;
    if (nui.toast[0] && (int32_t)(nui.toast_t - fm1_ms) > 0) {
        int32_t w = nf_w(nui.toast, 2) + 16;
        cv_rect(120 - w / 2, 92, w, 22, NUI_BG);
        nf_center(120, 96, nui.toast, NUI_TEXT, 2);
    }
    cv_blit(0, 0);
}

static void nui_bar(int32_t x, int32_t y, int32_t w, int32_t v, uint16_t c)   /* v 0..127 */
{
    cv_rect(x, y, w, 4, NUI_OFF);
    cv_rect(x, y, w * v / 127, 4, c);
}

static void nui_draw_bot(void)
{
    nat_t *s = NAT_ST(&trk[0]);
    static const char *const KN[4] = {"MOOD", "LIFE", "TONE", "VOL"};
    uint32_t k, sig = 2166136261u;
    char b[8];
    int own = eng_mem_owner[0] == ENGI_NATURE + 1u;
    int hot = (int32_t)(nui.hot_t - fm1_ms) > 0 ? nui.hot : 0;
    /* redraw only on a change (the levels move: in steps of 1/8) */
    for (k = 0; k < NL_COUNT; k++)
        sig = (sig ^ (nui.on[k] + 2u * (own ? (uint32_t)(s->v_lvl[k] >> 12) : 0u))) * 16777619u;
    for (k = 0; k < 4u; k++)
        sig = (sig ^ nui.knob[k]) * 16777619u;
    sig = (sig ^ (nui.space + 131u * nui.night + 7919u * (uint32_t)hot + 104729u * nui.help)) * 16777619u;
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
        nf_text(x, 4, NUI_LAYER[k], c, 1);
        while (seg < 8 && lv > (seg * 32767) / 9)      /* (8 segments, on a square-root-ish feel) */
            seg++;
        if (lv > 0 && seg == 0)
            seg = 1;
        for (i = 0; i < 8; i++)
            cv_rect(x + i * 5, 14, 4, 5, nui.on[k] && i < seg ? NUI_ACC : NUI_OFF);
        nf_text(x, 23, NUI_KNAME[12 + (k < 4u ? 2 * k : 7)], NUI_OFF, 1);
    }
    cv_rect(6, 36, 228, 1, NUI_PANEL);
    /* the four knobs */
    for (k = 0; k < 4u; k++) {
        int32_t x = 6 + (int32_t)k * 58;
        uint16_t c = hot == (int)(k + 1u) ? NUI_ACC : NUI_DIM;
        nf_text(x, 42, KN[k], c, 1);
        nui_num(b, nui.knob[k] * 100u / 127u);
        nf_text(x, 52, b, hot == (int)(k + 1u) ? NUI_ACC : NUI_TEXT, 2);
        nui_bar(x, 70, 50, nui.knob[k], hot == (int)(k + 1u) ? NUI_ACC : NUI_DIM);
    }
    cv_rect(6, 80, 228, 1, NUI_PANEL);
    /* SELECT and ALGORITHM, the hint */
    nf_text(6, 86, "SPACE", hot == 5 ? NUI_ACC : NUI_DIM, 1);
    nui_bar(42, 88, 60, nui.space, hot == 5 ? NUI_ACC : NUI_DIM);
    nf_text(122, 86, "TIME", hot == 6 ? NUI_ACC : NUI_DIM, 1);
    nf_text(152, 86, nui.night < 32u ? "DAY" : nui.night < 96u ? "DUSK" : "NIGHT", hot == 6 ? NUI_ACC : NUI_TEXT, 1);
    nui_bar(188, 88, 46, nui.night, hot == 6 ? NUI_ACC : NUI_DIM);
    nf_center(120, 102, "OTHER BUTTONS: HELP", NUI_OFF, 1);
    cv_blit(0, NUI_TOP_H);
}

static void nui_frame(void)
{
    static uint32_t kick;
    nat_t *s = NAT_ST(&trk[0]);
    nui_input();
    if ((nui.frame & 7u) == 0)                          /* the parameters again (the engine's memory is zeroed when it
                                                         * claims it, after power-on's switch: night_tg with it) */
        nui_write();
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
