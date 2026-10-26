/* SPDX-License-Identifier: GPL-3.0-only */
/* uikit.h: the pieces of a synth's screen on the FM-1's 240 x 240 display, from TastyWaves (github.com/marksweiss/
 * tastywaves, firmware/src/ui_screens.h + panel.h), made independent of its pages. Main loop only (never the ISR).
 *
 * Wants the Spleen fonts with FONT_M: #define FONTS_SPLEEN before #include "platform.c" (else FONT_S stands in).
 *
 * The layout TastyWaves settled on (and its users found intuitive):
 *   y   0..35   four knob columns, 60 px each: label, value, bar: what KNOB1..4 set on this page (ui_knob)
 *   y  38..65   the page's tab strip, the knob-set tag under the tab shown (ui_tabs); HOME has none
 *   y  66..199  the page's picture: the biggest thing on the screen, never a copy of the knob values (ui_pic)
 *   y 202       the status line: page n/N left, voices and CPU right (ui_status)
 *   y 218       the patch name, with a dot when edited and a flash for SAVED (ui_title)
 * Draw only what changed: keep a copy of what each piece showed and redraw on a difference (a full screen is 115 KB of
 * SPI, ~40 ms; a knob column 4 KB). A canvas holds 240 x 64 at most (CV_MAX): ui_pic draws a taller picture in strips.
 *
 *   ui_cat / ui_num / ui_copy / ui_eq / ui_fmt_time     text without printf
 *   ui_knob(i, label, value, frac, flags)               a knob column (UI_K_TOUCHED, UI_K_BIPOLAR, UI_K_SMALL)
 *   ui_tabs(y, names, n, cur, set, sets)                the tab strip
 *   ui_status(y, left, voices, vmax, cpu, latch)        the status line
 *   ui_title(y, text, flash, modified)                  the patch name (flash: a message in the accent colour instead)
 *   ui_help(lines, n)                                   a help page: "LABEL  TEXT" lines, the label in the accent colour
 *   ui_hold_hint(x, y, "HOLD FX: ", "BYPASS", colour)   the gesture grey, the state in its colour
 *   ui_pic(y, h, fn, ctx)                               a picture taller than a canvas, in 64-row strips
 *   ui_wave(y, h, s256, colour)                         one cycle of 256 samples (Q15), as on EDIT
 *   ui_gesture_t + ui_gesture(...)                      a button's short press vs a hold (ARP / SEQ / FX in TastyWaves)
 *   ui_msg_t + ui_msg / ui_msg_on                       a message that shows for a while (SAVED, STORE FULL)
 * The help convention: hold HOME for help; two pages: the global buttons, then the page's own lines. */
#pragma once
#include <stdint.h>

#ifdef FELUCCA_FONT_M
#define UI_FONT_M FONT_M
#else
#define UI_FONT_M FONT_S
#endif
#define UI_ACC  RGB(180, 140, 255)              /* lilac: what is being touched, the page, the state that is on */
#define UI_ACC2 RGB(120, 90, 180)
#define UI_ACC3 RGB(44, 32, 76)
#define UI_TXT  RGB(225, 225, 230)
#define UI_GRAY RGB(130, 130, 142)
#define UI_DIM  RGB(58, 58, 72)
#define UI_WARN RGB(255, 200, 90)               /* amber: recording, a bypass, a root note */
#define UI_BAD  RGB(255, 90, 90)

#define UI_KNOB_H  36u
#define UI_Y_TABS  38u
#define UI_TABS_H  28u
#define UI_Y_BODY  66u
#define UI_BODY_H  134u
#define UI_Y_LINEA 202u
#define UI_Y_PATCH 218u

/* ---- text */
static char *ui_cat(char *d, const char *s) { while (*s) *d++ = *s++; *d = 0; return d; }
static char *ui_num(char *d, int32_t v)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (v < 0) *d++ = '-';
    while (n) *d++ = t[--n];
    *d = 0;
    return d;
}
static int ui_eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void ui_copy(char *d, const char *s, uint32_t n)
{
    uint32_t i = 0;
    while (i + 1u < n && s[i]) { d[i] = s[i]; i++; }
    d[i] = 0;
}
/* a 0..127 time knob on dsp.h's curve (0.5 ms * 20000^(v/127)): "4.5MS" "70MS" "1.2S" "10S" */
static void ui_fmt_time(char *d, uint32_t v)
{
    static const uint16_t T16[16] = {256, 267, 279, 291, 304, 317, 331, 345, 362, 378, 395, 412, 431, 449, 470, 490};
    uint32_t n = (v > 127u ? 127u : v) * 9u / 5u;
    uint32_t ms10 = (((uint32_t)T16[n & 15u] << (n >> 4)) * 5u) >> 8;
    if (ms10 < 100u) { d = ui_num(d, (int32_t)(ms10 / 10u)); *d++ = '.'; *d++ = (char)('0' + ms10 % 10u); ui_cat(d, "MS"); }
    else if (ms10 < 10000u) { d = ui_num(d, (int32_t)((ms10 + 5u) / 10u)); ui_cat(d, "MS"); }
    else if (ms10 < 100000u) { d = ui_num(d, (int32_t)(ms10 / 10000u)); *d++ = '.'; *d++ = (char)('0' + ms10 / 1000u % 10u); *d++ = 'S'; *d = 0; }
    else { d = ui_num(d, (int32_t)((ms10 + 5000u) / 10000u)); ui_cat(d, "S"); }
}

/* ---- a knob column: frac 0..256 of the bar; flags: touched (accent), bipolar (from the middle), small value text */
enum { UI_K_TOUCHED = 1, UI_K_BIPOLAR = 2, UI_K_SMALL = 4 };
static void ui_knob(uint32_t i, const char *label, const char *val, uint32_t frac, uint32_t flags)
{
    int32_t n = 0, w, fill;
    uint16_t col = flags & UI_K_TOUCHED ? UI_ACC : UI_TXT, bar = flags & UI_K_TOUCHED ? UI_ACC : UI_ACC2;
    while (val[n]) n++;
    cv_begin(60, UI_KNOB_H, C_BLACK);
    cv_text((60 - text_w(&FONT_S, label)) / 2, -1, &FONT_S, label, UI_GRAY);
    if (n <= 4 && !(flags & UI_K_SMALL)) cv_text((60 - text_w(&UI_FONT_M, val)) / 2, 9, &UI_FONT_M, val, col);
    else cv_text((60 - text_w(&FONT_S, val)) / 2, 12, &FONT_S, val, col);
    cv_rect(4, 31, 52, 3, UI_DIM);
    w = (int32_t)(frac > 256u ? 256u : frac) * 52 >> 8;
    if (flags & UI_K_BIPOLAR) {
        fill = w - 26;
        if (fill >= 0) cv_rect(30, 31, fill + 1, 3, bar);
        else cv_rect(30 + fill, 31, -fill, 3, bar);
        cv_rect(30, 29, 1, 7, UI_GRAY);
    } else
        cv_rect(4, 31, w, 3, bar);
    cv_blit(i * 60u, 0);
}

/* ---- the tab strip: n tabs, cur filled; sets > 1: a "SET k/n" tag under it (the page button steps through the sets) */
static void ui_tabs(uint32_t y, const char *const *names, uint32_t n, uint32_t cur, uint32_t set, uint32_t sets)
{
    uint32_t i;
    int32_t w;
    if (!n) n = 1;
    w = (232 - 4 * ((int32_t)n - 1)) / (int32_t)n;
    cv_begin(240, UI_TABS_H, C_BLACK);
    for (i = 0; i < n; i++) {
        int32_t x = 4 + (int32_t)i * (w + 4);
        const char *t = names[i];
        if (i == cur) {
            cv_rect(x, 0, w, 15, UI_ACC);
            cv_text(x + (w - text_w(&FONT_S, t)) / 2, 0, &FONT_S, t, C_BLACK);
            if (sets > 1u) {
                char g[12], *d = g;
                if (w >= 64) d = ui_cat(d, "SET ");
                d = ui_num(d, (int32_t)set + 1);
                d = ui_cat(d, "/");
                ui_num(d, (int32_t)sets);
                cv_rect(x, 15, w, 13, UI_ACC3);
                cv_rect(x, 15, 1, 13, UI_ACC);
                cv_rect(x + w - 1, 15, 1, 13, UI_ACC);
                cv_rect(x, 27, w, 1, UI_ACC);
                cv_text(x + (w - text_w(&FONT_S, g)) / 2, 14, &FONT_S, g, C_WHITE);
            }
        } else {
            cv_rect(x, 0, w, 15, UI_DIM);
            cv_rect(x + 1, 1, w - 2, 13, C_BLACK);
            cv_text(x + (w - text_w(&FONT_S, t)) / 2, 0, &FONT_S, t, UI_GRAY);
        }
    }
    cv_blit(0, y);
}

/* ---- the status line: left (the page, "SEQ 1/3"), right "V 3/12 CPU 45%" in fixed widths (L instead of V: latch) */
static void ui_status(uint32_t y, const char *left, uint32_t voices, uint32_t vmax, uint32_t cpu, int latch)
{
    char r[24], *d = r;
    d = ui_cat(d, latch ? "L" : "V");
    if (voices < 10u) d = ui_cat(d, " ");
    d = ui_num(d, (int32_t)voices);
    d = ui_cat(d, "/");
    d = ui_num(d, (int32_t)vmax);
    d = ui_cat(d, " CPU ");
    if (cpu < 10u) d = ui_cat(d, " ");
    d = ui_num(d, (int32_t)cpu);
    ui_cat(d, "%");
    cv_begin(240, 14, C_BLACK);
    cv_text(4, 0, &FONT_S, left, UI_ACC);
    cv_text(236 - text_w(&FONT_S, r), 0, &FONT_S, r, cpu > 80u ? UI_BAD : UI_GRAY);
    if (latch) cv_text(236 - text_w(&FONT_S, r), 0, &FONT_S, "L", UI_ACC);
    cv_blit(0, y);
}

/* ---- the patch name; flash: msg instead, in the accent colour; modified: a dot (edited since loaded / saved) */
static void ui_title(uint32_t y, const char *text, const char *flash, int modified)
{
    const char *m = flash ? flash : text;
    cv_begin(240, 22, C_BLACK);
    cv_text((240 - text_w(&UI_FONT_M, m)) / 2, -2, &UI_FONT_M, m, flash ? UI_ACC : C_WHITE);
    if (!flash && modified) cv_rect(0, 6, 5, 6, UI_ACC);
    cv_blit(0, y);
}

/* ---- a help page, 14 lines of 16 px: "LABEL  TEXT" (two spaces split them; the label in the accent colour), a line
 * without two spaces in grey (the first: white, the title), a line starting with a space: continued text */
static void ui_help(const char *const *lines, uint32_t nl)
{
    uint32_t i, n, m;
    char lab[32];
    for (i = 0; i < 14u; i++) {
        const char *t = i < nl && lines[i] ? lines[i] : "";
        for (n = 0; t[n] && !(t[n] == ' ' && t[n + 1u] == ' '); n++) {}
        cv_begin(240, 16, C_BLACK);
        if (t[0] == ' ') cv_text(4, 0, &FONT_S, t + 1, UI_TXT);
        else if (!t[n]) cv_text(4, 0, &FONT_S, t, i ? UI_GRAY : UI_TXT);
        else {
            for (m = 0; m < n && m < 31u; m++) lab[m] = t[m];
            lab[m] = 0;
            cv_text(4, 0, &FONT_S, lab, UI_ACC);
            cv_text(4 + text_w(&FONT_S, lab), 0, &FONT_S, t + n, UI_TXT);
        }
        cv_blit(0, 2 + i * 16u);
    }
}

/* ---- "HOLD SEQ: ON": what a hold does, in grey, then the state in its colour (on: UI_ACC, off: UI_DIM) */
static void ui_hold_hint(int32_t x, int32_t y, const char *lab, const char *st, uint16_t c)
{
    cv_text(x, y, &FONT_S, lab, UI_GRAY);
    cv_text(x + text_w(&FONT_S, lab), y, &FONT_S, st, c);
}

/* ---- a picture taller than a canvas: fn(ctx) draws the whole picture in its own coordinates (0..h); it is called once
 * per 64-row strip with cv_oy set, and the drawing clips to the strip. Cheap pictures only: each strip draws it all. */
typedef void (*ui_pic_fn)(const void *ctx);
static void ui_pic(uint32_t y, uint32_t h, ui_pic_fn fn, const void *ctx)
{
    uint32_t top;
    for (top = 0; top < h; top += 64u) {
        cv_begin(240, h - top < 64u ? h - top : 64u, C_BLACK);
        cv_oy = -(int32_t)top;
        fn(ctx);
        cv_oy = 0;
        cv_blit(0, y + top);
        platform_poll();                        /* USB and the watchdog between the strips */
    }
}

/* ---- one cycle of a wave (256 samples, Q15) across the width, in a canvas of h rows (h <= 64), at screen row y */
static void ui_wave(uint32_t y, uint32_t h, const int16_t *s, uint16_t col)
{
    int32_t i, mid = (int32_t)h / 2, amp = mid - 3, px = 0, py = mid;
    cv_begin(240, h, C_BLACK);
    cv_rect(0, mid, 240, 1, UI_DIM);
    for (i = 0; i < 256; i++) {
        int32_t x = i * 239 >> 8, yy = mid - ((int32_t)s[i] * amp >> 15);
        if (i) cv_line(px, py, x, yy, col);
        px = x; py = yy;
    }
    cv_blit(0, y);
}

/* ---- a button's gesture: a short press (released before hold_ms) or a hold (reported once, at hold_ms, while still
 * down; the release then does nothing). Call each main-loop pass with the press edges, the buttons held, now (fm1_ms).
 * TastyWaves: ARP and SEQ press = their page, hold = on / off; FX press = page, hold = bypass. A 200 ms guard after a
 * hold ignores a bounce. */
enum { UI_G_NONE, UI_G_TAP, UI_G_HOLD };
typedef struct { uint8_t armed; uint32_t t, t_hold; } ui_gesture_t;
static uint32_t ui_gesture(ui_gesture_t *g, uint32_t edges, uint32_t held, uint32_t btn, uint32_t now, uint32_t hold_ms)
{
    if (edges >> btn & 1u && !g->armed && (!g->t_hold || now - g->t_hold >= 200u)) { g->armed = 1; g->t = now; }
    if (!g->armed) return UI_G_NONE;
    if (!(held >> btn & 1u)) { g->armed = 0; return UI_G_TAP; }
    if (now - g->t >= hold_ms) { g->armed = 0; g->t_hold = now ? now : 1u; return UI_G_HOLD; }
    return UI_G_NONE;
}

/* ---- a message for a while (SAVED, STORE FULL, FX BYPASS): ui_msg sets it, ui_msg_on gives it while it shows */
typedef struct { char text[16]; uint32_t until; } ui_msg_t;
static void ui_msg(ui_msg_t *m, const char *text, uint32_t now, uint32_t ms) { ui_copy(m->text, text, sizeof m->text); m->until = now + ms; }
static const char *ui_msg_on(const ui_msg_t *m, uint32_t now) { return m->text[0] && (int32_t)(m->until - now) > 0 ? m->text : 0; }
