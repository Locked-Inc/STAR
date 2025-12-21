# Building STAR RX72N Firmware

Complete guide to building the ThreadX-based firmware for RX72N.

## Quick Start

```bash
# 1. Install GNURX toolchain (see below)
# 2. Clone with submodules
git submodule update --init --recursive

# 3. Build
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
make

# Output files:
# - star-rx72n-firmware.elf  (ELF executable with debug info)
# - star-rx72n-firmware.hex  (Intel HEX for programming)
# - star-rx72n-firmware.bin  (Raw binary)
# - star-rx72n-firmware.map  (Memory map)
```

## Prerequisites

### 1. GNURX Toolchain Installation

The GNURX toolchain is a free GCC-based compiler for Renesas RX microcontrollers.

**Download:**
1. Visit https://llvm-gcc-renesas.com/rx-download-toolchains/
2. Register for free account (email verification required)
3. Download GNURX v14.2.0 or later for your platform:
   - macOS: `gcc-rx-*-macos.tar.gz`
   - Linux: `gcc-rx-*-linux.tar.gz`
   - Windows: `gcc-rx-*-windows.exe`

**Install on macOS/Linux:**
```bash
# Extract to /opt/gnurx
sudo mkdir -p /opt/gnurx
sudo tar -xzf gcc-rx-*-macos.tar.gz -C /opt/gnurx --strip-components=1

# Add to PATH (add to ~/.bashrc or ~/.zshrc)
export GNURX_ROOT=/opt/gnurx
export PATH=$GNURX_ROOT/bin:$PATH

# Verify installation
rx-elf-gcc --version
```

**Install on Windows:**
```cmd
# Run installer (default: C:\Program Files\GNURX)
# Add to PATH or set GNURX_ROOT environment variable
set GNURX_ROOT=C:\Program Files\GNURX
```

### 2. CMake

```bash
# macOS (Homebrew)
brew install cmake

# Ubuntu/Debian
sudo apt-get install cmake

# Windows
# Download from https://cmake.org/download/
```

### 3. Build Tools

```bash
# macOS
xcode-select --install

# Ubuntu/Debian
sudo apt-get install build-essential

# Windows
# Install MinGW or use Visual Studio
```

## Build Configurations

### Debug Build (default)
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
make
```
- Optimization: `-Og` (optimize for debugging)
- Debug symbols: Full (`-g3`)
- Size: Larger

### Release Build
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
make
```
- Optimization: `-O2`
- Debug symbols: Basic (`-g`)
- Size: Smaller, faster

## CLion Setup

### 1. Open Project
1. Launch CLion
2. File → Open → Select `star-rx72n-firmware` directory
3. CLion detects `CMakeLists.txt` automatically

### 2. Configure Toolchain
1. Settings (⌘,) → Build, Execution, Deployment → Toolchains
2. Add new toolchain (+ button)
3. Name: "GNURX"
4. C Compiler: `/opt/gnurx/bin/rx-elf-gcc`
5. C++ Compiler: `/opt/gnurx/bin/rx-elf-g++`
6. Make: System default
7. Debugger: `/opt/gnurx/bin/rx-elf-gdb`

### 3. Configure CMake Profile
1. Settings → Build, Execution, Deployment → CMake
2. Add profile (+ button)
3. Name: "RX72N Debug"
4. Build type: Debug
5. Toolchain: GNURX
6. CMake options: `-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-gnurx.cmake`

### 4. Build
- Build → Build Project (⌘F9)
- Or click hammer icon

## Build Output

Successful build produces:

```
build/
├── star-rx72n-firmware.elf  # Main executable
├── star-rx72n-firmware.hex  # Intel HEX format
├── star-rx72n-firmware.bin  # Raw binary
├── star-rx72n-firmware.map  # Memory map
└── CMakeFiles/               # Build artifacts
```

## Memory Usage

After building, CMake automatically runs `rx-elf-size`:

```
   text    data     bss     dec     hex filename
  12345    1234    5678   19257    4b39 star-rx72n-firmware.elf
```

- **text**: Code in Flash ROM
- **data**: Initialized data (copied from ROM to RAM at startup)
- **bss**: Uninitialized data (zero-filled in RAM)
- **dec/hex**: Total size

**RX72N Limits:**
- Flash: 4,194,304 bytes (4MB)
- RAM: 1,048,576 bytes (1MB)

## Programming the MCU

### Using Renesas E2 Lite Debugger

```bash
# Flash the HEX file
renesas-flash-programmer -d e2lite -p star-rx72n-firmware.hex
```

### Using Segger J-Link

```bash
# Create J-Link script (flash.jlink)
echo "device R5F572NN
si JTAG
speed 4000
loadfile star-rx72n-firmware.hex
r
g
q" > flash.jlink

# Flash
JLinkExe -CommandFile flash.jlink
```

## Debugging

### CLion + E2 Lite / J-Link

1. Run → Edit Configurations → Add Remote Debug
2. Name: "RX72N Debug"
3. GDB: `/opt/gnurx/bin/rx-elf-gdb`
4. Target Remote: `localhost:2331` (J-Link) or `localhost:2345` (E2 Lite)
5. Symbol file: `build/star-rx72n-firmware.elf`

**Start GDB Server:**
```bash
# J-Link
JLinkGDBServer -device R5F572NN -if JTAG -speed 4000

# E2 Lite
# Use Renesas Debug Tool or OpenOCD
```

## Troubleshooting

### "rx-elf-gcc: command not found"

Solution: GNURX toolchain not in PATH. Set `GNURX_ROOT`:
```bash
export GNURX_ROOT=/opt/gnurx
export PATH=$GNURX_ROOT/bin:$PATH
```

### "cannot find -lthreadx"

Solution: ThreadX submodule not initialized:
```bash
git submodule update --init --recursive
```

### Linker errors about missing sections

Solution: Check linker script (`linker/rx72n.ld`) matches your memory configuration.

### "Error: THREADX_ARCH not defined"

Solution: CMake toolchain file not loaded. Use:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
```

## Clean Build

```bash
# Remove build directory
rm -rf build

# Rebuild
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
make
```

## Next Steps

Once the firmware builds successfully:

1. **Test on hardware**: Flash to RX72N development board
2. **Verify LED blink**: Check LED toggles at 500ms intervals
3. **Add UART output**: Implement printf via SCI (UART)
4. **Port motor control**: Migrate from ESP32-S3 firmware
5. **Add Protocol Buffers**: Integrate nanopb for SPI communication

## References

- [GNURX Toolchain](https://llvm-gcc-renesas.com/)
- [ThreadX Documentation](https://github.com/eclipse-threadx/rtos-docs)
- [RX72N Datasheet](https://www.renesas.com/en/products/rx72n)
- [CLion Embedded Dev Guide](https://www.jetbrains.com/help/clion/embedded-overview.html)
