/* SPDX-License-Identifier: GPL-3.0-only */
/* firmware/lib/fx.h on the computer: every type stays bounded and falls silent after the input stops (no limit cycle),
 * the delay's echo comes at its time, the bypass crossfades without a jump, a type change clears the slot. */
#define DINGHY_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include "fx.h"

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static int32_t bus[512];
static uint32_t tone_ph;                         /* one tone for every test: its phase never jumps */

static void set(uint32_t slot, int32_t t, int32_t a, int32_t b, int32_t mix)
{
    fx_param[slot].type = t; fx_param[slot].a = a; fx_param[slot].b = b; fx_param[slot].mix = mix;
}
static int32_t run_blocks(uint32_t nb, int tone, int32_t *peak)     /* returns the last block's peak */
{
    uint32_t k, i; int32_t last = 0;
    for (k = 0; k < nb; k++) {
        for (i = 0; i < 256; i++) { int32_t x = tone ? dsp_sine(tone_ph) * 16 : 0; tone_ph += 40000000u; bus[2 * i] = bus[2 * i + 1] = x; }
        fx_process(bus, 256);
        last = 0;
        for (i = 0; i < 512; i++) { int32_t a = abs(bus[i]); if (a > last) last = a; if (a > *peak) *peak = a; }
    }
    return last;
}

int main(void)
{
    static const char *NM[6] = {"", "chorus", "flanger", "phaser", "delay", "reverb"};
    int32_t t;
    for (t = 1; t <= 5; t++) {
        int32_t pk = 0, tail;
        uint32_t i;
        for (i = 0; i < FX_SLOTS; i++) set(i, 0, 64, 64, 0);
        set(0, t, 90, 100, 127);
        fx_sync();
        run_blocks(200, 1, &pk);
        tail = run_blocks(t == 5 ? 3000 : t == 4 ? 3000 : 600, 0, &pk);
        CHECK(pk < (1 << 21), "%s: bounded (peak %.2f FS)", NM[t], pk / 1048576.0);
        CHECK(tail < 64, "%s: silent after the input stops (last %d)", NM[t], tail);
    }
    /* delay: an impulse comes back after A's time: (441 + A x 182) samples */
    {   uint32_t i, k, at = 0; int32_t pk = 0;
        set(0, FXT_DELAY, 40, 0, 127); fx_sync();
        run_blocks(400, 0, &pk);                                   /* (the time has glided to its target) */
        for (k = 0; k < 100 && !at; k++) {
            for (i = 0; i < 512; i++) bus[i] = (k == 0 && i < 2) ? 1 << 20 : 0;
            fx_process(bus, 256);
            for (i = 2; i < 512 && !at; i += 2) if (abs(bus[i]) > 1000) at = k * 256 + i / 2;
        }
        CHECK(abs((int)at - (441 + 40 * 182)) < 4, "delay: the echo at %u samples (want %d)", at, 441 + 40 * 182);
    }
    /* bypass: from a chorus to dry and back, no step bigger than the tone's own slope */
    {   uint32_t i, k; int32_t prev = 0, worst = 0, pk = 0;
        set(0, FXT_CHORUS, 60, 100, 127); fx_sync(); run_blocks(50, 1, &pk);
        for (k = 0; k < 40; k++) {
            if (k == 10) fx_bypass = 1;
            if (k == 25) fx_bypass = 0;
            for (i = 0; i < 256; i++) { int32_t x = dsp_sine(tone_ph) * 16; tone_ph += 40000000u; bus[2 * i] = bus[2 * i + 1] = x; }
            fx_process(bus, 256);
            for (i = 0; i < 256; i++) { int32_t d = abs(bus[2 * i] - prev); if (k && d > worst) { worst = d; if (getenv("FXDBG")) printf("  step %d at block %u frame %u\n", d, k, i); } prev = bus[2 * i]; }
        }
        CHECK(worst < 40000, "bypass on / off: no jump (largest step %d, the tone's own up to ~%d)", worst, 30700);
    }
    /* a type change clears the slot: a reverb replaced by nothing leaves the bus untouched at once */
    {   int32_t pk = 0; uint32_t i;
        set(0, FXT_REVERB, 100, 30, 127); fx_sync(); run_blocks(100, 1, &pk);
        set(0, FXT_NONE, 0, 0, 0); fx_sync();
        for (i = 0; i < 512; i++) bus[i] = 1234;
        fx_process(bus, 256);
        CHECK(bus[0] == 1234 && bus[511] == 1234, "slot off: the bus passes unchanged");
    }
    printf(fails ? "%d FAILED\n" : "all fx checks passed\n", fails);
    return fails != 0;
}
