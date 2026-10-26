/* SPDX-License-Identifier: GPL-3.0-only */
/* fx.h: a chain of FX_SLOTS effect slots in series on a stereo bus, with a click-free bypass. From TastyWaves
 * (github.com/marksweiss/tastywaves, firmware/src/fx.h), made independent of its patch. Integer only.
 *
 * Types: FXT_CHORUS, FXT_FLANGER (one modulated delay line a slot, two taps, the right LFO a quarter cycle ahead),
 * FXT_PHASER (4 all-pass stages a side), FXT_DELAY (ping-pong, up to 0.55 s; one in the chain), FXT_REVERB (4 combs +
 * 2 all-pass a side, Freeverb's lengths; one in the chain). A second DELAY or REVERB slot passes the bus through.
 *
 * The bus: int32 L R L R ..., Q15 x 32 (+-2^20 = full scale): multiply a Q15 mix by 32 going in, clip (dsp_softclip
 * after >> 5) coming out. The delay lines keep it >> 5 as int16.
 *
 *   main loop:  fx_param[i].type / a / b / mix (0..127 each: what A and B mean depends on the type, below), then
 *               fx_sync() whenever a type may have changed (cheap when nothing did); fx_bypass = 1 / 0.
 *   audio ISR:  fx_process(bus, n) once a block, after the voices are mixed into the bus.
 *
 *   CHORUS   A rate 0.02..1.7 Hz   B depth          FLANGER  A rate   B feedback (up to 0.79)
 *   PHASER   A rate                B feedback        DELAY    A time 10 ms..0.54 s (glides)   B feedback
 *   REVERB   A size (feedback)     B damping         MIX      0 dry .. 127 full wet (phaser: up to half, for its notches)
 *
 * Memory: about 20 KB of .pool for the slots, 96 KB for DELAY's line, 24 KB for REVERB's (pool: 336 KB). CPU: a slot
 * costs roughly a voice of TastyWaves (2 wavetable oscillators + SVF); four slots took the ISR from ~50 % to ~76 %.
 * Safety: fx_sync changes a slot only while its `on` is 0, and the ISR always finishes a block before the main loop
 * runs again, so the ISR never sees a half-cleared slot. */
#pragma once
#include "dsp.h"

#ifndef FX_SLOTS
#define FX_SLOTS 4u
#endif
#ifndef FX_BLOCK
#define FX_BLOCK 256u                           /* the most frames fx_process gets at once (HALF_FRAMES) */
#endif
#define FXT_NONE 0u
#define FXT_CHORUS 1u
#define FXT_FLANGER 2u
#define FXT_PHASER 3u
#define FXT_DELAY 4u
#define FXT_REVERB 5u
#define FX_DELEN 24576u                         /* samples of the delay a channel (0.557 s) */
#define FX_DLEN 2048u                           /* samples of the modulated delay (46 ms) */

static struct { volatile int32_t type, a, b, mix; } fx_param[FX_SLOTS];     /* main loop writes */
static volatile uint32_t fx_bypass;             /* main writes: 1 = the output is the dry bus (the chain keeps running) */
static int32_t fx_byp_g;                        /* ISR: 0 = the chain's output .. 256 = dry, one step a sample */
static volatile uint8_t fx_on[FX_SLOTS];        /* main writes (fx_sync): the ISR runs the slot */

typedef struct {
    volatile uint8_t type;                      /* 0 = the slot passes the bus through */
    uint8_t idx;
    uint32_t ph, w;                             /* LFO phase, write position */
    int32_t mix_s;                              /* smoothed mix, Q8 of 0..128 */
    int32_t fb;                                 /* flanger / phaser: the last wet sample (feedback) */
    int32_t px[2][4], py[2][4];                 /* phaser: the all-pass stages' last input and output */
    int16_t buf[FX_DLEN];
} fx_slot_t;

#ifdef DINGHY_HOST
static fx_slot_t fxs[FX_SLOTS];
static int32_t fx_dry[2u * FX_BLOCK];
#else
static fx_slot_t fxs[FX_SLOTS] __attribute__((section(".pool")));
static int32_t fx_dry[2u * FX_BLOCK] __attribute__((section(".pool")));
#endif

static void fx_modrun(int32_t *lr, uint32_t n, fx_slot_t *s)           /* chorus, flanger */
{
    const int32_t p[4] = {0, fx_param[s->idx].a, fx_param[s->idx].b, fx_param[s->idx].mix};
    uint32_t type = s->type, inc, i, w = s->w, ph = s->ph;
    int32_t a = p[1], b = p[2], mt = p[3] + (p[3] >> 6), ms = s->mix_s, fbk = 0, d0, dq;
    if (type == FXT_CHORUS) {                   /* A rate 0.02 .. 1.7 Hz, B depth +-0..127 samples around 15 ms (up to about half a semitone of wobble) */
        inc = DSP_LFO_INC[(a * 75) >> 7] >> 5;
        d0 = 661; dq = b;
    } else {                                    /* flanger: A rate, B feedback up to 0.79; delay 1.5 ms +- 60 samples */
        inc = DSP_LFO_INC[(a * 90) >> 7] >> 5;
        d0 = 70; dq = 60; fbk = (b * 205) >> 7;
    }
    for (i = 0; i < n; ) {
        uint32_t cn = n - i < 16u ? n - i : 16u, c;
        int32_t dl[2], ds[2], k;
        for (k = 0; k < 2; k++) {               /* the LFO runs once a 16-sample chunk; the delay slides linearly inside it */
            int32_t d_a = d0 * 256 + ((dsp_sine(ph + (k ? 0x40000000u : 0u)) * dq) >> 7);
            int32_t d_b = d0 * 256 + ((dsp_sine(ph + inc * cn + (k ? 0x40000000u : 0u)) * dq) >> 7);
            dl[k] = d_a; ds[k] = (d_b - d_a) / (int32_t)cn;
        }
        ph += inc * cn;
        for (c = 0; c < cn; c++, i++) {
            int32_t inl = lr[2u * i], inr = lr[2u * i + 1u], mono = (inl + inr) >> 1, wet[2], x;
            ms += (mt * 256 - ms + 512) >> 10;       /* (rounded: a rising mix reaches its target) */
            for (k = 0; k < 2; k++) {
                int32_t pos = ((int32_t)w << 8) - dl[k], fr, i0, i1;
                dl[k] += ds[k];
                pos &= (int32_t)(FX_DLEN << 8) - 1;
                i0 = pos >> 8; fr = pos & 255;
                i1 = (i0 + 1) & (int32_t)(FX_DLEN - 1u);
                wet[k] = (((int32_t)s->buf[i0] * (256 - fr) + (int32_t)s->buf[i1] * fr) >> 8) * 32;
            }
            x = (mono + (type == FXT_FLANGER ? (s->fb * fbk) >> 8 : 0)) >> 5;
            s->buf[w] = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
            s->fb = wet[0];
            w = (w + 1u) & (FX_DLEN - 1u);
            {   int32_t m = ms >> 8;
                lr[2u * i] = inl + (((wet[0] - inl) * m) >> 7);
                lr[2u * i + 1u] = inr + (((wet[1] - inr) * m) >> 7);
            }
        }
    }
    s->w = w; s->ph = ph; s->mix_s = ms;
}

static void fx_phaser(int32_t *lr, uint32_t n, fx_slot_t *s)
{
    const int32_t p[4] = {0, fx_param[s->idx].a, fx_param[s->idx].b, fx_param[s->idx].mix};
    uint32_t inc = DSP_LFO_INC[(p[1] * 90) >> 7] >> 5, ph = s->ph, i, cn, c;
    int32_t mt = p[3] + (p[3] >> 6), ms = s->mix_s, fbk = (p[2] * 205) >> 7, co[2] = {0, 0}, k, j;
    for (i = 0; i < n; ) {
        cn = n - i < 16u ? n - i : 16u;
        for (k = 0; k < 2; k++) co[k] = 16384 + ((dsp_sine(ph + (k ? 0x40000000u : 0u)) * 12000) >> 15);   /* the coefficient sweeps 0.13 .. 0.87 (Q15) */
        ph += inc * cn;
        for (c = 0; c < cn; c++, i++) {
            int32_t in[2], o[2], m;
            in[0] = lr[2u * i]; in[1] = lr[2u * i + 1u];
            ms += (mt * 256 - ms + 512) >> 10;
            m = ms >> 8;
            for (k = 0; k < 2; k++) {
                int32_t x = in[k] + (int32_t)(((int64_t)s->fb * fbk) >> 8);
                x = x > (1 << 20) ? (1 << 20) : x < -(1 << 20) ? -(1 << 20) : x;
                for (j = 0; j < 4; j++) {                      /* y = a x + x1 - a y1 */
                    int32_t y = (int32_t)(((int64_t)co[k] * (x - s->py[k][j])) >> 15) + s->px[k][j];
                    s->px[k][j] = x; s->py[k][j] = y;
                    x = y;
                }
                o[k] = x;
                if (!k) s->fb = x;
            }
            lr[2u * i] = in[0] + (((o[0] - in[0]) * m) >> 8);          /* at most half wet: the notches need the dry signal */
            lr[2u * i + 1u] = in[1] + (((o[1] - in[1]) * m) >> 8);
        }
    }
    s->ph = ph; s->mix_s = ms;
}

/* ---- DELAY: ping-pong, the left line is fed by the input and the right line's echo, the right line by the left line's echo */
#ifdef DINGHY_HOST
static int16_t fx_dl[2][FX_DELEN];
#else
static int16_t fx_dl[2][FX_DELEN] __attribute__((section(".pool")));
#endif
static struct { uint32_t w; int32_t ts; } fx_dly;       /* write position, the smoothed time (Q8 samples) */

static inline int32_t fx_tz8(int32_t p) { return p < 0 ? -((-p) >> 8) : p >> 8; }   /* >> 8 toward zero: the echoes die out (no -1 residue) */
static void fx_delay(int32_t *lr, uint32_t n, fx_slot_t *s)
{
    const int32_t p[4] = {0, fx_param[s->idx].a, fx_param[s->idx].b, fx_param[s->idx].mix};
    int32_t tt = (441 + p[1] * 182) * 256, fbq = (p[2] * 220) >> 7, mt = p[3] + (p[3] >> 6), ms = s->mix_s, ts = fx_dly.ts;
    uint32_t w = fx_dly.w, i;
    for (i = 0; i < n; i++) {
        int32_t inl = lr[2u * i], inr = lr[2u * i + 1u], pos, fr, i0, i1, wl, wr, x, y, m;
        ms += (mt * 256 - ms + 512) >> 10;
        ts += (tt - ts + 1024) >> 11;                   /* the time glides (no click, a short pitch bend while it moves) */
        pos = ((int32_t)w << 8) - ts;
        if (pos < 0) pos += (int32_t)(FX_DELEN << 8);        /* (the line is not a power of two long: no mask) */
        i0 = pos >> 8; fr = pos & 255;
        i1 = i0 + 1; if (i1 >= (int32_t)FX_DELEN) i1 = 0;
        wl = ((int32_t)fx_dl[0][i0] * (256 - fr) + (int32_t)fx_dl[0][i1] * fr) >> 8;
        wr = ((int32_t)fx_dl[1][i0] * (256 - fr) + (int32_t)fx_dl[1][i1] * fr) >> 8;
        x = ((inl + inr) >> 1) >> 5;
        y = x + fx_tz8(wr * fbq);
        fx_dl[0][w] = (int16_t)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
        y = fx_tz8(wl * fbq);
        fx_dl[1][w] = (int16_t)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
        w++; if (w >= FX_DELEN) w = 0;
        m = ms >> 8;
        lr[2u * i] = inl + (((wl * 32) * m) >> 7);
        lr[2u * i + 1u] = inr + (((wr * 32) * m) >> 7);
    }
    fx_dly.w = w; fx_dly.ts = ts; s->mix_s = ms;
}

/* ---- REVERB: 4 parallel low-passed comb filters and 2 all-pass filters a side (Freeverb's lengths), the right side a few samples longer */
static const uint16_t FX_RVL[12] = {1116, 1188, 1277, 1356, 556, 441, 1139, 1211, 1300, 1379, 579, 464};   /* L combs, L all-pass, R combs, R all-pass */
#define FX_RVN 12006u
#ifdef DINGHY_HOST
static int16_t fx_rv[FX_RVN];
#else
static int16_t fx_rv[FX_RVN] __attribute__((section(".pool")));
#endif
static struct { uint16_t pos[12]; int32_t lp[8], dc[2]; } fx_rvs;

static inline int32_t fx_mz(int64_t p)         /* p >> 15, rounded toward zero: a loop built on it always decays to silence (rounding to nearest leaves a limit cycle) */
{
    return (int32_t)(p < 0 ? -((-p) >> 15) : p >> 15);
}
static void fx_reverb(int32_t *lr, uint32_t n, fx_slot_t *s)
{
    const int32_t p[4] = {0, fx_param[s->idx].a, fx_param[s->idx].b, fx_param[s->idx].mix};
    int32_t fbq = 22938 + p[1] * 72, dmp = p[2] * 230, mt = p[3] + (p[3] >> 6), ms = s->mix_s;
    uint32_t i, ch, j, off[12];
    off[0] = 0;
    for (j = 1; j < 12u; j++) off[j] = off[j - 1u] + FX_RVL[j - 1u];
    for (i = 0; i < n; i++) {
        int32_t inl = lr[2u * i], inr = lr[2u * i + 1u], x = (inl + inr) / 512, m, o[2];       /* (headroom: the comb loops gain up to 47 at DC) */
        ms += (mt * 256 - ms + 512) >> 10;
        m = ms >> 8;
        for (ch = 0; ch < 2u; ch++) {
            uint32_t b = ch * 6u;
            int32_t acc = 0;
            for (j = 0; j < 4u; j++) {
                uint32_t q = b + j;
                int32_t y = fx_rv[off[q] + fx_rvs.pos[q]], l = fx_rvs.lp[ch * 4u + j], v;
                l = y + fx_mz((int64_t)(l - y) * dmp);          /* damping: a one-pole low-pass in the loop */
                fx_rvs.lp[ch * 4u + j] = l;
                v = x + fx_mz((int64_t)l * fbq);
                fx_rv[off[q] + fx_rvs.pos[q]] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
                if (++fx_rvs.pos[q] >= FX_RVL[q]) fx_rvs.pos[q] = 0;
                acc += y;
            }
            for (j = 4; j < 6u; j++) {                                      /* all-pass, gain 0.5 */
                uint32_t q = b + j;
                int32_t bo = fx_rv[off[q] + fx_rvs.pos[q]], v = acc + (bo < 0 ? -((-bo) >> 1) : bo >> 1);
                fx_rv[off[q] + fx_rvs.pos[q]] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
                if (++fx_rvs.pos[q] >= FX_RVL[q]) fx_rvs.pos[q] = 0;
                acc = bo - acc;
            }
            fx_rvs.dc[ch] += (acc * 65536 - fx_rvs.dc[ch]) >> 11;   /* a DC trap (Q16): the integer rounding in the loops leaves a small offset */
            o[ch] = (acc * 65536 - fx_rvs.dc[ch]) / 65536;
        }
        lr[2u * i] = inl + (((o[0] * 64) * m) >> 7);
        lr[2u * i + 1u] = inr + (((o[1] * 64) * m) >> 7);
    }
    s->mix_s = ms;
}

static void fx_slot_run(int32_t *lr, uint32_t n, fx_slot_t *s)
{
    switch (s->type) {
    case FXT_CHORUS: case FXT_FLANGER: fx_modrun(lr, n, s); break;
    case FXT_PHASER: fx_phaser(lr, n, s); break;
    case FXT_DELAY: fx_delay(lr, n, s); break;
    case FXT_REVERB: fx_reverb(lr, n, s); break;
    default: break;
    }
}

/* audio ISR: the chain in place on bus (n <= FX_BLOCK frames), then the bypass crossfade toward the dry input */
static void fx_process(int32_t *lr, uint32_t n)
{
    uint32_t i;
    int32_t bt = fx_bypass ? 256 : 0;
    if (n > FX_BLOCK) n = FX_BLOCK;
    if (fx_byp_g || bt)
        for (i = 0; i < 2u * n; i++) fx_dry[i] = lr[i];
    for (i = 0; i < FX_SLOTS; i++)
        if (fx_on[i]) fx_slot_run(lr, n, &fxs[i]);
    if (fx_byp_g != bt || bt)                   /* bypassed, or fading in / out of it */
        for (i = 0; i < 2u * n; i++) {
            if (!(i & 1u)) fx_byp_g += fx_byp_g < bt ? 1 : fx_byp_g > bt ? -1 : 0;
            lr[i] += (int32_t)(((int64_t)(fx_dry[i] - lr[i]) * fx_byp_g) >> 8);
        }
}

/* main loop: bring each slot to fx_param[i].type (clear its lines; the mix ramps in from 0) */
static void fx_sync(void)
{
    uint32_t i, j, tw[FX_SLOTS];
    for (i = 0; i < FX_SLOTS; i++) {
        tw[i] = (uint32_t)fx_param[i].type;
        if (tw[i] > FXT_REVERB) tw[i] = FXT_NONE;
        if (tw[i] == FXT_DELAY || tw[i] == FXT_REVERB)     /* one of each in the chain: a later slot with the same type does nothing */
            for (j = 0; j < i; j++) if ((uint32_t)fx_param[j].type == tw[i]) tw[i] = FXT_NONE;
    }
    for (i = 0; i < FX_SLOTS; i++)                         /* first every slot that changes goes off (a buffer changes owner only when nobody runs on it) */
        if (tw[i] != fxs[i].type) { fx_on[i] = 0; __asm__ volatile("" ::: "memory"); fxs[i].type = FXT_NONE; }
    for (i = 0; i < FX_SLOTS; i++) {
        uint32_t want = tw[i], k;
        fx_slot_t *s = &fxs[i];
        if (want == s->type) continue;
        fx_on[i] = 0;
        __asm__ volatile("" ::: "memory");
        s->type = FXT_NONE;
        for (k = 0; k < FX_DLEN; k++) s->buf[k] = 0;
        for (j = 0; j < 8u; j++) s->px[j >> 2][j & 3u] = s->py[j >> 2][j & 3u] = 0;
        if (want == FXT_DELAY) { for (k = 0; k < FX_DELEN; k++) fx_dl[0][k] = fx_dl[1][k] = 0; fx_dly.w = 0; fx_dly.ts = (441 + fx_param[i].a * 182) * 256; }
        if (want == FXT_REVERB) { for (k = 0; k < FX_RVN; k++) fx_rv[k] = 0; for (k = 0; k < 12u; k++) fx_rvs.pos[k] = 0; for (k = 0; k < 8u; k++) fx_rvs.lp[k] = 0; fx_rvs.dc[0] = fx_rvs.dc[1] = 0; }
        s->w = 0; s->ph = 0; s->mix_s = 0; s->fb = 0; s->idx = (uint8_t)i;
        __asm__ volatile("" ::: "memory");
        s->type = (uint8_t)want;
        fx_on[i] = want != FXT_NONE;
    }
}
