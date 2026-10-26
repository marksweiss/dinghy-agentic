/* SPDX-License-Identifier: GPL-3.0-only */
/* kitchensink: every firmware/lib module in one small synth, the modules' integration test and the example a new POC
 * starts from (copy the directory, rename, cut what you do not need). Not a product.
 *
 *   keys       play (8 voices: two detuned saws, a state-variable low-pass, an ADSR); notes go out over USB-MIDI
 *   HOME       CUTOFF RESO ATTACK RELEASE                         hold HOME: help (HOME again: back)
 *   FX         TYPE A B MIX of slot 1 (off chorus flanger phaser delay reverb)      hold FX: bypass the chain
 *   ARP        MODE RATE OCT GATE                                  hold ARP: arp on / off
 *   SAVE       saves the sound as patch 1 (the patch log; a power cut cannot corrupt it)
 *   PRESETS    loads patch 1..8 (those that exist)
 *   OCT- OCT+  the octave
 *   USB        the link (link.h): HELLO and the patch commands, so a browser page can back the patches up
 *   MIDI       notes in; MIDI clock drives the arp (start restarts it)
 *
 * Modules: dsp.h (voices), fx.h, arpseq.h, patchlog.h, link.h, uikit.h, splash.h. Host: APP=kitchensink tests/run_host.sh;
 * DINGHY_SPLASH=1 shows the splash; DINGHY_FLASH=file keeps the patches across runs. */
#include <stdint.h>
#define FONTS_SPLEEN                            /* uikit.h's FONT_M */
static void link_service(void);
#define APP_SERVICE link_service                /* the USB link beside the updater (platform_poll) */
static void ks_param(int id, int32_t v);
#define HOST_PARAM(id, v) ks_param(id, v)       /* host scripts: "T param ID V" */
#ifndef APP_NAME
#define APP_NAME "KITCHENSINK"
#endif
#include "platform.c"
#include "dsp.h"
#include "fx.h"

/* ---- the parameters: the main loop writes, the ISR reads once a block */
enum { P_CUT, P_RES, P_ATK, P_REL, P_FXT, P_FXA, P_FXB, P_FXM, P_ARP, P_RATE, P_OCT, P_GATE, P_COUNT };
static const struct { const char *name; int16_t lo, hi, def; } PD[P_COUNT] = {
    {"CUTOFF", 0, 127, 90}, {"RESO", 0, 127, 30}, {"ATTACK", 0, 127, 20}, {"RELEASE", 0, 127, 60},
    {"TYPE", 0, 5, 0}, {"A", 0, 127, 64}, {"B", 0, 127, 64}, {"MIX", 0, 127, 50},
    {"MODE", 1, 7, 1}, {"RATE", 0, 5, 3}, {"OCT", 1, 4, 1}, {"GATE", 1, 127, 64}};
static volatile int32_t par[P_COUNT];
static volatile uint32_t arp_enabled;           /* main writes: the ARP hold */

/* ---- voices (ISR) */
#define NV 8u
typedef struct { uint32_t ph1, ph2, age; dsp_env_t env; dsp_svf_t f; uint8_t note, gate, vel; } voice_t;
static voice_t vx[NV];
static uint32_t ages;
static dsp_ring_t ev;                           /* main -> ISR: on << 16 | note << 8 | velocity */

static void note_on(uint32_t note, uint32_t vel)
{
    dsp_vinfo_t vi[NV];
    uint32_t v;
    for (v = 0; v < NV; v++) { vi[v].note = vx[v].note; vi[v].gate = vx[v].gate; vi[v].level = vx[v].env.lvl >> 8; vi[v].age = vx[v].age; }
    v = dsp_voice_pick(vi, NV, note);
    if (vx[v].env.st == DSP_ENV_IDLE) { vx[v].f.ic1 = vx[v].f.ic2 = 0; }   /* from silence: a clean filter */
    vx[v].note = (uint8_t)note; vx[v].vel = (uint8_t)vel; vx[v].gate = 1; vx[v].age = ++ages;
    dsp_env_gate(&vx[v].env, 1);
}
static void note_off(uint32_t note)
{
    uint32_t v;
    for (v = 0; v < NV; v++)
        if (vx[v].note == note && vx[v].gate) { vx[v].gate = 0; dsp_env_gate(&vx[v].env, 0); }
}
#define ARP_NOTE_ON(n, v) note_on(n, v)
#define ARP_NOTE_OFF(n) note_off(n)
#include "arpseq.h"

static int32_t mixbus[2u * HALF_FRAMES];        /* the stereo bus, Q15 x 32 (fx.h) */
static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t v, i, e, s;
    DSP_CPU_BEGIN(t0);
    arp_cfg.mode = arp_enabled ? par[P_ARP] : ARP_OFF; arp_cfg.rate = par[P_RATE]; arp_cfg.oct = par[P_OCT]; arp_cfg.gate = par[P_GATE];
    while (dsp_ring_get(&ev, &e)) {             /* the keys: through the arp when it is on */
        uint32_t note = e >> 8 & 127u, vel = e & 127u;
        if (e >> 16) { if (arp_on()) arp_in_on(note, vel); else note_on(note, vel); }
        else { arp_in_off(note); if (!arp_sounding(note)) note_off(note); }
    }
    arp_run(n);
    for (i = 0; i < 2u * n; i++) mixbus[i] = 0;
    for (v = 0; v < NV; v++) {
        voice_t *p = &vx[v];
        uint32_t inc = dsp_note_inc(p->note), inc2 = inc + (inc >> 9);      /* the second saw ~3.4 cents sharp */
        if (p->env.st == DSP_ENV_IDLE) continue;
        for (s = 0; s < n; s += DSP_CR) {       /* control rate: the envelope and the filter once per 32 frames */
            dsp_svf_c c;
            int32_t lvl = dsp_env_step(&p->env, (uint32_t)par[P_ATK], 64, DSP_Q30 / 2, (uint32_t)par[P_REL], 0);
            int32_t amp = (int32_t)(((int64_t)lvl * p->vel) >> 22);         /* Q30 x 127 -> about Q15 / 2 */
            dsp_svf_set(&c, (par[P_CUT] + 8) * 256, (uint32_t)par[P_RES], DSP_LP);
            for (i = s; i < s + DSP_CR && i < n; i++) {
                int32_t x = ((int32_t)(p->ph1 >> 16) - 32768 + (int32_t)(p->ph2 >> 16) - 32768) / 2, y;
                p->ph1 += inc; p->ph2 += inc2;
                y = (dsp_svf_run(&p->f, &c, x) * amp) >> 15;
                mixbus[2u * i] += y * 8; mixbus[2u * i + 1u] += y * 8;      /* x 32 (the bus) / 4 (eight voices' headroom) */
            }
        }
    }
    fx_process(mixbus, n);
    for (i = 0; i < 2u * n; i++) out[i] += dsp_softclip(mixbus[i] >> 5);
    DSP_CPU_END(t0, dsp_cpu_now, dsp_cpu_max);
}
static int quiet(void)
{
    uint32_t v;
    for (v = 0; v < NV; v++) if (vx[v].env.st != DSP_ENV_IDLE) return 0;
    return 1;
}

/* ---- patches: the parameters in the patch log; the link serves them to a computer */
#include "patchlog.h"
#define LINK_PATCHES 1
#define LINK_ID1 'D'                            /* the link's two letters: app.json "link_id" (the web tool talks to these) */
#define LINK_ID2 'A'
#define LINK_QUIET() (quiet() && fm1_in.notes == 0u)
#include "link.h"
#include "uikit.h"
#include "splash.h"

static void ks_param(int id, int32_t v)
{
    if (id < 0 || id >= (int)P_COUNT) return;
    par[id] = v < PD[id].lo ? PD[id].lo : v > PD[id].hi ? PD[id].hi : v;
}
static void fx_apply(void)
{
    fx_param[0].type = par[P_FXT]; fx_param[0].a = par[P_FXA]; fx_param[0].b = par[P_FXB]; fx_param[0].mix = par[P_FXM];
    fx_sync();
}
static int patch_save(uint32_t id)
{
    static pl_patch_t p;
    uint32_t i;
    ui_copy(p.name, "SINK 1", sizeof p.name);
    p.name[5] = (char)('1' + id);
    p.n = P_COUNT;
    for (i = 0; i < P_COUNT; i++) p.v[i] = (int16_t)par[i];
    p.pw = 0;
    return pl_put(id, &p);
}
static int patch_load(uint32_t id, char *name)
{
    static pl_patch_t p;
    uint32_t i;
    if (pl_get(id, &p) != PL_OK) return -1;
    for (i = 0; i < P_COUNT; i++) ks_param((int)i, i < p.n ? p.v[i] : PD[i].def);   /* clamped: a stored value is never trusted */
    ui_copy(name, p.name, 16);
#ifdef DINGHY_HOST
    printf("%6u ms  KS patch %u loaded: cutoff %d\n", fm1_ms, id, (int)par[P_CUT]);
#endif
    return 0;
}

/* ---- the screen */
enum { PG_HOME, PG_FX, PG_ARP, PG_HELP };
static const char *const PG_NAME[3] = {"HOME", "FX", "ARP"};
static const uint8_t PG_PAR[3] = {P_CUT, P_FXT, P_ARP};
static const char *const FXN[6] = {"OFF", "CHOR", "FLNG", "PHAS", "DLY", "REV"};
static const char *const ARPN[8] = {"OFF", "UP", "DOWN", "UPDN", "UPD2", "RND", "PLAY", "CHRD"};
static const char *const RATEN[6] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char *const HELP[] = {
    "KITCHENSINK  HELP", "HOME  CUTOFF RESO ATK REL", "FX  SLOT 1, HOLD: BYPASS", "ARP  ITS PAGE, HOLD: ON/OFF",
    "SAVE  STORE AS PATCH 1", "PRESETS  LOAD PATCH 1..8", "OCT- OCT+  OCTAVE", "HOLD HOME  THIS HELP", "",
    "every firmware/lib module", " in one small synth"};

static void val_text(char *d, uint32_t p)
{
    int32_t v = par[p];
    if (p == P_ATK || p == P_REL) ui_fmt_time(d, (uint32_t)v);
    else if (p == P_FXT) ui_copy(d, FXN[v], 8);
    else if (p == P_ARP) ui_copy(d, ARPN[v], 8);
    else if (p == P_RATE) ui_copy(d, RATEN[v], 8);
    else ui_num(d, v);
}
typedef struct { uint32_t page, voices, fx_on, arp_on, bpm, ext; } pic_t;
static void pic_fn(const void *ctx)             /* the page's picture: what sounds, and the state of the holds */
{
    const pic_t *c = ctx;
    uint32_t v;
    for (v = 0; v < NV; v++) cv_rect(20 + (int32_t)v * 26, 20, 20, 50, c->voices >> v & 1u ? UI_ACC : UI_DIM);
    cv_text(20, 80, &FONT_S, "VOICES", UI_GRAY);
    if (c->page == PG_FX) ui_hold_hint(60, 112, "HOLD FX: ", c->fx_on ? "BYPASS" : "ON", c->fx_on ? UI_WARN : UI_ACC);
    if (c->page == PG_ARP) {                    /* the tempo the arp runs at: its own, or the incoming MIDI clock's */
        char t[24], *d = ui_cat(t, "TEMPO ");
        const char *src = c->ext ? "  MIDI CLOCK" : "  INTERNAL";
        int32_t w;
        d = ui_num(d, (int32_t)c->bpm);
        ui_cat(d, " BPM");
        w = text_w(&FONT_S, t) + text_w(&FONT_S, src);
        cv_text((240 - w) / 2 + text_w(&FONT_S, t), 98, &FONT_S, src, c->ext ? UI_ACC : UI_GRAY);
        cv_text((240 - w) / 2, 98, &FONT_S, t, UI_TXT);
        ui_hold_hint(60, 118, "HOLD ARP: ", c->arp_on ? "ON" : "OFF", c->arp_on ? UI_ACC : UI_DIM);
    }
}

static void app_main(void)
{
    uint32_t prev = 0, k, page = PG_HOME, last_page = ~0u, drawn_k[4] = {~0u, ~0u, ~0u, ~0u}, drawn_pic = ~0u, t_status = 0;
    int32_t oct = 0, e;
    uint8_t held[27] = {0};
    uint32_t touched = 4, clk_seen = 0, clk_n = 0, clk_t0 = 0, clk_bpm = 0;
    char name[16] = "INIT", drawn_title[24] = "";
    ui_gesture_t g_fx = {0}, g_arp = {0}, g_home = {0};
    ui_msg_t msg = {{0}, 0};
    for (k = 0; k < P_COUNT; k++) par[k] = PD[k].def;
    pl_init();
    patch_load(0, name);
    fx_apply();
#ifdef DINGHY_HOST
    if (getenv("DINGHY_SPLASH"))
#endif
        splash_play("KitchenSink", "every module, one synth");
    lcd_fill(0, 0, 240, 240, C_BLACK);

    for (;;) {
        uint32_t keys = fm1_in.notes, down = keys & ~prev, up = prev & ~keys, b, rel, base, voices = 0, pk, g;
        uint8_t st, d1, d2;
        platform_poll();
        prev = keys;
        base = (uint32_t)(53 + 12 * oct);       /* key 0 = F3 */
        for (k = 0; k < 27u; k++) {
            if (down >> k & 1u) { held[k] = (uint8_t)(base + k); dsp_ring_put(&ev, 1u << 16 | (uint32_t)held[k] << 8 | 100u); midi_send(0x90, held[k], 100); }
            if (up >> k & 1u) { dsp_ring_put(&ev, (uint32_t)held[k] << 8); midi_send(0x80, held[k], 0); }
        }
        while (midi_get(&st, &d1, &d2)) {
            if (st == 0xF8u) {                  /* the clock's tempo for the screen: ms over 4 beats (96 ticks), +-0.1 BPM */
                arp_ext.ticks++;
                if (clk_n % 96u == 0u) { if (clk_n) { uint32_t ms = fm1_ms - clk_t0; clk_bpm = ms ? (240000u + ms / 2u) / ms : 0; } clk_t0 = fm1_ms; }
                clk_n++;
            }
            else if (st == 0xFAu) { arp_ext.start++; clk_n = 0; }
            else if ((st & 0xF0u) == 0x90u && d2) dsp_ring_put(&ev, 1u << 16 | (uint32_t)d1 << 8 | d2);
            else if ((st & 0xF0u) == 0x80u || (st & 0xF0u) == 0x90u) dsp_ring_put(&ev, (uint32_t)d1 << 8);
        }
        (void)clk_seen;

        b = fm1_input_edges(&rel);
        if (b & (1u << BTN_OCT_DOWN) && oct > -2) oct--;
        if (b & (1u << BTN_OCT_UP) && oct < 2) oct++;
        if ((g = ui_gesture(&g_fx, b, fm1_in.buttons, BTN_FX, fm1_ms, 500)) == UI_G_TAP) page = PG_FX;
        else if (g == UI_G_HOLD) { fx_bypass ^= 1u; ui_msg(&msg, fx_bypass ? "FX BYPASS" : "FX ON", fm1_ms, 900); }
        if ((g = ui_gesture(&g_arp, b, fm1_in.buttons, BTN_ARP, fm1_ms, 500)) == UI_G_TAP) page = PG_ARP;
        else if (g == UI_G_HOLD) arp_enabled ^= 1u;
        if ((g = ui_gesture(&g_home, b, fm1_in.buttons, BTN_HOME, fm1_ms, 700)) == UI_G_TAP) page = PG_HOME;
        else if (g == UI_G_HOLD) page = PG_HELP;
        if (b & (1u << BTN_SAVE)) {
            int rc = patch_save(0);
            ui_msg(&msg, rc == PL_OK ? "SAVED" : rc == PL_E_FULL ? "STORE FULL" : "SAVE FAILED", fm1_ms, 900);
            if (rc == PL_OK) ui_copy(name, "SINK 1", sizeof name);
        }
        if ((e = fm1_enc_take(ENC_PRESETS)) != 0) {
            static int32_t sel;
            sel = (sel + e) & 7;
            if (patch_load((uint32_t)sel, name)) ui_msg(&msg, "EMPTY", fm1_ms, 600); else fx_apply();
        }
        if (page != PG_HELP)
            for (k = 0; k < 4u; k++)
                if ((e = fm1_enc_take(ENC_KNOB1 + k)) != 0) { uint32_t p = PG_PAR[page] + k; ks_param((int)p, par[p] + e); touched = k; if (page == PG_FX) fx_apply(); }
        if (pl_link_dirty) pl_link_dirty = 0;   /* (a computer changed patches: a real app refreshes its list) */

        /* LEDs: the keys that sound, the holds */
        for (k = 0; k < NV; k++) if (vx[k].env.st != DSP_ENV_IDLE) voices |= 1u << k;
        fm1_led_key(BTN_ARP, (int)arp_enabled);
        fm1_led_key(BTN_FX, (int)!fx_bypass && par[P_FXT] != 0);

        /* the screen: only what changed */
        if (page != last_page) { lcd_fill(0, 0, 240, 240, C_BLACK); last_page = page; for (k = 0; k < 4u; k++) drawn_k[k] = ~0u; drawn_pic = ~0u; drawn_title[0] = 1; }
        if (page == PG_HELP) { if (drawn_pic) { ui_help(HELP, sizeof HELP / sizeof HELP[0]); drawn_pic = 0; } continue; }
        for (k = 0; k < 4u; k++) {
            uint32_t p = PG_PAR[page] + k, key = (uint32_t)par[p] | (touched == k) << 16;
            if (key != drawn_k[k]) {
                char vt[12];
                val_text(vt, p);
                ui_knob(k, PD[p].name, vt, (uint32_t)((par[p] - PD[p].lo) * 256 / (PD[p].hi - PD[p].lo)), touched == k ? UI_K_TOUCHED : 0);
                drawn_k[k] = key;
            }
        }
        if (drawn_pic == ~0u) ui_tabs(UI_Y_TABS, PG_NAME, 3, page, 0, 1);
        {   uint32_t ext = arp_tap.ext, bb = arp_tap.beat_blocks;   /* MIDI clock: the 4-beat ms measure; until there is one, a
                                                                       beat in audio blocks (5.8 ms each, so +-1 BPM: it flickers) */
            uint32_t bpm = !ext ? (uint32_t)arp_cfg.bpm : clk_bpm ? clk_bpm : bb ? (10336u + bb / 2u) / bb : (uint32_t)arp_cfg.bpm;
            if (!ext) clk_n = 0, clk_bpm = 0;
            pk = voices | (uint32_t)fx_bypass << 8 | arp_enabled << 9 | page << 10 | ext << 12 | bpm << 13;
            if (pk != drawn_pic) { pic_t c = {page, voices, fx_bypass, arp_enabled, bpm, ext}; ui_pic(UI_Y_BODY, UI_BODY_H, pic_fn, &c); drawn_pic = pk; }
        }
        if (fm1_ms - t_status > 250u) {
            uint32_t nv = 0;
            for (k = 0; k < NV; k++) nv += voices >> k & 1u;
            ui_status(UI_Y_LINEA, PG_NAME[page], nv, NV, dsp_cpu_now / 10u, 0);
            t_status = fm1_ms;
        }
        {   const char *fl = ui_msg_on(&msg, fm1_ms);
            char want[24];
            ui_copy(want, fl ? fl : name, sizeof want);
            if (!ui_eq(want, drawn_title)) { ui_title(UI_Y_PATCH, name, fl, 0); ui_copy(drawn_title, want, sizeof drawn_title); }
        }
    }
}
