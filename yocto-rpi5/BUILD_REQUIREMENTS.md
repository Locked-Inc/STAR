# Build Requirements for STAR Raspberry Pi 5 Image

This document details the operating system and hardware requirements needed to build the custom Raspberry Pi 5 image with modern Java/Kotlin support.

## Operating System Requirements

### Supported Linux Distributions

The Yocto Project Scarthgap (5.0 LTS) release is officially supported on the following 64-bit Linux distributions:

#### Recommended (Tested & Well-Supported)
- **Ubuntu 22.04 LTS (Jammy Jellyfish)** - RECOMMENDED
- **Ubuntu 24.04 LTS (Noble Numbat)**
- **Debian 12 (Bookworm)**
- **Fedora 38, 39, 40**

#### Also Supported
- **AlmaLinux 9**
- **Rocky Linux 9**
- **openSUSE Leap 15.5**

### Why Ubuntu 22.04 LTS is Recommended

Ubuntu 22.04 LTS is the recommended choice because:
- Long Term Support (LTS) until 2027
- Well-tested with Yocto builds
- Large community support
- Stable package versions
- Pre-configured for development work

## Hardware Requirements

### Minimum Requirements
- **CPU**: 4-core processor (x86_64)
- **RAM**: 8 GB
- **Disk Space**: 150 GB free
- **Network**: Stable internet connection for downloading sources

### Recommended Requirements
- **CPU**: 8-core or higher processor (x86_64)
- **RAM**: 16 GB or more
- **Disk Space**: 200 GB+ free (SSD strongly recommended)
- **Network**: Fast, stable internet connection

### Build Time Expectations

| System Configuration | First Build Time | Subsequent Builds |
|---------------------|------------------|-------------------|
| 4-core, 8GB RAM, HDD | 12-16 hours | 30-60 minutes |
| 8-core, 16GB RAM, SSD | 6-8 hours | 15-30 minutes |
| 16-core, 32GB RAM, NVMe | 3-4 hours | 10-15 minutes |

**Note**: First build downloads and compiles everything (ROS2, OpenJDK 21, kernel, etc.). Subsequent builds use cached artifacts.

## Required System Packages

### Ubuntu/Debian Installation

Run this command to install all required dependencies:

```bash
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev pylint3 xterm python3-subunit \
    mesa-common-dev zstd liblz4-tool file locales
```

### Fedora Installation

```bash
sudo dnf install -y \
    gawk make wget tar bzip2 gzip python3 unzip perl patch \
    diffutils diffstat git cpp gcc gcc-c++ glibc-devel texinfo chrpath \
    ccache perl-Data-Dumper perl-Text-ParseWords perl-Thread-Queue \
    python3-pip python3-pexpect xz which python3-GitPython python3-jinja2 \
    SDL-devel xterm rpcgen socat cpio file zstd lz4
```

## System Configuration

### Locale Settings

Ensure your system has the `en_US.UTF-8` locale configured:

```bash
# Check current locale
locale

# If en_US.UTF-8 is missing, generate it:
sudo locale-gen en_US.UTF-8
sudo update-locale LANG=en_US.UTF-8
```

### Git Configuration

Configure Git with your identity (required for some build operations):

```bash
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
```

### Disk Space Management

Monitor disk space during builds:

```bash
# Check available space
df -h

# The build uses these directories:
# - downloads/    (~15-20 GB) - Source archives (shared, cached)
# - sstate-cache/ (~20-30 GB) - Build cache (shared, cached)
# - build/tmp/    (~80-120 GB) - Build artifacts (can be deleted)
```

## Docker Alternative (Optional)

If you don't want to install dependencies on your host system, you can use Docker:

```bash
# Use official Yocto Docker image (based on Ubuntu 22.04)
docker pull crops/poky:ubuntu-22.04

# Run build in container
docker run --rm -it \
    -v $(pwd):/workdir \
    -v $(pwd)/downloads:/workdir/downloads \
    -v $(pwd)/sstate-cache:/workdir/sstate-cache \
    crops/poky:ubuntu-22.04 \
    --workdir=/workdir
```

**Note**: Docker builds may be slower due to I/O overhead.

## What Gets Built

This image includes the following modern components:

### System Components
- **Kernel**: Linux 6.6+ (Raspberry Pi 5 optimized)
- **Init System**: systemd (reliable, well-supported)
- **Package Manager**: opkg (for runtime package installation)
- **SSH Server**: dropbear (lightweight)

### Development Stack
- **Java**: OpenJDK 21 (LTS) - Latest Java LTS release
- **Kotlin**: Compatible with OpenJDK 21 (install runtime separately)
- **Python**: 3.12+ (included with ROS2)
- **ROS2**: Jazzy (latest ROS2 LTS release)

### Network
- WiFi support (BCM43455/43456 firmware)
- Ethernet support
- wpa_supplicant for wireless configuration

## Kotlin Runtime Installation

The image includes OpenJDK 21, which supports all modern Kotlin versions. To install Kotlin on the Raspberry Pi after flashing:

### Method 1: SDKMAN (Recommended)

```bash
# On the Raspberry Pi (after first boot)
curl -s "https://get.sdkman.io" | bash
source "$HOME/.sdkman/bin/sdkman-init.sh"
sdk install kotlin
```

### Method 2: Manual Installation

```bash
# Download Kotlin compiler
cd /opt
wget https://github.com/JetBrains/kotlin/releases/download/v2.0.20/kotlin-compiler-2.0.20.zip
unzip kotlin-compiler-2.0.20.zip
rm kotlin-compiler-2.0.20.zip

# Add to PATH
echo 'export PATH="/opt/kotlinc/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Method 3: Deploy Pre-compiled JARs

The most efficient approach for embedded systems:

```bash
# On your development machine
# Build your Kotlin application as a fat JAR
./gradlew shadowJar

# Deploy to Raspberry Pi
scp build/libs/your-app.jar root@192.168.2.100:/opt/star/

# Run on Pi (no Kotlin compiler needed)
ssh root@192.168.2.100 "java -jar /opt/star/your-app.jar"
```

## Kotlin Version Compatibility

OpenJDK 21 supports all modern Kotlin versions:

| Kotlin Version | Status | Notes |
|----------------|--------|-------|
| Kotlin 2.0+ | RECOMMENDED | Latest features, best performance |
| Kotlin 1.9.x | Supported | Stable, well-tested |
| Kotlin 1.8.x | Supported | Older but compatible |
| Kotlin 1.7.x and below | Legacy | May work but not recommended |

## Troubleshooting

### Build Fails with "No space left on device"

```bash
# Check disk space
df -h

# Clean build artifacts (keeps downloads and sstate-cache)
cd yocto-rpi5
source setup-environment.sh
bitbake -c cleanall star-minimal-image

# Nuclear option: remove everything except downloads
rm -rf build/tmp sstate-cache
```

### Build Fails with Package Errors

```bash
# Clean specific package
bitbake -c cleanall <package-name>

# Example for OpenJDK
bitbake -c cleanall openjdk-21

# Retry build
bitbake star-minimal-image
```

### Python Version Issues

Yocto requires Python 3.8 or later. Check your version:

```bash
python3 --version
```

If too old, upgrade your Linux distribution.

### Git Clone Fails

If cloning layers fails due to network issues:

```bash
# Set git to use HTTP instead of git:// protocol
git config --global url."https://".insteadOf git://
```

## Verification Steps

After setting up your build environment, verify everything is ready:

```bash
# 1. Check disk space (need 150GB+ free)
df -h

# 2. Check Python version (need 3.8+)
python3 --version

# 3. Check Git is configured
git config --get user.name
git config --get user.email

# 4. Initialize Yocto layers
cd yocto-rpi5
./scripts/init-build.sh

# 5. Setup build environment
source setup-environment.sh

# 6. Verify layers are loaded
bitbake-layers show-layers
```

You should see all layers including:
- meta
- meta-poky
- meta-yocto-bsp
- meta-oe
- meta-python
- meta-networking
- meta-raspberrypi
- meta-java
- meta-star

## Building the Image

Once all requirements are met:

```bash
# 1. Initialize (first time only)
cd /home/user/STAR/yocto-rpi5
./scripts/init-build.sh

# 2. Setup environment (each terminal session)
source setup-environment.sh

# 3. Build the image
./scripts/build-image.sh

# Or manually:
bitbake star-minimal-image
```

## Post-Build

After successful build, the image will be at:
```
build/tmp/deploy/images/raspberrypi5/star-minimal-image-raspberrypi5.wic.bz2
```

Flash to SD card:
```bash
sudo ./scripts/flash-sd.sh /dev/sdX
```

**WARNING**: Replace `/dev/sdX` with your actual SD card device!

## Expected Results

After flashing and booting the Raspberry Pi 5:

```bash
# SSH into the Pi
ssh root@<pi-ip-address>

# Verify Java
java -version
# Expected: openjdk version "21.0.x"

# Verify Python
python3 --version
# Expected: Python 3.12.x

# Verify ROS2
source /opt/ros/jazzy/setup.bash
ros2 --version
# Expected: ros2 cli version 0.x.x

# Test Java compilation
echo 'public class Hello { public static void main(String[] args) { System.out.println("Hello from Java 21!"); } }' > Hello.java
javac Hello.java
java Hello
# Expected: "Hello from Java 21!"
```

## Support

For issues specific to:
- **Yocto builds**: Check https://docs.yoctoproject.org/
- **Raspberry Pi**: Check https://github.com/agherzan/meta-raspberrypi
- **OpenJDK**: Check https://git.yoctoproject.org/meta-java
- **ROS2**: Check https://docs.ros.org/

## Summary

Minimum viable build environment:
- Ubuntu 22.04 LTS (or compatible)
- 8 GB RAM
- 150 GB free disk space
- Internet connection
- Required system packages installed

The build will produce a modern, minimal Raspberry Pi 5 image with:
- OpenJDK 21 (latest LTS Java)
- Full Kotlin support
- ROS2 Jazzy
- Python 3.12+
- Optimized for robotics development
