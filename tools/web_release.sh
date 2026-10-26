#!/bin/sh
# Bundle an app's release build with the web tool (its Install button; GitHub Pages serves tools/web):
#   tools/web_release.sh APP X.Y.Z     build/APP-X.Y.Z.fwsc -> tools/web/firmware/APP.fwsc + release.json (from apps/APP/app.json)
set -e
cd "$(dirname "$0")/.."
A="${1:?usage: tools/web_release.sh APP X.Y.Z}"; V="${2:?usage: tools/web_release.sh APP X.Y.Z}"
F="build/$A-$V.fwsc"
[ -f "$F" ] || { echo "web_release.sh: no $F (APP=$A ./build.sh --release $V)"; exit 1; }
mkdir -p tools/web/firmware
cp "$F" "tools/web/firmware/$A.fwsc"
PY="${PYTHON:-$( [ -x .venv/bin/python ] && echo .venv/bin/python || echo python3 )}"
"$PY" -I - "$A" "$V" <<'PY'
import json, sys
from pathlib import Path
a, v = sys.argv[1], sys.argv[2]
meta = Path(f"apps/{a}/app.json")
cfg = json.loads(meta.read_text()) if meta.exists() else {"name": a.upper(), "identity": "FM-1_990"}
rel = {"app": a, "name": cfg.get("title", cfg["name"]), "version": v, "product": cfg["identity"], "pkg": f"firmware/{a}.fwsc",
       "linkId": cfg.get("link_id", "DA"), "patchIds": cfg.get("patch_ids", 0), "companions": cfg.get("companions", False),
       "backupFormat": cfg.get("backup_format", f"{a}-patches")}
Path("tools/web/firmware/release.json").write_text(json.dumps(rel, indent=2) + "\n")
print(json.dumps(rel))
PY
