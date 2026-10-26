/* SPDX-License-Identifier: GPL-3.0-only */
/* link.h: a request / reply link between the FM-1 and a computer (a browser page over Web MIDI, a Python tool) over
 * USB SysEx, beside the stock updater on the same port. From TastyWaves' "VW link" (github.com/marksweiss/tastywaves,
 * firmware/src/wtlink.h + patchlink.h; design: its docs/UPLOAD-DESIGN.md), made generic.
 *
 * Wire (7D = the non-commercial manufacturer id; LINK_ID1 LINK_ID2 = your app's two letters, default "DA"):
 *   request  F0 pack7( 7D id1 id2 cmd seq len_lo len_hi payload[len] sum ) F7
 *   reply    F0 pack7( 7D id1 id2 cmd rc seq len_lo len_hi payload[len] sum ) F7
 *   pack7; sum = ~(sum of the bytes from cmd up to the one before
 *   sum) & 0xFF. A request with a bad sum or length gets no reply (the tool retries). The updater's frames (00 59 ...)
 *   are left for ota_service(), so a computer can always install a new firmware.
 *
 * Commands: 1 HELLO -> proto 1, LINK_STAGE (your app's protocol version), 0, 0, 0, 0, 0, 0, identity[8] (FELUCCA_ID).
 *   12..15 PDIR / PGET / PPUT / PDEL: the patch log over the link (define LINK_PATCHES and include patchlog.h first).
 *   Anything else goes to your handler: define LINK_APP(cmd, payload, len, out, &n) returning an rc (0 = ok) and the
 *   reply's length in n (out holds 256 B); without it, rc 1.
 * Return codes: 0 ok, 1 bad arguments, 4 flash write failed, 7 busy (not quiet), 9 no flash, 10 no such item, 11 full,
 *   12 damaged. A tool should retry rc 7 after a moment.
 *
 * Wiring it in (the app, before #include "platform.c"):
 *   static void link_service(void);
 *   #define APP_SERVICE link_service           (platform_poll calls it before the updater looks at a frame)
 * then after platform.c (and patchlog.h for LINK_PATCHES): #include "link.h". LINK_QUIET(): nothing sounds and no key
 * is down (a flash write stops the audio ~17 ms a sector); default fm1_in.notes == 0. Main loop only: no ISR work here.
 * The host tests: tests/lib_link_check.c drives it with fake frames. */
#pragma once
#ifndef LINK_ID1
#define LINK_ID1 'D'
#define LINK_ID2 'A'
#endif
#ifndef LINK_STAGE
#define LINK_STAGE 1u
#endif
#ifndef LINK_QUIET
#define LINK_QUIET() (fm1_in.notes == 0u)
#endif
enum { LINK_CMD_HELLO = 1 };
enum { LINK_OK = 0, LINK_E_ARGS = 1 };

static uint8_t link_msg[9 + 256 + 1], link_wire[2 + (9 + 256 + 1) * 8 / 7 + 2];

static void link_reply(uint32_t cmd, uint32_t rc, uint32_t seq, const uint8_t *pl, uint32_t n)   /* n <= 256 */
{
    uint32_t i, s = 0, w;
    link_msg[0] = 0x7D; link_msg[1] = LINK_ID1; link_msg[2] = LINK_ID2;
    link_msg[3] = (uint8_t)cmd; link_msg[4] = (uint8_t)rc; link_msg[5] = (uint8_t)seq;
    link_msg[6] = (uint8_t)n; link_msg[7] = (uint8_t)(n >> 8);
    for (i = 0; i < n; i++) link_msg[8 + i] = pl[i];
    for (i = 3; i < 8u + n; i++) s += link_msg[i];
    link_msg[8 + n] = (uint8_t)~s;
    link_wire[0] = 0xF0;
    w = 1u + ota_pack7(link_msg, 9u + n, link_wire + 1);
    link_wire[w++] = 0xF7;
    ota_wire_send(link_wire, w);
}

#ifdef LINK_PATCHES
/* ---- the patch log over the link. A patch body (PGET reply, PPUT payload after the id): name[12] (zero padded) | n | pw |
 * int16 v[n] | int16 pv[pw], little endian: a log record without its header and CRC. The values are not interpreted here.
 *   PDIR  start, count (1..16)  -> per id 16 B: id, present, n, pw, name[12]
 *   PGET  id                    -> the body (rc 10: none)
 *   PPUT  id, body              -> writes it (a new record replaces the id's); rc 11 store full; the same body again: no write
 *   PDEL  id                    -> deletes it (rc 0 also when there was none) and its companion
 * Ids PL_IDS..PL_SLOTS-1 are companions of ids 0..PL_IDS-1 (extra data that does not fit one record: TastyWaves keeps its
 * second sequencer there): a companion counts only when newer than its patch, so write the patch first. The app learns
 * which ids changed from pl_link_dirty (a bit per id) to refresh what it shows. */
enum { PLK_CMD_PDIR = 12, PLK_CMD_PGET = 13, PLK_CMD_PPUT = 14, PLK_CMD_PDEL = 15 };
enum { PLK_E_ARGS = 1, PLK_E_FLASH = 4, PLK_E_BUSY = 7, PLK_E_NOFLASH = 9, PLK_E_NONE = 10, PLK_E_FULL = 11, PLK_E_BAD = 12 };
static volatile uint32_t pl_link_dirty;

static uint32_t plk_rc(int rc)
{
    return rc == PL_OK ? 0u : rc == PL_E_FULL ? PLK_E_FULL : rc == PL_E_NONE ? PLK_E_NONE : rc == PL_E_BAD ? PLK_E_BAD : rc == PL_E_NOFLASH ? PLK_E_NOFLASH :
           rc == PL_E_ARGS ? PLK_E_ARGS : PLK_E_FLASH;
}

/* a: payload, len: its length, out: 256 B reply buffer, *n: reply length. Returns the rc. */
static uint32_t pl_link(uint32_t cmd, const uint8_t *a, uint32_t len, uint8_t *out, uint32_t *n)
{
    static pl_patch_t p;
    uint32_t i, k, id;
    *n = 0;
    if (!pl.ready || !flash_ok) return PLK_E_NOFLASH;
    if (cmd == PLK_CMD_PDIR) {
        if (len != 2u || a[0] >= PL_IDS || a[1] == 0u || a[1] > 16u || a[0] + a[1] > PL_IDS) return PLK_E_ARGS;
        for (id = a[0]; id < (uint32_t)a[0] + a[1]; id++) {
            int here = (int)(pl_present() >> id & 1u), rc = here ? pl_get(id, &p) : PL_E_NONE;
            out[(*n)++] = (uint8_t)id;
            out[(*n)++] = (uint8_t)here;                             /* a listed id whose record is damaged still counts as taken (n 0) */
            out[(*n)++] = rc == PL_OK ? p.n : 0u;
            out[(*n)++] = rc == PL_OK ? p.pw : 0u;
            for (i = 0; i < PL_NAME; i++) out[(*n)++] = rc == PL_OK ? (uint8_t)p.name[i] : 0u;
        }
        return 0;
    }
    if (cmd == PLK_CMD_PGET) {
        int rc;
        if (len != 1u || a[0] >= PL_SLOTS) return PLK_E_ARGS;
        if (a[0] >= PL_IDS && pl_seq(a[0]) <= pl_seq(a[0] - PL_IDS)) return PLK_E_NONE;     /* a companion older than its patch is not its */
        rc = pl_get(a[0], &p);
        if (rc) return plk_rc(rc);
        for (i = 0; i < PL_NAME; i++) out[(*n)++] = (uint8_t)p.name[i];
        out[(*n)++] = p.n;
        out[(*n)++] = p.pw;
        for (i = 0; i < p.n; i++) { out[(*n)++] = (uint8_t)p.v[i]; out[(*n)++] = (uint8_t)((uint16_t)p.v[i] >> 8); }
        for (i = 0; i < p.pw; i++) { out[(*n)++] = (uint8_t)p.pv[i]; out[(*n)++] = (uint8_t)((uint16_t)p.pv[i] >> 8); }
        return 0;
    }
    if ((cmd == PLK_CMD_PPUT || cmd == PLK_CMD_PDEL) && !LINK_QUIET()) return PLK_E_BUSY;
    if (cmd == PLK_CMD_PPUT) {
        uint32_t pn, pw, o;
        static pl_patch_t old;
        int rc;
        if (len < 1u + PL_NAME + 2u || a[0] >= PL_SLOTS) return PLK_E_ARGS;
        if (a[0] >= PL_IDS && !(pl_present() >> (a[0] - PL_IDS) & 1u)) return PLK_E_NONE;       /* a companion needs its patch */
        pn = a[1u + PL_NAME];
        pw = a[2u + PL_NAME];
        if (pn > PL_MAXP || pw > PL_MAXPW || len != 1u + PL_NAME + 2u + 2u * pn + 2u * pw) return PLK_E_ARGS;
        for (i = 0; i < PL_NAME; i++) {
            uint8_t c = a[1u + i];
            if (c && (c < 32u || c > 126u)) return PLK_E_ARGS;       /* names are printable ASCII, zero padded */
            p.name[i] = (char)c;
        }
        p.name[PL_NAME] = 0;
        p.n = (uint8_t)pn;
        p.pw = (uint8_t)pw;
        o = 3u + PL_NAME;
        for (k = 0; k < pn; k++, o += 2u) p.v[k] = (int16_t)(a[o] | a[o + 1u] << 8);
        for (k = 0; k < pw; k++, o += 2u) p.pv[k] = (int16_t)(a[o] | a[o + 1u] << 8);
        if ((a[0] < PL_IDS || pl_seq(a[0]) > pl_seq(a[0] - PL_IDS)) && pl_get(a[0], &old) == PL_OK && old.n == p.n && old.pw == p.pw && !memcmp(old.name, p.name, sizeof p.name) &&
            !memcmp(old.v, p.v, 2u * p.n) && !memcmp(old.pv, p.pv, 2u * p.pw)) return 0;       /* already there: no record (a retry costs nothing) */
        rc = pl_put(a[0], &p);
        if (rc == PL_OK) pl_link_dirty |= 1u << (a[0] % PL_IDS);
        return plk_rc(rc);
    }
    if (cmd == PLK_CMD_PDEL) {
        int rc;
        if (len != 1u || a[0] >= PL_SLOTS) return PLK_E_ARGS;
        rc = pl_del(a[0]);
        if (rc == PL_OK && a[0] < PL_IDS) (void)pl_del(PL_IDS + a[0]);
        if (rc == PL_OK) pl_link_dirty |= 1u << (a[0] % PL_IDS);   /* its companion too (pattern 2: panel.h); a failure there leaves an older companion, which never counts */
        return plk_rc(rc);
    }
    return PLK_E_ARGS;
}
#endif /* LINK_PATCHES */

static void link_handle(const uint8_t *r)        /* r: a checked request: r[3] cmd, r[4] seq, r[5..6] len, payload at r + 7 */
{
    uint32_t cmd = r[3], seq = r[4], len = ota_rd16(r + 5), n = 0, rc = LINK_E_ARGS, i;
    const uint8_t *a = r + 7;
    static uint8_t out[256];
    if (cmd == LINK_CMD_HELLO) {
        static const char ID[] = FELUCCA_ID;
        for (i = 0; i < 16u; i++) out[i] = 0;
        out[0] = 1;
        out[1] = (uint8_t)LINK_STAGE;
        for (i = 0; i < 8u && i < sizeof ID - 1u; i++) out[8 + i] = (uint8_t)ID[i];
        n = 16;
        rc = LINK_OK;
    }
#ifdef LINK_PATCHES
    else if (cmd >= PLK_CMD_PDIR && cmd <= PLK_CMD_PDEL)
        rc = pl_link(cmd, a, len, out, &n);
#endif
    else {
#ifdef LINK_APP
        rc = LINK_APP(cmd, a, len, out, &n);
        if (n > 256u) n = 0;
#endif
    }
    (void)a; (void)len;
    link_reply(cmd, rc, seq, out, n);
}

/* main loop (platform_poll, APP_SERVICE): takes a pending frame only if it is ours; the updater's stay */
static void link_service(void)
{
    const uint8_t *p;
    uint32_t n, d, s = 0, i;
    if (!ota_frame_get(&p, &n))
        return;
    d = ota_unpack7(p, n, ota_dec, sizeof ota_dec);
    if (d < 8u || d > sizeof ota_dec || ota_dec[0] != 0x7D || ota_dec[1] != LINK_ID1 || ota_dec[2] != LINK_ID2) {
        if (d >= 3u && ota_dec[0] == 0x7D && ota_dec[1] == LINK_ID1 && ota_dec[2] == LINK_ID2)
            ota_frame_done();                    /* ours but too short or too long: dropped */
        return;                                  /* not ours: ota_service() takes it */
    }
    ota_frame_done();
    if (ota_rd16(ota_dec + 5) + 8u != d)
        return;
    for (i = 3; i + 1u < d; i++)
        s += ota_dec[i];
    if ((uint8_t)~s != ota_dec[d - 1u])
        return;
    link_handle(ota_dec);
}
