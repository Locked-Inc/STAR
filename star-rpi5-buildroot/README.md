# STAR Raspberry Pi 5 Buildroot

Custom embedded Linux distribution for the STAR robotics platform, running on Raspberry Pi 5.

## Overview

This buildroot creates a **production-only** embedded Linux image with:
- **Systemd** - Init system and service manager
- **ROS2 Jazzy** - Robot Operating System 2
- **Python 3.11** - Required for ROS2 and ML libraries
- **WiFi Networking** - Access Point + Station mode for ESP32 connectivity
- **star-gateway** - Go service bridging UI ↔ ROS2

**Note:** This image does NOT include development tools (GCC, CMake, Git). All compilation must be done on your development machine via cross-compilation.

## Quick Start

### Build the Image

```bash
# Download and configure buildroot
make download
make configure

# Build the complete image (takes 1-2 hours on first build)
make build

# Flash to SD card (replace /dev/sdX with your SD card device)
sudo dd if=buildroot-2025.02/output/images/sdcard.img of=/dev/sdX bs=4M status=progress conv=fsync
```

### First Boot

1. Insert SD card into Raspberry Pi 5
2. Connect to serial console or HDMI monitor
3. Login with username: `root` (no password)
4. Configure WiFi:
   ```bash
   /root/setup-network.sh
   ```

## Deploying the Go Gateway Service

The `star-gateway` Go service runs as a systemd service and bridges the UI to ROS2. Since the target image doesn't include the Go compiler, you must cross-compile on your development machine.

### Cross-Compilation Workflow

```bash
# 1. Navigate to the gateway project
cd ../star-gateway

# 2. Cross-compile for ARM64 (Pi5 architecture)
GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build -o star-gateway ./cmd/star-gateway

# 3. Deploy to the Pi5 (replace <pi5-ip> with actual IP)
scp star-gateway root@<pi5-ip>:/usr/bin/star-gateway

# 4. Set executable permissions
ssh root@<pi5-ip> 'chmod +x /usr/bin/star-gateway'

# 5. Restart the service
ssh root@<pi5-ip> 'systemctl restart robot-gateway'

# 6. Check service status
ssh root@<pi5-ip> 'systemctl status robot-gateway'
```

### Automated Deployment Script

For convenience, create a deployment script:

```bash
#!/bin/bash
# deploy-gateway.sh

PI5_IP="${1:-192.168.1.100}"  # Default IP or pass as argument

echo "Building star-gateway for ARM64..."
cd ../star-gateway
GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build -o star-gateway ./cmd/star-gateway

echo "Deploying to $PI5_IP..."
scp star-gateway root@$PI5_IP:/usr/bin/star-gateway
ssh root@$PI5_IP 'chmod +x /usr/bin/star-gateway && systemctl restart robot-gateway'

echo "Checking service status..."
ssh root@$PI5_IP 'systemctl status robot-gateway'
```

Usage:
```bash
chmod +x deploy-gateway.sh
./deploy-gateway.sh 192.168.4.1  # Deploy to Pi5 at 192.168.4.1
```

## Network Configuration

### WiFi Dual-Mode Architecture

The Pi5 operates in **dual-mode WiFi**:

1. **Station Mode (`wlan0`)**: Connects to your home/office WiFi for internet access
2. **Access Point Mode (`uap0`)**: Creates "STAR-Robot-AP" hotspot for ESP32 devices

```
Internet
   ↓
Home WiFi (wlan0)
   ↓
Raspberry Pi 5 (NAT Router)
   ↓
STAR-Robot-AP (uap0: 192.168.4.1)
   ↓
ESP32 Devices (192.168.4.10 - 192.168.4.100)
```

### Configure WiFi

Use the interactive setup tool:
```bash
/root/setup-network.sh
```

Or manually edit configurations:
- Upstream WiFi: `/etc/wpa_supplicant/wpa_supplicant.conf`
- Access Point: `/etc/hostapd/hostapd.conf`
- DHCP Server: `/etc/dnsmasq.d/ap-dhcp.conf`

### Network Services

Managed by systemd:
- `wpa_supplicant@wlan0.service` - Connects to upstream WiFi
- `wifi-ap-setup.service` - Creates virtual AP interface (uap0)
- `hostapd.service` - WiFi access point daemon
- `dnsmasq-ap.service` - DHCP/DNS server for AP network
- `wifi-nat.service` - NAT routing for internet sharing

## System Architecture

### Hardware Support

| Component | Configuration | Purpose |
|-----------|---------------|---------|
| SPI Bus | `dtparam=spi=on` | ESP32 motor controller communication (10 Mbps) |
| I2C Bus | `dtparam=i2c_arm=on` | Sensor/peripheral communication |
| Dual Cameras | CAM0 + CAM1 | Vision processing, navigation |
| GPU Memory | 128MB | Camera ISP, image processing |

### Communication Flow

```
User Interface (TypeScript)
         ↓ WebSocket/HTTP
star-gateway (Go on RPi5) - systemd service at /usr/bin/star-gateway
         ↓ ROS2 messages
ROS2 Nodes (C++ on RPi5)
         ↓ SPI (to be implemented)
ESP32 Motor Controllers (C firmware)
         ↓ MCPWM + Encoders
4× DC Motors with Hall Encoders
```

## Development Workflow

### Production-Only Philosophy

This image follows embedded Linux best practices:
- ✅ **Cross-compile** on development machine
- ✅ **Deploy binaries** to target
- ✅ **Version control** exact binaries that run
- ❌ **No on-target compilation** (prevents ad-hoc changes)
- ❌ **No development tools** (GCC, CMake removed for security/size)

### Updating the Image

```bash
# Modify buildroot configuration
make configure

# Rebuild (incremental - much faster than first build)
make build

# Flash updated image
sudo dd if=buildroot-2025.02/output/images/sdcard.img of=/dev/sdX bs=4M status=progress conv=fsync
```

### Adding New Packages

Edit `configs/rpi5_64_defconfig` and add packages:
```bash
# Example: Add htop system monitor
BR2_PACKAGE_HTOP=y
```

Then rebuild:
```bash
make build
```

## Build System Details

### Directory Structure

```
star-rpi5-buildroot/
├── configs/
│   ├── rpi5_64_defconfig          # Main buildroot configuration
│   └── config.txt                 # Raspberry Pi firmware config
├── overlays/
│   ├── etc/                       # System configuration files
│   ├── usr/                       # User binaries/libraries
│   └── root/                      # Root user files (setup scripts)
├── scripts/
│   ├── post-build.sh              # Post-build customization
│   └── post-image.sh              # Image generation
├── Makefile                       # Build automation
└── README.md                      # This file
```

### Build Targets

```bash
make download      # Download buildroot tarball
make configure     # Configure buildroot with rpi5_64_defconfig
make build         # Build complete image
make clean         # Clean build artifacts (keeps downloads)
make distclean     # Remove everything (including buildroot source)
```

## Troubleshooting

### Service Fails to Start

Check service logs:
```bash
journalctl -u robot-gateway.service -f
```

### WiFi Issues

Check network services:
```bash
systemctl status wpa_supplicant@wlan0
systemctl status hostapd
systemctl status dnsmasq-ap
systemctl status wifi-nat
```

View connected ESP32 clients:
```bash
cat /var/lib/misc/dnsmasq.leases
```

### Deployment Issues

Verify binary architecture:
```bash
file /usr/bin/star-gateway
# Should output: ELF 64-bit LSB executable, ARM aarch64
```

## Additional Resources

- [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html)
- [ROS2 Jazzy Documentation](https://docs.ros.org/en/jazzy/)
- [Raspberry Pi 5 Documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi-5.html)
- [STAR Project Documentation](../docs/star_documentation.pdf)

## License

Part of the STAR (Sensor and Actuator Abstraction Runtime) project.
