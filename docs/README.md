# STAR Robot - Zynq Development Documentation

This directory contains comprehensive guides for developing with the Zynq-7000 FPGA/ARM SoC for the STAR robotics project.

## Documentation Index

### 1. [Zynq Architecture: GPIO and I2C Guide](./zynq-architecture-gpio-i2c.md)
**Understanding how FPGA and Linux work together**

Covers:
- How PS (ARM) and PL (FPGA) interact
- Three methods to access GPIO pins
- Two methods to access I2C
- Pin assignment and device tree configuration
- Complete workflow from Vivado to Linux drivers

**Read this first if you're wondering:** "How do I control a GPIO pin or use I2C?"

---

### 2. [Data Persistence and Development Workflow](./data-persistence-guide.md)
**What persists, what resets, and how to develop**

Covers:
- What's volatile vs persistent on Zynq
- SD card as persistent storage
- Developing directly on the board vs baking into image
- Creating systemd services that persist
- Daily development workflow

**Read this first if you're wondering:** "Can I develop on the board and save changes, or do I need to rebuild the image every time?"

---

### 3. [FPGA Programming Guide](./fpga-programming-guide.md)
**Complete guide to FPGA bitstream workflow**

Covers:
- FPGA is volatile (SRAM-based), not flash
- Where bitstreams are actually stored (SD card)
- Boot sequence and when FPGA gets configured
- Three methods to update your FPGA design
- Complete example from SystemVerilog to running hardware

**Read this first if you're wondering:** "How do I program the FPGA and does it persist?"

---

### 4. [ROS on Zynq Integration Guide](./ros-on-zynq-guide.md)
**Complete guide to using ROS with Zynq hardware**

Covers:
- ROS architecture basics
- How ROS nodes access hardware (GPIO, I2C, FPGA)
- Complete C++ and Python examples
- Lidar integration with ROS
- Package structure and build system
- Launch files and systemd services

**Read this first if you're wondering:** "How does ROS work with the hardware?"

---

### 5. [PYNQ Image Customization Guide](./pynq-image-customization.md)
**How to add/remove features from the base image**

Covers:
- Package system (STAGE1-4)
- Current packages in STAR-Z2 image
- How to remove Jupyter Notebook
- How to add ROS support
- Creating custom packages
- Build time considerations

**Read this first if you're wondering:** "What's installed in my image and how do I customize it?"

---

### 6. [Lidar Visualization Options](./lidar-visualization-options.md)
**Three approaches to displaying lidar point cloud data**

Covers:
- Offboard visualization (laptop + RViz) - RECOMMENDED
- FPGA-accelerated rendering (advanced)
- Software rendering (not practical)
- Complete ROS visualization setup
- Why standard robotics uses base stations

**Read this first if you're wondering:** "How do I visualize the lidar data?"

---

## Quick Reference

### Architecture Overview
```
Zynq-7000 = PS (ARM + Linux) + PL (FPGA)
                     ↕
                 AXI Bus
```

### Storage Persistence
| What | Persists? |
|------|-----------|
| Files on SD card | ✅ YES |
| FPGA configuration | ❌ NO (reloads from SD card on boot) |
| systemd services | ✅ YES (stored on SD card) |
| ROS packages | ✅ YES (if installed to SD card) |

### Development Workflow
```
1. Edit code on board (SSH) or laptop
2. Test immediately
3. Changes persist on SD card
4. Once stable, bake into image for clean deployment
```

### FPGA Workflow
```
Laptop: Vivado → Bitstream (.bit) + XSA
        ↓
Board:  Copy .bit → Load with PYNQ Overlay
        OR
        Bake into BOOT.BIN for boot-time loading
```

### ROS Workflow
```
Robot (Zynq): ROS nodes publish sensor data
                    ↓ (WiFi/Ethernet)
Laptop: RViz subscribes and visualizes
```

## Common Questions

**Q: Do I need to rebuild the image every time I change code?**
A: No! Develop directly on the board via SSH. Changes persist on SD card.

**Q: Does the FPGA lose its configuration on reboot?**
A: Yes, but it automatically reloads from SD card (BOOT.BIN) on boot.

**Q: Can I use GPIO pins?**
A: Yes! Three ways: PS GPIO (MIO), EMIO, or PL GPIO. See zynq-architecture-gpio-i2c.md

**Q: Should I visualize lidar on the board or laptop?**
A: Laptop (RViz). See lidar-visualization-options.md for why.

**Q: How do I remove Jupyter and add ROS?**
A: See pynq-image-customization.md for complete instructions.

**Q: Where do I configure which peripherals (I2C, GPIO) are available?**
A: In Vivado when creating the hardware design (XSA file). See zynq-architecture-gpio-i2c.md

## Getting Started Checklist

- [ ] Read zynq-architecture-gpio-i2c.md to understand the hardware
- [ ] Read data-persistence-guide.md to understand development workflow
- [ ] Finish current build (running now)
- [ ] Boot the board and SSH in
- [ ] Test GPIO/I2C based on examples
- [ ] Create Vivado project for your hardware needs
- [ ] Decide if you want to customize image (remove Jupyter, add ROS)
- [ ] Set up ROS on laptop for visualization

## Build Status

Current build started: 2025-10-14
Flags: `REBUILD_PYNQ_ROOTFS=1 REBUILD_PYNQ_SDIST=1`

First build is slowest (building cross-compiler, rootfs).
Subsequent builds will be much faster.

## Related Files

- **Board spec**: `../board-config/STAR-Z2/STAR-Z2.spec`
- **PYNQ packages**: `../pynq-image/PYNQ/sdbuild/packages/`
- **ARM config**: `../pynq-image/PYNQ/sdbuild/ubuntu/jammy/arm/config`

## Need Help?

All documentation is self-contained in these markdown files. They include:
- Conceptual explanations
- Complete code examples
- Step-by-step procedures
- Common pitfalls and solutions

Start with the guide that matches your immediate question, then explore related guides as needed.
