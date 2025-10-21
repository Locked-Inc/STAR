# Quick Start Guide - STAR Yocto Minimal Build

Get your minimal Raspberry Pi 5 system running in 4 steps.

## Prerequisites

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev pylint3 xterm python3-subunit \
    mesa-common-dev zstd liblz4-tool file
```

**Disk Space**: Need ~100GB free

## Build Steps

### 1. Initialize Yocto Layers

```bash
cd yocto-rpi5
./scripts/init-build.sh
```

Downloads Poky, meta-openembedded, and meta-raspberrypi (~5 minutes).

**Note:** Uses git submodules for version tracking (reproducible builds).

### 2. Setup Environment

```bash
source setup-environment.sh
```

### 3. Build Image

```bash
./scripts/build-image.sh
```

**First build**: 4-8 hours, downloads ~10GB
**Subsequent builds**: Much faster (incremental)

Get coffee. Or sleep. Or both.

### 4. Flash to SD Card

```bash
# Find your SD card device
lsblk

# Flash it (replace sdX with your device)
sudo ./scripts/flash-sd.sh /dev/sdX
```

**⚠️ WARNING**: This erases the entire SD card!

## First Boot

1. Insert SD card into Raspberry Pi 5
2. Connect Ethernet cable
3. Power on
4. Find IP address (check router or use `nmap`)
5. SSH in:
   ```bash
   ssh root@<ip-address>
   ```
6. **IMMEDIATELY set password**:
   ```bash
   passwd
   ```

## What's Included

- **Init**: runit (minimal, fast)
- **SSH**: dropbear
- **Network**: WiFi + Ethernet (DHCP on eth0)
- **Package Manager**: opkg
- **Shell**: busybox
- **Size**: ~150-250MB

## Configure WiFi

On the Raspberry Pi:

```bash
# Generate WPA config
wpa_passphrase "YourSSID" "YourPassword" > /etc/wpa_supplicant/wpa_supplicant-wlan0.conf

# Edit network interfaces
vi /etc/network/interfaces
```

Uncomment WiFi section:
```
auto wlan0
iface wlan0 inet dhcp
    wpa-conf /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
```

Bring up WiFi:
```bash
ifup wlan0
```

## Add Packages

### Runtime (via opkg)

```bash
opkg update
opkg install <package-name>
```

### Build-time (via Yocto - Recommended)

Edit `meta-star/recipes-core/packagegroups/packagegroup-star-minimal.bb`:

```bitbake
RDEPENDS:${PN} = " \
    ...
    your-new-package \
"
```

Rebuild:
```bash
source setup-environment.sh
bitbake star-minimal-image
```

## Common Commands

### Runit Service Management

```bash
# List all services
ls /etc/service/

# Check service status
sv status <service-name>

# Restart service
sv restart <service-name>

# Stop service
sv stop <service-name>

# Start service
sv start <service-name>
```

### System

```bash
# Reboot
reboot

# Shutdown
halt

# Check disk usage
df -h

# Check memory
free -h
```

## Troubleshooting

### Can't find Raspberry Pi IP

```bash
# Scan your network (from your computer)
nmap -sn 192.168.1.0/24 | grep -B 2 "Raspberry"

# Or check your router's DHCP leases
```

### SSH not working

- Verify network connection (ping the Pi)
- Check if dropbear is running: `sv status dropbear`
- Check logs: `cat /var/log/dropbear/current`

### Out of disk space during build

```bash
# Clean build artifacts
cd build
bitbake -c clean star-minimal-image

# Nuclear option - delete everything
rm -rf build/tmp
```

### Build fails

1. Check the error message carefully
2. Search for the error in Yocto documentation
3. Clean the failed package: `bitbake -c cleanall <package-name>`
4. Rebuild: `bitbake star-minimal-image`

## Next Steps

Now that you have a minimal system:

1. **Secure it**: Set root password, configure SSH keys
2. **Add packages**: Install what you need for STAR robot
   - Python for scripts
   - Robot-specific tools
   - Sensor drivers
3. **Customize kernel**: Remove unnecessary drivers
4. **Create custom recipes**: For STAR-specific software

## Resources

- [Full README](README.md) - Detailed documentation
- [Yocto Project Docs](https://docs.yoctoproject.org/)
- [Raspberry Pi BSP](https://github.com/agherzan/meta-raspberrypi)

## Support

If you get stuck:
1. Check build logs in `build/tmp/work/`
2. Search the error online
3. Check Yocto mailing lists
4. File an issue in the STAR repo
