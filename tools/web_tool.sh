#!/bin/sh
# Serve the browser tool (install the bundled release, the patches over the link) and open it. Chrome or Edge (Web MIDI).
#   tools/web_tool.sh [port]     default 8000; Ctrl-C stops it
set -e
cd "$(dirname "$0")/.."
PORT="${1:-8000}"
PY=.venv/bin/python; [ -x "$PY" ] || PY=python3
URL="http://localhost:$PORT"
echo "web tool at $URL (Chrome or Edge; Connect to MIDI and allow SysEx)"
( sleep 1; open "$URL" 2>/dev/null || xdg-open "$URL" 2>/dev/null || true ) &
exec "$PY" -I tools/web_serve.py "$PORT"     # (caching off: a reload always gets the current scripts)
