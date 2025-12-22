#!/bin/bash
# STAR Robot OS Post-Build Script
# Fixes shadow file format for proper root login

set -u
set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Fix shadow file - Buildroot's default creates invalid format
# Shadow needs 9 colon-separated fields
if [ -e "${TARGET_DIR}/etc/shadow" ]; then
    # Check if root has malformed entry (8 empty fields)
    if grep -q "^root::::::::$" "${TARGET_DIR}/etc/shadow"; then
        echo "Fixing root shadow entry..."
        sed -i 's/^root::::::::$/root::0:0:99999:7:::/' "${TARGET_DIR}/etc/shadow"
    fi

    # Fix any other malformed entries (should have 9 fields)
    # Format: name:password:lastchanged:min:max:warn:inactive:expire:reserved
    sed -i 's/^\([^:]*\):\*:::::::$/\1:*:0:0:99999:7:::/' "${TARGET_DIR}/etc/shadow"
fi

# Enable network services for AP + Station mode
# These symlinks enable services at boot time
SYSTEMD_DIR="${TARGET_DIR}/etc/systemd/system"
WANTS_DIR="${SYSTEMD_DIR}/multi-user.target.wants"

if [ -d "${SYSTEMD_DIR}" ]; then
    echo "Enabling network services..."
    mkdir -p "${WANTS_DIR}"

    # Enable AP setup service
    if [ -e "${SYSTEMD_DIR}/wifi-ap-setup.service" ]; then
        ln -sf ../wifi-ap-setup.service "${WANTS_DIR}/wifi-ap-setup.service"
    fi

    # Enable hostapd service
    if [ -e "${SYSTEMD_DIR}/hostapd.service" ]; then
        ln -sf ../hostapd.service "${WANTS_DIR}/hostapd.service"
    fi

    # Enable dnsmasq AP service
    if [ -e "${SYSTEMD_DIR}/dnsmasq-ap.service" ]; then
        ln -sf ../dnsmasq-ap.service "${WANTS_DIR}/dnsmasq-ap.service"
    fi

    # Enable NAT service
    if [ -e "${SYSTEMD_DIR}/wifi-nat.service" ]; then
        ln -sf ../wifi-nat.service "${WANTS_DIR}/wifi-nat.service"
    fi

    # Enable wpa_supplicant service for station mode
    if [ -e "${SYSTEMD_DIR}/wpa_supplicant@wlan0.service" ]; then
        ln -sf "../wpa_supplicant@wlan0.service" "${WANTS_DIR}/wpa_supplicant@wlan0.service"
    fi
fi

# Make Python 3.11 the default (for ROS2), but keep Python 3.12 available (for Avahi bindings)
# Python 3.11 is at /opt/python3.11, Python 3.12 is Buildroot's default
echo "Setting Python 3.11 as default (keeping 3.12 for Avahi)..."
rm -f "${TARGET_DIR}/usr/bin/python3"
rm -f "${TARGET_DIR}/usr/bin/python"
rm -f "${TARGET_DIR}/usr/bin/pydoc3"
ln -sf /opt/python3.11/bin/python3.11 "${TARGET_DIR}/usr/bin/python3"
ln -sf /opt/python3.11/bin/python3.11 "${TARGET_DIR}/usr/bin/python"
# Keep python3.12 accessible directly if needed
# Python 3.12 remains at /usr/bin/python3.12 and /usr/lib/python3.12

echo "Post-build script completed"
