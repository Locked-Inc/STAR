#!/bin/bash
# STAR Robot WiFi Setup Helper Script

set -e

echo "========================================"
echo "  STAR Robot WiFi Configuration"
echo "========================================"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    exit 1
fi

# Function to configure WiFi
configure_wifi() {
    read -p "Enter your WiFi SSID: " SSID
    read -sp "Enter your WiFi Password: " PASSWORD
    echo ""
    read -p "Enter your country code (e.g., US, GB, DE): " COUNTRY

    if [ -z "$SSID" ] || [ -z "$PASSWORD" ]; then
        echo "Error: SSID and password cannot be empty"
        exit 1
    fi

    if [ -z "$COUNTRY" ]; then
        COUNTRY="US"
    fi

    # Create wpa_supplicant configuration
    cat > /etc/wpa_supplicant/wpa_supplicant.conf <<EOF
ctrl_interface=/var/run/wpa_supplicant
ctrl_interface_group=0
update_config=1
country=$COUNTRY

network={
    ssid="$SSID"
    psk="$PASSWORD"
    key_mgmt=WPA-PSK
    priority=1
}
EOF

    chmod 600 /etc/wpa_supplicant/wpa_supplicant.conf

    echo ""
    echo "WiFi configuration saved!"
}

# Function to start WiFi
start_wifi() {
    echo ""
    echo "Starting WiFi connection..."

    # Kill any existing wpa_supplicant processes
    pkill wpa_supplicant 2>/dev/null || true
    sleep 1

    # Bring up wlan0 interface
    ip link set wlan0 up

    # Start wpa_supplicant
    wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf

    # Wait a moment for connection
    sleep 3

    # Request DHCP address
    dhcpcd wlan0

    echo ""
    echo "WiFi connection initiated!"
    echo "Checking connection status..."
    sleep 2

    # Show connection status
    if iwconfig wlan0 2>/dev/null | grep -q "ESSID"; then
        echo ""
        echo "WiFi connected successfully!"
        ip addr show wlan0 | grep "inet "
    else
        echo ""
        echo "Warning: WiFi may not be connected. Check credentials and try again."
    fi
}

# Main menu
echo "1) Configure WiFi credentials"
echo "2) Start WiFi connection"
echo "3) Configure and start WiFi"
echo "4) Show WiFi status"
echo ""
read -p "Select an option (1-4): " OPTION

case $OPTION in
    1)
        configure_wifi
        ;;
    2)
        if [ ! -f /etc/wpa_supplicant/wpa_supplicant.conf ]; then
            echo "Error: WiFi not configured. Please configure first (option 1 or 3)"
            exit 1
        fi
        start_wifi
        ;;
    3)
        configure_wifi
        start_wifi
        ;;
    4)
        echo ""
        echo "=== WiFi Status ==="
        iwconfig wlan0 2>/dev/null || echo "WiFi interface not found"
        echo ""
        echo "=== IP Address ==="
        ip addr show wlan0 2>/dev/null || echo "WiFi interface not configured"
        ;;
    *)
        echo "Invalid option"
        exit 1
        ;;
esac

echo ""
echo "Done!"
