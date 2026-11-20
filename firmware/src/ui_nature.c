/* SPDX-License-Identifier: GPL-3.0-only */
/* NATURE's screen: a 110 x 50 pixel-art landscape drawn x2 in the graph panel (x 10..230, y 11..111), alive with the
 * engine's own state (eng_nature.c nat_t: the layers' levels, the mood, the gusts, the waves, thunder, crackles,
 * the bird singing). Dusk when calm, grey when stormy; a person sits on the hill (an umbrella in the rain), a fire
 * burns beside them, the pines sway, rain slants with the wind, lightning comes with the thunder.
 * Drawn straight into the canvas (no buffer of its own); one step of animation a draw (HOME: every other frame). */
#if FELUCCA_NATURE

#define NS_W 110
#define NS_H 50
#define NS_HZ 33                                       /* the horizon */
#define NS_X0 10                                       /* the scene on the canvas, 2 x 2 a pixel */
static int32_t ns_y0 = 11;                             /* the scene's top on the canvas (the app: below its header) */
#define NS_Y0 ns_y0
#define NC(h) ((uint16_t)(((((h) >> 19) & 0x1Fu) << 11) | ((((h) >> 10) & 0x3Fu) << 5) | (((h) >> 3) & 0x1Fu)))
#define NS_NDROP 70
#define NS_NSPARK 16
#define NS_NSMOKE 12

static struct {
    uint32_t t, rng, sway_ph;
    uint8_t init, flash, crackglow;
    int16_t cl_x[7], cl_y[7], cl_s[7];                 /* clouds: x Q4, y, size */
    int16_t dr_x[NS_NDROP], dr_y[NS_NDROP];            /* rain drops, Q4 */
    uint8_t ndrops;
    int16_t sp_x[NS_NSPARK], sp_y[NS_NSPARK], sp_vx[NS_NSPARK], sp_vy[NS_NSPARK];   /* sparks, Q4 */
    uint8_t sp_life[NS_NSPARK];
    int16_t sm_x[NS_NSMOKE], sm_y[NS_NSMOKE];          /* smoke, Q4 */
    uint8_t sm_life[NS_NSMOKE];
    int16_t fl_x, fl_y;                                /* the flock: x Q4 (-1: none) */
    int8_t bolt[NS_HZ + 2];                            /* the lightning's x on each row (-1: none) */
    int32_t prev_thun;
    uint8_t st_x[18], st_y[18], st_ph[18];             /* stars */
} ns;

static uint32_t ns_rnd(void)
{
    uint32_t s = ns.rng;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return ns.rng = s;
}
static int32_t ns_ur(int32_t lo, int32_t hi) { return lo + (int32_t)(((ns_rnd() >> 16) * (uint32_t)(hi - lo + 1)) >> 16); }

static uint16_t ns_mix(uint16_t a, uint16_t b, int32_t t)   /* t 0..256 */
{
    int32_t r = (a >> 11) & 31, g = (a >> 5) & 63, bl = a & 31;
    t = clamp(t, 0, 256);
    r += ((((b >> 11) & 31) - r) * t) >> 8;
    g += ((((b >> 5) & 63) - g) * t) >> 8;
    bl += (((b & 31) - bl) * t) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static inline void ns_px(int32_t x, int32_t y, uint16_t c)
{
    uint16_t s, *p;
    if ((uint32_t)x >= NS_W || (uint32_t)y >= NS_H)
        return;
    s = swap16(c);
    p = &cv_px[(uint32_t)(NS_Y0 + 2 * y) * cv_w + (uint32_t)(NS_X0 + 2 * x)];
    p[0] = p[1] = p[cv_w] = p[cv_w + 1] = s;
}
static inline uint16_t ns_get(int32_t x, int32_t y)
{
    if ((uint32_t)x >= NS_W || (uint32_t)y >= NS_H)
        return 0;
    return swap16(cv_px[(uint32_t)(NS_Y0 + 2 * y) * cv_w + (uint32_t)(NS_X0 + 2 * x)]);
}
static inline void ns_blend(int32_t x, int32_t y, uint16_t c, int32_t t) { ns_px(x, y, ns_mix(ns_get(x, y), c, t)); }
static inline int32_t ns_sin(uint32_t ph) { return sine_i(ph); }   /* Q15 */

/* the palettes: calm (dusk) and storm; mixed by how dark it is */
enum { PC_SKY0, PC_SKY1, PC_SKY2, PC_SKY3, PC_SEA0, PC_SEA1, PC_SEAL, PC_LAND, PC_LAND2, PC_CLOUD, PC_CLOUD2, PC_N };
static const uint32_t NS_CALM[PC_N] = {0x1d1a3b, 0x5b2a5e, 0xc4506a, 0xf5a65b, 0x173a5e, 0x1f5f7a, 0xf7c873,
                                       0x1f3d2b, 0x2c5a3a, 0xe88a8a, 0xb8607a};
static const uint32_t NS_NIGHT[PC_N] = {0x070a1a, 0x0d1530, 0x1a2448, 0x2b3560, 0x06121f, 0x0c2236, 0xc8d4f0,
                                        0x0c1a14, 0x13261c, 0x3a4466, 0x262d48};
static const uint32_t NS_STORM[PC_N] = {0x101318, 0x1c222c, 0x2b3440, 0x3d4855, 0x0f1d26, 0x1d3340, 0x5d7180,
                                        0x121c17, 0x1b2a21, 0x3a4350, 0x262d36};
#define C_WHITE NC(0xf4f1e8u)
#define C_FOAM NC(0xd8f3f0u)
#define C_SUN NC(0xffd86bu)
#define C_SUN2 NC(0xff8f5au)
#define C_TRUNK NC(0x4a2f27u)
#define C_PINE NC(0x1b4d34u)
#define C_PINE2 NC(0x2d7048u)
#define C_SAND NC(0xb98a5eu)
#define C_LOG NC(0x6b3e26u)
#define C_F1 NC(0xfff3a0u)
#define C_F2 NC(0xffb13bu)
#define C_F3 NC(0xe8452cu)
#define C_SMOKE NC(0x7a7480u)
#define C_SKIN NC(0xe0a77eu)
#define C_HOOD NC(0x3f6fd8u)
#define C_PANTS NC(0x2a2440u)
#define C_HAIR NC(0x2b1b17u)
#define C_UMB NC(0xe8452cu)
#define C_UMB2 NC(0xffd86bu)
#define C_RAIN NC(0x9fc3e0u)
#define C_BIRD NC(0x141018u)
#define C_BOLT NC(0xfffbe6u)
#define C_GLOW NC(0xfff0c8u)
#define C_MOON NC(0xe8ecf8u)
static const uint8_t NS_BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static void ns_init(void)
{
    uint32_t k;
    ns.rng = 0x2545F491u;
    for (k = 0; k < 7u; k++) {
        ns.cl_x[k] = (int16_t)(ns_ur(0, NS_W) * 16);
        ns.cl_y[k] = (int16_t)ns_ur(2, 18);
        ns.cl_s[k] = (int16_t)ns_ur(5, 11);
    }
    for (k = 0; k < 18u; k++) {
        ns.st_x[k] = (uint8_t)ns_ur(0, NS_W - 1);
        ns.st_y[k] = (uint8_t)ns_ur(0, 15);
        ns.st_ph[k] = (uint8_t)ns_ur(0, 255);
    }
    ns.fl_x = -1;
    ns.init = 1;
}

/* the ground's top at x: a hill sloping to a beach (sea) or a rolling meadow (no sea) */
static int32_t ns_top(int32_t x, int32_t sea, int32_t shore)
{
    int32_t f;
    if (!sea)
        return NS_HZ + 1 + ((ns_sin((uint32_t)x * 47820000u) * 2) >> 15);   /* sin(x * 0.07) * 2 */
    if (x < shore) {
        f = x * 256 / shore;                           /* (x / shore) ^ 1.6, near enough: 0.6 f^2 + 0.4 f */
        return NS_HZ - 2 + ((((f * f) >> 8) * 154 + f * 102) * 9 >> 16);
    }
    return NS_HZ + 7 + ((x - shore) * 3) / 5;          /* the beach: a gentle slope into the water */
}

static void graph_nature(track_t *t)
{
    nat_t *s = NAT_ST(t);
    int32_t wind = s->v_lvl[NL_WIND], sea = s->v_lvl[NL_SEA] > 650, rain = s->v_lvl[NL_RAIN], fire = s->v_lvl[NL_FIRE];
    int32_t bird = s->v_lvl[NL_BIRD], mood = s->v_mood, gust = s->v_gust;
    int32_t dark, windv, sun_a, shore, x, y, k, i, ncl, nt;
    uint16_t pal[PC_N], band[4];
    uint32_t tt;
    if (!ns.init)
        ns_init();
    tt = ++ns.t;
    if (!s->init) {                                    /* no scene yet: the engine's defaults, still */
        wind = 4000;
        sea = 1;
        mood = 6000;
    }
    dark = clamp(((mood * 294) >> 15) + ((rain * 64) >> 15), 0, 256);      /* 0..256 */
    windv = ((wind >> 7) * (77 + (gust >> 7))) >> 8;                        /* Q8, 0..~330 */
    nt = s->v_night >> 7;                                                   /* night 0..256 */
    for (k = 0; k < PC_N; k++)
        pal[k] = ns_mix(ns_mix(NC(NS_CALM[k]), NC(NS_NIGHT[k]), nt), NC(NS_STORM[k]), dark);
    shore = sea ? 46 : NS_W;

    /* lightning: a new strike when the thunder jumps up */
    if (s->thun > ns.prev_thun + 2000) {
        ns.flash = 4;
        x = ns_ur(20, NS_W - 10);
        for (y = 0; y < NS_HZ + 2; y++) {
            ns.bolt[y] = (int8_t)(y < 4 ? -1 : x);
            x += ns_ur(-1, 2) > 0 ? 1 : ns_ur(-1, 0);
        }
    }
    ns.prev_thun = s->thun;

    /* the sky: four bands, dithered */
    band[0] = pal[PC_SKY0], band[1] = pal[PC_SKY1], band[2] = pal[PC_SKY2], band[3] = pal[PC_SKY3];
    for (y = 0; y < NS_HZ; y++) {
        int32_t f = y * 3 * 16 / (NS_HZ - 1), bi = f >> 4 > 2 ? 2 : f >> 4, fr = f - (bi << 4);
        if (bi == 2 && f >= 48)
            fr = 16;
        for (x = 0; x < NS_W; x++) {
            uint16_t c = fr > NS_BAYER[y & 3][x & 3] ? band[bi + 1] : band[bi];
            if (ns.flash)
                c = ns_mix(c, C_BOLT, ns.flash > 2 ? 140 : 64);
            ns_px(x, y, c);
        }
    }
    /* stars, calm only (bright at night) */
    {
        int32_t sa = clamp(256 - dark * 2, 0, 256);
        if (nt > sa)
            sa = nt - (dark >> 1);
        if (sa > 0)
            for (k = 0; k < 18; k++)
                if (((tt >> 2) + ns.st_ph[k]) % 7u)
                    ns_blend(ns.st_x[k], ns.st_y[k] + (nt > 128 ? (k & 1) * 10 : 0), C_WHITE, sa);
    }
    /* the moon at night: a crescent */
    if (nt > 64 && dark < 200) {
        int32_t cx = 84, cy = 9, a = clamp((nt - 64) * 2 - dark, 0, 256);
        for (y = cy - 5; y <= cy + 5; y++)
            for (x = cx - 5; x <= cx + 5; x++)
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 25 && (x - cx - 2) * (x - cx - 2) + (y - cy + 1) * (y - cy + 1) > 16)
                    ns_blend(x, y, C_MOON, a);
    }
    /* the sun sinking into the sea, striped; fades in a storm */
    sun_a = clamp(256 - ((dark * 410) >> 8) - nt, 0, 256);
    if (sun_a) {
        int32_t cx = 80, cy = NS_HZ + 1, r = 8;
        for (y = cy - r; y <= cy && y < NS_HZ; y++) {
            if (y > cy - 4 && !(y & 1))
                continue;
            for (x = cx - r; x <= cx + r; x++)
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r)
                    ns_blend(x, y, ns_mix(C_SUN, C_SUN2, (y - (cy - r)) * 256 / r), sun_a);
        }
    }
    /* clouds: more and darker with the mood and the rain, drifting with the wind */
    ncl = 2 + ((dark * 5) >> 8);
    for (k = 0; k < ncl; k++) {
        static const int8_t OX[4] = {0, 7, -7, 2}, OY[4] = {0, 1, 1, -4}, RR[4] = {6, 5, 4, 4};   /* x10 of the size */
        int32_t cx, cy, sz = ns.cl_s[k], j;
        ns.cl_x[k] = (int16_t)(ns.cl_x[k] + 1 + ((windv * 6) >> 8) + (k >> 2));
        if ((ns.cl_x[k] >> 4) - sz * 2 > NS_W) {
            ns.cl_x[k] = (int16_t)(-(sz * 2) * 16);
            ns.cl_y[k] = (int16_t)ns_ur(2, 18 - ((dark * 8) >> 8));
        }
        cx = ns.cl_x[k] >> 4;
        cy = ns.cl_y[k];
        for (j = 0; j < 4; j++) {
            int32_t ox = cx + OX[j] * sz / 10, oy = cy + OY[j] * sz / 10, rr = RR[j] * sz / 10;
            for (y = oy - rr - 1; y <= oy + rr; y++)
                for (x = ox - rr - 1; x <= ox + rr + 1; x++)
                    if (y >= 0 && y < NS_HZ && (x - ox) * (x - ox) * 16 + (y - oy) * (y - oy) * 41 <= rr * rr * 16)
                        ns_px(x, y, y * 10 > oy * 10 + rr * 3 ? pal[PC_CLOUD2] : pal[PC_CLOUD]);
        }
    }
    if (ns.flash > 1)
        for (y = 0; y < NS_HZ + 2; y++)
            if (ns.bolt[y] >= 0)
                ns_px(ns.bolt[y], y, C_BOLT);

    /* the sea, or the meadow */
    for (y = NS_HZ; y < NS_H; y++) {
        int32_t d = (y - NS_HZ) * 256 / (NS_H - NS_HZ);
        uint16_t c0 = sea ? ns_mix(pal[PC_SEA1], pal[PC_SEA0], d) : pal[PC_LAND];
        for (x = 0; x < NS_W; x++) {
            uint16_t c = c0;
            if (sea && sun_a && (x > 80 ? x - 80 : 80 - x) < 9 - d * 4 / 256 && (y + tt / 6u) % 3u == 0 &&
                (ns_rnd() & 255u) < 180u)
                c = ns_mix(c, pal[PC_SEAL], (180 * sun_a) >> 8);
            ns_px(x, y, c);
        }
    }
    if (sea) {
        int32_t swell = s->swell, crash = s->crash + s->foam;
        static const uint32_t SP[3] = {34180000u, 54690000u, 75190000u};    /* 0.05 0.08 0.11 rad a frame */
        for (k = 0; k < 3; k++) {
            int32_t yb = NS_HZ + 3 + 4 * k, amp = k == 0 ? 154 : k == 1 ? 256 : 410 + ((swell * 640) >> 15);   /* Q8 px */
            for (x = shore - 8; x < NS_W; x++) {
                y = yb + ((ns_sin((uint32_t)x * 150384000u + tt * SP[k] + (uint32_t)k * 683565000u) * amp) >> 23) -
                    (k == 2 ? (swell * 3) >> 15 : 0);
                ns_blend(x, y, C_FOAM, 90 + 51 * k);
            }
        }
        for (x = shore - 10; x < NS_W; x++)
            for (y = NS_H - 4; y < NS_H; y++)
                if ((int32_t)(ns_rnd() >> 17) < ((crash * 29) >> 5) * (5 - (y - (NS_H - 4))) / 5)
                    ns_px(x, y, C_FOAM);
    }
    /* the land: the hill, the beach into the water, grass bending in the wind */
    for (x = 0; x < NS_W; x++) {
        int32_t top = ns_top(x, sea, shore), beach = sea && x >= shore - 7;
        if (top >= NS_H)
            break;
        for (y = top < 0 ? 0 : top; y < NS_H; y++) {
            uint16_t c;
            if (beach) {                               /* solid sand under the slope; wet (darker) at the water */
                c = (x + y) % 4 ? C_SAND : ns_mix(C_SAND, pal[PC_LAND], 77);
                if (x >= shore && y <= top + 1)
                    c = ns_mix(C_SAND, pal[PC_SEA1], 110);
            } else {
                c = y == top || (y == top + 1 && (x * 7) % 3 == 0) ? pal[PC_LAND2] : pal[PC_LAND];
            }
            ns_px(x, y, ns_mix(c, pal[PC_SKY1], (64 * dark + 120 * nt) >> 8));
        }
        if (!beach && (x * 5) % 3 == 0 && top > 0) {
            int32_t lean = (windv * 2 + ((ns_sin(tt * 205000000u + (uint32_t)x * 683565000u) * windv) >> 15)) >> 8;
            ns_px(x + lean, top - 1, pal[PC_LAND2]);
        }
    }
    if (sea) {                                         /* foam licking the sand */
        int32_t crash = s->crash + s->foam;
        for (x = shore; x < NS_W; x++) {
            y = ns_top(x, sea, shore) - 1;               /* (the water's edge, just over the slope) */
            if ((int32_t)(ns_rnd() >> 17) < 9830 + crash)
                ns_px(x, y, C_FOAM);
        }
    }
    /* the pines, swaying with the gusts */
    {
        static const uint8_t TX_SEA[2][2] = {{8, 15}, {17, 11}};
        static const uint8_t TX_LAND[6][2] = {{8, 15}, {17, 11}, {58, 13}, {70, 17}, {84, 12}, {99, 15}};
        const uint8_t (*tr)[2] = sea ? TX_SEA : TX_LAND;
        int32_t ntr = sea ? 2 : 6, sway;
        ns.sway_ph += 54700000u + (uint32_t)gust * 5215u;
        sway = ((((ns_sin(ns.sway_ph) * windv) >> 15) * 22) / 10) + windv * 3 / 2;   /* Q8 px */
        for (k = 0; k < ntr; k++) {
            int32_t tx = tr[k][0], th = tr[k][1], gy = ns_top(tx, sea, shore);
            for (y = gy - 3; y <= gy; y++)
                ns_px(tx, y, C_TRUNK);
            for (i = 0; i < th; i++) {
                int32_t half = i % 4 != 3 ? (th - i) * 42 / 100 : (th - i) * 30 / 100, off = (sway * i / th) >> 8;
                y = gy - 3 - i;
                for (x = tx - half + off; x <= tx + half + off; x++)
                    ns_px(x, y, ns_mix(x < tx + off ? C_PINE2 : C_PINE, pal[PC_SKY1], (64 * dark + 130 * nt) >> 8));
            }
        }
    }
    /* the fire */
    {
        int32_t fx = 31, fy = ns_top(31, sea, shore), hgt = 0;
        if (fire > 650) {
            int32_t glow = 64 + ((fire * 102) >> 15) + (ns.crackglow * 77 >> 8);
            for (y = fy - 6; y < NS_H; y++)
                for (x = fx - 12; x <= fx + 12; x++) {
                    int32_t d2 = (x - fx) * (x - fx) * 10 + (y - fy) * (y - fy) * 32;   /* x10 */
                    if (d2 < 1200)
                        ns_blend(x, y, C_GLOW, (glow * (1200 - d2) / 1200 * 154) >> 8);
                }
            ns_px(fx - 2, fy, C_LOG), ns_px(fx - 1, fy, C_LOG), ns_px(fx + 1, fy, C_LOG), ns_px(fx + 2, fy, C_LOG);
            ns_px(fx, fy, C_F3);
            hgt = 2 + ((fire * 3) >> 15) + ns_ur(0, 2);
            for (i = 0; i < hgt; i++) {
                int32_t w = (hgt - i) / 2, lean = (windv * i * 2) / (5 * 256);
                for (x = fx - w; x <= fx + w; x++)
                    if ((ns_rnd() & 255u) < 218u)
                        ns_px(x + lean, fy - 1 - i,
                              i * 100 < hgt * 35 && x == fx ? C_F1 : i * 100 < hgt * 70 ? C_F2 : C_F3);
            }
            if (s->v_crack) {                          /* a crackle: sparks fly */
                ns.crackglow = 255;
                s->v_crack = 0;
                for (k = 0, i = ns_ur(2, 5); k < NS_NSPARK && i; k++)
                    if (!ns.sp_life[k]) {
                        ns.sp_x[k] = (int16_t)(fx * 16 + ns_ur(-16, 16));
                        ns.sp_y[k] = (int16_t)((fy - 3) * 16);
                        ns.sp_vx[k] = (int16_t)(ns_ur(-6, 6) + ((windv * 5) >> 8));
                        ns.sp_vy[k] = (int16_t)ns_ur(-22, -10);
                        ns.sp_life[k] = 12;
                        i--;
                    }
            }
            if (tt % 5u == 0)
                for (k = 0; k < NS_NSMOKE; k++)
                    if (!ns.sm_life[k]) {
                        ns.sm_x[k] = (int16_t)(fx * 16);
                        ns.sm_y[k] = (int16_t)((fy - hgt - 1) * 16);
                        ns.sm_life[k] = 30;
                        break;
                    }
        }
        ns.crackglow = ns.crackglow > 40 ? (uint8_t)(ns.crackglow - 40) : 0;
        for (k = 0; k < NS_NSPARK; k++)
            if (ns.sp_life[k]) {
                ns.sp_x[k] = (int16_t)(ns.sp_x[k] + ns.sp_vx[k]);
                ns.sp_y[k] = (int16_t)(ns.sp_y[k] + ns.sp_vy[k]);
                ns.sp_vy[k] = (int16_t)(ns.sp_vy[k] + 1);
                ns.sp_life[k]--;
                ns_px(ns.sp_x[k] >> 4, ns.sp_y[k] >> 4, ns.sp_life[k] > 6 ? C_F1 : C_F2);
            }
        for (k = 0; k < NS_NSMOKE; k++)
            if (ns.sm_life[k]) {
                ns.sm_x[k] = (int16_t)(ns.sm_x[k] + 2 + ((windv * 8) >> 8));
                ns.sm_y[k] = (int16_t)(ns.sm_y[k] - 4);
                ns.sm_life[k]--;
                if (ns.sm_life[k] % 3u)
                    ns_blend(ns.sm_x[k] >> 4, ns.sm_y[k] >> 4, C_SMOKE, ns.sm_life[k] * 256 / 40);
            }
    }
    /* the person: sitting, hood up, facing the sea; an umbrella in the rain */
    {
        static const int8_t BODY[15][3] = {
            {0, 1, 0}, {1, 1, 0}, {2, 1, 0}, {3, 1, 0}, {0, 2, 1}, {1, 2, 1}, {0, 3, 1}, {1, 3, 1}, {0, 4, 1}, {1, 4, 1},
            {2, 3, 1}, {0, 5, 2}, {1, 5, 3}, {0, 6, 2}, {1, 6, 2},
        };
        const uint16_t BC[4] = {C_PANTS, C_HOOD, C_HAIR, C_SKIN};
        int32_t hx = 25, hy = ns_top(25, sea, shore);
        for (k = 0; k < 15; k++)
            ns_px(hx + BODY[k][0], hy - BODY[k][1], ns_mix(BC[(uint32_t)BODY[k][2]], pal[PC_SKY1], (51 * dark + 90 * nt) >> 8));
        if (rain > 9830) {
            int32_t ux = hx + 1, uy = hy - 10;
            for (x = ux - 4; x <= ux + 4; x++)
                ns_px(x, uy + ((x > ux + 2 || x < ux - 2) ? 1 : 0), (x - ux) & 1 ? C_UMB : C_UMB2);
            for (x = ux - 3; x <= ux + 3; x++)
                ns_px(x, uy - 1, C_UMB);
            for (y = uy + 1; y < hy - 4; y++)
                ns_px(ux, y, C_LOG);
        }
    }
    /* birds: one on the pine singing (a note), a flock passing in calm weather */
    if (bird > 650) {
        int32_t bx = 11, by = ns_top(8, sea, shore) - 10, sing = s->b_state == 1 && s->b_env > 1500;
        ns_px(bx, by, C_BIRD), ns_px(bx + 1, by, C_BIRD), ns_px(bx + (sing ? 2 : 1), by - 1, C_BIRD);
        if (sing) {
            static const int8_t NOTE[5][2] = {{0, 0}, {1, 0}, {1, -1}, {1, -2}, {2, -2}};
            int32_t nx = bx + 3 + (int32_t)((tt / 3u) % 3u), ny = by - 3 - (int32_t)((tt / 2u) % 4u);
            for (k = 0; k < 5; k++)
                ns_px(nx + NOTE[k][0], ny + NOTE[k][1], C_F1);
        }
        if (ns.fl_x < 0 && ((bird * (256 - dark)) >> 8) > 8192 && (ns_rnd() & 127u) == 0) {
            ns.fl_x = -(5 << 4);
            ns.fl_y = (int16_t)ns_ur(5, 15);
        }
    }
    if (ns.fl_x >= -(5 << 4) && ns.fl_x != -1) {
        static const int8_t FL[4][2] = {{0, 0}, {4, 2}, {8, -1}, {12, 3}};
        ns.fl_x = (int16_t)(ns.fl_x + 8);
        for (k = 0; k < 4; k++) {
            int32_t bx = (ns.fl_x >> 4) - FL[k][0], by = ns.fl_y + FL[k][1], up = (int32_t)((tt / 4u + (uint32_t)FL[k][0]) & 1u);
            if (by >= 0 && by < NS_HZ)
                ns_px(bx - 1, by - up, C_BIRD), ns_px(bx, by, C_BIRD), ns_px(bx + 1, by - up, C_BIRD);
        }
        if ((ns.fl_x >> 4) > NS_W + 20)
            ns.fl_x = -1;
    }
    /* rain: streaks slanted by the wind */
    {
        int32_t want = (rain * NS_NDROP) >> 15, slant = 6 + ((windv * 26) >> 8);   /* Q4 px a frame across */
        while (ns.ndrops < want) {
            ns.dr_x[ns.ndrops] = (int16_t)(ns_ur(-20, NS_W) * 16);
            ns.dr_y[ns.ndrops] = (int16_t)(ns_ur(-10, NS_H) * 16);
            ns.ndrops++;
        }
        if (ns.ndrops > want)
            ns.ndrops = (uint8_t)want;
        for (k = 0; k < ns.ndrops; k++) {
            ns.dr_x[k] = (int16_t)(ns.dr_x[k] + slant * 16 / 10);
            ns.dr_y[k] = (int16_t)(ns.dr_y[k] + 42);
            if ((ns.dr_y[k] >> 4) >= NS_H || (ns.dr_x[k] >> 4) >= NS_W + 5) {
                ns.dr_x[k] = (int16_t)(ns_ur(-20, NS_W) * 16);
                ns.dr_y[k] = (int16_t)(ns_ur(-8, 0) * 16);
            }
            for (i = 0; i < 3; i++)
                ns_blend((ns.dr_x[k] - slant * i * 6 / 10) >> 4, (ns.dr_y[k] >> 4) - i, C_RAIN, 192 - i * 51);
        }
    }
    /* wind streaks */
    if (windv > 128)
        for (k = 0; k < ((windv - 128) * 8) >> 8; k++) {
            int32_t yy = (int32_t)((uint32_t)(k * 13) + tt / 2u) % NS_HZ;
            int32_t x0 = (int32_t)((tt * (uint32_t)(3 + k) + (uint32_t)k * 37u) % (NS_W + 20)) - 10;
            for (x = x0; x < x0 + 5; x++)
                ns_blend(x, yy, C_WHITE, 64);
        }
    if (ns.flash)
        ns.flash--;
    /* not playing: the scene dimmed, how to start it */
    if (!s->playing && s->fade <= 0) {
        for (y = 0; y < NS_H; y++)
            for (x = 0; x < NS_W; x++)
                ns_px(x, y, ns_mix(ns_get(x, y), T_SURF, 150));   /* (the app says why) */
    }
}
#endif /* FELUCCA_NATURE */
