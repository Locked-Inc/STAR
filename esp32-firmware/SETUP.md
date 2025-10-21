# ESP32 Firmware Setup Guide

This guide covers setting up the development environment for the STAR ESP32 firmware project.

## Prerequisites

### System Requirements

- **Operating System**: Linux (Ubuntu 20.04+), macOS, or Windows (WSL2)
- **Python**: 3.9 or newer (ESP-IDF v5.5.1 requirement)
- **Git**: 2.20 or newer
- **Disk Space**: At least 2 GB free space

### Required Packages (Linux/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dng-utils libusb-1.0-0
```

### Python 3.9+ Setup (Ubuntu 20.04 Only)

Ubuntu 20.04 ships with Python 3.8, but ESP-IDF v5.5.1 requires Python 3.9 or newer. Follow these steps to install Python 3.9:

```bash
# Add deadsnakes PPA for newer Python versions
sudo apt-add-repository -y ppa:deadsnakes/ppa
sudo apt-get update

# Install Python 3.9 and required packages
sudo apt-get install -y python3.9 python3.9-venv python3.9-dev

# Verify installation
python3.9 --version  # Should show Python 3.9.5 or newer
```

**Note**: The ESP-IDF install script will automatically detect and use Python 3.9.

### Required Packages (macOS)

```bash
brew install cmake ninja dfu-util
```

## ESP-IDF Installation

### Version Information

This project uses **ESP-IDF v5.5.1**.

### Installation Steps

1. **Create ESP directory**

```bash
mkdir -p ~/esp
cd ~/esp
```

2. **Clone ESP-IDF v5.5.1**

```bash
git clone -b v5.5.1 --recursive https://github.com/espressif/esp-idf.git
```

This will take several minutes as it clones the ESP-IDF framework and all submodules.

3. **Run the install script**

```bash
cd ~/esp/esp-idf
./install.sh esp32,esp32s3
```

This installs the toolchain for ESP32 and ESP32-S3 chips. The installation will:
- Download and install the Xtensa toolchain
- Install Python dependencies
- Set up the build tools

4. **Set up environment variables**

To use ESP-IDF, you need to source the environment setup script:

```bash
source ~/esp/esp-idf/export.sh
```

**Note**: You need to run this command in every new terminal session where you want to use ESP-IDF.

5. **Verify installation**

```bash
idf.py --version
```

Expected output:
```
ESP-IDF v5.5.1
```

### Optional: Add to Shell Profile

To automatically source ESP-IDF in every terminal session, add this to your `~/.bashrc` or `~/.zshrc`:

```bash
# ESP-IDF
alias get_idf='source ~/esp/esp-idf/export.sh'
```

Then you can simply run `get_idf` to activate the environment.

## Building the Firmware

### 1. Clone the Project

If you haven't already:

```bash
cd ~/Documents/git/STAR
git clone <repository-url> esp32-firmware
cd esp32-firmware
```

### 2. Configure the Project

```bash
# Source ESP-IDF environment (if not already done)
source ~/esp/esp-idf/export.sh

# Optional: Configure via menuconfig
idf.py menuconfig
```

Key configuration options:
- **STAR WiFi Bridge Configuration** → OTA Update Configuration
- **STAR WiFi Bridge Configuration** → WiFi Configuration
- **Partition Table** → Custom partition table (already configured)

### 3. Build the Firmware

```bash
idf.py build
```

First build will take several minutes. Subsequent builds are much faster.

### 4. Flash to ESP32

```bash
# Auto-detect port and flash
idf.py flash

# Or specify port explicitly
idf.py -p /dev/ttyUSB0 flash
```

### 5. Monitor Output

```bash
idf.py monitor

# Or flash and monitor in one command
idf.py flash monitor
```

Press `Ctrl+]` to exit the monitor.

## Board Configuration

### ESP32-WROOM-32 (Default)

The default configuration targets ESP32-WROOM-32 with 4MB flash.

### ESP32-S3-WROOM-1-N16

For ESP32-S3 with 16MB flash:

```bash
idf.py set-target esp32s3
idf.py menuconfig
# Navigate to: STAR WiFi Bridge Configuration → Board Type
# Select: ESP32-S3-WROOM-1-N16
idf.py build
```

This automatically switches to the 16MB partition table.

## Running Tests

### Hardware Tests (Requires ESP32 Connected)

```bash
./run_tests.sh target
```

This will:
1. Build the test application
2. Auto-detect your ESP32 device
3. Flash the test firmware
4. Monitor test execution
5. Generate a test results summary

### Troubleshooting Test Runs

**Port Permission Issues:**
```bash
sudo chmod 666 /dev/ttyUSB0
# Or permanently:
sudo usermod -a -G dialout $USER
# (requires logout/login)
```

**Multiple Devices:**
```bash
# Specify device explicitly
export ESP_PORT=/dev/ttyUSB0
./run_tests.sh target
```

**No Device Detected:**
```bash
# Check if device is recognized
lsusb | grep -i 'CP210\|CH340\|FTDI'

# Check kernel messages
dmesg | tail -20
```

## Development Workflow

### Typical Development Cycle

```bash
# 1. Source environment
source ~/esp/esp-idf/export.sh

# 2. Make code changes
vim components/star_wifi_bridge/pynq_wifi_handler.c

# 3. Build
idf.py build

# 4. Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor

# 5. Run tests
./run_tests.sh target
```

### Code Formatting

```bash
# Format all C files
./apply_clang_format.sh

# Run pre-commit checks
./precommit.sh
```

## Project Structure

```
esp32-firmware/
├── CMakeLists.txt              # Top-level CMake configuration
├── sdkconfig.defaults          # Default ESP-IDF configuration
├── main/                       # Main application
├── components/                 # Custom components
│   ├── star_wifi_bridge/      # WiFi bridge protocol implementation
│   ├── star_bus/              # STAR bus communication
│   ├── star_bms_bq7850/       # Battery management system
│   ├── star_error_handler/    # Error handling
│   └── star_test/             # Testing framework
├── test_app/                   # Test application
├── docs/                       # Documentation
└── run_tests.sh               # Test runner script
```

## Common Issues

### Issue: `idf.py: command not found`

**Solution**: Source the ESP-IDF environment:
```bash
source ~/esp/esp-idf/export.sh
```

### Issue: `No module named 'serial'`

**Solution**: Install pyserial:
```bash
pip install pyserial
```

### Issue: Build fails with "Missing esp-mqtt submodule" or other submodule errors

This commonly happens when ESP-IDF is installed or cloned on a new system without properly initializing submodules.

**Solution**: Update ESP-IDF submodules:
```bash
cd ~/esp/esp-idf
git submodule update --init --recursive --force
```

If the esp_wifi/lib submodule is still broken after the above command:
```bash
cd ~/esp/esp-idf/components/esp_wifi
rm -rf lib
git clone https://github.com/espressif/esp32-wifi-lib.git lib
cd lib
# Checkout the commit expected by your ESP-IDF version
git fetch origin $(cd ../../.. && git ls-tree HEAD components/esp_wifi/lib | awk '{print $3}')
git checkout $(cd ../../.. && git ls-tree HEAD components/esp_wifi/lib | awk '{print $3}')
```

### Issue: Build fails with toolchain errors

**Solution**: Reinstall ESP-IDF tools:
```bash
cd ~/esp/esp-idf
./install.sh esp32,esp32s3
```

### Issue: Flash fails with "Permission denied"

**Solution**: Fix port permissions:
```bash
sudo chmod 666 /dev/ttyUSB0
```

### Issue: Wrong partition table for ESP32-S3

**Solution**: Use menuconfig to select correct board type:
```bash
idf.py menuconfig
# Navigate to: STAR WiFi Bridge Configuration → Board Type
```

## Additional Resources

- **ESP-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/en/v5.5.1/
- **ESP-IDF GitHub**: https://github.com/espressif/esp-idf
- **Project Documentation**: See `docs/README.md`
- **Protocol Guide**: See `docs/protocol/README.md`
- **OTA Updates**: See `docs/ota/README.md`

## Quick Reference

### Essential Commands

```bash
# Setup environment
source ~/esp/esp-idf/export.sh

# Configure
idf.py menuconfig

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash

# Monitor
idf.py -p /dev/ttyUSB0 monitor

# Clean build
idf.py fullclean

# Run tests
./run_tests.sh target
```

### Environment Variables

- `IDF_PATH` - Path to ESP-IDF (set by export.sh)
- `ESP_PORT` - Serial port for flashing/monitoring (optional)
- `IDF_TARGET` - Target chip (esp32 or esp32s3)

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review ESP-IDF documentation
3. Check project documentation in `docs/`
4. Review test logs in project root (generated by run_tests.sh)
