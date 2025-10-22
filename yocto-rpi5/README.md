# Minimal Yocto Build for STAR Robot (Raspberry Pi 5)

Ultra-minimal, debloated headless Linux system for Raspberry Pi 5 using Yocto Project.

## What You Get

A modern, optimized system with:
- **Java**: OpenJDK 21 (LTS) - Latest Java with full Kotlin support
- **ROS2**: Jazzy (latest LTS) - For robotics integration
- **Python**: 3.12+ - Modern Python with ROS2
- **Init**: systemd (reliable, well-supported)
- **SSH**: dropbear (lightweight remote access)
- **Network**: WiFi + Ethernet support
- **Package Manager**: opkg (for adding packages later)
- **Shell**: busybox
- **Size**: ~500MB-1GB root filesystem (includes Java + ROS2)
- **Optimized for robotics development with modern languages**

## Quick Start

### Prerequisites

Ubuntu/Debian dependencies:
```bash
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev pylint3 xterm python3-subunit \
    mesa-common-dev zstd liblz4-tool file
```

**Disk Space**: Need ~100GB free for build artifacts

### 1. Initialize Yocto Layers

Clone required layers (the script handles both submodules and direct cloning):
```bash
cd yocto-rpi5
./scripts/init-build.sh
```

This downloads:
- poky (Yocto reference)
- meta-openembedded
- meta-raspberrypi

**Note:** This project uses git submodules (`.gitmodules` is included) to track exact layer versions for reproducible builds. The init script automatically detects and uses them.

### 2. Setup Build Environment

```bash
source setup-environment.sh
```

### 3. Build Image

```bash
./scripts/build-image.sh
```

First build takes 4-8 hours depending on hardware. Subsequent builds are much faster.

### 4. Flash to SD Card

```bash
sudo ./scripts/flash-sd.sh /dev/sdX
```

**WARNING**: Replace `/dev/sdX` with your actual SD card device. Double-check with `lsblk`!

## Default Configuration

- **User**: `root` (no password - set one after first boot!)
- **Hostname**: `star-robot`
- **Network**: DHCP enabled on eth0
- **WiFi**: Manual configuration needed (see below)
- **SSH**: Enabled, accessible via network

## First Boot

1. Insert SD card into Raspberry Pi 5
2. Connect Ethernet cable (or use serial console)
3. Power on
4. Find IP: Check your router or use `nmap -sn 192.168.1.0/24`
5. SSH in: `ssh root@<ip-address>`
6. **Set root password**: `passwd`

## WiFi Setup

Connect to WiFi network:

```bash
# Create wpa_supplicant config
wpa_passphrase "YourSSID" "YourPassword" > /etc/wpa_supplicant/wpa_supplicant-wlan0.conf

# Start WiFi
ifup wlan0
```

Or edit `/etc/network/interfaces`:
```
auto wlan0
iface wlan0 inet dhcp
    wpa-conf /etc/wpa_supplicant/wpa_supplicant-wlan0.conf
```

## Adding Packages

### Method 1: Via opkg (Runtime)

```bash
opkg update
opkg install <package-name>
```

### Method 2: Via Yocto (Build-time - Recommended)

Edit `meta-star/recipes-core/images/star-minimal-image.bb`:

```bitbake
IMAGE_INSTALL:append = " \
    your-package \
    another-package \
"
```

Then rebuild:
```bash
cd yocto-rpi5
source setup-environment.sh
bitbake star-minimal-image
```

## Directory Structure

```
yocto-rpi5/
├── README.md                    # This file
├── setup-environment.sh         # Source this to setup build env
├── scripts/
│   ├── init-build.sh           # Initialize Yocto layers (run once)
│   ├── build-image.sh          # Build the image
│   └── flash-sd.sh             # Flash image to SD card
├── poky/                       # Yocto reference (cloned by init-build.sh)
├── meta-openembedded/          # Additional layers (cloned)
├── meta-raspberrypi/           # RPi BSP (cloned)
├── meta-runit/                 # Runit init (cloned)
├── meta-star/                  # Custom STAR layer
│   ├── conf/
│   │   ├── layer.conf
│   │   └── distro/
│   │       └── star-minimal.conf
│   └── recipes-core/
│       └── images/
│           └── star-minimal-image.bb
├── build/                      # Build directory (created by setup)
│   ├── conf/
│   │   ├── local.conf
│   │   └── bblayers.conf
│   └── tmp/                    # Build artifacts
├── downloads/                  # Source downloads (shared/cached)
└── sstate-cache/              # Build cache (speeds up rebuilds)
```

## Build Configuration

### Customize Build

Edit `build/conf/local.conf`:
- `BB_NUMBER_THREADS` - Parallel bitbake tasks (default: # of CPU cores)
- `PARALLEL_MAKE` - Parallel make jobs (default: # of CPU cores)
- Enable/disable features

### Add Custom Layers

```bash
cd build
bitbake-layers add-layer ../path/to/your/layer
```

## Development Workflow

### Clean Build
```bash
cd build
bitbake -c cleanall star-minimal-image
bitbake star-minimal-image
```

### Build Specific Package
```bash
bitbake <package-name>
```

### Generate SDK
```bash
bitbake star-minimal-image -c populate_sdk
```

## Troubleshooting

### Build Fails
1. Check logs in `build/tmp/work/`
2. Search error message in Yocto docs
3. Clean and rebuild: `bitbake -c cleanall <package>`

### Out of Disk Space
```bash
# Clean build artifacts
cd build
bitbake -c clean star-minimal-image

# Or nuke everything and rebuild
rm -rf build/tmp
```

### Can't SSH to Pi
- Check network connection
- Verify IP address (try serial console)
- Ensure dropbear service is running: `sv status dropbear`

## Image Output

Built images are in:
```
build/tmp/deploy/images/raspberrypi5/
```

Look for:
- `star-minimal-image-raspberrypi5.wic.bz2` - SD card image (compressed)
- `star-minimal-image-raspberrypi5.wic` - SD card image (uncompressed)

## Yocto Basics

### What is Yocto?
Build system for creating custom Linux distributions. Think "compile your own Debian" but for embedded systems.

### Why Yocto for Robotics?
- Full control over every package
- Minimal size (no bloat)
- Reproducible builds
- Security (only include what you need)
- Custom kernel configuration

### Learning Resources
- [Yocto Project Quick Start](https://docs.yoctoproject.org/brief-yoctoprojectqs/index.html)
- [Yocto Dev Manual](https://docs.yoctoproject.org/dev-manual/index.html)
- [BitBake User Manual](https://docs.yoctoproject.org/bitbake/)

## Next Steps

Once you have the system booting:

1. **Set root password** - Critical for security
2. **Configure WiFi** - For wireless access
3. **Verify Java and Kotlin** - Modern Java is pre-installed:
   - See [Java & Kotlin Setup Guide](JAVA_KOTLIN_SETUP.md)
   - Includes OpenJDK 21 (LTS)
   - Full Kotlin support (2.0+)
   - Ready for ROS2 integration

4. **Check Build Requirements** - Before building the image:
   - See [Build Requirements](BUILD_REQUIREMENTS.md)
   - OS requirements (Ubuntu 22.04 LTS recommended)
   - Hardware requirements (8GB RAM, 150GB disk minimum)
   - System dependencies and configuration

5. **Add packages incrementally** - Start with what you need for STAR robot:
   - `python3` - For scripts (included with ROS2)
   - `i2c-tools` - Already included
   - `can-utils` - For CAN bus
   - Robot-specific packages

5. **Create custom recipes** - For STAR-specific software
6. **Optimize kernel** - Remove unnecessary drivers

## Support

- Yocto docs: https://docs.yoctoproject.org/
- Raspberry Pi BSP: https://github.com/agherzan/meta-raspberrypi
- Runit: http://smarden.org/runit/

## License

See main STAR project LICENSE.
