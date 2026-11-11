#!/usr/bin/env bash
# Starts the replaying OCP viewer and the file watcher.
set -euo pipefail
cd "$(dirname "$0")"
PORT="${1:-3939}"
.venv/bin/python -c 'import socket, sys; socket.create_server(("127.0.0.1", int(sys.argv[1]))).close()' "$PORT"
.venv/bin/python viewer_server.py --port "$PORT" &
VIEWER=$!
trap 'kill "$VIEWER" 2>/dev/null || true' EXIT
for attempt in {1..100}; do
    kill -0 "$VIEWER" 2>/dev/null || exit 1
    if curl --silent --fail "http://127.0.0.1:$PORT/health" >/dev/null; then
        break
    fi
    sleep 0.1
done
curl --silent --fail "http://127.0.0.1:$PORT/health" >/dev/null
.venv/bin/ocp123d -o "$PORT" view.py
