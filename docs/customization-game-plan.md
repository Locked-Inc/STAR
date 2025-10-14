# STAR Robot Image Customization Game Plan

## Current Status
- ✅ Built default PYNQ image: 7.5 GB
- ✅ Image location: `/home/bsikar/Documents/git/STAR/pynq-image/PYNQ/sdbuild/output/STAR-Z2-3.0.1.img`
- ✅ Flashed and tested on board
- ✅ Hardware testing completed:
  - USB ports working (FTDI FT232RL detected)
  - I2C buses available (2 buses: /dev/i2c-0, /dev/i2c-1)
  - Ethernet working (SSH via 192.168.2.99)
  - **UART1 NOT enabled** (40-pin pins 8/10 not configured)
- 📋 Next: Customize image with all requirements + enable UART1

## Your Requirements

### Hardware Setup
1. **STAR-Z2 Board** (Zynq-7000)
2. **SICK TiM561 Lidar** (Ethernet connection)
3. **USB Camera** (for person detection)
4. **ESP32** (connected to 40-pin header for WiFi and motor control)
5. **Arduino Shield** (on board)
6. **40-pin Raspberry Pi Header** (on board)

### Software Stack
1. **Java/Kotlin/Gradle**
   - Java 17 (LTS) - need to upgrade from Java 8
   - Kotlin 2.x
   - Gradle with gradlew script

2. **ESP-IDF** (ESP32 development)
   - ESP-IDF toolchain
   - Serial communication to ESP32
   - WiFi control via ESP32
   - Motor control commands to ESP32

3. **Python**
   - Person detection (TensorFlow Lite or similar)
   - OpenCV for camera processing
   - NumPy/SciPy

4. **ROS 2**
   - ROS 2 Humble
   - sick_scan driver for TiM561
   - SLAM processing
   - Autonomous navigation

5. **FPGA Acceleration**
   - PYNQ library for PS-PL communication
   - Custom SLAM acceleration in FPGA fabric
   - AXI interfaces

## What to Remove (Bloat)

### High Priority Removals (~1.5 GB savings):

1. **gcc-mb** (~500 MB)
   - MicroBlaze soft processor compiler
   - You don't need MicroBlaze (soft CPU in FPGA)
   - You're using ARM processors and custom FPGA logic

2. **Jupyter Notebook** (~300 MB)
   - Web-based notebook interface
   - Not needed for embedded robotics
   - Using SSH/terminal only

3. **X11/Desktop Environment** (~300 MB)
   - xserver-xorg, xinit, xorg
   - fluxbox, eterm
   - All GTK2, Cairo, Pango GUI libraries
   - libx11-*, libxcb-*, x11proto-* packages
   - You're not using a display

4. **Samba** (~50 MB)
   - File sharing server
   - Not needed

5. **Bluetooth** (~20 MB)
   - bluez, bluez-tools
   - Using ESP32 for WiFi, not Bluetooth

6. **Python 2.7** (~50 MB)
   - End of life, not needed
   - python2.7, libpython2-*

7. **Documentation** (~100 MB)
   - man-db, manpages
   - pandoc, texinfo, tex-common

8. **Audio/Video Codecs** (~40 MB)
   - libavcodec, libavformat, libswresample
   - Just need camera capture, not encoding

9. **Unnecessary Build Tools** (~150 MB)
   - automake, autoconf (if building on laptop)
   - cmake (maybe keep if building on board)
   - Haskell tools: alex, happy, hscolour

10. **sigrok** (~50 MB)
    - Logic analyzer support
    - Not using logic analyzers

## What to Add (~1.3 GB)

### Required Additions:

1. **Java 17** (replace Java 8)
   - OpenJDK 17 (LTS recommended)
   - In multistrap.config: `openjdk-17-jdk` instead of `openjdk-8-jdk`

2. **Gradle** (~100 MB)
   - Build system for Java/Kotlin

3. **Kotlin** (~50 MB)
   - Kotlin 2.x compiler

4. **ESP-IDF** (~500 MB)
   - ESP32 toolchain
   - Python dependencies
   - Ninja build system
   - Serial communication tools

5. **ROS 2 Humble** (~500 MB)
   - ros-humble-ros-base
   - ros-humble-sick-scan
   - ros-humble-slam-toolbox (or cartographer)
   - ros-humble-nav2 (for autonomous navigation)

6. **TensorFlow Lite** (~100 MB)
   - For person detection
   - ARM-optimized version

7. **Additional Tools**
   - v4l-utils (USB camera)
   - ninja-build (ESP-IDF)
   - gradle wrapper tools
   - i2c-tools (for I2C debugging)

8. **Enable UART1** (NEW - Critical for ESP32)
   - Modify device tree to enable PS UART1
   - Map to 40-pin header pins 8 (TX) and 10 (RX)
   - Required for ESP32 communication at 115200 baud

## Net Result

| Item | Size | Notes |
|------|------|-------|
| Current image | 7.5 GB | Default PYNQ with bloat |
| Remove bloat | -1.5 GB | |
| Add requirements | +1.3 GB | |
| **Final optimized** | **~7.3 GB** | Fits your exact needs |

With optimization: **6.5-7 GB final image**

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│              STAR-Z2 (Zynq-7000)                   │
│                                                     │
│  ┌────────────────┐        ┌──────────────────┐   │
│  │   ARM (PS)     │◄──────►│   FPGA (PL)      │   │
│  │                │  AXI   │                  │   │
│  │  • Linux       │        │  • SLAM          │   │
│  │  • ROS 2       │        │    Acceleration  │   │
│  │  • Java/Kotlin │        │  • Custom Logic  │   │
│  │  • Python      │        │                  │   │
│  │  • Person Det. │        │                  │   │
│  └────────────────┘        └──────────────────┘   │
│         │                           │              │
│    ┌────┴─────┬──────────┬─────────┴──────┐       │
│    │          │          │                │       │
│  USB      Ethernet   40-pin       Arduino         │
│  Camera   (TiM561)   Header        Shield         │
│    │          │          │                │       │
└────┼──────────┼──────────┼────────────────┼───────┘
     │          │          │                │
     ▼          ▼          ▼                ▼
  Person    SICK       ESP32          Sensors/
  Video    Lidar    (WiFi/Motor)     Actuators
           Scanner   Control
```

## Communication Flow

1. **Lidar → ROS**: Ethernet (TiM561) → ROS sick_scan node → SLAM processing
2. **Camera → Person Detection**: USB camera → v4l2 → Python/TensorFlow → Detection results
3. **SLAM Data → FPGA**: ROS node → PYNQ MMIO → FPGA acceleration → Results back to ROS
4. **Navigation Decision → ESP32**: ROS → Serial/SPI → ESP32 → Motor control
5. **ESP32 → WiFi**: ESP32 handles WiFi communication (see esp-firmware dir)

## Implementation Steps

### Phase 1: Test Current Image ✅ COMPLETED
1. ✅ Flash 7.5 GB image to SD card
2. ✅ Boot STAR-Z2 board
3. ✅ Verify SSH access (192.168.2.99)
4. ✅ Test peripherals:
   - USB: Working (FTDI FT232RL detected as /dev/ttyUSB0)
   - I2C: 2 buses available (/dev/i2c-0, /dev/i2c-1)
   - Ethernet: Working
   - UART: ttyPS0 (console), **ttyPS1 NOT available** ← Need to enable
5. ⚠️  FPGA bitstream: Not tested yet

### Phase 2: Customize Image 📋 READY TO START
1. Create custom multistrap.config (remove bloat packages)
2. Modify STAGE2 config (remove gcc-mb, jupyter)
3. Add Java 17, Gradle, Kotlin packages
4. Create ESP-IDF package
5. Create ROS 2 package with sick_scan
6. Add TensorFlow Lite
7. **Enable UART1 in device tree** (for 40-pin header)
8. Create custom device tree overlay
9. Rebuild image (~30-45 minutes with cached stages)

### Phase 3: Hardware Integration
1. Connect SICK TiM561 lidar to Ethernet
2. Test ROS sick_scan driver
3. Connect USB camera, test v4l2
4. Implement person detection
5. Connect ESP32 to 40-pin header
6. Test ESP32 communication (UART/SPI)
7. Test motor control via ESP32

### Phase 4: FPGA Acceleration
1. Design SLAM acceleration logic in Vivado
2. Create AXI interfaces for PS-PL communication
3. Generate bitstream
4. Integrate with ROS via PYNQ Python API
5. Benchmark performance improvement

### Phase 5: Integration & Testing
1. Full system integration
2. Autonomous navigation testing
3. Performance optimization
4. Final tuning

## Configuration Files to Modify

### For Customization (Phase 2):

1. **`PYNQ/sdbuild/ubuntu/jammy/arm/multistrap.config`**
   - Remove bloat packages list
   - Change openjdk-8-jdk → openjdk-17-jdk

2. **`PYNQ/sdbuild/ubuntu/jammy/arm/config`**
   - Remove: `gcc-mb jupyter sigrok`
   - Keep: `ssl udev bootpy clear_pl_statefile`

3. **Create new packages:**
   - `packages/gradle/` - Gradle installation
   - `packages/kotlin/` - Kotlin compiler
   - `packages/esp_idf/` - ESP-IDF toolchain
   - `packages/ros/` - ROS 2 + sick_scan
   - `packages/tensorflow_lite/` - TFLite for person detection

4. **`board-config/STAR-Z2/STAR-Z2.spec`**
   - Update STAGE4_PACKAGES to include new packages

5. **Device Tree Modifications** (NEW - for UART1)
   - Locate: `board-config/STAR-Z2/petalinux_bsp/` (if exists)
   - Or create: `packages/uart1_enable/` package
   - Modify system-user.dtsi to enable UART1
   - Map UART1 to 40-pin header pins 8/10

## Notes

### Why No MicroBlaze?
- MicroBlaze = soft processor (CPU) in FPGA fabric
- You already have ARM Cortex-A9 processors (better performance)
- Your FPGA usage: custom hardware acceleration (not another CPU)
- gcc-mb compiler is 500 MB of wasted space

### Java Version Upgrade
- Java 8 is old (released 2014)
- Java 17 is LTS (Long Term Support)
- Kotlin 2.x requires Java 11+
- Available in Ubuntu 22.04 Jammy repos

### Memory Considerations
- Zynq-7000 has 512 MB RAM total
- Without Jupyter: ~150-200 MB idle RAM usage
- With ROS + your apps: ~300-400 MB usage
- Leaves room for processing

## Current Image Details
- **File**: `STAR-Z2-3.0.1.img`
- **Size**: 7.5 GB
- **Location**: `/home/bsikar/Documents/git/STAR/pynq-image/PYNQ/sdbuild/output/`
- **Ubuntu**: 22.04 Jammy (ARM)
- **Kernel**: 5.15.19-xilinx-v2022.1
- **Default credentials**: xilinx/xilinx

## Hardware Test Results Summary

### ✅ Working:
- SSH access over Ethernet (192.168.2.99)
- USB ports (FTDI FT232RL detected as /dev/ttyUSB0)
- I2C buses (2 available: /dev/i2c-0, /dev/i2c-1)
- Ethernet connectivity
- Basic system functionality

### ⚠️ Needs Configuration:
- **UART1 for 40-pin header** - NOT enabled in device tree
  - Current: Only ttyPS0 (console) available
  - Required: ttyPS1 mapped to pins 8/10 for ESP32
  - Solution: Enable in device tree during image rebuild

### 📝 ESP32 Connection Options:
1. **After customization** (recommended): Direct to 40-pin header pins 8/10 via UART1
2. **Temporary workaround**: GPIO software UART (~9600 baud) or external USB adapter

## Next Action
1. ✅ Flash current image to SD card
2. ✅ Boot and test
3. ✅ Verify peripherals
4. 📋 **START IMAGE CUSTOMIZATION** ← We are here
   - Remove bloat (Jupyter, gcc-mb, X11, etc.)
   - Add requirements (Java 17, ROS 2, ESP-IDF, etc.)
   - **Enable UART1 for ESP32**
   - Rebuild optimized image
