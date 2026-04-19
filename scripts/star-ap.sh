#!/usr/bin/env bash
# STAR: star-ap -- toggle the STAR-Robot WiFi access point without
# restarting the whole robot stack.
#
#   star-ap up       # bring the STAR-Robot AP up
#                    #   WARNING: this disconnects wlan0 from whatever
#                    #   network you're SSH'd over. Only run when you
#                    #   have another way back in (console keyboard,
#                    #   ethernet, or you're happy to reconnect to the
#                    #   STAR-Robot AP from your laptop).
#   star-ap down     # bring the STAR-Robot AP down; wlan0 will
#                    #   reconnect to its previous station network.
#   star-ap status   # print current state
#   star-ap          # same as status

set -eu

AP_CON="STAR-Hotspot"
WIFI_IFACE="${STAR_WIFI_IFACE:-wlan0}"
WIFI_SSID="${STAR_WIFI_SSID:-STAR-Robot}"
AP_IP="192.168.50.1"

ap_is_active() {
    nmcli -t -f NAME,DEVICE connection show --active 2>/dev/null \
        | grep -q "^${AP_CON}:"
}

status() {
    if ap_is_active; then
        echo "star-ap: UP  (SSID='$WIFI_SSID', http://$AP_IP)"
    else
        local cur
        cur=$(nmcli -t -f NAME,DEVICE,TYPE connection show --active 2>/dev/null \
              | awk -F: -v iface="$WIFI_IFACE" '$2==iface && $3=="802-11-wireless" {print $1; exit}')
        if [[ -n "$cur" ]]; then
            echo "star-ap: DOWN (wlan0 is on '$cur' as a station)"
        else
            echo "star-ap: DOWN (wlan0 has no active connection)"
        fi
    fi
}

up() {
    if ap_is_active; then
        echo "star-ap: already UP"
        return 0
    fi
    echo "star-ap: bringing up '$AP_CON' on $WIFI_IFACE..."
    echo "         WARNING: this will drop any SSH session on wlan0."
    # The AP connection profile is expected to already exist (start.sh
    # creates it on first boot with the AP enabled). If it doesn't,
    # guide the user to run start.sh --with-ap once to create it.
    if ! nmcli -t -f NAME connection show | grep -qx "$AP_CON"; then
        echo "star-ap: no '$AP_CON' profile found. Run:" >&2
        echo "           ./start.sh --with-ap            (one time, creates it)" >&2
        return 1
    fi
    sudo nmcli device disconnect "$WIFI_IFACE" >/dev/null 2>&1 || true
    sleep 1
    sudo nmcli connection up "$AP_CON"
    echo "star-ap: UP. Join '$WIFI_SSID' on your laptop, ssh star@$AP_IP"
}

down() {
    if ! ap_is_active; then
        echo "star-ap: already DOWN"
        return 0
    fi
    echo "star-ap: bringing down '$AP_CON'..."
    sudo nmcli connection down "$AP_CON"
    # Let NetworkManager try to reattach wlan0 to its previous SSID.
    sudo nmcli device connect "$WIFI_IFACE" >/dev/null 2>&1 || true
    echo "star-ap: DOWN"
}

case "${1:-status}" in
    up)     up ;;
    down)   down ;;
    status) status ;;
    -h|--help)
        sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
        ;;
    *)
        echo "star-ap: unknown subcommand '$1' (use up | down | status)" >&2
        exit 2
        ;;
esac
