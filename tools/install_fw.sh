#!/bin/sh
# Install a firmware on the connected FM-1 over USB (asks before it writes).
#   tools/install_fw.sh [file.fwsc]     default build/$APP.fwsc (APP: default dinghy)
#   tools/install_fw.sh --info          what runs on the FM-1
set -e
cd "$(dirname "$0")/.."
PY=.venv/bin/python
[ -x "$PY" ] || { echo "install_fw.sh: no .venv (tools/install.sh)"; exit 1; }
if [ "$#" -eq 0 ]; then set -- "build/${APP:-dinghy}.fwsc"; fi
exec "$PY" tools/fm1_install.py "$@"
