#!/bin/sh
# A new POC from apps/kitchensink (every firmware/lib module, working): apps/NAME/ with NAME.c, app.json, README.md,
# DEVICE-TEST-PLAN.md and tests/smoke.txt; it builds and runs at once. Then cut what the POC does not need.
#   tools/new_app.sh NAME "Title"        NAME: a-z 0-9, 2..20 chars (the directory, the file, the build's APP=)
set -e
cd "$(dirname "$0")/.."
N="${1:?usage: tools/new_app.sh NAME \"Title\"}"; T="${2:-$1}"
echo "$N" | grep -Eq '^[a-z][a-z0-9]{1,19}$' || { echo "new_app.sh: NAME: a-z 0-9, 2..20 characters, a letter first"; exit 1; }
[ ! -e "apps/$N" ] || { echo "new_app.sh: apps/$N exists"; exit 1; }
mkdir -p "apps/$N/tests"
U=$(echo "$N" | tr a-z A-Z | cut -c1-12)
sed -e "s/kitchensink/$N/g" -e "s/KITCHENSINK/$U/g" -e "s/\"KitchenSink\", \"every module, one synth\"/\"$T\", \"a proof of concept\"/" \
    apps/kitchensink/kitchensink.c > "apps/$N/$N.c"
L=$(echo "$U" | cut -c1-2)
cat > "apps/$N/app.json" <<J
{
  "name": "$U",
  "title": "$T",
  "identity": "FM-1_998",
  "data_family": "$(echo "$U" | tr -cd 'A-Z0-9' | cut -c1-7)",
  "colour": "#B48CFF",
  "link_id": "$L",
  "patch_ids": 32,
  "about": "FM-1_998 is a placeholder: before a release pick a free identity (tools/README.md)."
}
J
sed -i.bak -e "s/#define LINK_ID1 'D'/#define LINK_ID1 '$(echo "$L" | cut -c1)'/" -e "s/#define LINK_ID2 'A'/#define LINK_ID2 '$(echo "$L" | cut -c2)'/" "apps/$N/$N.c" && rm "apps/$N/$N.c.bak"
cat > "apps/$N/README.md" <<R
# $T

A proof of concept for the M-VAVE FM-1, built with dinghy-agentic (from apps/kitchensink). Spec: specs/$N.md, plan: plans/$N.md.

\`\`\`sh
APP=$N ./build.sh                                     # build/$N.fwsc
APP=$N sh tests/run_host.sh apps/$N/tests/smoke.txt   # host mode: build/host/$N.wav and screens
tools/install_fw.sh build/$N.fwsc                     # onto the FM-1
\`\`\`
R
cat > "apps/$N/DEVICE-TEST-PLAN.md" <<D
# $T: device test plan

Install: \`APP=$N ./build.sh\`, then \`tools/install_fw.sh build/$N.fwsc\`. Report results by row number.

## 1. First boot

| # | Do | Expect |
|---|----|--------|
| 1.1 | Switch on | the splash, then the HOME page with four knob values on top |
| 1.2 | Play a chord | it sounds; the voices light on the screen; V n/8 on the status line |
| 1.3 | Hold HOME | the help page; HOME again: back |
D
sed "s/kitchensink/$N/g" tests/scripts/kitchensink.txt > "apps/$N/tests/smoke.txt"
echo "apps/$N: build with APP=$N ./build.sh; host: APP=$N sh tests/run_host.sh apps/$N/tests/smoke.txt"
