/* SPDX-License-Identifier: GPL-3.0-only */
/* splash.h: a power-on animation drawn by code, no image (~3 s, ~1.3 KB of flash): sine waves roll in like the sea, the
 * front one grows and crashes, and out of the spray rises the name (the small font at twice its size, so mixed case
 * works: up to ~14 characters), then the tagline. From TastyWaves (github.com/marksweiss/tastywaves).
 *
 *   splash_play("TastyWaves", "Sounds Delicious!");     main loop, at the start of app_main, after the settings load
 *
 * Integer only (dsp.h's sine), the screen in four strips of 60 rows a frame (a canvas holds 64), platform_poll() called
 * while it waits so USB (the updater) and the watchdog are served. It blocks ~3 s: the OCT- rescue comes before the app,
 * so it never stands in the way of an install. In host mode, gate it (the test scripts start at 0 ms):
 *   #ifdef DINGHY_HOST  if (getenv("DINGHY_SPLASH"))  #endif  splash_play(...);
 * If the screen cannot keep up (a full frame is ~4 strips of 3 ms DMA), the animation plays slower, never skips. */
#pragma once
#include "dsp.h"

#define SPL_N 70u                               /* frames, SPL_MS apart */
#define SPL_MS 36u
#define SPL_CRASH 28u                           /* the frame the front wave breaks */
#define SPL_DROPS 96u
static struct { int16_t x, y, vx, vy; } spl_d[SPL_DROPS];     /* spray, 1/16 px */
static uint8_t spl_mask[16][120];               /* the name in the small font, drawn twice its size */
static int16_t spl_yb[240], spl_yf[240];        /* this frame's far and front water lines (screen rows) */
static int32_t spl_wave(uint32_t ph, int32_t x, int32_t a, uint32_t wl)  /* a wave's height at column x: a sine with a sharper crest
                                                                             that leans forward as it grows (a: px, wl: phase per px) */
{
    uint32_t th = ph - (uint32_t)x * wl;
    th -= (uint32_t)(dsp_sine(th - 0x40000000u) * a * 300);
    return (a * (dsp_sine(th) + dsp_sine(2u * th - 0x40000000u) / 3)) >> 15;
}
#define SPL_WL1 0x01000000u                     /* the far swell: one cycle in 256 px */
#define SPL_WL2 0x01C00000u                     /* the front waves: shorter */
static void splash_play(const char *nm, const char *sub)
{
    static const uint16_t FADE[6] = {RGB(30, 24, 48), RGB(44, 32, 76), RGB(80, 60, 130), RGB(120, 90, 180), RGB(150, 115, 220), RGB(180, 140, 255)};
    static const uint16_t SUB[5] = {RGB(58, 58, 72), RGB(90, 90, 104), RGB(130, 130, 142), RGB(180, 180, 190), RGB(225, 225, 230)};
    uint32_t k, top, i, j, t0, rnd = 0x2545F491u, tw = (uint32_t)text_w(&FONT_S, nm);
    int32_t a2 = 6, ty = 160, x;
    cv_begin(120, 16, C_BLACK);                 /* the title's pixels, once */
    cv_text(0, 0, &FONT_S, nm, C_WHITE);
    for (j = 0; j < 16u; j++) for (i = 0; i < 120u; i++) spl_mask[j][i] = cv_px[j * 120u + i] != 0u;
    t0 = fm1_ms;
    for (k = 0; k < SPL_N; k++) {
        uint32_t ph1 = k * 0x06000000u, ph2 = k * 0x0A000000u + 0x50000000u;
        if (k < SPL_CRASH) a2 = 6 + (int32_t)k * 34 / (int32_t)SPL_CRASH;          /* the front wave builds */
        else if (k < SPL_CRASH + 8u) a2 = 40 - (int32_t)(k - SPL_CRASH) * 4;     /* and collapses */
        else a2 = 8;
        if (k >= SPL_CRASH && k < SPL_CRASH + 4u)           /* the crash: spray thrown off the crests, a quarter a frame */
            for (i = (k - SPL_CRASH) * (SPL_DROPS / 4u); i < (k - SPL_CRASH + 1u) * (SPL_DROPS / 4u); i++) {
                int32_t h;
                do { rnd = rnd * 1664525u + 1013904223u; x = (int32_t)((rnd >> 8) % 240u); h = spl_wave(ph2, x, a2, SPL_WL2); }
                while (h < a2 / 2);
                spl_d[i].x = (int16_t)(x * 16);
                spl_d[i].y = (int16_t)((176 - h) * 16);
                spl_d[i].vx = (int16_t)((int32_t)((rnd >> 20) & 63u) - 24);
                spl_d[i].vy = (int16_t)(-64 - (int32_t)((rnd >> 3) & 95u));
            }
        if (k > SPL_CRASH)
            for (i = 0; i < SPL_DROPS && i < (k - SPL_CRASH) * (SPL_DROPS / 4u); i++) { spl_d[i].x += spl_d[i].vx; spl_d[i].y += spl_d[i].vy; spl_d[i].vy += 8; }
        for (x = 0; x < 240; x++) {             /* the water lines, once a frame (the strips share them) */
            spl_yb[x] = (int16_t)(148 - spl_wave(ph1, x, 9, SPL_WL1));
            spl_yf[x] = (int16_t)(176 - spl_wave(ph2, x, a2, SPL_WL2));
        }
        if (k >= SPL_CRASH + 4u && ty > 64) { ty -= (ty - 60) / 4; if (ty < 64) ty = 64; }   /* the name rises out of it, easing in */
        for (top = 0; top < 240u; top += 60u) {
            cv_begin(240, 60, C_BLACK);
            cv_oy = -(int32_t)top;
            for (x = 0; x < 240; x++) cv_rect(x, spl_yb[x], 1, 240 - spl_yb[x], RGB(12, 34, 78));     /* the far swell */
            if (k >= SPL_CRASH + 4u) {          /* the name, behind the front water while it rises */
                uint16_t c = FADE[k - SPL_CRASH - 4u < 6u ? k - SPL_CRASH - 4u : 5u];
                int32_t x0 = 120 - (int32_t)tw;
                for (j = 0; j < 16u; j++) for (i = 0; i < tw && i < 120u; i++)
                    if (spl_mask[j][i]) cv_rect(x0 + 2 * (int32_t)i, ty + 2 * (int32_t)j, 2, 2, c);
            }
            for (x = 0; x < 240; x++) {         /* the front wave, foam on its crest */
                cv_rect(x, spl_yf[x], 1, 240 - spl_yf[x], RGB(22, 92, 165));
                cv_rect(x, spl_yf[x], 1, 2, RGB(200, 236, 255));
            }
            if (k > SPL_CRASH)
                for (i = 0; i < SPL_DROPS && i < (k - SPL_CRASH) * (SPL_DROPS / 4u); i++)
                    if (spl_d[i].y < 180 * 16) cv_rect(spl_d[i].x / 16, spl_d[i].y / 16, 2 + (int32_t)(i & 1u), 2 + (int32_t)(i & 1u), i & 2u ? RGB(230, 246, 255) : RGB(150, 205, 250));
            if (k >= SPL_CRASH + 14u) {
                uint32_t s = k - SPL_CRASH - 14u;
                cv_text((240 - text_w(&FONT_S, sub)) / 2, ty + 40, &FONT_S, sub, SUB[s < 5u ? s : 4u]);
            }
            cv_oy = 0;
            cv_blit(0, top);
        }
        while (fm1_ms - t0 < (k + 1u) * SPL_MS) platform_poll();
    }
    while (fm1_ms - t0 < 3000u) platform_poll();                    /* the last frame held a moment */
    lcd_fill(0, 0, 240, 240, C_BLACK);
}

