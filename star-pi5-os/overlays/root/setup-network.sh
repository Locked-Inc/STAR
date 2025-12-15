#!/bin/bash
# STAR Robot Network Configuration Helper
# Unified script for WiFi Station (upstream) and Access Point (ESP32 hotspot)

set -e

echo "========================================"
echo "  STAR Robot Network Configuration"
echo "========================================"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    exit 1
fi

WPA_CONF="/etc/wpa_supplicant/wpa_supplicant.conf"
HOSTAPD_CONF="/etc/hostapd/hostapd.conf"

# Function to configure upstream WiFi (station mode)
configure_station() {
    echo ""
    echo "=== Configure Upstream WiFi (Station Mode) ==="
    echo ""
    read -p "Enter your WiFi SSID: " SSID
    read -sp "Enter your WiFi Password: " PASSWORD
    echo ""
    read -p "Enter your country code (e.g., US, GB, DE) [US]: " COUNTRY
    COUNTRY=${COUNTRY:-US}

    if [ -z "$SSID" ] || [ -z "$PASSWORD" ]; then
        echo "Error: SSID and password cannot be empty"
        return 1
    fi

    mkdir -p /etc/wpa_supplicant
    cat > "$WPA_CONF" <<EOF
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

    chmod 600 "$WPA_CONF"
    echo ""
    echo "Upstream WiFi configuration saved!"
    echo "  SSID: $SSID"
    echo "  Country: $COUNTRY"
}

# Function to configure hotspot (AP mode)
configure_hotspot() {
    echo ""
    echo "=== Configure Hotspot (Access Point Mode) ==="
    echo ""
    read -p "Enter hotspot SSID [STAR-Robot-AP]: " AP_SSID
    AP_SSID=${AP_SSID:-STAR-Robot-AP}

    read -sp "Enter hotspot password (min 8 chars) [starrobot123]: " AP_PASSWORD
    echo ""
    AP_PASSWORD=${AP_PASSWORD:-starrobot123}

    if [ ${#AP_PASSWORD} -lt 8 ]; then
        echo "Error: Password must be at least 8 characters"
        return 1
    fi

    read -p "Enter WiFi channel (1-11) [6]: " CHANNEL
    CHANNEL=${CHANNEL:-6}

    read -p "Enter country code (US, GB, DE, etc.) [US]: " COUNTRY
    COUNTRY=${COUNTRY:-US}

    mkdir -p /etc/hostapd
    cat > "$HOSTAPD_CONF" <<EOF
# STAR Robot AP Configuration
interface=uap0
driver=nl80211
ssid=$AP_SSID
hw_mode=g
channel=$CHANNEL
ieee80211n=1
wmm_enabled=1
country_code=$COUNTRY
wpa=2
wpa_passphrase=$AP_PASSWORD
wpa_key_mgmt=WPA-PSK
wpa_pairwise=CCMP
rsn_pairwise=CCMP
max_num_sta=10
logger_syslog=-1
logger_syslog_level=2
ctrl_interface=/var/run/hostapd
ctrl_interface_group=0
EOF

    chmod 600 "$HOSTAPD_CONF"
    echo ""
    echo "Hotspot configuration saved!"
    echo "  SSID: $AP_SSID"
    echo "  Channel: $CHANNEL"
    echo "  Country: $COUNTRY"
}

# Function to start all network services
start_network() {
    echo ""
    echo "Starting network services..."

    # Enable IP forwarding
    sysctl -w net.ipv4.ip_forward=1 > /dev/null

    # Start AP services
    echo "  Starting AP interface..."
    systemctl start wifi-ap-setup.service || { echo "  Warning: Failed to create AP interface"; }
    sleep 1

    echo "  Starting hostapd..."
    systemctl start hostapd.service || { echo "  Warning: Failed to start hostapd"; }
    sleep 1

    echo "  Starting DHCP server..."
    systemctl start dnsmasq-ap.service || { echo "  Warning: Failed to start dnsmasq"; }

    echo "  Starting NAT routing..."
    systemctl start wifi-nat.service || { echo "  Warning: Failed to start NAT"; }

    # Start station mode if configured
    if [ -f "$WPA_CONF" ]; then
        echo "  Starting station mode..."
        systemctl start wpa_supplicant@wlan0.service || { echo "  Warning: Failed to start wpa_supplicant"; }
        sleep 2
        dhcpcd wlan0 2>/dev/null || { echo "  Warning: Failed to get DHCP on wlan0"; }
    else
        echo "  Skipping station mode (not configured)"
    fi

    echo ""
    echo "Network services started!"
}

# Function to stop all network services
stop_network() {
    echo ""
    echo "Stopping network services..."

    systemctl stop wifi-nat.service 2>/dev/null || true
    systemctl stop dnsmasq-ap.service 2>/dev/null || true
    systemctl stop hostapd.service 2>/dev/null || true
    systemctl stop wifi-ap-setup.service 2>/dev/null || true
    systemctl stop wpa_supplicant@wlan0.service 2>/dev/null || true

    echo "Network services stopped."
}

# Function to enable services at boot
enable_boot() {
    echo ""
    echo "Enabling network services at boot..."

    systemctl enable wifi-ap-setup.service
    systemctl enable hostapd.service
    systemctl enable dnsmasq-ap.service
    systemctl enable wifi-nat.service
    systemctl enable wpa_supplicant@wlan0.service

    echo "Services will start automatically on boot."
}

# Function to disable services at boot
disable_boot() {
    echo ""
    echo "Disabling network services at boot..."

    systemctl disable wifi-ap-setup.service 2>/dev/null || true
    systemctl disable hostapd.service 2>/dev/null || true
    systemctl disable dnsmasq-ap.service 2>/dev/null || true
    systemctl disable wifi-nat.service 2>/dev/null || true
    systemctl disable wpa_supplicant@wlan0.service 2>/dev/null || true

    echo "Services will not start automatically on boot."
}

# Function to show status
show_status() {
    echo ""
    echo "=========================================="
    echo "           NETWORK STATUS"
    echo "=========================================="

    echo ""
    echo "=== Station Interface (wlan0) ==="
    if ip addr show wlan0 &>/dev/null; then
        ip addr show wlan0 | grep -E "inet |state"
        iwconfig wlan0 2>/dev/null | grep -E "ESSID|Signal" || true
    else
        echo "Interface not available"
    fi

    echo ""
    echo "=== Access Point Interface (uap0) ==="
    if ip addr show uap0 &>/dev/null; then
        ip addr show uap0 | grep -E "inet |state"
    else
        echo "Interface not available (AP not running)"
    fi

    echo ""
    echo "=== Service Status ==="
    echo -n "  wifi-ap-setup: "
    systemctl is-active wifi-ap-setup.service 2>/dev/null || echo "inactive"
    echo -n "  hostapd: "
    systemctl is-active hostapd.service 2>/dev/null || echo "inactive"
    echo -n "  dnsmasq-ap: "
    systemctl is-active dnsmasq-ap.service 2>/dev/null || echo "inactive"
    echo -n "  wifi-nat: "
    systemctl is-active wifi-nat.service 2>/dev/null || echo "inactive"
    echo -n "  wpa_supplicant: "
    systemctl is-active wpa_supplicant@wlan0.service 2>/dev/null || echo "inactive"

    echo ""
    echo "=== Connected ESP32 Clients ==="
    if [ -f /var/lib/misc/dnsmasq.leases ]; then
        if [ -s /var/lib/misc/dnsmasq.leases ]; then
            cat /var/lib/misc/dnsmasq.leases
        else
            echo "No clients connected"
        fi
    else
        echo "DHCP not running"
    fi

    echo ""
    echo "=== NAT Rules ==="
    if iptables -t nat -L POSTROUTING -n 2>/dev/null | grep -q MASQUERADE; then
        echo "NAT is active (MASQUERADE enabled)"
    else
        echo "NAT not configured"
    fi
}

# Function to run full setup
full_setup() {
    echo ""
    echo "=== Full Network Setup ==="
    echo "This will configure both station and AP mode."
    echo ""

    configure_station
    configure_hotspot
    start_network
    enable_boot

    echo ""
    echo "=========================================="
    echo "  Setup Complete!"
    echo "=========================================="
    echo ""
    echo "Your Pi is now configured as:"
    echo "  - WiFi Station: Connected to your home network"
    echo "  - WiFi Hotspot: ESP32 devices can connect"
    echo ""
    echo "ESP32 Connection Info:"
    grep "^ssid=" "$HOSTAPD_CONF" 2>/dev/null | sed 's/ssid=/  SSID: /' || echo "  SSID: STAR-Robot-AP"
    echo "  Gateway: 192.168.4.1"
    echo "  DHCP Range: 192.168.4.10 - 192.168.4.100"
    echo ""
}

# Main menu
echo "1) Configure upstream WiFi (station mode)"
echo "2) Configure hotspot (AP for ESP32)"
echo "3) Start network services"
echo "4) Stop network services"
echo "5) Enable services at boot"
echo "6) Disable services at boot"
echo "7) Show network status"
echo "8) Full setup (configure all + start + enable)"
echo ""
read -p "Select an option (1-8): " OPTION

case $OPTION in
    1)
        configure_station
        ;;
    2)
        configure_hotspot
        ;;
    3)
        start_network
        ;;
    4)
        stop_network
        ;;
    5)
        enable_boot
        ;;
    6)
        disable_boot
        ;;
    7)
        show_status
        ;;
    8)
        full_setup
        ;;
    *)
        echo "Invalid option"
        exit 1
        ;;
esac

echo ""
echo "Done!"
