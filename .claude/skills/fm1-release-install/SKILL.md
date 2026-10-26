---
name: fm1-release-install
description: Install a build on the user's FM-1, check what runs on it, and cut a versioned release - identity, release build, web tool bundle, tag and GitHub release, the hub. Use when asked to flash, install, release or check the device. Hardware writes and publishing need the user's yes.
---
# Install and release

Install (the user's FM-1; they confirm the write):
- `tools/install_fw.sh build/<app>.fwsc` (it shows what runs, asks, writes, checks the identity after the restart);
  `tools/install_fw.sh --info` alone shows what runs. Or the web tool (`tools/web_tool.sh` after `tools/web_release.sh`).
- Broken? OCT- at power-on (rescue), install again; the FM-1 Firmware Hub installs the official firmware.

Release (tools/README.md has the full checklist):
1. A unique identity in app.json (https://thefirmwarehub.com/catalog.json), the same for every release.
2. Tested on the device from this commit. `APP=<app> ./build.sh --release X.Y.Z`.
3. `tools/web_release.sh <app> X.Y.Z`; commit; push (with the user's yes); tag; `gh release create` with the .fwsc.
4. Byte-identity: the build, the release asset, the web copy (`shasum -a 256`).
5. The hub: skill `hub-submission`.
