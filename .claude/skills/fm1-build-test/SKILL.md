---
name: fm1-build-test
description: Build an FM-1 app and test it on the computer - device build with the chip checks, host mode, the module tests, the full suite. Use after any change, or when asked to build, test, listen or screenshot.
---
# Build and test

1. `APP=<name> ./build.sh` (no APP: Dinghy) -> `build/<name>.fwsc`. macOS: Docker running. Read the `ok` lines: image
   N B of 581564, RAM, pool; any FAIL line (float, 64-bit division, ROM constant, register access outside hal/, size) is
   the chip refusing: docs/RULES.md.
2. Host: `APP=<name> sh tests/run_host.sh <script>` (skill fm1-host-mode); look at the screens, measure the WAV.
3. Modules: `sh tests/run_lib.sh` when firmware/lib changed (and it runs kitchensink).
4. Everything: `sh tests/run_tests.sh` after the builds it lists (the SMK-37 ones too): the update path, the loader, the
   installer.
A host pass does not prove the device build; a device build does not prove the sound: say which you ran.
