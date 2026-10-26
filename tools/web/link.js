// SPDX-License-Identifier: GPL-3.0-only
// firmware/lib/link.h's wire format for the browser (and node): request / reply over USB SysEx, and the patch commands
// (patchlog.h over the link). From TastyWaves' vwlink.js (github.com/marksweiss/tastywaves), made generic: the app's two
// id letters come from the caller. No DOM, no Web MIDI: a transport object is passed in
//   transport.send(Uint8Array)  -> Promise<boolean>        (false: could not send)
//   transport.recv(timeoutMs)   -> Promise<Uint8Array|null> (one whole F0..F7 message, or null on timeout)
//   transport.drain()           drop anything received so far

export const CMD = { HELLO: 1, PDIR: 12, PGET: 13, PPUT: 14, PDEL: 15 };
export const RC_TEXT = {
  1: 'bad arguments (or a command this firmware does not have)', 4: 'flash write failed', 7: 'busy: keys down or sound playing, try again',
  9: 'no flash', 10: 'no such patch', 11: 'the patch store is full (delete a patch first)', 12: 'that stored patch is damaged' };
export class LinkError extends Error {}

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) { acc |= b << nb; nb += 8; while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; } }
  if (nb) out.push(acc & 0x7F);
  return Uint8Array.from(out);
}
export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) { acc |= (b & 0x7F) << nb; nb += 7; while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; } }
  return Uint8Array.from(out);
}
const idBytes = (id) => [id.charCodeAt(0) & 0x7F, id.charCodeAt(1) & 0x7F];

export function frame(id, cmd, seq, payload = new Uint8Array(0)) {
  const body = new Uint8Array(8 + payload.length), [a, b] = idBytes(id);
  body.set([0x7D, a, b, cmd, seq, payload.length & 255, payload.length >> 8]);
  body.set(payload, 7);
  let s = 0; for (let i = 3; i < body.length - 1; i++) s += body[i];
  body[body.length - 1] = ~s & 0xFF;
  const p = pack7(body), out = new Uint8Array(p.length + 2);
  out[0] = 0xF0; out.set(p, 1); out[out.length - 1] = 0xF7;
  return out;
}
export function parse(id, pkt) {            // a reply -> {cmd, rc, seq, payload}, or null
  if (!pkt || pkt.length < 2 || pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const d = unpack7(pkt.subarray(1, pkt.length - 1)), [a, b] = idBytes(id);
  if (d.length < 9 || d[0] !== 0x7D || d[1] !== a || d[2] !== b || (d[6] | d[7] << 8) + 9 !== d.length) return null;
  let s = 0; for (let i = 3; i < d.length - 1; i++) s += d[i];
  if ((~s & 0xFF) !== d[d.length - 1]) return null;
  return { cmd: d[3], rc: d[4], seq: d[5], payload: d.subarray(8, d.length - 1) };
}

// ---- patch bodies: name[12] | n | pw | int16 v[n] | int16 pv[pw], little endian (patchlog.h's record without its header)
export const PATCH_NAME = 12, MAX_PARAMS = 80, MAX_WORDS = 33;
const latin1 = (b) => String.fromCharCode(...b).replace(/\0+$/, '');
const sameBytes = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const cat = (a, b) => { const o = new Uint8Array(a.length + b.length); o.set(a); o.set(b, a.length); return o; };
export function cleanName(name) { return name.replace(/[^\x20-\x7e]/g, '').slice(0, PATCH_NAME).replace(/\s+$/, '') || 'PATCH'; }
export function encodeBody({ name, params, words = [] }) {
  if (params.length > MAX_PARAMS || words.length > MAX_WORDS) throw new LinkError('patch too large');
  const b = new Uint8Array(PATCH_NAME + 2 + 2 * params.length + 2 * words.length), dv = new DataView(b.buffer), nm = cleanName(name);
  for (let i = 0; i < nm.length; i++) b[i] = nm.charCodeAt(i);
  b[PATCH_NAME] = params.length; b[PATCH_NAME + 1] = words.length;
  let o = PATCH_NAME + 2;
  for (const v of params) { dv.setInt16(o, v, true); o += 2; }
  for (const v of words) { dv.setInt16(o, v, true); o += 2; }
  return b;
}
export function decodeBody(b) {
  if (b.length < PATCH_NAME + 2) throw new LinkError('patch body too short');
  const n = b[PATCH_NAME], pw = b[PATCH_NAME + 1];
  if (n > MAX_PARAMS || pw > MAX_WORDS || b.length !== PATCH_NAME + 2 + 2 * n + 2 * pw) throw new LinkError('patch body has the wrong size');
  const dv = new DataView(b.buffer, b.byteOffset, b.length), params = [], words = [];
  let o = PATCH_NAME + 2;
  for (let i = 0; i < n; i++, o += 2) params.push(dv.getInt16(o, true));
  for (let i = 0; i < pw; i++, o += 2) words.push(dv.getInt16(o, true));
  return { name: latin1(b.subarray(0, PATCH_NAME)), params, words };
}

export class Link {
  // id: the app's two letters (link.h LINK_ID1 / LINK_ID2); ids: patch ids (PL_IDS); companions: patches have companions at ids + k
  constructor(transport, { id = 'DA', ids = 32, companions = false, tries = 4, timeout = 1500 } = {}) {
    Object.assign(this, { t: transport, id, ids, companions, tries, timeout, seq: 0 });
  }
  async request(cmd, payload = new Uint8Array(0), { timeout, tries } = {}) {
    this.seq = this.seq % 255 + 1;
    const pkt = frame(this.id, cmd, this.seq, payload);
    for (let k = 0; k < (tries || this.tries); k++) {
      this.t.drain();
      if (!await this.t.send(pkt)) throw new LinkError('could not send to the FM-1 (unplugged?)');
      const end = Date.now() + (timeout || this.timeout);
      for (let left = end - Date.now(); left > 0; left = end - Date.now()) {
        const r = parse(this.id, await this.t.recv(left));
        if (r && r.cmd === cmd && r.seq === this.seq) {
          if (r.rc) throw new LinkError('FM-1 answered: ' + (RC_TEXT[r.rc] || 'error ' + r.rc));
          return r.payload;
        }
      }
    }
    throw new LinkError('no answer from the FM-1 (cable pulled, switched off, or a firmware without this link?)');
  }
  async hello() {
    const d = await this.request(CMD.HELLO);
    if (d.length !== 16) throw new LinkError('unexpected HELLO reply');
    return { proto: d[0], stage: d[1], identity: latin1(d.subarray(8, 16)) };
  }
  async patchDirectory() {
    const out = [];
    for (let start = 0; start < this.ids; start += 16) {
      const n = Math.min(16, this.ids - start), d = await this.request(CMD.PDIR, Uint8Array.of(start, n));
      if (d.length !== 16 * n) throw new LinkError('unexpected PDIR reply');
      for (let i = 0; i < n; i++) { const e = d.subarray(16 * i, 16 * i + 16); out.push({ id: e[0], present: !!e[1], n: e[2], pw: e[3], name: latin1(e.subarray(4, 16)) }); }
    }
    return out;
  }
  async getBody(id) { return (await this.request(CMD.PGET, Uint8Array.of(id))).slice(); }
  async getCompanion(id) {
    if (!this.companions) return null;
    try { return await this.getBody(this.ids + id); } catch (e) { if (e instanceof LinkError && /no such patch/.test(e.message)) return null; throw e; }
  }
  async putBody(id, body) {                  // written and read back: throws unless the store holds exactly this
    decodeBody(body);
    await this.request(CMD.PPUT, cat(Uint8Array.of(id), body), { timeout: 5000 });
    if (!sameBytes(await this.getBody(id), body)) throw new LinkError('read-back of patch ' + (id + 1) + ' differs from what was sent');
  }
  async getPatch(id) { return { body: await this.getBody(id), companion: await this.getCompanion(id) }; }
  async putPatch(id, { body, companion = null }) {   // the patch first, then its companion (it counts only while newer)
    await this.putBody(id, body);
    if (!this.companions) return;
    if (companion) await this.putBody(this.ids + id, companion);
    else await this.request(CMD.PDEL, Uint8Array.of(this.ids + id), { timeout: 5000 });
  }
  async deletePatch(id) { await this.request(CMD.PDEL, Uint8Array.of(id), { timeout: 5000 }); }
  async renamePatch(id, name) { const p = await this.getPatch(id); await this.putPatch(id, { ...p, body: encodeBody({ ...decodeBody(p.body), name }) }); }
  async backup(format) {
    const d = await this.patchDirectory(), patches = [], hex = (b) => Array.from(b).map((v) => v.toString(16).padStart(2, '0')).join('');
    for (const e of d) if (e.present) {
      const { body, companion } = await this.getPatch(e.id);
      patches.push({ id: e.id, name: e.name, body: hex(body), ...(companion ? { companion: hex(companion) } : {}) });
    }
    return { format, version: 1, patches };
  }
}
export function parseBackup(obj, format, ids = 32) {
  if (!obj || obj.format !== format || obj.version !== 1 || !Array.isArray(obj.patches)) throw new LinkError(`not a ${format} backup`);
  const seen = new Set(), unhex = (s) => Uint8Array.from(s.match(/../g).map((h) => parseInt(h, 16)));
  return obj.patches.map((p) => {
    if (!Number.isInteger(p.id) || p.id < 0 || p.id >= ids || seen.has(p.id) || typeof p.body !== 'string' || !/^([0-9a-f]{2})+$/.test(p.body)) throw new LinkError('damaged entry in the backup');
    seen.add(p.id);
    const body = unhex(p.body), d = decodeBody(body);
    let companion = null;
    if (p.companion !== undefined) {
      if (typeof p.companion !== 'string' || !/^([0-9a-f]{2})+$/.test(p.companion)) throw new LinkError('damaged companion in the backup');
      companion = unhex(p.companion); decodeBody(companion);
    }
    return { id: p.id, name: d.name, body, companion };
  });
}
