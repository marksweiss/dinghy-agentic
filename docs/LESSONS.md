# Lessons from building TastyWaves

What it took to take a Dinghy fork to a released 1.0 synth (github.com/marksweiss/tastywaves: a 12-voice wavetable synth,
arp, two chord sequencers, effects, a patch store, a browser tool, listed on the FM-1 Firmware Hub). The agents and skills
here are built on these. Each line is something that cost time once.

## Working with the person who has the FM-1

- **They test on the device; you cannot.** Every feature ends with a section in the app's `docs/DEVICE-TEST-PLAN.md`
  (numbered rows: Do / Expect), and they report back by number ("23.7 fails: ..."). Fix exactly what the row says, add a
  row for the fix, ask them to retest that row.
- **Host mode first.** Everything you can check without the FM-1 you check in host mode before asking for a device test:
  scripted keys and knobs, screens as PNG (look at them), sound as WAV (measure it), a flash image across "power cycles".
- **Do only what was asked.** Do not change the order of operations or algorithms nobody asked about; a "while I'm here"
  rewrite of working DSP is a regression risk on a device you cannot run. Propose it instead.
- **Commit after each feature,** with a message that says what changed and why. Outward-facing steps (pushing, a public
  repo, a release, renaming, publishing a site, emailing the hub) wait for an explicit yes.
- **Ask when the answer changes what you build** (a control mapping, what PLAY means), with a recommendation; decide the rest.
  Their answers can redefine a feature mid-way (TastyWaves' SEQ / PLAY behaviour went through four versions): treat the
  latest answer as the spec and update the test plan.
- **Say what is untested.** "Built, host checks pass, not run on the FM-1" is a complete and honest status.

## The chip and the platform

- **Integers only.** No float, no 64-bit divide (64-bit multiply is fine). Curves (exp, tan, tanh) are tables computed
  offline (`tools/gen_dsp_tables.py`). A division by zero does not trap: guard divisors.
- **The audio ISR** (`app_audio`, 256 frames every 5.8 ms): no flash, no screen, no waiting. Control-rate work once per 32
  frames. Measure with `DSP_CPU_*` and show it on the status line; TastyWaves ran 12 voices x 2 wavetable oscillators + SVF at
  ~50 % and ~76 % with four effects. Never claim headroom you did not measure on the device.
- **Main loop <-> ISR:** a single-producer ring for events (`dsp_ring_t`), `volatile` fields with one writer each, set a
  pitch before its gate, and for arrays the ISR reads: set the length to 0 first, change, then set the length.
- **Flash:** an erase stops everything (audio too) ~17 ms a sector: save when quiet, never on every knob step. A patch store
  must survive a power cut mid-write: an append-only log with CRCs (`patchlog.h`); a cut leaves the old record or the new one.
- **The screen:** a canvas holds 240 x 64 at most: taller pictures go in strips. A full screen is ~115 KB of SPI (~40 ms): redraw
  only what changed. FONT_L has capitals only; for mixed case draw FONT_S at twice its size. The LCD's DMA disturbs the ADC:
  the platform no longer reads the MASTER knob during it.
- **Flash budget:** the app has 581,564 B. Wavetables are big (TastyWaves' 12 factory tables: ~430 KB); a code-drawn splash
  was 1.3 KB. Watch the "image N B of 581564" line every build.
- **Identity:** one `FM-1_` number per firmware, unique in https://thefirmwarehub.com/catalog.json, the same for every
  release. TastyWaves shipped one three other firmwares used and had to move.

## The panel: what users found intuitive

- The four knobs always set the four values at the top of the screen; a page button opens its page, pressing it again steps
  its tabs; a tag under the tab says which knob set ("SET 1/2").
- **Press = page, hold = switch** for anything with an on / off state (ARP, SEQ, FX bypass), the same on every button, with a
  status line on the page: "HOLD SEQ: ON", "PLAY: PLAYING". Asymmetric gestures between similar buttons confused the user.
- PLAY: the transport when a sequencer is lit, LATCH otherwise. Hold HOME for help: a global page, then the page's own.
- Show state where the user looks: a dot when the patch is edited, a flash for SAVED, the CPU on the status line.

## Testing pitfalls

- Host scripts: keep lines in time order (0.4's harness sorts them, older ones did not), and press buttons with `btndown` /
  `btnup` for holds. A test tone must keep its phase across helpers, or a delay line shows a jump that is not there.
- Feedback loops in integer DSP: round toward zero (`>> n` of a negative number never reaches 0: a limit cycle). Test that
  every effect falls silent after the input stops.
- Measure a displayed tempo in ms over several beats, not in audio blocks: a 5.8 ms grid makes 100 BPM read 99 / 100.
- Browsers cache scripts: serve the web tool with `Cache-Control: no-store` (`tools/web_serve.py`).
- macOS: the system Python has no Pillow (use `.venv`); headless Chrome will not go below ~500 px wide.
- A review of the whole diff before a release found real bugs both times (a MIDI start not resetting one lane's beat; a
  save half-failing without telling the UI). Do it.

## Shipping

- The browser installer (Felucca's `fm1ota.js`, vendored unchanged) plus GitHub Pages is what users actually use:
  `tools/web_release.sh`, a Pages workflow, optionally a custom domain (DNS records "DNS only" so GitHub can issue HTTPS).
- Installing another firmware erases yours' patches: say so, and give a backup / restore in the web tool.
- Licensing: GPL-3.0-only, a LICENSING.md that says who made which part (Felucca, SLOOP, Dinghy, your code), vendored files
  unchanged and credited; Dinghy's logo is not GPL and stays out.
- The hub listing is an email with tests done first (install from the official firmware, leave to another).
