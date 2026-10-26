#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run firmware/src/dinghy.c on this computer (tests/host.c): build/host/dinghy.wav and the screens as PNG.
#   tests/run_host.sh [script]               your own script (tests/host.c says what a line can do)
#   APP=name tests/run_host.sh [script]      a POC in apps/name/ (build/host/name.wav, name-*.png)
#   BOARD=smk37 tests/run_host.sh [script]   the SMK-37 build (velocity, pads, faders, wheels, outer keys)
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
A="${APP:-dinghy}"; N=$A; D=; SRC=firmware/src/dinghy.c
[ "$A" = dinghy ] || { SRC="apps/$A/$A.c"; D="-Iapps/$A"; [ -f "$SRC" ] || { echo "run_host.sh: no $SRC"; exit 1; }; }
if [ "${BOARD:-fm1}" = smk37 ]; then N=$A-smk37; D="$D -DBOARD_SMK37=1"; fi
cc -O2 -Wall -Wno-unused-function -DDINGHY_HOST $D -DAPP_VER="\"${APP_VER:-dev}\"" -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -Ifirmware/lib -o build/host/$N "$SRC"
build/host/$N build/host/$N ${1:+"$1"}
PY="${PYTHON:-$( [ -x .venv/bin/python ] && echo .venv/bin/python || echo python3 )}"
"$PY" - "$N" <<'PY'
import sys
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/host").glob(sys.argv[1] + "-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
    print("screen ->", p.with_suffix(".png"))
PY
