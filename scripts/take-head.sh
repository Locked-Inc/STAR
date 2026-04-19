#!/usr/bin/env bash
# STAR: take-head -- shut the desktop down, return the Pi5 to pure
# console. The headless STAR runtime is unaffected; compliance +
# gateway + SPI bridge keep running.
#
# Frees ~1 GB RAM and 1-2 CPU cores that gdm + gnome-shell + tracker
# were using.

set -eu

if [[ "$(systemctl is-active gdm.service 2>/dev/null)" != "active" ]]; then
    echo "desktop already down. Nothing to do."
    exit 0
fi

echo "[take-head] stopping GNOME display manager..."
sudo systemctl stop gdm.service

# gnome-shell + mutter + tracker sometimes hang around for a few
# seconds; give them room.
sleep 2
if systemctl is-active gdm.service >/dev/null 2>&1; then
    echo "[take-head] WARN: gdm still active after stop." >&2
    exit 1
fi
echo "[take-head] desktop down. ~1 GB RAM back."
