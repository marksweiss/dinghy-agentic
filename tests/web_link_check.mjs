// SPDX-License-Identifier: GPL-3.0-only
// tools/web/link.js against the real firmware/lib/link.h + patchlog.h (build/host/lib_link_serve): HELLO, the patch
// commands, rename, backup / restore, busy while a key is down. Run by tests/run_lib.sh: node tests/web_link_check.mjs
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { Link, LinkError, encodeBody, decodeBody, parseBackup } from '../tools/web/link.js';

let fails = 0;
const check = (ok, what) => { console.log((ok ? 'ok    ' : 'FAIL  ') + what); if (!ok) fails++; };
const srv = spawn('build/host/lib_link_serve', [], { stdio: ['pipe', 'pipe', 'inherit'] });
const lines = createInterface({ input: srv.stdout });
const q = []; let waiter = null;
lines.on('line', (l) => { const d = l === '-' ? null : Uint8Array.from(l.match(/../g).map((h) => parseInt(h, 16))); if (waiter) { const w = waiter; waiter = null; w(d); } else q.push(d); });
const transport = {
  async send(p) { srv.stdin.write(Array.from(p).map((v) => v.toString(16).padStart(2, '0')).join('') + '\n'); return true; },
  recv(ms) { if (q.length) return Promise.resolve(q.shift()); return new Promise((r) => { const t = setTimeout(() => { waiter = null; r(null); }, ms); waiter = (d) => { clearTimeout(t); r(d); }; }); },
  drain() { q.length = 0; },
};
const link = new Link(transport, { id: 'DA', ids: 32, tries: 1, timeout: 2000 });

const h = await link.hello();
check(h.proto === 1 && h.identity === 'FM-1_9XX', `HELLO (${h.identity})`);
const body = encodeBody({ name: 'Warm Pad', params: [90, 30, -5, 127], words: [3, 60, 64] });
await link.putPatch(4, { body });
let d = await link.patchDirectory();
check(d.length === 32 && d[4].present && d[4].name === 'Warm Pad' && d[4].n === 4 && d[4].pw === 3, 'put, then the directory lists it');
const back = decodeBody(await link.getBody(4));
check(back.params.join() === '90,30,-5,127' && back.words.join() === '3,60,64', 'get gives the same values');
await link.renamePatch(4, 'Cold Pad');
check((await link.patchDirectory())[4].name === 'Cold Pad', 'rename');
const bk = await link.backup('test-patches');
check(bk.patches.length === 1 && bk.patches[0].id === 4, 'backup');
await link.deletePatch(4);
check(!(await link.patchDirectory())[4].present, 'delete');
for (const p of parseBackup(JSON.parse(JSON.stringify(bk)), 'test-patches')) await link.putPatch(p.id, p);
check((await link.patchDirectory())[4].name === 'Cold Pad', 'restore from the backup');
srv.stdin.write('keys 1\n');
let busy = null; try { await link.deletePatch(4); } catch (e) { busy = e; }
check(busy instanceof LinkError && /busy/.test(busy.message), 'a key down: busy, and a clear message');
srv.stdin.write('keys 0\n');
let bad = null; try { parseBackup({ format: 'other', version: 1, patches: [] }, 'test-patches'); } catch (e) { bad = e; }
check(bad instanceof LinkError, 'a backup of another app is refused');
srv.stdin.end();
console.log(fails ? `${fails} FAILED` : 'all web link checks passed');
process.exit(fails ? 1 : 0);
