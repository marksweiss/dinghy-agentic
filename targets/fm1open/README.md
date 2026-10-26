# The second target: M-VAVE FM1 Open

[FM1 Open](https://github.com/m-vave/fm1_open) is M-VAVE's official firmware kit for the FM-1 (October 2026, after the
custom-firmware wave): the source of an 8-track groove instrument (sequencer, tracks, MIDI routing, screen, projects,
samples, a DX7 voice library), a **binary SDK** (the FM / sample engine, Bluetooth audio and MIDI, DIN / USB / BLE MIDI,
flash, the device), and a **browser workbench** that updates the FM-1 and previews the instrument from the same C sources
compiled to WebAssembly. `fetch.sh` downloads a pinned, checksum-verified kit into `kit/` (not committed).

## Dinghy or FM1 Open?

| | Dinghy target (`firmware/`, `apps/`) | FM1 Open target (`targets/fm1open/kit`) |
| --- | --- | --- |
| Licence | GPL-3.0-only, all source | the APP GPL-3.0 with a linking exception; the engine a binary, FM-1 only |
| Engine | yours (firmware/lib: voices, filter, envelopes, effects, arp / sequencers) | M-VAVE's DX7, 303, 808, PSG, pluck, sampler; or yours in the APP |
| Extras | USB audio in, TRS MIDI in, SMK-37 | Bluetooth audio + MIDI, DIN / USB / BLE MIDI out, samples, projects, 8 tracks |
| Audio | 44.1 kHz, 256 frames, Q15, `app_audio()` | 44.1 kHz, 128 frames, **Q23**, `fm1_engine_render` + the APP's `fm1_output_mix` |
| Build | `./build.sh` (Linux, or Docker on macOS) | **Windows x64** (PowerShell, JieLi Windows toolchain 2.5.3, pinned by hash) |
| Try it without the FM-1 | host mode: screens as PNG, sound as WAV, scripted | the browser workbench (WASM build of the APP) |
| Identity | yours: `FM-1_9NN` etc. (app.json) | yours: **`FM-1_8NNNNNN`** (its packager insists), `app/version.h` |

Choose FM1 Open when the POC wants M-VAVE's engines, samples, Bluetooth or an 8-track groove box; choose Dinghy for your
own engine, the Mac host loop and full source. **Never mix them**: Dinghy, Felucca, SLOOP, TastyWaves and firmware/lib are
GPL-3.0 without a linking exception, so their code must not be linked with the proprietary engine; copy only code you
wrote yourself into an FM1 Open firmware (you may license your own code as you like).

## Setting up

1. `targets/fm1open/fetch.sh` (any machine): the kit in `targets/fm1open/kit/`, the offline workbench beside it.
2. A Windows x64 machine or VM (on a Mac: UTM / Parallels / VMware with Windows 11 ARM runs x64 binaries): install
   [JieLi toolchain 2.5.3](https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe), set `FM1_PI32_TOOLCHAIN` to its
   `pi32` folder (normally `C:\JL\pi32`). The build checks the toolchain's 22 files against `sdk/manifest.json`; do not
   switch the checks off.
3. Build in PowerShell from `kit/`:
   - firmware only: `./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'` -> `build/dx7/generated/FM1_OFFICIAL_OPEN.fwsc`
   - browser preview: `.\tools\Build-Web.ps1 -LocalPreview` (downloads WASI SDK 27 the first time) -> `build/preview/index.html`
   - a distribution: set `FM1_SOURCE_URL` to the HTTPS archive of your exact source, then `.\build.bat --web` and
     `.\build.bat --package` (GPL: whoever gets the firmware gets the matching source).
4. Install: the workbench's update page, or (the standard FM-1 update protocol) `tools/install_fw.sh file.fwsc`; check with
   `tools/install_fw.sh --info`. The workbench can return the FM-1 to the original V15 too.

The Linux / Docker toolchain Dinghy uses is a different JieLi build than the one the kit pins: an FM1 Open build on macOS
without Windows is an open question (not supported here).

## Where a POC goes in

- **Your own sound:** `app/board/audio.c` is the audio interrupt (in the APP): it calls the binary `fm1_engine_render` (128
  frames, Q23) and then `fm1_output_mix` (`app/audio_output.c`), where the APP mixes USB and Bluetooth audio. Add your
  voices there, or render instead of the engine. Every function the interrupt reaches, and every table it reads (const
  ones too), must carry `FM1_AUDIO_CODE` / `FM1_AUDIO_DATA` (`.volatile_ram_code`): the flash may be programming while it
  runs. RAM is the budget, not flash.
- **The instrument:** `app/instrument.h`: 8 tracks (`GROOVE_TRACKS`), sources DX7 / 303 / 808 / PSG / pluck / sample, 64-step
  patterns with chords, gates, probability, ties, parameter locks, per-track tuning. Contexts are spelled out in each
  comment: the audio interrupt, the MIDI routing task, the control task (everything else; no blocking in the first two).
- **The screen and panel:** `app/screen.c` (the pages), `app/board/panel.c` + `fm1_input.h` (keys, buttons, encoders).
- **MIDI:** `app/midi_router.c` (`fm1_app_midi_received`, routing, clock); `fm1_midi_send_port` for DIN / USB / BLE.
- **Your SysEx:** `app/transfer.c` (`fm1_app_sysex`): the workbench's transfers; add your own commands beside them.
- **Storage:** `app/project.c`, `app/presets.c`, `app/samples.c` over `fm1_flash_map` / the SDK's flash API.

## Testing

Preview in the workbench first (the same C, in the browser), then on the FM-1. The hub's tests apply as for Dinghy apps
(tools/README.md): install from the official firmware, leave again to another firmware. Switching between an FM1 Open
firmware and a Dinghy one erases each other's projects and patches: back up first.
