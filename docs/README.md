# STAR Robot - Development Documentation

This directory contains comprehensive guides for developing the STAR robotics project with Raspberry Pi 5.

---

## Current Platform Documentation

### [RPi5 Current] [Raspberry Pi 5 Setup Guide](../raspberry-pi-5-setup-guide.md)
**Complete setup guide for Raspberry Pi 5**

Covers:
- OS installation (Raspberry Pi OS)
- Initial configuration
- Network setup (WiFi/Ethernet)
- Installing required packages
- Hardware interface testing
- Performance tuning

**Read this first** for setting up the Raspberry Pi 5.

---

### [RPi5 Current] [Hardware Specifications](./hardware-specifications.md)
**Complete hardware specification for the STAR robot**

Covers:
- Raspberry Pi 5 specifications
- SICK TiM561 Lidar
- USB cameras
- ESP32 WiFi bridge (optional)
- Power requirements
- Bill of materials

---

### [RPi5 Current] [Hardware Testing Guide](./hardware-testing-guide.md)
**Step-by-step hardware verification procedures**

Covers:
- USB webcam testing
- UART communication
- GPIO testing
- I2C bus verification
- Network connectivity

---

### [RPi5 Current] [ESP32 WiFi Setup](../esp32-wifi-setup.md)
**Optional ESP32 WiFi bridge setup**

Covers:
- ESP32 to Raspberry Pi 5 connection
- UART configuration
- ESP-IDF setup
- Communication protocol

---

### [RPi5 Current] [SICK TiM561 ROS2 Setup Guide](./sick-tim561-ros2-setup.md)
**Complete setup guide for integrating the SICK TiM561 lidar with ROS2**

Covers:
- Hardware connection and network configuration
- Driver installation (binary and source)
- Launching and verifying data stream
- RViz2 visualization setup
- SLAM examples (Hector SLAM, SLAM Toolbox, Cartographer)
- Data recording and playback
- Integration with STAR robot

**Read this for lidar setup** - most content is platform-agnostic.

---

### [RPi5 Current] [Voltage Levels and Safety](./voltage-levels-safety.md)
**Critical safety information for GPIO connections**

Covers:
- Raspberry Pi 5 GPIO voltage levels (3.3V)
- Safe connections to ESP32 and other peripherals
- What NOT to connect to avoid damage

---

## Quick Reference (Raspberry Pi 5)

### Platform Overview
```
Raspberry Pi 5 = ARM Cortex-A76 Quad-Core @ 2.4GHz
                 + 4GB/8GB RAM
                 + Built-in WiFi/Bluetooth/Ethernet
                 + 40-pin GPIO header
```

### Storage and Persistence
| What | Persists? |
|------|-----------|
| Files on SD card | ✅ YES |
| Installed packages | ✅ YES |
| systemd services | ✅ YES |
| User data | ✅ YES |

### Development Workflow
```
1. SSH into Raspberry Pi 5
2. Edit code directly or via git
3. Test immediately
4. Changes persist automatically
5. Use systemd for auto-start services
```

### ROS Workflow
```
Robot (Raspberry Pi 5): ROS nodes publish sensor data
                             ↓ (WiFi/Ethernet)
Laptop: RViz subscribes and visualizes
```

---

## Common Questions (Raspberry Pi 5)

**Q: Do I need to rebuild an image every time I change code?**
A: No! Develop directly on the Raspberry Pi via SSH. Use standard Raspberry Pi OS.

**Q: How do I use GPIO pins?**
A: Use standard Raspberry Pi GPIO libraries (gpiozero, RPi.GPIO, lgpio). See hardware-testing-guide.md

**Q: Can I use the Zynq/FPGA features?**
A: No - Raspberry Pi 5 does not have FPGA. We're using pure software approach which is sufficient for this project.

**Q: Should I visualize lidar on the board or laptop?**
A: Laptop (RViz). Use the Pi for data processing, laptop for visualization.

**Q: Do I need the ESP32?**
A: No - Raspberry Pi 5 has built-in WiFi. ESP32 is optional for additional features.

**Q: How do I install ROS 2?**
A: See sick-tim561-ros2-setup.md - though full ROS 2 on Raspberry Pi OS requires some configuration.

---

## Getting Started Checklist (Raspberry Pi 5)

- [ ] Read raspberry-pi-5-setup-guide.md
- [ ] Flash Raspberry Pi OS to SD card
- [ ] Boot the Raspberry Pi 5 and configure network
- [ ] SSH into the board
- [ ] Test hardware interfaces (USB, GPIO, UART, I2C)
- [ ] Install required software (OpenCV, Java, etc.)
- [ ] Connect and test SICK TiM561 lidar
- [ ] Set up ROS 2 if needed for SLAM

---

## Platform Information

**Current Platform**: Raspberry Pi 5 (Broadcom BCM2712, ARM Cortex-A76 quad-core)
**OS**: Raspberry Pi OS (64-bit)

---

## Need Help?

- For Raspberry Pi 5 setup: See `../raspberry-pi-5-setup-guide.md`
- For hardware specs: See `./hardware-specifications.md`
- For testing: See `./hardware-testing-guide.md`
- For lidar: See `./sick-tim561-ros2-setup.md`
