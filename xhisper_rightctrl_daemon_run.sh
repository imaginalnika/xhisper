#!/bin/bash
# Runs the right-ctrl daemon using the local venv if present.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_PY="$SCRIPT_DIR/.venv/bin/python3"

if [ -x "$VENV_PY" ]; then
  exec "$VENV_PY" "$SCRIPT_DIR/xhisper_rightctrl_daemon.py"
else
  exec python3 "$SCRIPT_DIR/xhisper_rightctrl_daemon.py"
fi
