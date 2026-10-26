---
name: fm1open-target
description: Work on the FM1 Open target (M-VAVE's official kit) - fetch it, choose it over Dinghy, the licence rule, the Windows build, the audio hook and contexts, the identity, testing in the browser workbench. Use when a POC targets FM1 Open.
---
# FM1 Open

- Fetch: `targets/fm1open/fetch.sh` -> `targets/fm1open/kit/` (pinned version, SHA-256 checked; not committed).
- Guide: `targets/fm1open/README.md` (when to choose it, where code goes). Agent: `fm1open-expert`.
- Licence: the APP is GPL-3.0 with a linking exception for the binary engine; Dinghy / TastyWaves / firmware/lib are GPL
  without one: never copy them in. Distribute with `FM1_SOURCE_URL` pointing at your exact source.
- Audio: `app/board/audio.c` -> `fm1_engine_render` (binary, 128 frames, Q23) -> `fm1_output_mix` (APP): your DSP goes there,
  marked FM1_AUDIO_CODE / FM1_AUDIO_DATA (RAM-resident, flash may be programming). Contexts: audio interrupt, MIDI routing
  task, control task; no blocking in the first two.
- Identity: `FM-1_8NNNNNN` in `app/version.h` (its packager checks the form); pick an unused one (the hub's catalogue).
- Build: Windows x64 only (JieLi 2.5.3, hash-checked): give the user the PowerShell commands; preview in the browser workbench.
