/* SPDX-License-Identifier: GPL-3.0-only */
/* firmware/lib/dsp.h on the computer: pitch, sine, LFO, envelope times, the filter's response and stability, glide,
 * soft clip, voice stealing, the event ring. Run by tests/run_lib.sh. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "dsp.h"

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static double svf_gain(int32_t cut_q8, uint32_t reso, uint32_t mode, double hz)    /* steady-state gain at hz */
{
    dsp_svf_t f = {0, 0}; dsp_svf_c c; double in = 0, out = 0; uint32_t ph = 0, inc = (uint32_t)(hz * 4294967296.0 / 44100), i;
    dsp_svf_set(&c, cut_q8, reso, mode);
    for (i = 0; i < 44100; i++) {
        int32_t x = dsp_sine(ph) / 2, y = dsp_svf_run(&f, &c, x);
        ph += inc;
        if (i > 22050) { in += (double)x * x; out += (double)y * y; }
    }
    return sqrt(out / in);
}

int main(void)
{
    uint32_t i, worst = 0;
    double err = 0;
    /* pitch: A4 = 440 Hz; a fine step of 256 is one semitone */
    CHECK(fabs(dsp_note_inc(69) * 44100.0 / 4294967296.0 - 440.0) < 0.01, "A4 at 440 Hz (%.3f)", dsp_note_inc(69) * 44100.0 / 4294967296.0);
    CHECK(dsp_pitch_inc(60 * 256 + 256) == dsp_note_inc(61) || abs((int)(dsp_pitch_inc(61 * 256) - dsp_note_inc(61))) < 2, "pitch_inc(61 x 256) = note 61");
    CHECK(fabs((double)dsp_pitch_inc(60 * 256 + 128) / dsp_note_inc(60) - pow(2, 0.5 / 12)) < 1e-4, "half a semitone up");
    /* sine within 0.2 % of full scale */
    for (i = 0; i < 4096; i++) { double e = fabs(dsp_sine(i << 20) - 32767 * sin(2 * M_PI * i / 4096)); if (e > err) err = e; }
    CHECK(err < 70, "sine within 0.2 %% of full scale (worst %.0f)", err);
    /* LFO shapes bounded */
    { int32_t sh = 0; uint32_t s; int ok = 1; for (s = 0; s <= DSP_LFO_SH; s++) for (i = 0; i < 1000; i++) { int32_t v = dsp_lfo(s, i * 4294967u * 7u, &sh); if (v > 32767 || v < -32767) ok = 0; } CHECK(ok, "LFO shapes within +-32767"); }
    /* envelope: attack 0 reaches full in one step; attack 64 takes about env_time(64) */
    { dsp_env_t e = {0, 0}; uint32_t n = 0; dsp_env_gate(&e, 1); while (e.st == DSP_ENV_A && n < 100000) { dsp_env_step(&e, 64, 64, DSP_Q30 / 2, 64, 0); n++; }
      double ms = n * 32 / 44.1, want = 0.5 * pow(20000, 64 / 127.0);
      CHECK(fabs(ms - want) / want < 0.05, "attack 64: %.0f ms (want %.0f)", ms, want);
      n = 0; while (e.st != DSP_ENV_S && n < 100000) { dsp_env_step(&e, 64, 20, DSP_Q30 / 2, 64, 0); n++; }
      CHECK(e.lvl == DSP_Q30 / 2, "decay settles on the sustain");
      dsp_env_gate(&e, 0); n = 0; while (e.st != DSP_ENV_IDLE && n < 1000000) { dsp_env_step(&e, 64, 20, DSP_Q30 / 2, 30, 0); n++; }
      CHECK(e.st == DSP_ENV_IDLE && e.lvl == 0, "release ends idle at 0 (%u steps)", n); }
    /* filter: low-pass at 1 kHz passes 100 Hz, cuts 8 kHz by > 30 dB; high-pass the other way; resonance peaks */
    { int32_t c1k = (int32_t)((69 + 12 * log2(1000 / 440.0)) * 256);
      double lo = svf_gain(c1k, 0, DSP_LP, 100), hi = svf_gain(c1k, 0, DSP_LP, 8000), hpl = svf_gain(c1k, 0, DSP_HP, 100), pk = svf_gain(c1k, 120, DSP_LP, 1000);
      CHECK(lo > 0.95 && lo < 1.05, "LP 1 kHz: 100 Hz passes (%.3f)", lo);
      CHECK(hi < 0.03, "LP 1 kHz: 8 kHz -%.0f dB", -20 * log10(hi));
      CHECK(hpl < 0.03, "HP 1 kHz: 100 Hz -%.0f dB", -20 * log10(hpl));
      CHECK(pk > 4.0, "resonance 120: a peak at the cutoff (x%.1f)", pk); }
    /* filter stable at top resonance and cutoff with a loud square: no overflow, decays after */
    { dsp_svf_t f = {0, 0}; dsp_svf_c c; int32_t y = 0, mx = 0; dsp_svf_set(&c, 134 * 256, 127, DSP_LP);
      for (i = 0; i < 44100; i++) { y = dsp_svf_run(&f, &c, (i & 64) ? 60000 : -60000); if (abs(y) > mx) mx = abs(y); }
      for (i = 0; i < 44100; i++) y = dsp_svf_run(&f, &c, 0);
      CHECK(mx < (1 << 21) && abs(y) < 4, "top resonance stays bounded (peak %d) and rings out (%d)", mx, y); }
    /* glide reaches its target */
    { int32_t p = 0; for (i = 0; i < 10000; i++) p = dsp_glide(p, 60 * 256, DSP_GLIDE_K[60]); CHECK(p == 60 * 256, "glide arrives"); }
    /* soft clip: never above 32767, transparent below the knee, monotonic */
    { int32_t last = -40000, ok = 1; for (i = 0; i < 400000; i += 7) { int32_t x = (int32_t)i - 200000, y = dsp_softclip(x); if (y > 32767 || y < -32767 || y < last) ok = 0; last = y; } CHECK(ok && dsp_softclip(10000) == 10000, "soft clip bounded, monotonic, transparent under 0.5 FS"); }
    /* voices: same note first, then a free one, then the oldest held one */
    { dsp_vinfo_t v[3] = {{60, 1, 100, 5}, {64, 0, 0, 6}, {67, 1, 100, 7}};
      CHECK(dsp_voice_pick(v, 3, 60) == 0, "the same note retriggers its voice");
      CHECK(dsp_voice_pick(v, 3, 72) == 1, "a free voice next");
      v[1].gate = 1; v[1].level = 100; CHECK(dsp_voice_pick(v, 3, 72) == 0, "all busy: the oldest is stolen"); }
    /* ring: order, full */
    { dsp_ring_t r = {0}; uint32_t v, n = 0, ok = 1; for (i = 0; i < DSP_RING_N + 5; i++) n += dsp_ring_put(&r, i) == 0;
      for (i = 0; dsp_ring_get(&r, &v); i++) if (v != i) ok = 0;
      CHECK(n == DSP_RING_N && ok && i == DSP_RING_N, "ring keeps order, refuses when full"); }
    (void)worst;
    printf(fails ? "%d FAILED\n" : "all dsp checks passed\n", fails);
    return fails != 0;
}
