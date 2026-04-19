#!/usr/bin/env bash
# STAR: star-eth -- direct-ethernet-to-laptop SSH mode.
#
# The Pi's eth0 profile (netplan-eth0) is configured for NetworkManager's
# "shared" method: when a cable is plugged in, NM runs dnsmasq on eth0,
# hands out DHCP on 10.42.0.0/24, and assigns the Pi 10.42.0.1. Your
# laptop auto-configures and you can:
#
#     ssh star@10.42.0.1            # fixed Pi IP (recommended)
#     ssh star@star-desktop.local   # mDNS (avahi) — also works
#
# Because connection.autoconnect=yes, this just happens automatically
# when you plug the cable in — at boot or at any time. This helper is
# for forcing the state, checking it, or toggling autoconnect off if you
# want eth0 to only come up when you ask.
#
#   star-eth up       # force activate the shared profile now
#   star-eth down     # deactivate (cable can stay plugged in)
#   star-eth status   # show carrier, IP, leased peer, autoconnect state
#   star-eth auto on  # autoconnect on carrier-detect (default)
#   star-eth auto off # require `star-eth up` to bring it up
#   star-eth          # same as status

set -eu

CON="netplan-eth0"
IFACE="eth0"
PI_IP="10.42.0.1"

eth_is_active() {
    nmcli -t -f NAME,DEVICE connection show --active 2>/dev/null \
        | grep -q "^${CON}:${IFACE}$"
}

has_carrier() {
    local f="/sys/class/net/${IFACE}/carrier"
    [ -r "$f" ] && [ "$(cat "$f" 2>/dev/null || echo 0)" = "1" ]
}

autoconnect_state() {
    nmcli -t -f connection.autoconnect connection show "$CON" 2>/dev/null \
        | awk -F: '{print $2}'
}

peer_lease() {
    # Look in NM's dnsmasq lease file for the peer (laptop) IP + MAC.
    local lf="/var/lib/NetworkManager/dnsmasq-${IFACE}.leases"
    if [ -r "$lf" ]; then
        awk '{printf "%s  (mac %s)\n", $3, $2}' "$lf" | head -1
    fi
}

status() {
    local carrier=no active=no ipv4=""
    has_carrier && carrier=yes
    eth_is_active && active=yes
    ipv4=$(ip -4 -o addr show dev "$IFACE" 2>/dev/null | awk '{print $4}' | head -1)
    local auto
    auto=$(autoconnect_state)

    if [ "$active" = yes ]; then
        echo "star-eth: UP    (carrier=$carrier, ${IFACE}=${ipv4:-none}, autoconnect=$auto)"
        echo "          ssh star@${PI_IP}   or   ssh star@$(hostname).local"
        local peer
        peer=$(peer_lease || true)
        if [ -n "$peer" ]; then
            echo "          peer lease: $peer"
        fi
    else
        echo "star-eth: DOWN  (carrier=$carrier, autoconnect=$auto)"
        if [ "$carrier" = yes ] && [ "$auto" = yes ]; then
            echo "          carrier detected but profile not active — try 'star-eth up'"
        elif [ "$carrier" = no ]; then
            echo "          plug in an ethernet cable (will auto-activate if autoconnect=yes)"
        fi
    fi
}

up() {
    if eth_is_active; then
        echo "star-eth: already UP"
        status
        return 0
    fi
    echo "star-eth: activating '$CON' on $IFACE..."
    sudo nmcli connection up "$CON"
    sleep 1
    status
}

down() {
    if ! eth_is_active; then
        echo "star-eth: already DOWN"
        return 0
    fi
    echo "star-eth: deactivating '$CON'..."
    sudo nmcli connection down "$CON"
    echo "star-eth: DOWN"
}

auto() {
    case "${1:-}" in
        on)
            sudo nmcli connection modify "$CON" connection.autoconnect yes
            echo "star-eth: autoconnect ON — eth0 will activate on carrier-detect"
            ;;
        off)
            sudo nmcli connection modify "$CON" connection.autoconnect no
            echo "star-eth: autoconnect OFF — run 'star-eth up' to activate"
            ;;
        *)
            echo "star-eth: usage: star-eth auto on|off" >&2
            return 2
            ;;
    esac
}

case "${1:-status}" in
    up)     up ;;
    down)   down ;;
    status) status ;;
    auto)   auto "${2:-}" ;;
    -h|--help)
        sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'
        ;;
    *)
        echo "star-eth: unknown subcommand '$1' (use up | down | status | auto on|off)" >&2
        exit 2
        ;;
esac
