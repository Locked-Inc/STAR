#!/usr/bin/env bash
# setup-rpi5.sh -- Configure RPi5 to communicate with BeagleBone Blue over USB
#
# Run this ONCE on the Raspberry Pi 5 to set up the USB networking interface
# for deploying firmware to the BeagleBone Blue.
#
# The BBB creates two USB network interfaces when connected:
#   usb0 (RNDIS): 192.168.7.2  -- Windows/Linux compatible
#   usb1 (CDC ECM): 192.168.6.2  -- macOS/Linux compatible
#
# This script configures the RPi5 as the gateway (192.168.6.1 / 192.168.7.1)
# and optionally enables NAT so the BBB can reach the internet for apt.

set -euo pipefail

echo "=== RPi5 <-> BeagleBone Blue USB Network Setup ==="

# Detect the BBB USB interface
# On RPi5 (Linux), the BBB typically creates a usb0 or enx* interface
BBB_IFACE=""
for iface in usb0 usb1; do
    if ip link show "${iface}" > /dev/null 2>&1; then
        BBB_IFACE="${iface}"
        break
    fi
done

# Check for enx* interfaces (systemd naming)
if [ -z "${BBB_IFACE}" ]; then
    for iface in $(ip -o link show | grep -oP 'enx[a-f0-9]+'); do
        BBB_IFACE="${iface}"
        break
    done
fi

if [ -z "${BBB_IFACE}" ]; then
    echo "ERROR: No BBB USB interface found."
    echo "Make sure the BeagleBone Blue is connected via USB."
    exit 1
fi

echo "Found BBB interface: ${BBB_IFACE}"

# Configure IP address
echo "[1/4] Configuring IP on ${BBB_IFACE}..."
sudo ip addr flush dev "${BBB_IFACE}"
sudo ip addr add 192.168.6.1/30 dev "${BBB_IFACE}"
sudo ip link set "${BBB_IFACE}" up

# Also add the RNDIS subnet in case BBB uses usb0
sudo ip addr add 192.168.7.1/30 dev "${BBB_IFACE}" 2>/dev/null || true

# Test connectivity
echo "[2/4] Testing connectivity..."
if ping -c 1 -W 2 192.168.6.2 > /dev/null 2>&1; then
    echo "  BBB reachable at 192.168.6.2"
    BBB_IP="192.168.6.2"
elif ping -c 1 -W 2 192.168.7.2 > /dev/null 2>&1; then
    echo "  BBB reachable at 192.168.7.2"
    BBB_IP="192.168.7.2"
else
    echo "  WARNING: BBB not reachable. Check USB connection."
    exit 1
fi

# Enable IP forwarding and NAT (so BBB can reach the internet)
echo "[3/4] Enabling NAT for BBB internet access..."
UPSTREAM_IFACE=$(ip route | grep default | awk '{print $5}' | head -1)

if [ -n "${UPSTREAM_IFACE}" ]; then
    sudo sysctl -w net.ipv4.ip_forward=1 > /dev/null
    sudo iptables -t nat -A POSTROUTING -o "${UPSTREAM_IFACE}" -j MASQUERADE
    sudo iptables -A FORWARD -i "${BBB_IFACE}" -o "${UPSTREAM_IFACE}" -j ACCEPT
    sudo iptables -A FORWARD -i "${UPSTREAM_IFACE}" -o "${BBB_IFACE}" -m state --state RELATED,ESTABLISHED -j ACCEPT
    echo "  NAT enabled: ${BBB_IFACE} -> ${UPSTREAM_IFACE}"
else
    echo "  WARNING: No upstream interface found. BBB won't have internet."
fi

# Configure SSH key
echo "[4/4] Setting up SSH key..."
if [ ! -f ~/.ssh/id_rsa ]; then
    ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa > /dev/null 2>&1
fi
sshpass -p temppwd ssh-copy-id -o StrictHostKeyChecking=no "debian@${BBB_IP}" 2>/dev/null || true

echo ""
echo "=== Setup Complete ==="
echo "BBB address: ${BBB_IP}"
echo "SSH: ssh debian@${BBB_IP}"
echo "Deploy: ./scripts/deploy.sh --build --run"
echo "Flash:  ./scripts/flash-emmc.sh <image.img.xz>"
