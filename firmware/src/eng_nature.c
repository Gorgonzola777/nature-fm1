/* SPDX-License-Identifier: MIT
 * NATURE: a generative soundscape engine for Felucca (M-VAVE FM-1).
 *
 * One held note plays a whole scene that never repeats: WIND, SEA, RAIN, FIRE and BIRDS, plus thunder
 * that comes by itself in a storm. Everything is synthesised (noise, filters, random events): no samples.
 *
 *   EDIT 1 "MIX"    WIND SEA RAIN FIRE      the level of each layer (0 = off, not computed)
 *   EDIT 2 "SCENE"  BIRD MOOD EVOL TONE
 *     BIRD  birdsong level (birds go quiet as the weather turns stormy)
 *     MOOD  calm .. storm: gustier wind, bigger and faster waves, heavier rain, thunder above ~60 %
 *     EVOL  how much the scene lives by itself: each layer's presence and the MOOD wander slowly
 *           (new targets every 8..30 s / 15..60 s); 0 = a fixed mix
 *     TONE  dark .. bright: moves every filter; the key does the same (around C4), so low keys are
 *           darker / closer, high keys brighter / further. The track's FLT also moves it.
 *
 * The keys (engine_t.keys: chromatic from F3; the NATURE app owns the white keys, places and layers):
 *   BLACK  events: F#3 THUNDER, G#3 BIG WAVE, A#3 GUST, C#4 DOWNPOUR, D#4 CRACKLE, F#4 BIRD, G#4 BIRD CHORUS,
 *          A#4 CALM, C#5 STORM, D#5 CRICKETS, F#5 SHUFFLE (the mix picks new targets now). An event whose layer is
 *          off brings it in for a while (a boost that fades over ~10 s). Pressure (aftertouch; NAT_AT: the
 *          track's, 0..127) on the key held plays the event live: louder thunder, a bigger wave, a harder gust ..
 *   WHITE  start (or resume) the scene; nothing else here.
 * Night (nat_t.night_tg, set by the host: 0 day .. 32767 night): crickets come with it (and the CRICKETS event).
 * The layer levels and the mood glide to new values over ~2 s, so a new place fades in.
 * The scene's amplitude is its own (engine_t.ownenv): the voice ends when it has faded out (playing = 0).
 *
 * Polyphony 1: the scene is one per part. Its state lives in the part's engine memory (engines.c eng_mem)
 * and runs on across notes, so retriggering a key does not restart the weather.
 *
 * Fixed point only, no allocation, no library calls, written for 44.1 kHz with CTL = 32 sample blocks.
 * Uses from the host / dsp.c: noise32, clamp, mul_wrap, sine_i, tsvf_coef_k, soft_knee, voice_amp, FE_T,
 * FE_OCT, the FE_NOISE_* pole constants. */

enum { NL_WIND, NL_SEA, NL_RAIN, NL_FIRE, NL_BIRD, NL_COUNT };
#define NAT_NCRICK 3                       /* crickets, each its own pitch and rhythm */
#define NAT_NCHIME 4                       /* wind chimes ringing at once */

#define NAT_BPS (FS / CTL)                 /* control blocks a second (1378 at 44.1 kHz) */
#define NAT_HZ_INC ((uint32_t)((1ull << 32) / FS))   /* phase increment of 1 Hz (97391 at 44.1 kHz) */

/* each layer's make-up gain (Q8): alone at full level they sit near -18 dBFS RMS */
static const int16_t NAT_GAIN[NL_COUNT] = {100, 400, 380, 110, 600};
#define TH_MID 24                          /* the thunder's body and crack (x16) */
#define TH_CRK 16

typedef struct { int32_t ic1, ic2; } nat_f_t;               /* an SVF's state */
typedef struct { int32_t a1, a2, a3, k; } nat_c_t;          /* its coefficients, per block */

typedef struct {
    int32_t rng;
    uint8_t init;
    /* the generative mix: presence of each layer and the mood's wander, Q23 (Q15 << 8) */
    int32_t wan[NL_COUNT], wan_tg[NL_COUNT], wan_cnt[NL_COUNT];
    int32_t storm, storm_tg, storm_cnt;
    /* noise colours */
    int32_t pk0, pk1, pk2, brown, brown2;
    /* wind */
    int32_t gust, gust_tg, gust_cnt;
    nat_f_t w_bp, w_bp2, w_lp;
    /* sea */
    uint32_t wave_ph, wave_inc;
    int32_t swell, crash, crash_tg, foam, wave_big;
    nat_f_t s_lp, s_hi;
    /* rain */
    nat_f_t r_hiss, r_drop, r_big;
    /* fire */
    int32_t flick, flick_tg, flick_cnt, crack_n;
    nat_f_t f_lp, f_crk;
    /* birds */
    uint32_t b_ph, b_vph;
    int32_t b_state, b_cnt, b_syl, b_sylno, b_base, b_f0, b_f1, b_len, b_pos, b_kind, b_amp, b_env;
    /* thunder */
    int32_t thun, thun_flk, thun_flk_tg, thun_flk_cnt;
    int32_t tcrk, tcrk_arm;               /* the crack overhead (Q15; pressed hard, or a big natural one), armed */
    nat_f_t t_lp, t_mid, t_crk;
    /* output DC blocker */
    int32_t dc;
    /* the keys: latch, the key the scene is tuned to, the fade (Q15), the events' boosts (Q15) */
    uint8_t playing, chorus, last_key;    /* last_key: the event + 1 of the key pressed last, 0 = a white key */
    int32_t press;                        /* the pressure, smoothed, Q15 */
    int32_t lv[NL_COUNT], mood_s;         /* the levels and the mood, gliding to the parameters (Q23) */
    int32_t night_tg, night_s;            /* night: the host's target (Q15), gliding (Q23) */
    int32_t cr_boost;                     /* the CRICKETS event (Q15) */
    /* the host's settings (0 = the default): the glide's shift (blocks: 2^n, 6..13), the events' fade shift (11..15),
     * the crickets' and the thunder's amounts (64 = as designed, 0..127) */
    uint8_t glide_sh, ev_sh, cr_amt, th_amt;
    /* the host's: CHIMES (level 0 = off, the key 0..11 from C, the scale, decay), LO-FI (crush, rate, wobble, dust),
     * the master filter (64 open, below a low-pass, above a high-pass; its resonance); all 0..127 */
    uint8_t ch_lvl, ch_key, ch_scale, ch_decay, lf_crush, lf_rate, lf_wob, lf_dust, mf_filt, mf_reso;
    uint32_t ch_ph[NAT_NCHIME][2];
    int32_t ch_env[NAT_NCHIME], ch_note[NAT_NCHIME], ch_next;
    uint32_t wob_ph, flut_ph;
    int32_t hold_n, hold_v;
    nat_f_t mf;
    uint32_t cr_ph[NAT_NCRICK];
    int32_t cr_cnt[NAT_NCRICK], cr_pulses[NAT_NCRICK], cr_env[NAT_NCRICK], cr_on[NAT_NCRICK];
    int32_t scene_note, fade, boost[NL_COUNT];
    /* for the screen (ui: the pixel scene reads these, written once a block) */
    int16_t v_lvl[NL_COUNT];      /* each layer's level now, Q15 (after EVOL; BIRD after the storm) */
    int16_t v_mood, v_gust;       /* Q15 */
    int16_t v_night, v_crick;     /* night (Q15), the crickets' level (Q15) */
    uint8_t v_crack;              /* a crackle burst started: 255, the screen lets it fade */
} nat_t;

#ifdef ENG_MEM                      /* Felucca: the part's engine memory */
ENG_MEM_FITS("NATURE", sizeof(nat_t));
#define NAT_ST(t) ((nat_t *)ENG_MEM((uint32_t)((t) - trk)))
#else                               /* a standalone host */
static nat_t nat_st[NPART];
#define NAT_ST(t) (&nat_st[(t) - trk])
#endif

/* ------------------------------------------------------------ helpers --- */
static inline uint32_t nat_rnd(nat_t *s) { return noise32(&s->rng); }

/* uniform in lo..hi */
static int32_t nat_urand(nat_t *s, int32_t lo, int32_t hi)
{
    return lo + (int32_t)(((nat_rnd(s) >> 16) * (uint32_t)(hi - lo + 1)) >> 16);
}

/* a smoothed random walk in Q23: a new target in lo..hi (Q15) every minb..maxb blocks (at 44.1 kHz),
 * followed with a lag of 2^sh blocks */
static void nat_walk(nat_t *s, int32_t *v, int32_t *tg, int32_t *cnt, int32_t lo, int32_t hi, int32_t minb,
                     int32_t maxb, uint32_t sh)
{
    if (--*cnt <= 0) {
        *tg = nat_urand(s, lo, hi) * 256;
        *cnt = FE_T(nat_urand(s, minb, maxb));
    }
    *v += (*tg - *v) >> (sh + FE_OCT);
}

static void nat_coef(nat_c_t *c, int32_t cut, int32_t reso)   /* cut 0..127 << 8 (30 Hz .. 16 kHz), reso 0..127 */
{
    tsvf_t t;
    int32_t k = 8192 - clamp(reso, 0, 127) * 7600 / 127;
    tsvf_coef_k(&t, clamp(cut, 0, 127 << 8), k);
    c->a1 = t.a1;
    c->a2 = t.a2;
    c->a3 = t.a3;
    c->k = k;
}

/* one sample of a trapezoidal SVF: returns the low-pass, *bp the band-pass at unity peak gain */
static inline int32_t nat_svf(const nat_c_t *c, nat_f_t *f, int32_t in, int32_t *bp)
{
    int32_t v3 = in - f->ic2;
    int32_t v1 = (c->a1 * f->ic1 + c->a2 * v3 + 4096) >> 13;
    int32_t v2 = f->ic2 + ((c->a2 * f->ic1 + c->a3 * v3 + 4096) >> 13);
    f->ic1 = clamp(2 * v1 - f->ic1, -150000, 150000);
    f->ic2 = clamp(2 * v2 - f->ic2, -150000, 150000);
    if (bp)
        *bp = (v1 * c->k) >> 12;
    return v2;
}

/* level 0..127 -> Q15, squared (a fader that feels even) */
static inline int32_t nat_lvl(int32_t x) { return (x * x * 2032) >> 10; }

static void nat_new_wave(nat_t *s, int32_t mood)
{
    /* a wave every 5..11 s, up to 2.5 s quicker in a storm; bigger in a storm */
    int32_t ms = nat_urand(s, 5000, 11000) - ((mood * 2500) >> 15);
    uint32_t blocks = (uint32_t)(ms * NAT_BPS / 1000);
    s->wave_inc = 0xFFFFFFFFu / (blocks ? blocks : 1u);
    s->wave_big = ((nat_urand(s, 14000, 32767) * (16384 + (mood >> 1))) >> 15);
}

static void nat_init(nat_t *s, const voice_t *v)
{
    int32_t k;
    uint32_t seed = (v->age + 1u) * 0x9E3779B9u ^ 0x5EA5EA5Eu;
    s->rng = (int32_t)(seed ? seed : 1u);
    for (k = 0; k < NL_COUNT; k++) {
        s->wan[k] = s->wan_tg[k] = 32767 << 8;
        s->wan_cnt[k] = FE_T(nat_urand(s, 2000, 12000));
    }
    s->storm = s->storm_tg = 0;
    s->storm_cnt = FE_T(nat_urand(s, 4000, 20000));
    s->gust = s->gust_tg = 12000 << 8;
    nat_new_wave(s, 0);
    s->wave_ph = (uint32_t)nat_urand(s, 0, 0x7FFF) << 16;
    s->b_state = 0;
    s->b_cnt = FE_T(nat_urand(s, 300, 2500));
    s->scene_note = 60;
    for (k = 0; k < NAT_NCRICK; k++)
        s->cr_cnt[k] = FE_T(nat_urand(s, 200, 1500));
    s->init = 1;
}

/* birds: a phrase of 2..7 syllables of one of four kinds, then silence */
enum { BK_FALL, BK_RISE, BK_TRILL, BK_WARBLE };

static void nat_bird_syllable(nat_t *s)
{
    int32_t b = s->b_base, ms = nat_urand(s, 40, 150);
    switch (s->b_kind) {
    case BK_FALL:
        s->b_f0 = b + (b >> 1);
        s->b_f1 = b - (b >> 3);
        break;
    case BK_RISE:
        s->b_f0 = b - (b >> 2);
        s->b_f1 = b + (b >> 2);
        break;
    case BK_TRILL:
        s->b_f0 = s->b_f1 = b;
        ms = nat_urand(s, 25, 60);
        break;
    default:
        s->b_f0 = s->b_f1 = (s->b_sylno & 1) ? b + (b >> 2) : b;
        break;
    }
    s->b_len = (ms * NAT_BPS) / 1000;
    if (s->b_len < 4)
        s->b_len = 4;
    s->b_pos = 0;
    s->b_amp = nat_urand(s, 14000, 32767);
}

static void nat_bird_block(nat_t *s, int32_t active)
{
    switch (s->b_state) {
    case 0:                                            /* silence until the next phrase */
        if (--s->b_cnt > 0)
            return;
        if (!active) {
            s->b_cnt = FE_T(1000);
            return;
        }
        s->b_kind = (int32_t)(nat_rnd(s) & 3u);
        s->b_base = nat_urand(s, 1900, 4300);
        s->b_syl = nat_urand(s, 2, s->b_kind == BK_TRILL ? 12 : 7);
        s->b_sylno = 0;
        nat_bird_syllable(s);
        s->b_state = 1;
        return;
    case 1:                                            /* a syllable */
        if (++s->b_pos < s->b_len)
            return;
        if (++s->b_sylno >= s->b_syl) {
            s->b_state = 0;                            /* phrase over: 0.4 .. 6 s of quiet (a chorus: 0.1 .. 0.6 s) */
            if (s->chorus) {
                s->chorus--;
                s->b_cnt = FE_T(nat_urand(s, 140, 800));
            } else {
                s->b_cnt = FE_T(nat_urand(s, 550, 8300));
            }
            return;
        }
        s->b_state = 2;
        s->b_cnt = FE_T(s->b_kind == BK_TRILL ? nat_urand(s, 15, 40) : nat_urand(s, 40, 190));
        return;
    default:                                           /* the gap between syllables */
        if (--s->b_cnt > 0)
            return;
        nat_bird_syllable(s);
        s->b_state = 1;
        return;
    }
}

/* -------------------------------------------------------------- engine --- */
enum { EV_THUNDER, EV_WAVE, EV_GUST, EV_DOWNPOUR, EV_CRACKLE, EV_BIRD, EV_CHORUS, EV_CALM, EV_STORM, EV_CRICKETS,
       EV_SHUFFLE, EV_COUNT };

#ifndef NAT_OCT
#define NAT_OCT 0
#endif
/* a black key's event (EV_*), -1 for a white key: the 11 black keys from F#3 (key 1) to F#5 (key 25) */
static int32_t nat_event_of(uint32_t note)
{
    static const int8_t EVK[24] = {-1, EV_THUNDER, -1, EV_WAVE, -1, EV_GUST, -1, -1, EV_DOWNPOUR, -1, EV_CRACKLE, -1,
                                   -1, EV_BIRD, -1, EV_CHORUS, -1, EV_CALM, -1, -1, EV_STORM, -1, EV_CRICKETS, -1};
    int32_t k = (int32_t)note - 53 - 12 * (int32_t)(NAT_OCT);
    if (k == 25)
        return EV_SHUFFLE;
    k = (k % 24 + 24) % 24;                            /* (another octave: the same two octaves again) */
    return EVK[k];
}

static void nat_event(nat_t *s, int32_t ev)
{
    switch (ev) {
    /* (the events under pressure start at about half: pressing harder takes them the rest of the way, nat_render) */
    case EV_THUNDER:
    {
        int32_t th = nat_urand(s, 18000, 21000);
        if (th > s->thun)
            s->thun = th;
    }
        s->boost[NL_RAIN] = clamp(s->boost[NL_RAIN] + 6000, 0, 32767);
        break;
    case EV_WAVE:
        if (s->boost[NL_SEA] < 24000)
            s->boost[NL_SEA] = 24000;
        s->wave_big = 22000;
        s->wave_ph = 0x80000000u;                      /* half way up: it breaks in a second or two */
        s->wave_inc = 0xFFFFFFFFu / (uint32_t)(5 * NAT_BPS);
        break;
    case EV_GUST:
        if (s->boost[NL_WIND] < 22000)
            s->boost[NL_WIND] = 22000;
        s->gust = s->gust_tg = 19000 << 8;
        s->gust_cnt = FE_T(nat_urand(s, 1500, 3000));
        break;
    case EV_DOWNPOUR:
        if (s->boost[NL_RAIN] < 24000)
            s->boost[NL_RAIN] = 24000;
        s->storm_tg = clamp(s->storm_tg + (7000 << 8), -(24000 << 8), 24000 << 8);
        s->storm_cnt = FE_T(14000);
        break;
    case EV_CRACKLE:
        if (s->boost[NL_FIRE] < 24000)
            s->boost[NL_FIRE] = 24000;
        s->crack_n = 6;
        s->v_crack = 255;
        break;
    case EV_BIRD:
    case EV_CHORUS:
        if (s->boost[NL_BIRD] < 22000)
            s->boost[NL_BIRD] = 22000;
        if (s->b_state == 0)
            s->b_cnt = 1;
        s->chorus = ev == EV_CHORUS ? 4 : 0;
        break;
    case EV_CALM:
        s->storm = s->storm_tg = -(12000 << 8);
        s->storm_cnt = FE_T(20000);
        break;
    case EV_STORM:
        s->storm_tg = 24000 << 8;
        s->storm_cnt = FE_T(20000);
        s->boost[NL_WIND] = s->boost[NL_RAIN] = 26000;
        break;
    case EV_CRICKETS:
        s->cr_boost = 32767;
        break;
    default: {                                         /* SHUFFLE */
        int32_t k;
        for (k = 0; k < NL_COUNT; k++)
            s->wan_cnt[k] = 1;
        s->storm_cnt = 1;
        break;
    }
    }
}

#ifndef NAT_AT
#define NAT_AT(t) 0                       /* the host's pressure for track t, 0..127 */
#define NAT_AT_ON(t) ((void)0)
#endif

static void nature_note_on(track_t *t, voice_t *v)
{
    nat_t *s = NAT_ST(t);
    int32_t ev = nat_event_of(v->note);
    if (!s->init) {
        nat_init(s, v);
        NAT_AT_ON(t);
    }
    s->last_key = (uint8_t)(ev + 1);
    if (ev >= 0)
        nat_event(s, ev);
    s->playing = 1;                                    /* any key: the scene plays (again, after a pause) */
}

/* the voice ends when the scene has faded out */
static int nature_done(track_t *t, voice_t *v)
{
    nat_t *s = NAT_ST(t);
    (void)v;
    return !s->playing && s->fade <= 0;
}

/* the keys: chromatic from F3, the octave buttons move them (NAT_OCT: the host's octave, 0 = none) */
static int32_t nature_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(53 + 12 * (int32_t)(NAT_OCT) + (int32_t)k, 0, 127);
}

static void nature_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    nat_t *s = NAT_ST(t);
    const int16_t *p = t->p;
    int32_t lvl[NL_COUNT], evol = p[P_E6], mood, calm, tone, k, g, lo, gsh;
    nat_c_t cw, cw2, cwl, csl, csh, crh, crd, crb, cfl, cfc, ctl, ctm, ctc;
    int32_t wamp = 0, whis = 0, rumble = 0, hiss = 0, rthr = 0, rbig = 0, rhiss = 0, roar = 0, cthr = 0;
    int32_t tl[NL_COUNT], thmid = 0, tck = 0;
    int32_t benv0 = 0, benv1 = 0, thamp = 0, crl = 0, cre0[NAT_NCRICK], cre1[NAT_NCRICK], chl = 0, chw = 0, mfm, crush, hold,
            dust;
    uint32_t crinc[NAT_NCRICK], chinc[NAT_NCHIME];
    nat_c_t cmf;
    uint32_t binc = 0, vinc = 0, i;
    if (!s->init)
        nat_init(s, v);
    {   /* the fade: ATK / REL (0 .. ~10 s, squared) */
        int32_t pt = s->playing ? p[P_ATK] : p[P_REL], bl = (pt * pt * 7) >> 3, st = 32767 / (bl > 0 ? bl : 1);
        if (s->playing)
            s->fade = s->fade + st > 32767 ? 32767 : s->fade + st;
        else
            s->fade = s->fade - st < 0 ? 0 : s->fade - st;
    }

    /* -------- the generative mix: presence of each layer (1 - EVOL * 0.78 .. 1), the mood's wander */
    lo = 32767 - evol * 200;
    gsh = s->glide_sh ? clamp(s->glide_sh, 6, 13) : 8;
    for (k = 0; k < NL_COUNT; k++) {
        int32_t L;
        nat_walk(s, &s->wan[k], &s->wan_tg[k], &s->wan_cnt[k], lo, 32767, 11000, 41000, 12);
        s->lv[k] += ((nat_lvl(p[P_E0 + k]) << 8) - s->lv[k]) >> (gsh + FE_OCT);   /* glide (~0.5 s by default) */
        L = s->lv[k] >> 8;
        lvl[k] = L > 40 ? (L * (s->wan[k] >> 8)) >> 15 : 0;
        if (s->boost[k]) {                             /* an event's boost: at least ~75 % while it lasts */
            int32_t b = (s->boost[k] * 24000) >> 15;
            if (b > lvl[k])
                lvl[k] = b;
            s->boost[k] -= (s->boost[k] >> ((s->ev_sh ? s->ev_sh : 13) + FE_OCT)) + 1;
            if (s->boost[k] < 0)
                s->boost[k] = 0;
        }
    }
    nat_walk(s, &s->storm, &s->storm_tg, &s->storm_cnt, -evol * 120, evol * 120, 20000, 80000, 13);
    s->mood_s += ((p[P_E5] * 258 << 8) - s->mood_s) >> (gsh + FE_OCT);
    s->night_s += ((clamp(s->night_tg, 0, 32767) << 8) - s->night_s) >> (gsh + 1 + FE_OCT);
    mood = clamp((s->mood_s >> 8) + (s->storm >> 8), 0, 32767);
    /* TONE, the key around C4 and the track's FLT: an offset of every cutoff (0..127 << 8 units) */
    tone = (p[P_E7] - 64) * 85 + (((s->scene_note - 60) * 85)) + (m->cutoff >> 1);
    {   /* pressure on the key held last: quick in, ~0.2 s out */
        int32_t pr = v->gate ? (int32_t)(NAT_AT(t)) * 258 : 0, pl = -1;
        s->press += (pr - s->press) >> ((pr > s->press ? 3 : 7) + FE_OCT);
        pr = s->press;
        for (k = 0; k < NL_COUNT; k++)
            tl[k] = tone;
        if (pr < 9000)
            s->tcrk_arm = 1;
        if (pr > 300) {
            switch (s->last_key) {
            case EV_CRICKETS + 1:
                if (pr > s->cr_boost)
                    s->cr_boost = pr;
                break;
            case EV_THUNDER + 1:                       /* a distant rumble at a touch .. overhead pressed hard */
                g = 18000 + ((pr * 14767) >> 15);
                if (g > s->thun)
                    s->thun = g;
                if (pr > 22000 && s->tcrk_arm) {       /* pressed hard: it cracks right overhead */
                    s->tcrk_arm = 0;
                    s->tcrk = 32767;
                }
                break;
            case EV_WAVE + 1:
                pl = NL_SEA;
                g = 22000 + ((pr * 10767) >> 15);
                if (g > s->wave_big)
                    s->wave_big = g;
                g = 24000 + ((pr * 8767) >> 15);
                if (g > s->boost[NL_SEA])
                    s->boost[NL_SEA] = g;
                if (s->crash_tg > 0) {                 /* broken already: pressed, it crashes harder, foams more */
                    g = (s->wave_big * (24576 + (pr >> 2))) >> 15;
                    if (g > s->crash_tg)
                        s->crash_tg = g;
                    if (pr > 8000 && s->foam < (g >> 1))
                        s->foam = g >> 1;
                }
                break;
            case EV_GUST + 1:
                pl = NL_WIND;
                g = 19000 + ((pr * 13767) >> 15);
                if (g * 256 > s->gust_tg)
                    s->gust_tg = s->gust = g * 256;
                g = 22000 + ((pr * 10767) >> 15);
                if (g > s->boost[NL_WIND])
                    s->boost[NL_WIND] = g;
                break;
            case EV_DOWNPOUR + 1:
                pl = NL_RAIN;
                g = 24000 + ((pr * 8767) >> 15);
                if (g > s->boost[NL_RAIN])
                    s->boost[NL_RAIN] = g;
                mood = clamp(mood + (pr >> 2), 0, 32767);
                break;
            case EV_CRACKLE + 1:
                pl = NL_FIRE;
                g = 24000 + ((pr * 8767) >> 15);
                if (g > s->boost[NL_FIRE])
                    s->boost[NL_FIRE] = g;
                if (!s->crack_n && (nat_rnd(s) >> 17) < (uint32_t)(pr >> 5)) {
                    s->crack_n = nat_urand(s, 1, 4);
                    s->v_crack = 255;
                }
                break;
            case EV_BIRD + 1:
            case EV_CHORUS + 1:
                pl = NL_BIRD;
                g = 22000 + ((pr * 10767) >> 15);
                if (g > s->boost[NL_BIRD])
                    s->boost[NL_BIRD] = g;
                if (s->b_state == 0 && s->b_cnt > (32767 - pr) >> 4)
                    s->b_cnt = (32767 - pr) >> 4;      /* pressed harder: the next phrase sooner */
                break;
            case EV_CALM + 1:
                mood = clamp(mood - pr, 0, 32767);
                break;
            case EV_STORM + 1:
                mood = clamp(mood + (pr >> 1), 0, 32767);
                break;
            default:
                break;
            }
            /* what is pressed comes forward, louder (up to +6 dB) and brighter; the rest steps back (up to -8 dB) */
            if (pl >= 0 || s->last_key == EV_THUNDER + 1)
                for (k = 0; k < NL_COUNT; k++) {
                    if (k == pl) {
                        lvl[k] = (lvl[k] * (32768 + pr)) >> 15;
                        tl[k] = tone + ((pr * 22) >> 7);
                    } else
                        lvl[k] = (lvl[k] * (32767 - ((pr * 19) >> 5))) >> 15;
                }
        }
    }
    calm = 32767 - mood;
    lvl[NL_BIRD] = (lvl[NL_BIRD] * (calm >> 1) + lvl[NL_BIRD] * 8192) >> 15;   /* birds hide in a storm */

    for (k = 0; k < NL_COUNT; k++)
        s->v_lvl[k] = (int16_t)lvl[k];
    s->v_mood = (int16_t)mood;
    s->v_gust = (int16_t)(s->gust >> 8);

    /* -------- wind: pink noise through a gusting band-pass (+ a whistle in a storm) and a low rumble */
    if (lvl[NL_WIND]) {
        int32_t depth = 12000 + (mood >> 1), minb = 400, maxb = 4200 - (mood >> 4);
        nat_walk(s, &s->gust, &s->gust_tg, &s->gust_cnt, 0, 32767, minb, maxb, 9);
        g = s->gust >> 8;
        wamp = 32767 - depth + ((g * depth) >> 15);
        whis = (mood * g) >> 15;
        nat_coef(&cw, (54 << 8) + ((g * 32) >> 7) + tl[NL_WIND], 70 + (mood >> 10));
        nat_coef(&cw2, (78 << 8) + ((g * 26) >> 7) + tl[NL_WIND], 116);
        nat_coef(&cwl, (30 << 8) + ((g * 22) >> 7) + (tl[NL_WIND] >> 1), 20);
    }
    /* -------- sea: slow swells of brown noise, a crash of bright noise at each crest, the foam after */
    if (lvl[NL_SEA]) {
        uint32_t ph0 = s->wave_ph;
        int32_t x, crest = 20316;                      /* the crest at 62 % of the wave */
        s->wave_ph += s->wave_inc;
        if (s->wave_ph < ph0)
            nat_new_wave(s, mood);
        x = (int32_t)(s->wave_ph >> 17);
        if (x < crest) {
            int32_t u = (x * 32767) / crest, sw = (((u * u) >> 15) * s->wave_big) >> 15;
            if (sw > s->swell)
                s->swell = sw;
            else
                s->swell -= s->swell >> (11 + FE_OCT);
        } else {
            if ((int32_t)(ph0 >> 17) < crest) {        /* the wave breaks */
                s->crash_tg = s->wave_big;             /* the crash swells in over ~0.2 s */
            }
            s->swell -= s->swell >> (11 + FE_OCT);
        }
        s->crash += (s->crash_tg - s->crash) >> (7 + FE_OCT);
        s->crash_tg -= s->crash_tg >> (9 + FE_OCT);
        s->foam += ((s->crash >> 1) - s->foam) >> (9 + FE_OCT);   /* the foam: behind the crash, slower out */
        if (s->crash < s->foam)
            s->foam -= s->foam >> (12 + FE_OCT);
        rumble = 5000 + ((s->swell * 26000) >> 15);
        hiss = s->crash + s->foam;
        nat_coef(&csl, (34 << 8) + ((s->swell * 22) >> 7) + tl[NL_SEA], 10);
        nat_coef(&csh, (74 << 8) + ((s->crash * 26) >> 7) + ((s->foam * 10) >> 7) + tl[NL_SEA], 15);
    }
    /* -------- rain: a hiss and drops, small (bright, many) and big (lower, few) */
    if (lvl[NL_RAIN]) {
        int32_t rate = 25 + ((mood * 700) >> 15);      /* drops a second */
        rthr = (int32_t)(((uint32_t)rate * NAT_HZ_INC) >> 16);   /* per sample, of 65536 */
        rbig = rthr >> 3;
        rhiss = 4000 + (mood >> 1);
        nat_coef(&crh, (98 << 8) + tl[NL_RAIN], 10);
        nat_coef(&crd, (nat_urand(s, 82, 108) << 8) + tl[NL_RAIN], 112);
        nat_coef(&crb, (nat_urand(s, 62, 76) << 8) + tl[NL_RAIN], 100);
    }
    /* -------- fire: a flickering low roar and crackles in bursts */
    if (lvl[NL_FIRE]) {
        nat_walk(s, &s->flick, &s->flick_tg, &s->flick_cnt, 4000, 32767, 40, 400, 5);
        g = s->flick >> 8;
        roar = 7000 + (g >> 2);
        if (!s->crack_n && (nat_rnd(s) >> 16) < (uint32_t)((3 + (g >> 12)) * 65536 / NAT_BPS))
        {
            s->crack_n = nat_urand(s, 1, 7);           /* a burst of 1..7 crackles */
            s->v_crack = 255;
        }
        cthr = s->crack_n ? (int32_t)((300u * NAT_HZ_INC) >> 16) : 0;   /* ~ every 3 ms inside a burst */
        nat_coef(&cfl, (30 << 8) + ((g * 16) >> 7) + (tl[NL_FIRE] >> 1), 35);
        nat_coef(&cfc, (nat_urand(s, 86, 112) << 8) + tl[NL_FIRE], 45);
    }
    /* -------- birds: chirps of a sine, its pitch swept, a trill's vibrato */
    if (lvl[NL_BIRD]) {
        nat_bird_block(s, 1);
        if (s->b_state == 1) {
            int32_t f = s->b_f0 + ((s->b_f1 - s->b_f0) * s->b_pos) / s->b_len;
            f += (f * ((p[P_E7] - 64) + (s->scene_note - 60))) >> 8;   /* TONE and the key: a little */
            binc = (uint32_t)clamp(f, 300, 7000) * NAT_HZ_INC;
            vinc = (s->b_kind == BK_TRILL ? 28u : 9u) * NAT_HZ_INC;
            benv0 = s->b_env;
            benv1 = (sine_i((uint32_t)(s->b_pos + 1) * (0x80000000u / (uint32_t)s->b_len)) * s->b_amp) >> 15;
            if (benv1 < 0)
                benv1 = 0;
        } else {
            benv0 = s->b_env;
            benv1 = 0;
        }
        s->b_env = benv1;
    } else {
        nat_bird_block(s, 0);
        s->b_env = 0;
    }
    /* -------- crickets: at night (calm nights most), or called: pulsed trills of a high sine */
    s->cr_boost -= (s->cr_boost >> ((s->ev_sh ? s->ev_sh : 13) + FE_OCT)) + (s->cr_boost > 0);
    crl = ((((s->night_s >> 8) * calm) >> 15) * 24000) >> 15;
    crl = clamp((crl * (s->cr_amt ? s->cr_amt : 64)) >> 6, 0, 32767);
    if (s->cr_boost > crl)
        crl = s->cr_boost;
    s->v_night = (int16_t)(s->night_s >> 8);
    s->v_crick = (int16_t)crl;
    for (k = 0; k < NAT_NCRICK; k++) {
        static const int16_t CR_HZ[NAT_NCRICK] = {4400, 3900, 5100};
        cre0[k] = s->cr_env[k];
        if (--s->cr_cnt[k] <= 0) {
            if (s->cr_pulses[k] > 0)
                s->cr_pulses[k]--;
            if (s->cr_pulses[k] > 0) {
                s->cr_on[k] = s->cr_pulses[k] & 1;     /* odd: a pulse ~10 ms, even: the gap ~16 ms */
                s->cr_cnt[k] = FE_T(s->cr_on[k] ? 14 : 22);
            } else if (crl > 300 && (nat_rnd(s) & 3u)) {
                s->cr_pulses[k] = nat_urand(s, 3, 5) * 2;   /* a chirp of 3..5 pulses */
                s->cr_on[k] = 1;
                s->cr_cnt[k] = FE_T(14);
            } else {
                s->cr_on[k] = 0;
                s->cr_cnt[k] = FE_T(nat_urand(s, 350, 1400));   /* 0.25 .. 1 s between chirps */
            }
        }
        s->cr_env[k] += ((s->cr_on[k] ? 32767 : 0) - s->cr_env[k]) >> 1;
        cre1[k] = s->cr_env[k];
        crinc[k] = (uint32_t)(CR_HZ[k] + ((CR_HZ[k] * (p[P_E7] - 64)) >> 9)) * NAT_HZ_INC;
    }
    if (crl <= 300)
        for (k = 0; k < NAT_NCRICK; k++)
            cre0[k] = cre1[k] = 0;
    /* -------- thunder: in a storm with rain, now and then a long low rumble */
    if (lvl[NL_RAIN] && mood > 20000 && s->thun < 3000 &&
        nat_rnd(s) < (uint32_t)((mood - 20000) * (lvl[NL_RAIN] >> 10)) * (uint32_t)(s->th_amt ? s->th_amt : 64) / 16u)
    {
        s->thun = nat_urand(s, 16000, 32767);
        if (s->thun > 29000)
            s->tcrk = s->thun - 8000;                  /* a big one: right overhead */
    }
    if (s->thun) {   /* a low roll, its body (what a small speaker plays: the louder the higher) */
        nat_walk(s, &s->thun_flk, &s->thun_flk_tg, &s->thun_flk_cnt, 6000, 32767, 60, 700, 6);
        thamp = (s->thun * (s->thun_flk >> 8)) >> 15;
        thmid = (thamp * (6000 + (s->thun >> 1))) >> 15;
        nat_coef(&ctl, (22 << 8) + ((s->thun * 36) >> 7) + (tone >> 2), 25);
        nat_coef(&ctm, (56 << 8) + ((s->thun * 20) >> 7) + (tone >> 1), 30);
        s->thun -= (s->thun >> (12 + FE_OCT)) + 1;
        if (s->thun < 0)
            s->thun = 0;
    }
    if (s->tcrk) {   /* the crack: bright noise, flickering, gone in ~0.3 s */
        tck = (s->tcrk * (s->thun ? s->thun_flk >> 8 : 32767)) >> 15;
        nat_coef(&ctc, (84 << 8) + (tone >> 1), 20);
        s->tcrk -= (s->tcrk >> (8 + FE_OCT)) + 1;
        if (s->tcrk < 0)
            s->tcrk = 0;
    }

    /* -------- wind chimes: struck in the key and scale, more in the gusts; two partials (a bar's 1 : 2.76) */
    {
        static const uint8_t CH_SCALE[5][7] = {{0, 2, 4, 7, 9, 12, 14}, {0, 3, 5, 7, 10, 12, 15}, {0, 1, 5, 7, 8, 12, 13},
                                               {0, 2, 4, 6, 8, 10, 12}, {0, 2, 4, 6, 7, 9, 11}};   /* PENTA MAJ, PENTA
                                                * MIN, IN SEN, WHOLE, LYDIAN */
        static const uint16_t C5_HZ16[12] = {8372, 8870, 9397, 9956, 10548, 11175, 11840, 12544, 13290, 14080, 14917, 15804};
        int32_t gq = lvl[NL_WIND] ? (s->gust >> 8) : 4000, thr = 12 + ((gq * 150) >> 15), dk;
        int32_t wv = ((sine_i(s->wob_ph) * s->lf_wob) >> 7) + ((sine_i(s->flut_ph) * s->lf_wob) >> 9);   /* Q15-ish */
        s->wob_ph += 1870000u;                         /* ~0.6 Hz: the tape's wow */
        s->flut_ph += 21800000u;                       /* ~7 Hz: its flutter */
        chw = wv;
        if (s->ch_lvl && (int32_t)(nat_rnd(s) >> 16) < thr) {   /* a chime struck */
            int32_t v = s->ch_next++ % NAT_NCHIME, deg = nat_urand(s, 0, 6), n = CH_SCALE[s->ch_scale % 5u][deg] + s->ch_key % 12u;
            uint32_t hz16 = (uint32_t)C5_HZ16[n % 12] << (n / 12), inc;
            inc = (hz16 * NAT_HZ_INC) >> 4;
            s->ch_note[v] = (int32_t)inc;
            s->ch_env[v] = nat_urand(s, 14000, 32767);
        }
        dk = 79 - s->ch_decay * 73 / 127;              /* ~0.3 .. 4 s */
        for (k = 0; k < NAT_NCHIME; k++) {
            s->ch_env[k] -= (s->ch_env[k] * dk) >> 15;
            if (s->ch_env[k] < 40)
                s->ch_env[k] = 0;
            chinc[k] = (uint32_t)s->ch_note[k] + (uint32_t)(((int32_t)((uint32_t)s->ch_note[k] >> 15) * wv) >> 13);
        }
        chl = s->ch_lvl ? nat_lvl(s->ch_lvl) : 0;
    }
    if (crl > 300)                                     /* (the tape's wobble bends the crickets too) */
        for (k = 0; k < NAT_NCRICK; k++)
            crinc[k] += (uint32_t)(((int32_t)(crinc[k] >> 15) * chw) >> 13);
    if (binc)
        binc += (uint32_t)(((int32_t)(binc >> 15) * chw) >> 13);
    /* -------- the master filter (a DJ filter) */
    mfm = s->mf_filt < 58 ? 1 : s->mf_filt > 70 ? 2 : 0;
    if (mfm)
        nat_coef(&cmf, mfm == 1 ? (20 + s->mf_filt * 107 / 58) << 8 : ((s->mf_filt - 70) * 105 / 57) << 8,
                 s->mf_reso * 110 / 127);
    crush = s->lf_crush ? s->lf_crush * 12 / 127 : 0;  /* bits taken: 0 .. 12 (16 .. 4 left) */
    hold = 1 + s->lf_rate * 23 / 127;
    dust = s->lf_dust ? (s->lf_dust * s->lf_dust) >> 6 : 0;   /* clicks (of 65536 a sample) and hiss */

    /* ------------------------------------------------------------ samples */
    for (i = 0; i < n; i++) {
        uint32_t r1 = nat_rnd(s), r2 = nat_rnd(s);
        int32_t w = (int32_t)(r1 >> 17) - 16384, w2 = (int32_t)(r2 >> 17) - 16384, acc = 0, y, bp, pk;
        /* pink (Kellet's three poles) and brown (a leaky integrator) from the white */
        s->pk0 = (mul_wrap(s->pk0, FE_NOISE_PINK0_P) >> 15) + ((w * FE_NOISE_PINK0_G) >> 15);
        s->pk1 = (mul_wrap(s->pk1, FE_NOISE_PINK1_P) >> 15) + ((w * FE_NOISE_PINK1_G) >> 15);
        s->pk2 = (mul_wrap(s->pk2, FE_NOISE_PINK2_P) >> 15) + ((w * FE_NOISE_PINK2_G) >> 14);
        pk = (s->pk0 + s->pk1 + s->pk2 + ((w * 6055) >> 15)) >> 1;
        s->brown = (mul_wrap(s->brown, FE_NOISE_BROWN_P) >> 15) + ((w * FE_NOISE_BROWN_G) >> 15);
        s->brown2 = (mul_wrap(s->brown2, FE_NOISE_BROWN_P) >> 15) + ((w2 * FE_NOISE_BROWN_G) >> 15);

        if (lvl[NL_WIND]) {
            int32_t bp2, lp;
            nat_svf(&cw, &s->w_bp, pk, &bp);
            nat_svf(&cw2, &s->w_bp2, pk, &bp2);
            lp = nat_svf(&cwl, &s->w_lp, s->brown, 0);
            y = clamp(bp * 3 + ((bp2 * whis) >> 14) + lp, -60000, 60000);
            acc += (((((y * wamp) >> 15) * (lvl[NL_WIND] >> 3)) >> 12) * NAT_GAIN[NL_WIND]) >> 8;
        }
        if (lvl[NL_SEA]) {
            int32_t lo2 = nat_svf(&csl, &s->s_lp, s->brown2, 0), hi = nat_svf(&csh, &s->s_hi, w, 0);
            y = ((clamp(lo2, -60000, 60000) * rumble) >> 14) + ((clamp(hi, -60000, 60000) * hiss) >> 14);
            acc += ((((y >> 1) * (lvl[NL_SEA] >> 3)) >> 12) * NAT_GAIN[NL_SEA]) >> 8;
        }
        if (lvl[NL_RAIN]) {
            int32_t imp = 0, big = 0, h, d, bb;
            if ((int32_t)(r2 & 0xFFFFu) < rthr)
                imp = (r1 & (1u << 16)) ? 20000 + (int32_t)((r2 >> 18) & 0x3FFFu) : -20000 - (int32_t)((r2 >> 18) & 0x3FFFu);
            if ((int32_t)((r2 >> 16) & 0xFFFFu) < rbig)
                big = (r1 & 0x8000u) ? 26000 : -26000;
            nat_svf(&crh, &s->r_hiss, w2, &h);
            nat_svf(&crd, &s->r_drop, imp, &d);
            nat_svf(&crb, &s->r_big, big, &bb);
            y = clamp(((h * rhiss) >> 15) + d * 2 + bb * 2, -60000, 60000);
            acc += (((y * (lvl[NL_RAIN] >> 3)) >> 12) * NAT_GAIN[NL_RAIN]) >> 8;
        }
        if (lvl[NL_FIRE]) {
            int32_t imp = 0, lr, c;
            if (cthr && (int32_t)(r2 & 0xFFFFu) < cthr) {
                imp = (r1 & 0x4000u) ? 16000 + (int32_t)((r1 >> 17) & 0x3FFFu) : -16000 - (int32_t)((r1 >> 17) & 0x3FFFu);
                if (--s->crack_n <= 0) {
                    s->crack_n = 0;
                    cthr = 0;
                }
            }
            lr = nat_svf(&cfl, &s->f_lp, s->brown, 0);
            nat_svf(&cfc, &s->f_crk, imp, &c);
            y = clamp(((clamp(lr, -100000, 100000) * roar) >> 13) + c * 2, -60000, 60000);
            acc += (((y * (lvl[NL_FIRE] >> 3)) >> 12) * NAT_GAIN[NL_FIRE]) >> 8;
        }
        if (benv0 | benv1) {
            int32_t e = benv0 + (((benv1 - benv0) * (int32_t)i) >> CTL_LOG2), vib = sine_i(s->b_vph);
            uint32_t inc = binc + (uint32_t)(((int32_t)(binc >> 15) * vib) >> (s->b_kind == BK_TRILL ? 13 : 15));
            s->b_ph += inc;
            s->b_vph += vinc;
            y = (sine_i(s->b_ph) * e) >> 15;
            acc += ((((y >> 1) * (lvl[NL_BIRD] >> 3)) >> 12) * NAT_GAIN[NL_BIRD]) >> 8;
        }
        if (crl > 300) {
            int32_t cy = 0;
            for (k = 0; k < NAT_NCRICK; k++)
                if (cre0[k] | cre1[k]) {
                    int32_t e = cre0[k] + (((cre1[k] - cre0[k]) * (int32_t)i) >> CTL_LOG2);
                    s->cr_ph[k] += crinc[k];
                    cy += (sine_i(s->cr_ph[k]) * e) >> 15;
                }
            acc += ((((cy >> 1) * (crl >> 3)) >> 12) * 70) >> 8;
        }
        if (thamp) {
            y = clamp(nat_svf(&ctl, &s->t_lp, s->brown2, 0), -60000, 60000);
            acc += (y * thamp) >> 14;
            nat_svf(&ctm, &s->t_mid, pk, &bp);
            acc += ((clamp(bp, -60000, 60000) * thmid) >> 15) * TH_MID >> 4;
        }
        if (tck) {
            nat_svf(&ctc, &s->t_crk, w, &bp);
            acc += ((clamp(bp, -60000, 60000) * tck) >> 15) * TH_CRK >> 4;
        }
        if (chl) {                                     /* the chimes */
            int32_t cy = 0;
            for (k = 0; k < NAT_NCHIME; k++)
                if (s->ch_env[k]) {
                    int32_t e = s->ch_env[k], e2 = (e * e) >> 15;
                    uint32_t i2 = chinc[k] * 2u + (chinc[k] >> 1) + (chinc[k] >> 2);   /* x 2.75 */
                    s->ch_ph[k][0] += chinc[k];
                    s->ch_ph[k][1] += i2;
                    cy += ((sine_i(s->ch_ph[k][0]) * e) >> 15) + ((sine_i(s->ch_ph[k][1]) * e2) >> 17);
                }
            acc += ((((cy >> 1) * (chl >> 3)) >> 12) * 150) >> 8;
        }
        if (dust) {                                    /* vinyl: crackles and a little hiss */
            if ((int32_t)(r1 & 0xFFFFu) < (dust >> 4))
                acc += (r2 & 1u) ? 9000 + (int32_t)((r1 >> 20) & 0x1FFFu) : -9000 - (int32_t)((r1 >> 20) & 0x1FFFu);
            acc += (w2 * (dust >> 4)) >> 12;
        }
        /* a DC blocker (~14 Hz) and the knee */
        s->dc += (shl(acc, 8) - s->dc) >> (9 + FE_OCT);
        y = soft_knee(acc - (s->dc >> 8), 24000);
        if (mfm) {                                     /* the master filter: low-pass or high-pass */
            int32_t bp, lp = nat_svf(&cmf, &s->mf, y, &bp);
            y = mfm == 1 ? lp : y - lp - bp;
            y = soft_knee(clamp(y, -100000, 100000), 24000);
        }
        if (crush)                                     /* LO-FI: fewer bits, a lower rate, the tape's level wobble */
            y &= ~((1 << crush) - 1);
        if (hold > 1) {
            if (--s->hold_n <= 0) {
                s->hold_n = hold;
                s->hold_v = y;
            }
            y = s->hold_v;
        }
        if (chw)
            y = (y * (32767 - (((sine_i(s->flut_ph) + 32768) >> 1) * s->lf_wob >> 9))) >> 15;
        y = (y * s->fade) >> 15;
        out[i] += voice_amp(y, m, i);
    }
}

/* {WIND, SEA, RAIN, FIRE, BIRD, MOOD, EVOL, TONE}, {ATK DEC SUS REL}: slow in, held, slow out */
static const preset_t NATURE_PRESETS[] = {
    {"SHORE", {45, 110, 0, 0, 45, 30, 60, 64}, {70, 64, 127, 90}, 0, 0, FX(0, 0, 0, 50), PAT(5)},
    {"STORM", {110, 70, 110, 0, 0, 118, 45, 56}, {70, 64, 127, 90}, 0, 0, FX(0, 0, 0, 40), PAT(5)},
    {"FOREST", {40, 0, 0, 0, 105, 15, 70, 70}, {70, 64, 127, 90}, 0, 0, FX(0, 0, 10, 70), PAT(5)},
    {"CAMPFIRE", {30, 0, 0, 110, 25, 20, 50, 60}, {70, 64, 127, 90}, 0, 0, FX(0, 0, 0, 35), PAT(5)},
    {"RAINY", {30, 0, 100, 0, 15, 60, 50, 64}, {70, 64, 127, 90}, 0, 0, FX(0, 0, 0, 45), PAT(5)},
    {"DRIFT", {70, 85, 60, 45, 60, 50, 127, 64}, {70, 64, 127, 90}, 0, 0, FX(0, 0, 0, 60), PAT(5)},
};

static const engine_t ENG_NATURE = {
    .name = "NATURE",
    .page_title = {"MIX", "SCENE"},
    .edit = {
        {"WIND", F_PCT, 0, 127, 60, 0, 0},
        {"SEA", F_PCT, 0, 127, 80, 0, 0},
        {"RAIN", F_PCT, 0, 127, 0, 0, 0},
        {"FIRE", F_PCT, 0, 127, 0, 0, 0},
        {"BIRD", F_PCT, 0, 127, 40, 0, 0},
        {"MOOD", F_PCT, 0, 127, 30, 0, 0},
        {"EVOL", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = NATURE_PRESETS,
    .npresets = NELEM(NATURE_PRESETS),
    .knob = {P_E5, P_E6, P_E7, P_REL},     /* HOME: MOOD EVOL TONE, release */
    .poly = 1,
    .note_on = nature_note_on,
    .render = nature_render,
    .keys = nature_keys,
    .ownenv = 1,
    .done = nature_done,
    .mem = sizeof(nat_t),
};
