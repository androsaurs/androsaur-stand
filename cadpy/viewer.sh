#!/usr/bin/env bash
# Starts the OCP viewer and the file watcher; open the printed URL in a browser.
set -e
cd "$(dirname "$0")"
PORT="${1:-3939}"
.venv/bin/python -m ocp_viewer --port "$PORT" &
VIEWER=$!
trap 'kill $VIEWER 2>/dev/null' EXIT
sleep 3
.venv/bin/ocp123d -o "$PORT" view.py
