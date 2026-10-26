/* SPDX-License-Identifier: GPL-3.0-only */
/* firmware/lib/link.h as a process, for the browser library's test (tests/web_link_check.mjs): each stdin line is one
 * SysEx frame in hex (F0 .. F7), each reply goes to stdout in hex ("-" when none). A key down: the line "keys 1". */
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


#include <ctype.h>
int main(void)
{
    static char line[4096];
    pl_init();
    while (fgets(line, sizeof line, stdin)) {
        uint32_t n = 0, i;
        static uint8_t f[2048];
        if (!strncmp(line, "keys ", 5)) { fm1_in.notes = (uint32_t)atoi(line + 5); continue; }
        for (i = 0; line[i] && isxdigit((unsigned char)line[i]) && isxdigit((unsigned char)line[i + 1]); i += 2) {
            unsigned v; sscanf(line + i, "%2x", &v); f[n++] = (uint8_t)v;
        }
        if (n < 3 || f[0] != 0xF0 || f[n - 1] != 0xF7) { puts("-"); fflush(stdout); continue; }
        memcpy(frame, f + 1, n - 2); frame_n = n - 2; rep_n = 0;
        { uint32_t r0 = replies; link_service();
          if (replies == r0) puts("-");
          else { static uint8_t w[2048]; uint32_t k, m = 0; w[m++] = 0xF0; m += ota_pack7(rep, rep_n, w + 1); w[m++] = 0xF7;
                 for (k = 0; k < m; k++) printf("%02x", w[k]); printf("\n"); } }
        fflush(stdout);
    }
    return 0;
}
