# ESP32 WiFi Bridge Setup for Raspberry Pi 5

## Overview

This guide documents the process of setting up an ESP32-WROOM-32 as an optional WiFi bridge/controller for the Raspberry Pi 5.

**Note**: The Raspberry Pi 5 has built-in WiFi, so the ESP32 is optional. It can be used for:
- Additional wireless control channel
- Motor control relay
- Backup communication
- Custom wireless features

**Board**: ESP32-WROOM-32
**Connection**: UART (GPIO 14/15 on Raspberry Pi 5)
**Communication**: Custom serial protocol at 115200 baud

## Prerequisites

- ESP32-WROOM-32 development board
- Raspberry Pi 5 (already set up)
- Jumper wires for GPIO connections
- USB cable for ESP32 programming
- Linux laptop for development (Ubuntu 24.04)

## Phase 1: Laptop Setup (ESP-IDF Installation)

### System Requirements

- Ubuntu 24.04 LTS (or similar Linux distribution)
- Git, Python 3.12+, CMake, Ninja
- ~5GB free disk space

### Install System Dependencies

```bash
sudo apt update
sudo apt install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

**Status**: [x] Completed

### Clone ESP-IDF v5.5

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git
```

**Status**: [x] Completed

### Install ESP-IDF Tools

```bash
cd ~/esp/esp-idf
./install.sh esp32
```

This installs the ESP32 toolchain (xtensa-esp-elf, debugger, etc.) to `~/.espressif/`.

**Status**: [x] Completed

### Source ESP-IDF Environment

Before using ESP-IDF tools, you must source the environment:

```bash
. ~/esp/esp-idf/export.sh
```

Or add this to your `~/.bashrc` for automatic setup:

```bash
echo 'alias get_idf=". ~/esp/esp-idf/export.sh"' >> ~/.bashrc
```

**Status**: [IN PROGRESS] In Progress

## Phase 2: ESP-Hosted Firmware Build

(To be completed)

## Phase 3: Hardware Connection

### Physical Wiring

Connect the ESP32 to the Raspberry Pi 5 40-pin GPIO header:

```
Raspberry Pi 5          ESP32-WROOM-32
────────────────        ──────────────
Pin 8 (GPIO 14 TX)  ->  RX (GPIO3)
Pin 10 (GPIO 15 RX) <-  TX (GPIO1)
Pin 6 (GND)         --  GND
Pin 1 (3.3V)*       ->  3.3V (optional)
```

*Note: For reliable WiFi operation, consider using a separate 3.3V power supply for the ESP32.

### Enable UART on Raspberry Pi 5

```bash
ssh pi@star-robot.local

# Enable serial hardware
sudo raspi-config
# Interface Options -> Serial Port
# Login shell: No
# Serial hardware: Yes

sudo reboot
```

### Verify Connection

```bash
# Check that UART device exists
ls -l /dev/ttyAMA0

# Test with minicom
sudo apt install -y minicom
sudo minicom -D /dev/ttyAMA0 -b 115200
```

## Phase 4: Raspberry Pi 5 Communication Setup

### Install Python Serial Library

```bash
sudo apt install -y python3-serial
```

### Test Communication

Create a simple test script on the Raspberry Pi 5 to communicate with the ESP32.

## Phase 5: WiFi Configuration & Testing

(To be completed)

## Troubleshooting

(To be added as issues arise)

## References

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [Raspberry Pi 5 Setup Guide](raspberry-pi-5-setup-guide.md)
- [Raspberry Pi GPIO Pinout](https://pinout.xyz/)
- [ESP32 Datasheet](https://www.espressif.com/en/products/socs/esp32)
