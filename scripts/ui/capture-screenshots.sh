#!/usr/bin/env bash
# STAR UI screenshot wrapper
#
# Boots the Vite dev server (if not already running on port 5173), waits until
# it responds, runs the puppeteer capture script, then tears the dev server
# back down.
#
# Usage:
#   scripts/ui/capture-screenshots.sh

set -eu

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
UI_DIR="$REPO_ROOT/star-ui"
OUT_DIR="$REPO_ROOT/docs/ui-screenshots"
CAP_SCRIPT="$REPO_ROOT/scripts/ui/capture-screenshots.js"
CAP_WORKDIR="${CAP_WORKDIR:-$REPO_ROOT/.cache/ui-capture}"
URL="${URL:-http://localhost:5173/}"
PORT="${PORT:-5173}"

mkdir -p "$OUT_DIR"
mkdir -p "$CAP_WORKDIR"

# Install puppeteer-core in a side directory so we don't pollute star-ui's deps.
if [ ! -d "$CAP_WORKDIR/node_modules/puppeteer-core" ]; then
    echo "[capture] installing puppeteer-core into $CAP_WORKDIR"
    (cd "$CAP_WORKDIR" && npm init -y >/dev/null 2>&1 || true)
    (cd "$CAP_WORKDIR" && npm install --no-audit --no-fund puppeteer-core >/dev/null)
fi

# Start dev server if nothing is listening on the port.
DEV_PID=""
if ! curl -fsS "$URL" >/dev/null 2>&1; then
    echo "[capture] starting vite dev server"
    (cd "$UI_DIR" && nohup npm run dev > "$CAP_WORKDIR/vite-dev.log" 2>&1 & echo $! > "$CAP_WORKDIR/vite.pid")
    DEV_PID="$(cat "$CAP_WORKDIR/vite.pid")"
    # Wait up to 30s for the server to come up.
    for i in $(seq 1 30); do
        if curl -fsS "$URL" >/dev/null 2>&1; then break; fi
        sleep 1
    done
    if ! curl -fsS "$URL" >/dev/null 2>&1; then
        echo "[capture] dev server failed to come up; see $CAP_WORKDIR/vite-dev.log"
        kill "$DEV_PID" 2>/dev/null || true
        exit 1
    fi
fi

cleanup() {
    if [ -n "$DEV_PID" ]; then
        echo "[capture] stopping vite dev server (pid $DEV_PID)"
        kill "$DEV_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# Run capture script using the puppeteer-core install in CAP_WORKDIR.
NODE_PATH="$CAP_WORKDIR/node_modules" node "$CAP_SCRIPT" --url "$URL" --out "$OUT_DIR"
