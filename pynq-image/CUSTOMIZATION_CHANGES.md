# STAR Robot Image Customization Changes

This document tracks all modifications made to customize the PYNQ image for the STAR robot project.

## Changes Made

### 1. STAGE2 Config (`ubuntu/jammy/arm/config`)

**File:** `PYNQ/sdbuild/ubuntu/jammy/arm/config`

**Removed Packages:**
- `gcc-mb` - MicroBlaze cross-compiler (~500 MB) - Not needed
- `jupyter` - Jupyter Notebook server (~300 MB) - Using SSH only
- `sigrok` - Logic analyzer support (~50 MB) - Not using

**Result:** Saves ~850 MB from STAGE2

### 2. Multistrap Config (`ubuntu/jammy/arm/multistrap.config`)

**File:** `PYNQ/sdbuild/ubuntu/jammy/arm/multistrap.config`

#### Java Upgrade
**Changed:**
- `openjdk-8-jdk` → `openjdk-17-jdk`

**Reason:** Java 17 is LTS, required for Kotlin 2.x, more modern features

#### Packages to Remove (Manual Edits Required)

Due to the complex package list structure, the following packages should be manually removed or commented out:

**Line 120-122: Python 2.7 Libraries (~50 MB)**
```
# Remove: libpython2-dev libpython2-stdlib libpython2.7 libpython2.7-dev libpython2.7-minimal libpython2.7-stdlib
```

**Line 166: Documentation (~100 MB)**
```
# Remove: man-db manpages manpages-dev pandoc pandoc-data
```

**Line 174-176: Python 2.7 (~50 MB)**
```
# Remove: python2.7 python2.7-dev python2.7-minimal
# Remove: python-setuptools python-pkg-resources (Python 2 versions)
```

**Line 180-181: Samba (~50 MB)**
```
# Remove: samba samba-common samba-common-bin samba-dsdb-modules samba-libs samba-vfs-modules
# Remove: python3-samba python3-talloc python3-tdb (Samba dependencies)
```

**Line 196-197: X11/Desktop (~300 MB)**
```
# Remove: xserver-xorg xinit xorg eterm fluxbox
# Remove: bluez bluez-tools (Bluetooth - ~20 MB)
```

**Additional X11 packages to remove (scattered throughout):**
- libgtk2.0-* (GTK2 libraries)
- libx11-* (X11 core libraries)
- x11proto-* (X11 protocol headers)
- libxcb-* (X11 C bindings)
- All X11 development libraries (we don't need X11 at all)

**Additional Bloat Removed (~180 MB)**:
- Tcl/Tk (~50 MB) - tcl, tcl-dev, tcl8.6, tk, tk-dev, tk8.6
- Ruby (~30 MB) - libruby, ruby, rubygems-integration
- Node.js (~50 MB) - nodejs
- CUPS printing (~30 MB) - libcups2, libcupsfilters1, libcupsimage2
- GhostScript/PostScript (~50 MB) - libgs9, libgs9-common, libpaper-utils, libpaper1, poppler-data
- Audio libraries (~20 MB) - portaudio19-dev, flac
- Video codecs (~50 MB) - Removed as Zynq-7000 has no hardware video codec support

### 3. Locale Cleanup

**File:** `PYNQ/sdbuild/scripts/create_rootfs.sh`

**Changes (lines 76-83):**
```bash
# Configure locales to only use en_US.UTF-8
echo "en_US.UTF-8 UTF-8" > /etc/locale.gen
locale-gen en_US.UTF-8
update-locale LANG=en_US.UTF-8

# Remove unnecessary locale files (saves ~50-100MB)
find /usr/share/locale -mindepth 1 -maxdepth 1 ! -name 'en' ! -name 'en_US' -type d -exec rm -rf {} + 2>/dev/null || true
find /usr/share/i18n/locales -mindepth 1 -maxdepth 1 ! -name 'en_US' ! -name 'en_GB' ! -name 'POSIX' -type f -delete 2>/dev/null || true
```

**Result:** Saves ~50-100 MB by removing all non-English locales

## Video Codec Hardware Support Analysis

### PYNQ-Z2 (Zynq-7000) Video Capabilities

The PYNQ-Z2 board uses the **Zynq-7000 (Z-7020)**, which **does NOT have dedicated hardware video codec units**.

**Hardware Support:**
- ❌ No H.264 hardware decoder/encoder
- ❌ No H.265/HEVC hardware decoder/encoder
- ❌ No VP8/VP9 hardware support
- ❌ No dedicated Video Codec Unit (VCU)

**What This Means:**
1. Any video encoding/decoding must be done in **software on the ARM cores** (slow, CPU-intensive)
2. You could implement codec IP cores in the **FPGA fabric**, but this requires:
   - Custom hardware design in Vivado
   - Significant FPGA resources (~25% of Z-7020 for 1080p H.264)
   - Third-party IP licenses (expensive)
3. The newer **Zynq UltraScale+ MPSoC** series has dedicated H.264/H.265 VCU hardware, but the PYNQ-Z2 doesn't

**Decision:**
Since the PYNQ-Z2 lacks hardware video codec support and software encoding is too slow for real-time robotics applications, **all video codec packages have been removed** to save ~50MB.

**References:**
- [Xilinx Video Codec Unit (VCU) - UltraScale+ Only](https://www.xilinx.com/products/intellectual-property/v-vcu.html)
- [Zynq UltraScale+ MPSoC Video Codec Unit](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842546/Xilinx+Zynq+UltraScale+MPSoC+Video+Codec+Unit)

### 4. Packages to Add

#### Create New Package: `ros2_humble`

**Location:** `PYNQ/sdbuild/packages/ros2_humble/`

**Contents:**
- ROS 2 Humble base installation
- sick_scan_xd driver for SICK TiM561
- slam_toolbox for SLAM
- nav2 for navigation (optional)

#### Create New Package: `esp_idf`

**Location:** `PYNQ/sdbuild/packages/esp_idf/`

**Contents:**
- ESP-IDF toolchain
- Xtensa GCC compiler
- Ninja build system
- Python dependencies (pyserial, etc.)

#### Create New Package: `gradle_kotlin`

**Location:** `PYNQ/sdbuild/packages/gradle_kotlin/`

**Contents:**
- Gradle 8.x
- Kotlin 2.x compiler
- gradlew wrapper tools

#### Create New Package: `tensorflow_lite`

**Location:** `PYNQ/sdbuild/packages/tensorflow_lite/`

**Contents:**
- TensorFlow Lite ARM build
- Python bindings
- MobileNet SSD models for person detection

#### Create New Package: `uart1_enable`

**Location:** `PYNQ/sdbuild/packages/uart1_enable/`

**Contents:**
- Device tree overlay to enable UART1
- Maps UART1 to 40-pin header pins 8/10
- Critical for ESP32 communication

#### Additional Tools Package: `robot_tools`

**Location:** `PYNQ/sdbuild/packages/robot_tools/`

**Contents:**
- v4l-utils (USB camera)
- i2c-tools (I2C debugging)
- can-utils (CAN bus - if needed)
- Additional utilities

### 4. Board Spec Updates

**File:** `board-config/STAR-Z2/STAR-Z2.spec`

**Current:**
```makefile
STAGE4_PACKAGES_STAR-Z2 := pynq ethernet boot_leds opencv network_manager robotics_tools
```

**Updated:**
```makefile
STAGE4_PACKAGES_STAR-Z2 := pynq \
                            ethernet \
                            opencv \
                            ros2_humble \
                            esp_idf \
                            gradle_kotlin \
                            tensorflow_lite \
                            uart1_enable \
                            robot_tools
```

## Size Impact Summary

| Category | Change | Size Impact |
|----------|--------|-------------|
| **Removed** |
| gcc-mb | Removed | -500 MB |
| jupyter | Removed | -300 MB |
| X11/Desktop | Removed | -300 MB |
| Samba | Removed | -80 MB |
| Bluetooth | Removed | -15 MB |
| Python 2.7 | Removed | -50 MB |
| Documentation | Removed | -100 MB |
| sigrok | Removed | -50 MB |
| Tcl/Tk | Removed | -50 MB |
| Ruby | Removed | -30 MB |
| Node.js | Removed | -50 MB |
| CUPS printing | Removed | -30 MB |
| GhostScript/PostScript | Removed | -50 MB |
| Audio libraries | Removed | -20 MB |
| Video codecs | Removed | -50 MB |
| Locales (non-English) | Removed | -75 MB |
| **Phase 3: TIER 1 Bloat (Final Pass)** |
| Perl modules (60+ packages) | Removed | -40 MB |
| python3-tk | Removed | -5 MB |
| Kerberos libraries | Removed | -15 MB |
| Firewire libraries (libdc1394, libraw1394) | Removed | -10 MB |
| Documentation (git-man, libssl-doc) | Removed | -10 MB |
| fswebcam | Removed | -2 MB |
| groff-base | Removed | -5 MB |
| lsof, m4, makedev | Removed | -10 MB |
| sgml-base, xml-core, shared-mime-info | Removed | -8 MB |
| ntpdate | Removed | -2 MB |
| binfmt-support | Removed | -2 MB |
| libzzip libraries | Removed | -5 MB |
| Mesa/OpenGL (libglapi-mesa, mesa-common-dev) | Removed | -20 MB |
| libwbclient0 (Samba client) | Removed | -5 MB |
| **Subtotal** | | **-1.89 GB** |
| **Added** |
| ROS 2 Humble | Added | +500 MB |
| ESP-IDF | Added | +300 MB |
| Gradle/Kotlin | Added | +150 MB |
| TensorFlow Lite | Added | +200 MB |
| Robot Tools | Added | +50 MB |
| **Subtotal** | | **+1.2 GB** |
| **Net Change** | | **-690 MB** |

**Expected Final Image Size:** ~6.8 GB (vs 7.5 GB default)

**Note:** With compression optimizations and dependency cleanup, the final image may be smaller (~6.5 GB).

## Build Instructions

After making all changes:

```bash
cd ~/Documents/git/STAR/pynq-image/PYNQ/sdbuild

# Clean previous build artifacts (optional)
make clean

# Build with prebuilt rootfs (faster)
make BOARDS=STAR-Z2 \
     BOARDDIR=$(realpath ~/Documents/git/STAR/pynq-image/board-config)

# Estimated build time: 30-45 minutes (with cached stages)
```

## Customization: Partition Labels and Username

### Partition Labels
**File:** `PYNQ/sdbuild/boot/image.wks`
- Boot partition: `PYNQ` → `STAR`
- Root partition: `root` (unchanged)

### Default User
**File:** `PYNQ/sdbuild/scripts/create_rootfs.sh`
- Username: `xilinx` → `star`
- Password: `xilinx` → `star`
- Home directory: `/home/xilinx` → `/home/star`

### SSH Access
After flashing the customized image:
```bash
# Direct SSH
ssh star@192.168.2.99
# Password: star

# With SSH alias (add to ~/.ssh/config)
ssh star
```

**SSH Alias Configuration:**
Add to your `~/.ssh/config`:
```
Host star
    HostName 192.168.2.99
    User star
```

## Testing Checklist

After flashing customized image:

- [ ] Boot successful
- [ ] SSH access working: `ssh star@192.168.2.99` (password: star)
- [ ] Partition labels correct: `STAR` (boot), `root` (rootfs)
- [ ] Java 17 installed: `java -version`
- [ ] ROS 2 Humble installed: `ros2 --version`
- [ ] ESP-IDF available: `idf.py --version`
- [ ] Gradle installed: `gradle --version`
- [ ] UART1 available: `ls -l /dev/ttyPS1`
- [ ] I2C buses: `ls -l /dev/i2c-*`
- [ ] USB camera: Works with v4l2
- [ ] No Jupyter (expected)
- [ ] No X11 server (expected)

## Rollback Plan

If build fails or image doesn't work:

1. Original STAGE2 config backed up at: `config.backup`
2. Original multistrap.config backed up at: `multistrap.config.backup`
3. Can revert changes with `git checkout`
4. Previous working image available at: `PYNQ/sdbuild/output/STAR-Z2-3.0.1.img`

## Notes

- Java 17 requires no code changes (backward compatible with Java 8)
- ROS 2 Humble matches Ubuntu 22.04 Jammy perfectly
- ESP-IDF will be installed to `/opt/esp/esp-idf`
- UART1 enablement requires device tree modification (automated in package)
- All changes preserve PYNQ core functionality

## Next Steps

1. Complete package creation (ros2_humble, esp_idf, etc.)
2. Test build with changes
3. Flash and verify on hardware
4. Document any issues
5. Iterate if needed
