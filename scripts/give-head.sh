#!/usr/bin/env bash
# STAR: give-head -- start the desktop environment on top of the
# headless STAR runtime. The robot services keep running; this only
# brings up GNOME so you can see / interact with the screen.
#
# Boot flow: Pi5 defaults to multi-user.target (console login). The
# star-robot.service starts there with --headless. When you want a GUI:
#
#   $ give-head
#
# To go back to pure console (and save resources again):
#
#   $ take-head
#
# Auto-boot unaffected; the default target stays multi-user so the
# next power-on still lands on console.

set -eu

if [[ "$(systemctl is-active gdm.service 2>/dev/null)" == "active" ]]; then
    echo "desktop already running (gdm is active). Nothing to do."
    exit 0
fi

echo "[give-head] starting GNOME display manager..."
sudo systemctl start gdm.service

# Wait for gdm to actually bind the VT.
for _ in $(seq 1 20); do
    if systemctl is-active gdm.service >/dev/null 2>&1; then
        break
    fi
    sleep 0.25
done

if systemctl is-active gdm.service >/dev/null 2>&1; then
    echo "[give-head] desktop is up. Switch to VT with Ctrl+Alt+F1-F7 if needed."
else
    echo "[give-head] WARN: gdm did not come up in time. Run:" >&2
    echo "    sudo systemctl status gdm.service" >&2
    exit 1
fi
