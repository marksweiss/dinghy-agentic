#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# firmware/lib on the computer: each module's own test, then apps/kitchensink (every module together) in host mode:
# a session that saves a patch, and a second run on the same flash image (a power cycle) that must load it.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/host
CC="${CC:-cc} -O1 -Wall -Wno-unused-function -Ifirmware/lib"
fail=0
for t in dsp fx arpseq patchlog link; do
    SAN=; [ "$t" = fx ] || SAN="-fsanitize=address,undefined"
    $CC $SAN -o build/host/lib_${t}_check tests/lib_${t}_check.c -lm
    echo "== lib/$t"
    build/host/lib_${t}_check | tail -1 | grep -q "passed\|0 failures" || { build/host/lib_${t}_check | grep FAIL; fail=1; }
done
$CC -o build/host/lib_link_serve tests/lib_link_serve.c
if command -v node >/dev/null; then echo "== tools/web/link.js against link.h"; node tests/web_link_check.mjs | tail -1 | grep -q passed || { node tests/web_link_check.mjs | grep FAIL; fail=1; }; else echo "(no node: tools/web/link.js not checked)"; fi
echo "== apps/kitchensink"
F=build/host/kitchensink.flash; rm -f "$F"
APP=kitchensink DINGHY_FLASH=$F sh tests/run_host.sh tests/scripts/kitchensink.txt > build/host/ks1.log
APP=kitchensink DINGHY_FLASH=$F sh tests/run_host.sh tests/scripts/kitchensink2.txt > build/host/ks2.log
PY="${PYTHON:-$( [ -x .venv/bin/python ] && echo .venv/bin/python || echo python3 )}"
"$PY" - <<'PY' || fail=1
import re, struct, wave
from pathlib import Path
fails = 0
def check(ok, what):
    global fails
    print(("ok    " if ok else "FAIL  ") + what); fails += not ok
l1, l2 = Path("build/host/ks1.log").read_text(), Path("build/host/ks2.log").read_text()
w = wave.open("build/host/kitchensink.wav")   # (run 2's: silent; run 1's is checked through its log and screens)
check("patch 0 loaded: cutoff 60" in l2, "run 2 loads the patch run 1 saved (cutoff 60)")
check("patch 0 loaded" not in l1, "run 1 starts with no patch (an empty log)")
for n in ("fx", "bypass", "saved", "arp", "help"):
    check(Path(f"build/host/kitchensink-{n}.png").exists(), f"screen {n}")
check(l1.count("MIDI out 90") == 7, "the keys' notes go out over MIDI (two chords of 3 and a note)")
exit(fails)
PY
[ $fail = 0 ] && echo "all lib checks passed" || { echo "lib checks FAILED"; exit 1; }
