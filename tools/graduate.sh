#!/bin/sh
# Graduate a finished synth from apps/NAME into its own repository (TastyWaves' path): a clone of this toolkit with its
# full history (Dinghy / Felucca lineage included), trimmed to that app, the firmware/lib modules it uses and their tests;
# the app made the default build; a starting README / LICENSING entry; a "toolkit" remote for moving fixes both ways.
# Local only: nothing is pushed or created on GitHub (the release-engineer does that, with your yes).
#   tools/graduate.sh NAME [DEST]        DEST: default ../NAME (must not exist)
set -e
cd "$(dirname "$0")/.."
N="${1:?usage: tools/graduate.sh NAME [DEST]}"
DEST="${2:-../$N}"
[ -f "apps/$N/$N.c" ] && [ -f "apps/$N/app.json" ] || { echo "graduate.sh: no apps/$N/$N.c + app.json"; exit 1; }
[ ! -e "$DEST" ] || { echo "graduate.sh: $DEST exists"; exit 1; }
[ -z "$(git status --porcelain -- "apps/$N" firmware tools tests)" ] || { echo "graduate.sh: commit apps/$N and the toolkit first (the clone takes HEAD)"; exit 1; }
SRC=$(pwd); SHA=$(git rev-parse --short HEAD)
URL=$(git remote get-url origin 2>/dev/null || echo "$SRC")
git clone -q "$SRC" "$DEST"
cd "$DEST"
git remote remove origin
git remote add toolkit "$URL"                  # git fetch toolkit: the toolkit's fixes, to merge or cherry-pick

# ---- what the app uses
USED=$(grep -ho '#include "[a-z_]*\.h"' "apps/$N/$N.c" | sed 's/#include "\(.*\)\.h"/\1/' | sort -u)
uses() { echo "$USED" | grep -qx "$1"; }
for m in dsp fx arpseq patchlog link uikit splash; do
    if ! uses $m; then git rm -q "firmware/lib/$m.h"; rm -f "tests/lib_${m}_check.c"; fi
done
uses dsp || uses fx || uses splash || git rm -q --ignore-unmatch firmware/lib/dsp_tables.h tools/gen_dsp_tables.py
uses link || git rm -q --ignore-unmatch tests/lib_link_serve.c tests/web_link_check.mjs
uses uikit || git rm -q --ignore-unmatch firmware/gen/spleen_font.h

# ---- other apps, specs, plans, the second target, the toolkit's own plan
for d in apps/*/; do a=$(basename "$d"); [ "$a" = "$N" ] || git rm -rq "apps/$a"; done
for f in specs/*.md plans/*.md; do
    case "$(basename "$f")" in README.md|"$N".md|"$N"-*) ;; *) git rm -q "$f" ;; esac
done
git rm -rq --ignore-unmatch targets docs/AGENTIC-PLAN.md
git rm -q --ignore-unmatch tests/scripts/kitchensink.txt tests/scripts/kitchensink2.txt

# ---- the app is the default build: build.py, run_host.sh, install_fw.sh
sed -i.bak "s/app = a.app or os.environ.get(\"APP\") or \"dinghy\"/app = a.app or os.environ.get(\"APP\") or \"$N\"/" tools/build.py
sed -i.bak "s/A=\"\${APP:-dinghy}\"/A=\"\${APP:-$N}\"/" tests/run_host.sh
sed -i.bak "s/build\/\${APP:-dinghy}.fwsc/build\/\${APP:-$N}.fwsc/" tools/install_fw.sh
rm -f tools/build.py.bak tests/run_host.sh.bak tools/install_fw.sh.bak
grep -q "or \"$N\"" tools/build.py || { echo "graduate.sh: could not set the default app in tools/build.py"; exit 1; }

# ---- tests/run_lib.sh: the modules it kept, then the app's own smoke script
LIBS=$(for m in dsp fx arpseq patchlog link; do if uses $m; then printf '%s ' $m; fi; done)
python3 -I - "$N" "$LIBS" <<'PY'
import re, sys
from pathlib import Path
n, libs = sys.argv[1], sys.argv[2].split()
p = Path("tests/run_lib.sh"); s = p.read_text()
s = re.sub(r"for t in [a-z ]+; do", "for t in " + " ".join(libs or ["none"]) + "; do", s, count=1)
if not libs: s = s.replace('for t in none; do', 'for t in; do')
a = s.index('echo "== apps/kitchensink"'); b = s.index('[ $fail = 0 ]')
s = s[:a] + f'''echo "== apps/{n}"
if [ -f apps/{n}/tests/smoke.txt ]; then APP={n} sh tests/run_host.sh apps/{n}/tests/smoke.txt > build/host/{n}-smoke.log || fail=1; fi
''' + s[b:]
if "link" not in libs: s = re.sub(r"\$CC -o build/host/lib_link_serve.*\n.*web_link_check.*\n", "", s)
p.write_text(s)
t = Path("tests/run_tests.sh"); ts = t.read_text()
t.write_text(ts.replace("firmware/lib modules (dsp, fx, arpseq, patchlog, link) and apps/kitchensink (all of them in one app, a power cycle)",
                        f"firmware/lib modules this app uses and apps/{n}'s smoke script"))
PY

# ---- README, LICENSING, CLAUDE.md
TITLE=$(python3 -I -c "import json; c=json.load(open('apps/$N/app.json')); print(c.get('title', c['name']))")
git mv README.md docs/DINGHY-AGENTIC.md
{ echo "# $TITLE"; echo; [ -f "apps/$N/README.md" ] && sed '1,/^$/d' "apps/$N/README.md"; cat <<R

## Build and install

\`\`\`sh
tools/install.sh                 # once: the venv, JieLi's toolchain, the AC79 SDK files; a build and the host tests
./build.sh                       # build/$N.fwsc ($N is this repository's default app)
sh tests/run_host.sh apps/$N/tests/smoke.txt   # on your computer: the screens and the sound
tools/install_fw.sh              # onto the connected FM-1 (asks first)
\`\`\`

Releases, the browser installer and the FM-1 Firmware Hub: tools/README.md. The device test plan: apps/$N/DEVICE-TEST-PLAN.md.

## Credits and licence

GPL-3.0-only (LICENSE; LICENSING.md says who made which part). Graduated from
[dinghy-agentic](https://github.com/marksweiss/dinghy-agentic) ($SHA), a fork of [Dinghy](https://github.com/zvenson/dinghy)
(zvenson) on [Felucca](https://github.com/hugelton/Felucca) (Leo Kuroshita, Hügelton Instruments) and
[SLOOP](https://github.com/isod89/sloop-fm1) (isod89). "M-VAVE" and "FM-1" belong to their owners; this firmware is not
made or endorsed by them.
R
} > README.md
python3 -I - "$N" "$TITLE" <<'PY'
import sys
from pathlib import Path
n, title = sys.argv[1], sys.argv[2]
p = Path("LICENSING.md"); s = p.read_text()
s = s.replace("dinghy-agentic is free software", f"{title} is free software", 1)
s = s.replace("## Third-party material", f"""| {title}: `apps/{n}/` (the firmware), its specs, plans and tests | Copyright (C) 2026 its author(s); fill in |

## Third-party material""", 1)
p.write_text(s)
c = Path("CLAUDE.md"); cs = c.read_text()
c.write_text(f"""> This repository is **{title}** (`apps/{n}`, the default build), graduated from dinghy-agentic. The coordinator's job here:
> polish and release this one firmware (tools/README.md); fixes to firmware/lib or the platform also go back to the toolkit
> (`git fetch toolkit`; skill `graduate-app`).

""" + cs)
PY
git add -A
git commit -qm "$TITLE: graduated from dinghy-agentic $SHA (apps/$N, the modules it uses, the default build)"
echo "$DEST: $TITLE graduated (one commit on top of the toolkit's history; remote 'toolkit' = $URL)."
echo "Next: cd $DEST && tools/install.sh && ./build.sh && sh tests/run_lib.sh; then give it a real identity and a repo (tools/README.md)."
