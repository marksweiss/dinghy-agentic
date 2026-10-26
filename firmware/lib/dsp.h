/* SPDX-License-Identifier: GPL-3.0-only */
/* dsp.h: integer-only building blocks for a synth engine on the FM-1 (no FPU, no 64-bit divide). From TastyWaves'
 * engine (github.com/marksweiss/tastywaves, firmware/src/synth.h), made independent of its patch.
 *
 * Include after platform.c. Everything is static inline / static; pay only for what you call. Tables: dsp_tables.h
 * (tools/gen_dsp_tables.py), const, in flash.
 *
 *   Pitch       dsp_note_inc(n), dsp_pitch_inc(p)       note / note x 256 (fine) -> 32-bit phase step at 44.1 kHz
 *   Oscillator  dsp_sine(ph)                             a cycle over 2^32 -> +-32767 (parabola, ~0.1 %)
 *               dsp_noise(&seed)                         white, +-32767
 *   LFO         dsp_lfo(shape, ph, &sh)                  DSP_LFO_SINE TRI SAW SQUARE SH -> +-32767
 *   Envelope    dsp_env_t, dsp_env_gate, dsp_env_step   ADSR per control step (DSP_CR frames), Q30, times 0..127
 *   Filter      dsp_svf_t, dsp_svf_set, dsp_svf_run     trapezoidal state-variable LP / BP / HP, cutoff as a note
 *   Glide       dsp_glide(cur, target, k)                one-pole per control step (DSP_GLIDE_K)
 *   Output      dsp_softclip(x)                          linear to 0.5 FS, then tanh: never above 32767
 *   Voices      dsp_voice_pick(...)                      same note, else free, else quietest released, else oldest
 *   Events      dsp_ring_t, dsp_ring_put / dsp_ring_get  main loop -> audio ISR, single producer / consumer
 *   CPU         DSP_CPU_BEGIN / DSP_CPU_END              the ISR's share of its 5.8 ms budget, permille
 *
 * Rules these follow (docs/RULES.md): Q15 audio, Q30 / Q28 coefficients, 64-bit multiply but never divide, a
 * divisor never 0, control-rate work once per DSP_CR frames, the ISR and the main loop share only volatile
 * fields with one writer each. */
#pragma once
#include <stdint.h>
#include "dsp_tables.h"

#define DSP_CR 32u                              /* frames per control step: tables assume it (DSP_TABLES_CR) */
#define DSP_Q30 (1 << 30)
_Static_assert(DSP_CR == DSP_TABLES_CR, "dsp.h: DSP_CR differs from the tables (tools/gen_dsp_tables.py)");

/* ---- pitch */
static inline uint32_t dsp_note_inc(uint32_t note)            /* MIDI note 0..131 -> phase step */
{
    if (note > 131u) note = 131u;
    return DSP_INC_C9[note % 12u] >> (10u - note / 12u);
}
static inline uint32_t dsp_pitch_inc(int32_t p)               /* note x 256 (1/256 semitone) -> phase step */
{
    uint32_t n, base;
    if (p < 0) p = 0;
    if (p > 131 * 256 + 255) p = 131 * 256 + 255;
    n = (uint32_t)p >> 8;
    base = DSP_INC_C9[n % 12u] >> (10u - n / 12u);
    return base + (uint32_t)(((uint64_t)base * DSP_FINE16[(uint32_t)p & 255u]) >> 16);
}

/* ---- oscillators */
static inline int32_t dsp_sine(uint32_t ph)
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}
static inline int32_t dsp_noise(uint32_t *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return (int32_t)*seed >> 16;
}

enum { DSP_LFO_SINE, DSP_LFO_TRI, DSP_LFO_SAW, DSP_LFO_SQUARE, DSP_LFO_SH };
/* ph: the LFO phase (add DSP_LFO_INC[rate] each control step); sh: sample-and-hold state (value << 1 | last top bit) */
static inline int32_t dsp_lfo(uint32_t shape, uint32_t ph, int32_t *sh)
{
    switch (shape) {
    case DSP_LFO_TRI: { int32_t x = (int32_t)(ph >> 15) - 65536; return ((x < 0 ? -x : x) - 32768) * 32767 / 32768; }
    case DSP_LFO_SAW: { int32_t v = (int32_t)(ph >> 16) - 32768; return v < -32767 ? -32767 : v; }
    case DSP_LFO_SQUARE: return ph < 0x80000000u ? 32767 : -32767;
    case DSP_LFO_SH:
        if ((uint32_t)(*sh & 1) != ph >> 31) {   /* a new value each cycle: a hash of the phase bits */
            uint32_t s = (uint32_t)*sh * 1664525u + 1013904223u + ph;
            *sh = (int32_t)(((int32_t)s >> 16) * 2) | (int32_t)(ph >> 31);
        }
        return *sh >> 1;
    default: return dsp_sine(ph);
    }
}

/* ---- ADSR envelope, Q30 (0 .. 2^30), stepped once per DSP_CR frames. Times 0..127 = 0.5 ms .. 10 s; sustain Q30.
 * lin: linear decay / release (else exponential, -60 dB in the time). A gate on restarts the attack from where the
 * level is (no click); off goes to release. */
enum { DSP_ENV_IDLE, DSP_ENV_A, DSP_ENV_D, DSP_ENV_S, DSP_ENV_R };
typedef struct { uint8_t st; int32_t lvl; } dsp_env_t;
static inline void dsp_env_gate(dsp_env_t *e, int on)
{
    if (on) e->st = DSP_ENV_A;
    else if (e->st != DSP_ENV_IDLE) e->st = DSP_ENV_R;
}
static inline int32_t dsp_env_step(dsp_env_t *e, uint32_t a, uint32_t d, int32_t s, uint32_t r, int lin)
{
    int32_t lvl = e->lvl;
    a &= 127u; d &= 127u; r &= 127u;
    switch (e->st) {
    case DSP_ENV_A:
        if (lvl > DSP_Q30 - DSP_ENV_STEP[a]) lvl = DSP_Q30;         /* lvl + step must not reach 2^31 */
        else lvl += DSP_ENV_STEP[a];
        if (lvl >= DSP_Q30) { lvl = DSP_Q30; e->st = DSP_ENV_D; }
        break;
    case DSP_ENV_D:
        if (lin) lvl -= DSP_ENV_STEP[d];
        else lvl = s + (int32_t)(((int64_t)(lvl - s) * DSP_ENV_COEF[d]) >> 30);
        if (lvl - s < (1 << 14)) { lvl = s; e->st = DSP_ENV_S; }
        break;
    case DSP_ENV_S:
        lvl += (s - lvl) >> 3;                  /* follows a sustain change without a step */
        break;
    case DSP_ENV_R:
        if (lin) lvl -= DSP_ENV_STEP[r];
        else lvl = (int32_t)(((int64_t)lvl * DSP_ENV_COEF[r]) >> 30);
        if (lvl < (1 << 16)) { lvl = 0; e->st = DSP_ENV_IDLE; }
        break;
    default:
        lvl = 0;
        break;
    }
    return e->lvl = lvl;
}

/* ---- state-variable filter (Simper / Cytomic trapezoidal): set the coefficients once per control step, run per
 * sample. Cutoff as a note number x 256 (0 .. 135 x 256: 8 Hz .. 18 kHz), resonance 0..127. The states carry the
 * signal x 16 for precision; input and output are the same scale (Q15, up to a few times full scale is fine: the
 * worst-case state is ~33 x 2 x |x| x 16 at the top resonance, so keep |x| < 2^21 / 33). */
enum { DSP_LP, DSP_BP, DSP_HP };
typedef struct { int32_t ic1, ic2; } dsp_svf_t;
typedef struct { int32_t a1, a2, a3, k; uint8_t mode; } dsp_svf_c;
static inline void dsp_svf_set(dsp_svf_c *c, int32_t cutoff_q8, uint32_t reso, uint32_t mode)
{
    int32_t n, g, fr;
    uint32_t den;
    if (cutoff_q8 < 0) cutoff_q8 = 0;
    if (cutoff_q8 > 134 * 256 + 255) cutoff_q8 = 134 * 256 + 255;
    n = cutoff_q8 >> 8; fr = cutoff_q8 & 255;
    g = DSP_SVF_G[n] + (int32_t)(((int64_t)(DSP_SVF_G[n + 1] - DSP_SVF_G[n]) * fr) >> 8);
    c->k = DSP_SVF_2R[reso > 127u ? 127u : reso];
    den = 65536u + (uint32_t)(((int64_t)g * (g + c->k)) >> 40);     /* 1 + g (g + k), Q16; >= 65536: never 0 */
    c->a1 = (int32_t)((0xFFFFFFFFu / den) << 12);                   /* 1 / den, Q28 */
    c->a2 = (int32_t)(((int64_t)g * c->a1) >> 28);
    c->a3 = (int32_t)(((int64_t)g * c->a2) >> 28);
    c->mode = (uint8_t)mode;
}
static inline int32_t dsp_svf_run(dsp_svf_t *f, const dsp_svf_c *c, int32_t in)
{
    int32_t x = in * 16, v3 = x - f->ic2, v1, v2, y;
    v1 = (int32_t)(((int64_t)c->a1 * f->ic1 + (int64_t)c->a2 * v3) >> 28);
    v2 = f->ic2 + (int32_t)(((int64_t)c->a2 * f->ic1 + (int64_t)c->a3 * v3) >> 28);
    f->ic1 = 2 * v1 - f->ic1;
    f->ic2 = 2 * v2 - f->ic2;
    y = c->mode == DSP_BP ? v1 : c->mode == DSP_HP ? x - (int32_t)(((int64_t)c->k * v1) >> 28) - v2 : v2;
    return y / 16;
}

/* ---- glide: cur moves toward target once per control step; k = DSP_GLIDE_K[0..127] (Q16; 65536 = jump) */
static inline int32_t dsp_glide(int32_t cur, int32_t target, int32_t k)
{
    int32_t d = target - cur;
    if (k >= 65536 || (d < 64 && d > -64)) return target;
    return cur + (int32_t)(((int64_t)d * k) >> 16);
}

/* ---- the output: linear to 0.5 full scale, then 0.5 FS x tanh, never above 32767 (a sum of voices stays clean) */
static inline int32_t dsp_softclip(int32_t x)
{
    int32_t a = x < 0 ? -x : x, u, i, y;
    if (a <= 16384) return x;
    u = a - 16384;
    if (u > 131071) u = 131071;
    i = u >> 9;
    y = 16384 + DSP_SOFTCLIP[i] + (((DSP_SOFTCLIP[i + 1] - DSP_SOFTCLIP[i]) * (u & 511)) >> 9);
    return x < 0 ? -y : y;
}

/* ---- voice allocation: the voice to use for `note`. Per voice: note, gate (key held), level (0 = silent; e.g. the
 * amp envelope >> 8), age (bigger = newer). Order: the same note (retrigger), else a silent voice, else the quietest
 * released one, else the oldest held one (stolen). */
typedef struct { uint8_t note, gate; int32_t level; uint32_t age; } dsp_vinfo_t;
static inline uint32_t dsp_voice_pick(const dsp_vinfo_t *v, uint32_t n, uint32_t note)
{
    uint32_t i, pick = 0, best = 0xFFFFFFFFu;
    for (i = 0; i < n; i++) {
        uint32_t score;
        if (v[i].note == note && (v[i].gate || v[i].level)) score = 0;
        else if (!v[i].gate && !v[i].level) score = 1u + (v[i].age & 0xFFFFFFu);
        else if (!v[i].gate) score = 0x40000000u + ((uint32_t)v[i].level >> 8);
        else score = 0x80000000u + (v[i].age & 0x3FFFFFFFu);
        if (score < best) { best = score; pick = i; }
    }
    return pick;
}

/* ---- events, main loop -> audio ISR: a power-of-two ring of 32-bit words, one writer (main) and one reader (ISR).
 * The writer fills the slot, then publishes head; the reader reads the slot, then publishes tail. Full: dropped
 * (returns -1). Pack your own fields: e.g. type << 16 | note << 8 | velocity. */
#define DSP_RING_N 64u
typedef struct { volatile uint32_t head, tail; uint32_t e[DSP_RING_N]; } dsp_ring_t;
static inline int dsp_ring_put(dsp_ring_t *r, uint32_t v)
{
    uint32_t h = r->head;
    if (h - r->tail >= DSP_RING_N) return -1;
    r->e[h % DSP_RING_N] = v;
    __asm__ volatile("" ::: "memory");
    r->head = h + 1u;
    return 0;
}
static inline int dsp_ring_get(dsp_ring_t *r, uint32_t *v)
{
    uint32_t t = r->tail;
    if (t == r->head) return 0;
    *v = r->e[t % DSP_RING_N];
    __asm__ volatile("" ::: "memory");
    r->tail = t + 1u;
    return 1;
}

/* ---- CPU: the ISR's time against its budget (one block of 256 frames = 5.805 ms). In app_audio:
 *   DSP_CPU_BEGIN(t0); ... render ...; DSP_CPU_END(t0, dsp_cpu_now, dsp_cpu_max);
 * then the main loop shows dsp_cpu_now / dsp_cpu_max (permille). Under 500 leaves room for USB and the screen;
 * TastyWaves ran 12 voices x 2 wavetable oscillators + SVF at ~500, 760 with four effects. */
#define DSP_BUDGET_TICKS (256u * (FM1_TICKS_PER_US * 10000u) / 441u)
static volatile uint32_t dsp_cpu_now, dsp_cpu_max;
#define DSP_CPU_BEGIN(t0) uint32_t t0 = fm1_ticks()
#define DSP_CPU_END(t0, now, max) do { uint32_t _p = (fm1_ticks() - (t0)) * 1000u / DSP_BUDGET_TICKS; \
                                       (now) = _p; if (_p > (max)) (max) = _p; } while (0)
