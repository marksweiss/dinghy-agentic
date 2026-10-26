# firmware/lib: the modules

Building blocks for synth firmwares on the Dinghy target, taken from TastyWaves (github.com/marksweiss/tastywaves) and made
independent of its patch and pages. Each is one header with its contract at the top; each has a host test; all of them
run together in `apps/kitchensink` (host and device). `sh tests/run_lib.sh` checks them all.

| Module | What it gives | Costs | Test |
| --- | --- | --- | --- |
| `dsp.h` (+ `dsp_tables.h`) | pitch (note, 1/256 semitone), sine, noise, LFO shapes, ADSR (Q30, 0.5 ms .. 10 s), a trapezoidal state-variable filter (LP / BP / HP, cutoff as a note), glide, soft clip, voice stealing, the main -> ISR event ring, the CPU meter | tables ~7 KB flash | `tests/lib_dsp_check.c` |
| `fx.h` | 4 effect slots in series: chorus, flanger, phaser, ping-pong delay (0.55 s), reverb; a click-free bypass that keeps tails ringing | ~20 KB pool + 96 KB (delay) + 24 KB (reverb); CPU about a voice a slot | `tests/lib_fx_check.c` |
| `arpseq.h` | an arpeggiator (7 modes, 1/4..1/32 with triplets, 1-4 octaves, gate, swing, latch-friendly) and two 32-step sequencers on one clock, with chords (fixed shapes or the held keys), per-pattern arp and level, a modulation lane, MIDI clock | ~2 KB RAM | `tests/lib_arpseq_check.c` |
| `patchlog.h` | a patch store that survives power cuts: an append-only log of CRC'd records in one store slot; 32 ids + 32 companions, <= 80 values + 33 words a record, compaction | one slot (16 KB flash), ~5 KB RAM | `tests/lib_patchlog_check.c` (power-cut and fault injection) |
| `link.h` | a request / reply link to a computer over USB SysEx, beside the updater: HELLO, your own commands, the patch commands (list, get, put, delete) | ~1 KB RAM | `tests/lib_link_check.c`, `tests/web_link_check.mjs` (with `tools/web/link.js`) |
| `uikit.h` | the screen's pieces: knob row, tabs, status line, title with a save flash, help pages, hold hints, strips for tall pictures, a wave plot; button gestures (press vs hold), timed messages | needs FONTS_SPLEEN for FONT_M | kitchensink's screens |
| `splash.h` | a power-on animation for any name and tagline: waves crash, the name rises from the spray (~3 s, drawn by code) | ~1.3 KB flash | look at it in host mode (DINGHY_SPLASH=1) |

How they fit: skill `synth-modules` (include order, hooks, ISR vs main) and `apps/kitchensink/kitchensink.c`.

Rules for changing one: keep its contract (the header comment), add a check to its test that fails without the change,
run `sh tests/run_lib.sh`, and note it in the module's header. A behaviour TastyWaves depended on is a contract.
