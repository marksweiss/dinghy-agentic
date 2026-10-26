/* SPDX-License-Identifier: GPL-3.0-only */
/* firmware/lib/link.h on the computer: fake SysEx frames in, replies decoded: HELLO, a bad sum (no reply), the updater's
 * frames left alone, an app command, and the patch commands through patchlog.h (put, list, get, the same put again
 * writes nothing, busy while a key is down, delete). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_ID "FM-1_9XX"
#define NSLOT 8u
#define SLOT_SECT 4u
#define SLOTS_AT 0x00097000u
static struct { uint32_t notes; } fm1_in;
static uint8_t flash[0x100000], flash_ok = 1;
static void fm1_wdt_feed(void) {}
static uint32_t crc32(const uint8_t *p, uint32_t n)
{ uint32_t c = 0xFFFFFFFFu, k; while (n--) { c ^= *p++; for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; } return ~c; }
static int st_read(uint32_t off, void *d, uint32_t n) { memcpy(d, flash + off, n); return 0; }
static uint32_t writes;
static int fl_erase4k(uint32_t off, uint32_t *t) { *t = 0; memset(flash + off, 0xFF, 0x1000); writes++; return 0; }
static int fl_write(uint32_t off, const uint8_t *s, uint32_t n) { uint32_t i; for (i = 0; i < n; i++) flash[off + i] &= s[i]; writes++; return 0; }

/* ota.c's pieces the link uses */
static uint32_t ota_pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{ uint32_t acc = 0, nb = 0, o = 0; while (n--) { acc |= (uint32_t)*in++ << nb; nb += 8u; while (nb >= 7u) { out[o++] = (uint8_t)(acc & 0x7Fu); acc >>= 7; nb -= 7u; } } if (nb) out[o++] = (uint8_t)(acc & 0x7Fu); return o; }
static uint32_t ota_unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{ uint32_t acc = 0, nb = 0, o = 0; while (n--) { acc |= (uint32_t)(*in++ & 0x7Fu) << nb; nb += 7u; if (nb >= 8u) { if (o < max) out[o] = (uint8_t)acc; o++; acc >>= 8; nb -= 8u; } } return o; }
static uint32_t ota_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint8_t ota_dec[600];
static uint8_t frame[700]; static uint32_t frame_n, frame_taken;
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!frame_n) return 0; *p = frame; *n = frame_n; return 1; }
static void ota_frame_done(void) { frame_n = 0; frame_taken = 1; }
static uint8_t rep[400]; static uint32_t rep_n, replies;
static int ota_wire_send(const uint8_t *p, uint32_t n) { rep_n = ota_unpack7(p + 1, n - 2, rep, sizeof rep); replies++; return 0; }

#include "patchlog.h"
#define LINK_PATCHES 1
static uint32_t my_cmd(uint32_t cmd, const uint8_t *a, uint32_t len, uint8_t *out, uint32_t *n)
{ if (cmd != 40 || len != 1) return 1; out[0] = (uint8_t)(a[0] + 1); *n = 1; return 0; }
#define LINK_APP(c, a, l, o, n) my_cmd(c, a, l, o, n)
#include "link.h"

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok    "); else { printf("FAIL  "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void send(uint32_t cmd, const uint8_t *pl, uint32_t n, int corrupt)    /* a request frame, then one service pass */
{
    static uint8_t m[600]; static uint32_t seq;
    uint32_t i, s = 0;
    m[0] = 0x7D; m[1] = LINK_ID1; m[2] = LINK_ID2; m[3] = (uint8_t)cmd; m[4] = (uint8_t)++seq; m[5] = (uint8_t)n; m[6] = (uint8_t)(n >> 8);
    for (i = 0; i < n; i++) m[7 + i] = pl[i];
    for (i = 3; i < 7 + n; i++) s += m[i];
    m[7 + n] = (uint8_t)(~s + (corrupt ? 1 : 0));
    frame_n = ota_pack7(m, 8 + n, frame); frame_taken = 0; rep_n = 0;
    link_service();
}

int main(void)
{
    uint8_t body[1 + 12 + 2 + 6], x = 41;
    uint32_t r0, w0, i;
    pl_init();
    send(1, 0, 0, 0);
    CHECK(rep_n == 9 + 16 && rep[3] == 1 && rep[4] == 0 && rep[8] == 1 && !memcmp(rep + 16, "FM-1_9XX", 8), "HELLO: proto 1 and the identity");
    r0 = replies; send(1, 0, 0, 1);
    CHECK(replies == r0 && frame_taken, "a bad sum: taken, no reply");
    { static const uint8_t up[] = {0x00, 0x59, 0x30, 1, 2, 3}; frame_n = ota_pack7(up, sizeof up, frame); frame_taken = 0; link_service();
      CHECK(!frame_taken && frame_n, "the updater's frame stays for ota_service"); frame_n = 0; }
    send(40, &x, 1, 0);
    CHECK(rep[4] == 0 && rep_n == 10 && rep[8] == 42, "an app command (LINK_APP)");
    send(99, 0, 0, 0);
    CHECK(rep[4] == 1, "an unknown command: rc 1");
    /* the patch log: put id 3 "PAD", list it, get it back */
    memset(body, 0, sizeof body); body[0] = 3; memcpy(body + 1, "PAD", 3); body[13] = 3; body[14] = 0;
    body[15] = 10; body[16] = 0; body[17] = 0xFF; body[18] = 0xFF; body[19] = 0x34; body[20] = 0x12;   /* 10, -1, 0x1234 */
    send(14, body, sizeof body, 0);
    CHECK(rep[4] == 0 && (pl_present() & 8u), "PPUT id 3: stored");
    { uint8_t q[2] = {0, 8}; send(12, q, 2, 0); CHECK(rep[4] == 0 && rep_n == 9 + 8 * 16 && rep[8 + 3 * 16 + 1] == 1 && !memcmp(rep + 8 + 3 * 16 + 4, "PAD", 3), "PDIR lists it"); }
    { uint8_t q = 3; send(13, &q, 1, 0); CHECK(rep[4] == 0 && rep[8 + 12] == 3 && rep[8 + 14] == 10 && rep[8 + 16] == 0xFF && rep[8 + 19] == 0x12, "PGET gives the body back"); }
    w0 = writes; send(14, body, sizeof body, 0);
    CHECK(rep[4] == 0 && writes == w0, "the same PPUT again writes nothing");
    fm1_in.notes = 1; send(15, body, 1, 0); fm1_in.notes = 0;
    CHECK(rep[4] == 7, "PDEL while a key is down: busy (7)");
    send(15, body, 1, 0);
    CHECK(rep[4] == 0 && !(pl_present() & 8u) && (pl_link_dirty & 8u), "PDEL: gone, and pl_link_dirty tells the app");
    for (i = 0; i < 1; i++) (void)i;
    printf(fails ? "%d FAILED\n" : "all link checks passed\n", fails);
    return fails != 0;
}
