#!/bin/sh
# /usr/local/sbin/wifi-fix.sh
# Minimal, BusyBox-safe startup script for Yocto to force client Wi-Fi.

set -eu

IFACE="${IFACE:-wlp1s0}"
CONF="${CONF:-/etc/wpa_supplicant/wpa_supplicant.conf}"
COUNTRY="${COUNTRY:-US}"
AP_ADDR="192.168.43.1/24"

log() { echo "[wifi-fix] $*" ; logger -t wifi-fix "$*"; }

cmd_ok() { command -v "$1" >/dev/null 2>&1; }

# 1) Stop anything that might grab the card (AP/hostapd/old supplicant)
for svc in hostapd startwlanap wlan-ap create_ap connman wpa_supplicant@${IFACE}.service wpa_supplicant; do
    if cmd_ok systemctl; then systemctl stop "$svc" 2>/dev/null || true; fi
done
killall hostapd 2>/dev/null || true
killall wpa_supplicant 2>/dev/null || true

# 2) Unblock radio, switch to managed mode, bring link up
cmd_ok rfkill && rfkill unblock wifi || true
ip link set "$IFACE" down || true
iw dev "$IFACE" set type managed || true
ip link set "$IFACE" up

# 3) Set regulatory domain (helps on 5 GHz/DFS)
iw reg set "$COUNTRY" 2>/dev/null || true

# 4) Remove leftover AP address if present
if ip addr show "$IFACE" | grep -q "$AP_ADDR"; then
    ip addr del "$AP_ADDR" dev "$IFACE" || true
    log "Removed stale AP address $AP_ADDR"
fi

# 5) Start wpa_supplicant (client mode) with small retry
if [ ! -s "$CONF" ]; then
    log "ERROR: $CONF not found or empty"; exit 1
fi

TRIES=0
MAX_TRIES=3
while [ $TRIES -lt $MAX_TRIES ]; do
    TRIES=$((TRIES+1))
    log "Starting wpa_supplicant (attempt $TRIES/$MAX_TRIES)"
    wpa_supplicant -B -i "$IFACE" -c "$CONF" -D nl80211 && break
    sleep 2
done

# 6) Wait briefly for association
S=0
while [ $S -lt 10 ]; do
    if iw "$IFACE" link 2>/dev/null | grep -q "Connected to"; then
        break
    fi
    S=$((S+1))
    sleep 1
done
iw "$IFACE" link || true

# 7) DHCP with udhcpc (BusyBox). Fall back to dhclient if present.
if cmd_ok udhcpc; then
    # -n: exit if lease fails; -q: be quiet-ish
    udhcpc -i "$IFACE" -n -q || {
        log "udhcpc failed once; retrying"; udhcpc -i "$IFACE" -n -q || true
    }
elif cmd_ok dhclient; then
    dhclient "$IFACE" || true
else
    log "WARNING: No DHCP client found (udhcpc/dhclient)."
fi

# 8) Final status (helps debugging in journal)
IP4=$(ip -4 -o addr show dev "$IFACE" 2>/dev/null | awk '{print $4}' | paste -sd, -)
SSID=$(iw "$IFACE" link 2>/dev/null | awk -F': ' '/SSID/ {print $2}')
log "Interface: $IFACE | SSID: ${SSID:-unknown} | IPv4: ${IP4:-none}"

exit 0

