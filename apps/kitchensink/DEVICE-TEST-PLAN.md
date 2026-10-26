# KitchenSink: device test plan

KitchenSink exercises every firmware/lib module and the platform hooks dinghy-agentic added to Dinghy 0.4; passing it on
a real FM-1 is what makes the modules safe to build on.

Install: `APP=kitchensink ./build.sh`, then `tools/install_fw.sh build/kitchensink.fwsc` (or the web tool:
`APP=kitchensink ./build.sh --release 0.1.0 && tools/web_release.sh kitchensink 0.1.0 && tools/web_tool.sh`).
Report results by row number.

## 1. Boot, sound, screen

| # | Do | Expect |
|---|----|--------|
| 1.1 | Switch on | the splash (~3 s: waves crash, "KitchenSink" rises), then HOME: CUTOFF RESO ATTACK RELEASE on top, three tabs, eight voice bars |
| 1.2 | Play a chord, then single notes up the keyboard | two detuned saws through a low-pass, clean, no clicks; a voice bar lights per note; V n/8 on the status line; the notes go out over USB-MIDI |
| 1.3 | Turn KNOB1..4 on HOME | cutoff, resonance, attack, release change the sound at once; the turned knob's value lights |
| 1.4 | Turn MASTER while a chord sounds, and while the screen redraws (turn a knob at the same time) | the volume follows smoothly, no jumps (the platform no longer reads the knob during the screen's DMA) |
| 1.5 | Play 9+ notes | the oldest voice is stolen, no hang; the CPU % on the status line stays under ~80 |

## 2. Effects (fx.h)

| # | Do | Expect |
|---|----|--------|
| 2.1 | Press FX, turn KNOB1 through OFF CHOR FLNG PHAS DLY REV, playing | each effect audible; KNOB2 / KNOB3 change it; MIX (KNOB4) from dry to wet |
| 2.2 | DLY or REV, play staccato, stop | the tail dies away to silence (no hum, no click at the end) |
| 2.3 | Hold FX half a second, then again | "FX BYPASS" then "FX ON"; the effect fades out and back in without a click; a ringing tail continues underneath |

## 3. Arpeggiator (arpseq.h)

| # | Do | Expect |
|---|----|--------|
| 3.1 | Hold ARP half a second | ARP lights; the ARP page says HOLD ARP: ON |
| 3.2 | Hold a chord; turn MODE, RATE, OCT, GATE | the arpeggio follows each change |
| 3.3 | Send MIDI clock from a DAW and press Start, or run `tools/send_clock.py 100 20` (100 BPM for 20 s, with Start) | the ARP page shows TEMPO 100 BPM  MIDI CLOCK (INTERNAL and 120 when no clock comes); the arp follows the tempo; Start restarts it on the beat |

## 4. Patches (patchlog.h) and the link (link.h)

| # | Do | Expect |
|---|----|--------|
| 4.1 | Set a distinctive sound, press SAVE | "SAVED", the title becomes SINK 1 |
| 4.2 | Switch off and on | the saved sound comes back (SINK 1) |
| 4.3 | Web tool: Connect | "Connected: FM-1_998, link stage 1"; the patch list shows SINK 1 |
| 4.4 | Web tool: Backup all, Rename, Delete, then Restore from the file | each works; the FM-1 still plays; after Restore the patch is back |
| 4.5 | During 4.4 hold a key while pressing Delete | "busy: keys down or sound playing, try again" |

## 5. The update path (must never break)

| # | Do | Expect |
|---|----|--------|
| 5.1 | Install Dinghy (`./build.sh`, `tools/install_fw.sh build/dinghy.fwsc`), then KitchenSink again | both install; the link's SysEx never gets in the way of the updater |
| 5.2 | Hold OCT- while switching on | the USB rescue; an install works from there |
