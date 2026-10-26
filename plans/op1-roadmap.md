# An OP-1-style instrument on the FM-1: the roadmap

Source: the Teenage Engineering OP-1 Operator's Manual v1.2 (2012). Goal: as much of the OP-1's feature set as the M-VAVE
FM-1 can carry, adapted to its 240 x 240 screen, 27 keys, 14 buttons and 7 encoders, built with dinghy-agentic's loop
(docs/WORKFLOW.md), one device-testable milestone at a time. This is a roadmap, not a spec: each milestone below becomes
`specs/<app>-mN.md` and `plans/<app>-mN.md` when it starts.

**Working name:** `pocketlab` (placeholder; see Open questions). The app re-creates the OP-1's *ideas and workflow*, not its
look: no Teenage Engineering artwork, names, logos or sounds; its own graphics in the FM-1's style. "OP-1" is TE's
trademark and appears only in docs as the inspiration.

## 1. The two machines

| | OP-1 (2011) | FM-1 (Dinghy target) | Consequence |
| --- | --- | --- | --- |
| CPU | 400 MHz DSP-class, 800 MMACS | JieLi AC79 (pi32v2), no FPU; one 5.8 ms audio block; TastyWaves: 12 voices x 2 wavetable osc + SVF ~50 %, + 4 fx ~76 % | a shared pool of ~8-12 voices, cheap engines, effects on a budget |
| RAM | 64 MB | 98 KB data + 336 KB pool | audio buffers in seconds, not minutes |
| Storage | 512 MB flash | 581 KB app + ~300 KB data area (8 x 16 KB slots, 8 blobs, settings) | no 6-minute tape; samples of a few seconds; compact patches |
| Audio in | mic, line in, FM radio | none on the Dinghy target (USB audio goes *to* the computer) | sampling only from the synth's own output (resampling) or from files sent over USB |
| Sensors | accelerometer | none | G-force LFO -> MIDI CC / encoder sources |
| Screen | 320 x 160 AMOLED, 60 fps | 240 x 240 LCD over SPI (~40 ms a full frame, 240 x 64 canvas) | animated pictures redraw a region at ~15-25 fps |
| Keys | 24 keys, 4 colour encoders, ~30 keys incl. SHIFT, T1-T4, 1-8 | 27 keys (F3-G5), KNOB1-4, SELECT, ALGORITHM, PRESETS, MASTER, 14 buttons, no SHIFT | a mapping (section 3) |
| I/O | USB MIDI, disk mode | USB-MIDI in/out, TRS MIDI in, USB audio out, the link (SysEx) to a browser | DISK mode -> the web tool |

FM1 Open (the other target) adds USB / Bluetooth audio *in*, sample storage (~20 s) and M-VAVE's own DX7 / sample
engines, but needs Windows to build, cannot use firmware/lib (GPL without a linking exception) and has its own engine
architecture. Recommendation: **Dinghy target** for this project (our own engines, host mode on the Mac, full source);
revisit FM1 Open only if live audio sampling becomes the priority.

## 2. The feature map

Legend: **Full** = as on the OP-1; **Adapted** = the same idea, changed to fit; **Reduced** = a smaller version; **No** = the
hardware cannot (and what stands in).

### Synthesizer mode (manual ch. 5, 17.1)
| OP-1 | Here | How |
| --- | --- | --- |
| 8 sound slots per mode, module pages T1-T4 (engine, envelope, effect, LFO) | Full | slots 1-8 via PRESETS + keys; pages EDIT / ENV / FX / LFO (section 3) |
| Swap one module (SHIFT+T1..T4) vs a whole sound (SHIFT+1..8) | Full | browser lists on hold of the page button |
| FM (4-operator, topology, freq, amount, detune) | Full | new `fm4.h`: 4 ops, ~8 algorithms, sine ops, integer phase; ~8 voices |
| Cluster (up to 6 oscillators, spread, unison) | Reduced | supersaw-style, up to 6 osc -> fewer voices when wide (voice cost scales) |
| Digital (wave, shaper, octave, detune/ring mod, "digitalness") | Full | phase-accumulator waves, wavefolder/shaper, ring mod, bit / sample-rate crush |
| Pulse (dual pulse train, filter, amplitude, modulation) | Full | two band-limited pulses (PolyBLEP), PWM, SVF from dsp.h |
| Phase (phase distortion: shift, amount, filter, tilt) | Full | Casio-style PD on a sine, cheap |
| String (waveguide string: tension, impulse, decay, detune) | Full | Karplus-Strong with fractional delay; ~3 KB a voice of RAM |
| Dr Wave (frequency-domain synthesis, 8-bit, filter, phase, chorus) | Adapted | additive spectra rendered into a wavetable at control rate (TastyWaves' oscillator), 8-bit character |
| Sampler (6 s, start, loop in/out, end, reverse, gain) | Reduced | samples of a few seconds in store slots (mu-law or 8-bit, 22 kHz) sent from the web tool, or resampled from the output (M11) |
| Envelope ADSR | Full | dsp.h |
| Play modes poly / mono / legato / unison, portamento | Full | dsp.h glide + voice allocation; unison stacks voices |
| Save a sound (hold slot 5 s), recall last saved (SHIFT+mode), undo | Full | patchlog.h; "recall" = reload the slot; a one-level undo copy in RAM |
| Dump a sound to tape and back (LIFT / DROP) | Adapted | a sound clipboard (LIFT copies the sound, DROP pastes into a slot or a tape position as an event) |

### Effects and LFOs (ch. 5.5-5.6, 15, 17.2)
| OP-1 | Here | How |
| --- | --- | --- |
| Delay, Spring (reverb), Grid (feedback plate) | Full / Adapted | fx.h delay and reverb; Grid = a reverb variant with three size knobs |
| Punch (hard low-pass + "power") | Full | SVF + soft clip / compressor stage, new fx type |
| Nitro (dual resonant filter, follow) | Full | two SVFs with an envelope follower, new fx type |
| Phone (telephone: band-pass, bit-crush, baud) | Full | band-pass + decimate + crush, new fx type |
| Per-sound effect + master effect | Adapted | one per-sound slot and one master slot (pool memory: one delay line, one reverb total) |
| LFO: Value, Tremolo, Random (all params of a module, with envelope), tempo-synced or free | Full | a modulation router: one LFO a sound, any parameter as destination |
| LFO: MIDI (CC as modulation) | Full | the router takes CC 1..4 sources |
| LFO: Crank, Bend (manual) | Adapted | SELECT encoder as the crank; pitch bend via MIDI |
| LFO: Element (G-force, mic, radio, line) | No | stand-ins: MIDI CC, aftertouch, the crank |

### Drum mode (ch. 6)
| OP-1 | Here | How |
| --- | --- | --- |
| DBOX (2-osc drum synth: pitch, wave, envelope, cross-mod, filter) | Full | cheap, no storage: the default drum engine, a kit = 27 keys of DBOX settings |
| Drum sampler (12 s, a sample split across keys: in/out, pitch, direction, loop/once, level) | Reduced | one kit sample of ~4-6 s (22 kHz 8-bit) per kit slot, the same key layout editor; samples via the web tool or resampling |
| Dynamic envelope | Full | attack / mid / release levels over a region |
| Standard key layout across kits | Full | the same layout convention, a factory DBOX kit |
| FM-radio drum kits, mic sampling | No | resample the synth instead (M11) |

### Sequencers (ch. 7)
| OP-1 | Here | How |
| --- | --- | --- |
| One sequencer each for synth and drum, four types | Full | arpseq.h's clock and pattern store, new sequencer kinds on it |
| Endless (99 steps, insert / long notes / rests / delete, direction, length, swing, pattern overlay, rotate, crank, transposition) | Full | step input as TastyWaves' REC; 99 steps (RAM), a pattern mask, the crank on SELECT |
| Pattern (16-step grid, live record, live edit, length, move section, direction, swing) | Full | a grid on the screen (TastyWaves' SEQ grid), REC for live record |
| Tombola (notes bounce in a spinning drum: speed, gravity, mass, open) | Full | a small 2D physics loop at control rate in the main loop; a fun picture; velocity from impact |
| Finger (a pattern per white key, two players, join / replace / fill) | Full | 16 white keys = 16 patterns, two lanes, three modes; animated players in our own style |
| Sequencing external gear | Full | notes go out over USB-MIDI |

### Tape, mixer, album (ch. 8, 9, 13.1)
| OP-1 | Here | How |
| --- | --- | --- |
| 4-track tape, 6 min audio, overdub, speed, reverse, scrub | **Adapted** | an **event tape**: 4 tracks recording the notes, sound changes and knob moves, played back live through the engines (no audio stored, so minutes fit in RAM; ~16 bytes an event). Speed and reverse act on the playback clock; pitch with speed optional |
| Lift / drop / split / join / slide takes, bars, beat match | Full (on events) | takes as event ranges; bars from the tempo |
| Tape tricks: loop in/out/toggle, break, reverse, chop, memo 1/2 | Full | on the event tape and the mixer; memo = two parameter snapshots |
| Real audio tape | Reduced | a **RAM looper** (M11): ~6-8 s of mono audio in the pool (22 kHz, 8-bit mu-law), lost at power-off; record the output, overdub, reverse, speed; resample into the sampler |
| Mixer: 4 track levels and pans, 3-band EQ, master effect, drive + release (a compressor) | Full | a mixer stage after the voices; the event tape's tracks route to it |
| Album (record the master to 2 sides of 6 min) | No / Adapted | USB audio already sends the master to the computer: record it in a DAW ("Album = your computer") |
| Signal path screen | Full | a picture of the path, warnings when a level is 0 |

### Tempo, help, connectivity (ch. 10-14)
| OP-1 | Here | How |
| --- | --- | --- |
| Tempo, tap tempo, metronome (with its own pitch), sync to external clock | Full | arpseq.h's clock + MIDI clock (have); tap on a button; a click voice |
| Free / beat match (tempo locked to tape speed) | Full | on the event tape |
| Help key, hold Help + key shows its name, battery | Full | hold HOME = help (have); help + key = the key's function; battery from the ADC |
| SHIFT on most keys | Adapted | a hold modifier (section 3) |
| COM: OP-1 mode / controller mode (knobs as CC, relative / absolute) / disk mode | Full / Adapted | controller mode sends CCs; disk mode -> the web tool (sounds, kits, patterns, tapes, samples over the link) |
| Ableton / Reason scripts | No | controller mode with generic CCs |
| Time / date tools | No | (no clock chip) |

**Coverage:** of ~70 user-facing features, roughly 45 full, 15 adapted or reduced, 10 impossible on this hardware (audio
input, radio, accelerometer, minutes of audio, time of day). The OP-1's *workflow* (modes, four knobs on four colours,
module pages, sequencer types, tape + tricks, mixer) carries over whole.

## 3. Mapping the controls

| OP-1 | FM-1 |
| --- | --- |
| Blue / green / white / orange encoders | KNOB1..4 (the four values on top of every page, in the same order, colour-coded on screen) |
| Synth / Drum / Tape / Mixer mode keys | HOME: tap steps SYNTH -> DRUM -> TAPE -> MIXER (the mode in the status line); hold HOME = help |
| T1-T4 (module pages / tracks / mixer pages) | EDIT, ENV, FX, LFO (in tape: tracks 1-4; in mixer: mix, EQ, master fx, master out) |
| SHIFT | hold **SEL** (the modifier; a dim "SHIFT" tag on screen while held), so SEL+key, SEL+knob = fine / second function |
| Sound keys 1-8 / tape tricks 1-8 | PRESETS turns the slot; in tape and mixer, SEL + the 8 lowest white keys = tricks |
| Sequencer key, SHIFT+seq = browser | SEQ (press: its page; hold: on / off, the TastyWaves convention); SEL + SEQ = sequencer type |
| REC, PLAY, STOP | REC, PLAY (PLAY again = stop; the transport convention) |
| Rewind / FF arrows, octave arrows | OCT- / OCT+ (octave); in tape: SEL + OCT = rewind / forward, ALGORITHM = scrub |
| Lift / Drop / Split | SAVE (short = drop, hold = lift), SEL + SAVE = split; saving a sound = hold PRESETS' slot via SEL + SAVE (spec decides) |
| Tempo key / Help / Mic / COM | GLO pages: tempo (tap = GLO tap), metronome, COM modes, settings |
| Volume knob | MASTER |

The final mapping is a spec decision (M0) and a question for the user; the principle is TastyWaves': the same gesture means
the same thing everywhere, and every page says what holds do.

## 4. Architecture

- One app `apps/pocketlab/`, split into headers by subsystem (the same one-translation-unit style): `engines/` (one header
  each: fm4, cluster, digital, pulse, phase, string, drwave, sampler, dbox, behind one engine interface: `init(params)`,
  `note_on/off`, `render(voice, buf, n)`, a per-voice cost), `modmatrix.h`, `seqkinds.h` (endless, pattern, tombola,
  finger on arpseq.h's clock), `tape.h` (event tape), `looper.h` (RAM audio), `mixer.h` (levels, pans, EQ, drive),
  `ui_*.h` (one picture per page).
- **New firmware/lib modules** where they are general (each with a host test, like the existing ones): `engine.h` (the voice
  engine interface and a shared voice pool), `fm4.h`, `ks.h` (string), `sampler.h` (compressed sample playback from flash),
  `eventtape.h`, `looper.h`, `mixer.h` (EQ + compressor), more `fx.h` types (punch, nitro, phone, plate).
- **Voices:** one shared pool (~12) across synth, drum, the sequencers and the 4 tape tracks; each engine reports its cost
  and the allocator refuses or steals by budget (the CPU meter on the status line keeps it honest).
- **Storage** (store.h slots, 8 x 16 KB): patchlog for sounds and kits (two slots: synth, drum), patterns and sequences
  (one slot), event tape (two slots: saved when stopped and quiet), samples (three slots ~48 KB: a few seconds), settings.
  A storage budget per milestone; STORE FULL handled everywhere.
- **Screen:** each mode keeps the layout (knob row, tabs, picture, status, name), the picture is where the OP-1's charm goes:
  animated engine pictures, the tombola, the finger players, the tape reels, VU meters, at a redraw budget (only the
  moving region; 15-25 fps).

## 5. Milestones

Each is one or more POC rounds through the toolkit (spec -> plan -> build -> host checks -> review -> device test), ends in
a device test plan section the user runs, and is committed separately. Estimates are rough (P = plan steps).

| # | Milestone | What the user can do at the end | Builds on / new | Risk |
| --- | --- | --- | --- | --- |
| M0 | **Shell** | the four modes, SEL as shift, module pages, the knob colours, help + key help, a splash, a placeholder engine | kitchensink, uikit | low |
| M1 | **Synth engines I**: Digital, Pulse, Phase, FM | play 4 engines, swap engine keeping the other modules, 8 sound slots, save / recall / undo | engine.h, fm4.h, dsp.h, patchlog | CPU of FM at 8+ voices |
| M2 | **Envelope + play modes** | ADSR picture, poly / mono / legato / unison, portamento | dsp.h | low |
| M3 | **Effects** | per-sound + master effect: delay, spring, plate, punch, nitro, phone | fx.h + 4 types | pool memory |
| M4 | **LFOs** | value, tremolo, random, MIDI CC, crank; tempo-synced or free | modmatrix.h | design of "any parameter" |
| M5 | **Synth engines II**: String, Cluster, Dr Wave, Sampler (from web uploads) | all synth engines | ks.h, wavetable from spectra, sampler.h, the web tool's upload | RAM for strings, sample storage |
| M6 | **Drum mode** | DBOX kits on 27 keys, the drum sampler with its layout editor, dynamic envelope | dbox, sampler.h | layout editor UX on 240x240 |
| M7 | **Sequencers** | endless, pattern, tombola, finger, for synth and drum, one playing at a time | arpseq.h + seqkinds.h | tombola / finger pictures and CPU |
| M8 | **Tempo** | tap tempo, metronome, sync, free / beat match | arpseq.h clock | low |
| M9 | **Event tape** | 4 tracks, record / overdub / play / reverse / speed, takes: lift, drop, split, join, slide, bars | eventtape.h | voice budget with 4 tracks playing |
| M10 | **Mixer** | levels, pans, EQ, master fx, drive + release, the signal path screen, tape tricks incl. memo | mixer.h | CPU (EQ + compressor on the master) |
| M11 | **RAM looper + resampling** | a few seconds of real audio: record the output, overdub, reverse, speed, drop it into the sampler | looper.h | pool memory shared with fx |
| M12 | **COM + web tool** | controller mode (CC knobs), and the browser: backup / restore sounds, kits, patterns, tapes, samples | link.h commands, tools/web | link bandwidth for samples |
| M13 | **Polish + release** | performance pass, animations, factory sounds and kits, identity, hub listing | release-engineer | — |

Suggested order: M0 -> M1 -> M2 -> M3 -> M7 (sequencers early: they are what makes it fun) -> M8 -> M6 -> M4 -> M5 ->
M9 -> M10 -> M11 -> M12 -> M13. M0-M3 alone already make a playable 4-engine synth with effects.

## 6. Budgets to watch (from TastyWaves' numbers)

- **CPU:** 12 voices of 2 wavetable osc + SVF were ~50 %; FM 4-op is ~3-4x a sine voice; KS strings are cheap; Cluster with
  6 osc is ~3 voices. Budget per mode, measured on the device each milestone (DSP_CPU on the status line).
- **Pool (336 KB):** fx delay 96 KB + reverb 24 KB + slots 20 KB; looper ~170 KB; strings ~25 KB; the screen canvas 30 KB.
  M11 may need a smaller delay line or a shared buffer.
- **Flash (581 KB app):** engines are code (small); avoid big tables (generate wavetables from spectra at run time).
  The data area holds the samples (only a few seconds).

## 7. How to run it with the toolkit

Each milestone starts with one prompt to the coordinator (CLAUDE.md), e.g.:

> M1 of plans/op1-roadmap.md: the pocketlab synth engines Digital, Pulse, Phase and FM, with 8 sound slots, engine swap and
> save / recall / undo, on the M0 shell.

The coordinator has the spec-writer turn the milestone into `specs/pocketlab-m1.md` (you approve it), the plan-writer
into `plans/pocketlab-m1.md`, then builds it with the implementer and the experts, checks it in host mode, has it
reviewed and gives you the device test section. The milestone's research questions (e.g. FM algorithms in integer DSP, a
cheap plate reverb) go to the researcher first.

## 8. Open questions for you

1. **Name** (the identity and splash follow): `pocketlab` is a placeholder.
2. **SHIFT:** hold SEL as the modifier (recommended), or GLO?
3. **Priorities:** the suggested order puts sequencers before the remaining engines; OK?
4. **Target:** Dinghy (recommended), or a parallel FM1 Open experiment for real audio sampling later?
5. **Scope of the tape:** the event tape (minutes of performance, no audio) plus a few-second RAM looper, as proposed?
