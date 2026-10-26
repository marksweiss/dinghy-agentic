---
name: new-poc
description: Start a new synth proof of concept in apps/<name> from apps/kitchensink - scaffold, build, host-run, then cut it down to the spec. Use when a plan's first step says to start a POC, or the user asks for a new firmware.
---
# Start a POC

1. `tools/new_app.sh <name> "<Title>"` -> `apps/<name>/` (`<name>.c` = kitchensink renamed, `app.json` with the placeholder
   identity FM-1_998 and link letters, `README.md`, `DEVICE-TEST-PLAN.md`, `tests/smoke.txt`).
2. `APP=<name> ./build.sh` (device build, checks) and `APP=<name> sh tests/run_host.sh apps/<name>/tests/smoke.txt` (host).
3. Cut what the spec does not use, top to bottom: kitchensink's sections are marked (params, voices, audio, patches, screen,
   main loop). Remove a module's #include only after its last use is gone; keep `FONTS_SPLEEN` if uikit.h stays.
4. Rename the parameters, pages and help lines to the spec's. Keep the gesture and layout conventions (docs/LESSONS.md).
5. Before a release: a real identity (tools/README.md), `colour`, `title` in app.json.

The pieces and their contracts: skill `synth-modules`, `docs/MODULES.md`.
