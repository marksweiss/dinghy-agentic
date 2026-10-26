// SPDX-License-Identifier: GPL-3.0-only
// The page of tools/web/index.html: install the bundled release (Felucca's installer), and the patches over link.h.
// Everything app-specific comes from firmware/release.json (tools/web_release.sh writes it):
//   { app, name, version, product, pkg, linkId, patchIds, companions, backupFormat }
import { Link, LinkError, decodeBody, encodeBody, cleanName, parseBackup } from './link.js';
import { Updater } from './fm1ota.js';              // the FM-1 update protocol, from Felucca (web/fm1ota.js, unchanged)
import { productOf, logicalImage } from './fm1pkg.js';

const $ = (id) => document.getElementById(id);
const PORT_RE = /fm-1|felucca|ota|composite|sinco|usb-midi/i;
let access = null, link = null, rel = null, fwImage = null, installing = false, patches = [];
const status = (t, cls = 'mute') => { $('status').textContent = t; $('status').className = cls; };
const istat = (t, cls = 'mute') => { $('installStatus').textContent = t; $('installStatus').className = cls; };

class MidiTransport {
  constructor(out, inp) { this.out = out; this.q = []; this.waiter = null; inp.onmidimessage = (e) => { if (e.data[0] === 0xF0) this.push(e.data); }; }
  push(d) { if (this.waiter) { const w = this.waiter; this.waiter = null; clearTimeout(w.t); w.r(d); } else this.q.push(d); }
  async send(p) { try { this.out.send(p); return true; } catch (e) { return false; } }
  recv(ms) { if (this.q.length) return Promise.resolve(this.q.shift()); return new Promise((r) => { this.waiter = { r, t: setTimeout(() => { this.waiter = null; r(null); }, ms) }; }); }
  drain() { this.q.length = 0; }
}
async function midiAccess() {
  if (access) return access;
  if (!navigator.requestMIDIAccess) throw new Error('This browser has no Web MIDI. Use Chrome or Edge.');
  access = await navigator.requestMIDIAccess({ sysex: true });
  return access;
}

// ---- the bundled release
(async () => {
  try {
    const r = await fetch('firmware/release.json', { cache: 'no-store' });
    if (!r.ok) throw new Error('no release bundled (tools/web_release.sh)');
    rel = await r.json();
    document.title = `${rel.name} firmware tool`;
    $('title').textContent = `${rel.name}`;
    const pkg = new Uint8Array(await (await fetch(rel.pkg, { cache: 'no-store' })).arrayBuffer());
    if (productOf(pkg) !== rel.product) throw new Error('the bundled package is damaged');
    fwImage = logicalImage(pkg);
    $('fwName').textContent = rel.name; $('fwVer').textContent = rel.version;
    Object.assign($('fwDownload'), { href: rel.pkg, download: `${rel.app}-${rel.version}.fwsc` }).classList.remove('hide');
    $('btnInstall').disabled = !navigator.requestMIDIAccess;
  } catch (e) { $('installCard').classList.add('hide'); }
})();
window.addEventListener('beforeunload', (e) => { if (installing) { e.preventDefault(); e.returnValue = ''; } });
$('btnInstall').onclick = async () => {
  if (installing || !fwImage || !confirm(`Install ${rel.name} ${rel.version} on the connected FM-1?`)) return;
  let midi;
  try { midi = await midiAccess(); } catch (e) { istat(e.message, 'bad'); return; }
  if (link) { link = null; $('patchSec').classList.add('hide'); status('Disconnected for the install; connect again afterwards.'); }
  installing = true; $('btnInstall').disabled = true; $('installBar').classList.remove('hide'); $('installBar').value = 0;
  const total = 74 + 1167;
  const step = (k, a) => {
    if (k === 'start') istat('Starting the update…');
    if (k === 'verify') { istat('Checking the package…'); $('installBar').value = (a / total) * 100; }
    if (k === 'loader') istat('Waiting for the update loader…');
    if (k === 'write') { istat('Writing… do not unplug'); $('installBar').value = ((74 + a) / total) * 100; }
    if (k === 'reboot') istat('Restarting…');
    if (k === 'done') { $('installBar').value = 100; istat(`Installed: ${rel.name} ${rel.version} is running.`, 'ok'); }
  };
  try {
    const up = new Updater(midi, () => {});
    const ota = await up.find((id) => /^ota-/i.test(id.model));      // an install cut off earlier: finish it
    if (ota) { ota.link.close(); if (!(await up.resume(fwImage, step, rel.product))) istat('The FM-1 stopped answering. Replug it and press Install again.', 'bad'); }
    else await up.install(fwImage, rel.product, step);
  } catch (e) { istat((e && e.message) || String(e), 'bad'); }
  installing = false; $('btnInstall').disabled = false;
};

// ---- the device: connect, HELLO, patches
$('btnAccess').onclick = async () => {
  try { await midiAccess(); } catch (e) { status(e.message, 'bad'); return; }
  const fill = () => {
    const outs = [...access.outputs.values()], sel = $('port');
    sel.replaceChildren(...outs.map((o) => new Option(o.name, o.id)));
    const want = outs.find((o) => PORT_RE.test(o.name)); if (want) sel.value = want.id;
  };
  access.onstatechange = fill; fill();
  $('port').classList.remove('hide'); $('btnConnect').classList.remove('hide');
  status('Pick the FM-1 port and press Connect.');
};
$('btnConnect').onclick = async () => {
  const out = access.outputs.get($('port').value), inp = out && [...access.inputs.values()].find((i) => i.name === out.name);
  if (!inp) { status('No matching MIDI input for that port.', 'bad'); return; }
  const r = rel || {};
  link = new Link(new MidiTransport(out, inp), { id: r.linkId || 'DA', ids: r.patchIds || 32, companions: !!r.companions });
  try {
    const h = await link.hello();
    status(`Connected: ${h.identity}, link stage ${h.stage}.` + (r.product && h.identity !== r.product ? ` (This page's release is ${r.product}.)` : ''));
    if (r.patchIds) await refreshPatches(); else $('patchSec').classList.add('hide');
  } catch (e) { link = null; status(e.message, 'bad'); }
};
async function guarded(what, f) { try { await f(); } catch (e) { status(`${what}: ${e.message}`, 'bad'); } }
async function refreshPatches() {
  const d = await link.patchDirectory();
  patches = d.filter((e) => e.present);
  $('patchSec').classList.remove('hide');
  $('patches').replaceChildren(...(patches.length ? patches.map((p) => {
    const row = document.createElement('div'); row.className = 'card prow';
    row.innerHTML = '<span class="mute"></span><span class="pname"></span><span class="mute"></span><span class="row"></span>';
    row.children[0].textContent = String(p.id + 1).padStart(2, '0');
    row.children[1].textContent = p.name;
    row.children[2].textContent = `${p.n} values` + (p.pw ? ` · ${p.pw} words` : '');
    const mk = (t, f) => Object.assign(document.createElement('button'), { textContent: t, onclick: f });
    row.children[3].append(mk('Rename…', () => { const n = prompt('New name (up to 12 characters)', p.name); if (n !== null) guarded('Rename', async () => { await link.renamePatch(p.id, cleanName(n)); await refreshPatches(); }); }),
      mk('Delete', () => { if (confirm(`Delete patch ${p.id + 1} "${p.name}"? Back up first if you may want it again.`)) guarded('Delete', async () => { await link.deletePatch(p.id); await refreshPatches(); }); }));
    return row;
  }) : [Object.assign(document.createElement('div'), { className: 'mute', textContent: 'No patches saved yet.' })]));
  $('pcount').textContent = `${patches.length} of ${link.ids} saved`;
}
$('btnBackup').onclick = () => guarded('Backup', async () => {
  const bk = await link.backup(rel.backupFormat || `${rel.app}-patches`);
  const a = Object.assign(document.createElement('a'), { href: URL.createObjectURL(new Blob([JSON.stringify(bk)], { type: 'application/json' })), download: `${rel.app}-patches-${new Date().toISOString().slice(0, 10)}.json` });
  a.click();
  status(`Backed up ${bk.patches.length} patches.`, 'ok');
});
$('btnRestore').onclick = () => $('restoreFile').click();
$('restoreFile').onchange = () => guarded('Restore', async () => {
  const f = $('restoreFile').files[0]; if (!f) return;
  const list = parseBackup(JSON.parse(await f.text()), rel.backupFormat || `${rel.app}-patches`, link.ids);
  if (!confirm(`Restore ${list.length} patches? Each goes back under its own number (replacing what is there); the others stay.`)) return;
  for (const p of list) await link.putPatch(p.id, p);
  await refreshPatches();
  status(`Restored ${list.length} patches.`, 'ok');
  $('restoreFile').value = '';
});
void decodeBody; void encodeBody; void LinkError;
