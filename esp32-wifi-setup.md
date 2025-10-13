# ESP32 WiFi Setup for PYNQ-Z2

## Overview

This guide documents the process of setting up an ESP32-WROOM-32 as a WiFi adapter for the PYNQ-Z2 board using ESP-Hosted.

**Board**: ESP32-WROOM-32
**Solution**: ESP-Hosted (Official Espressif)
**Connection**: UART or SPI (configurable via Kconfig)
**Result**: PYNQ-Z2 will have `wlan0` interface for WiFi

## Prerequisites

- ESP32-WROOM-32 development board
- PYNQ-Z2 board (already set up)
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

(To be completed)

## Phase 4: PYNQ-Z2 Linux Driver Setup

(To be completed)

## Phase 5: WiFi Configuration & Testing

(To be completed)

## Troubleshooting

(To be added as issues arise)

## References

- [ESP-Hosted GitHub](https://github.com/espressif/esp-hosted)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [PYNQ-Z2 Setup Guide](pynq-z2-setup-guide.md)
