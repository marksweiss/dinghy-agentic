#!/bin/sh
# Fetch M-VAVE's FM1 Open development kit (application source + binary SDK + workbench), pinned by version and SHA-256,
# into targets/fm1open/kit/ (not committed: M-VAVE's terms ride with it, see its LICENSE.md and THIRD_PARTY.md).
#   targets/fm1open/fetch.sh            the pinned version below
# A newer kit: change VER and the two hashes (from the release's SHA256SUMS-<VER>), and re-read its README for changes.
set -e
cd "$(dirname "$0")"
VER=V14
ZIP_SHA=6324230590b8e332bc544128abd7b919a2efd5982db60ea9a11663d4ea820344
WB_SHA=4b3197fdb23d9922a122b72fd11cdf9100215cc9c812de381b0888662d59e6c8
BASE=https://github.com/m-vave/fm1_open/releases/download/$VER
mkdir -p dl
for f in "FM1-Open-$VER-Development-Kit.zip:$ZIP_SHA" "FM1-Open-$VER-Workbench.html:$WB_SHA"; do
    n=${f%%:*}; h=${f##*:}
    [ -f "dl/$n" ] || curl -fL -o "dl/$n" "$BASE/$n"
    got=$(shasum -a 256 "dl/$n" | cut -d' ' -f1)
    [ "$got" = "$h" ] || { echo "fetch.sh: dl/$n: SHA-256 $got, want $h"; rm -f "dl/$n"; exit 1; }
done
rm -rf kit && mkdir kit
python3 -I - "dl/FM1-Open-$VER-Development-Kit.zip" kit <<'PY'      # (the zip is made on Windows: \ in its paths)
import sys, zipfile, os
z, out = zipfile.ZipFile(sys.argv[1]), os.path.realpath(sys.argv[2])
for i in z.infolist():
    p = os.path.realpath(os.path.join(out, i.filename.replace("\\", "/")))
    if not p.startswith(out + os.sep): raise SystemExit("unsafe path in the zip: " + i.filename)
    if i.filename.endswith(("/", "\\")): os.makedirs(p, exist_ok=True); continue
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with z.open(i) as src, open(p, "wb") as dst: dst.write(src.read())
PY
cp "dl/FM1-Open-$VER-Workbench.html" kit/
echo "FM1 Open $VER in targets/fm1open/kit (the offline workbench: kit/FM1-Open-$VER-Workbench.html)"
