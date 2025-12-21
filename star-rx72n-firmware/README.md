# STAR RX72N Firmware

ThreadX-based motor control firmware for Renesas RX72N (R5F572NNHGFP#30).

## Quick Start

**Prerequisites:** [Docker Desktop](https://www.docker.com/products/docker-desktop)

### Build Scripts

| Script | Purpose | When to Use |
|--------|---------|-------------|
| `./build.sh` | Build firmware | After code changes |
| `./clean.sh` | Clean build artifacts | Before fresh rebuild |
| `./flash.sh` | Flash to hardware | After build, with E2 Lite connected |
| `./debug.sh` | Start debugging | When troubleshooting |

### Mac / Linux
```bash
chmod +x *.sh           # First time only
./build.sh              # Build firmware
./flash.sh              # Flash to RX72N
```

### Windows (PowerShell)
```powershell
# Build
docker build -t rx72n-build .
docker run --rm -v ${PWD}:/work -w /work rx72n-build bash -c "mkdir -p build && cd build && cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake .. && make -j`$(nproc)"

# Flash (install rfp-cli first)
./flash.sh
```

### Windows (WSL2 / Git Bash)
```bash
./build.sh              # Same as Mac/Linux
./flash.sh
```

**Build Performance:**
- First build: ~3-5 min (downloads Docker image, builds ThreadX)
- Incremental: ~5-30 sec (only recompiles changed files)
- Clean rebuild: ~1-2 min

**Why was development slow?** We were reinstalling CMake/build tools every Docker run. The `Dockerfile` now caches everything.

## Build Output

- `build/star-rx72n-firmware.elf` - Executable with debug symbols (231 KB)
- `build/star-rx72n-firmware.hex` - Intel HEX for flashing (20 KB)
- `build/star-rx72n-firmware.bin` - Raw binary (~60 KB)
- `build/star-rx72n-firmware.map` - Linker memory map (195 KB)

**Memory usage:** 7 KB code + 53 KB RAM (ThreadX stacks)

## Flashing & Debugging

### Flash to Hardware

```bash
./build.sh          # Build firmware
./flash.sh          # Flash to RX72N via E2 Lite
```

**Prerequisites:** [Renesas Flash Programmer](https://www.renesas.com/us/en/software-tool/renesas-flash-programmer-programming-gui) + E2 Lite debugger

### Debug with GDB

```bash
# Terminal 1: Start GDB server
JLinkGDBServer -device R5F572NN -if JTAG -speed 4000 -port 2331

# Terminal 2: Build and debug
./build.sh          # Build firmware
./debug.sh          # Launch GDB debugger
```

**See [FLASH.md](FLASH.md) for complete flashing and debugging guide.**

## Architecture

- **MCU:** Renesas RX72N (R5F572NNHGFP#30)
  - 240 MHz RXv3 core
  - 4MB Flash, 1MB RAM, 32KB Data Flash
  - 100-pin LFQFP package (14×14mm)
  - 182 GPIO pins (no mux/decoder needed!)
- **RTOS:** ThreadX (Azure RTOS / Eclipse ThreadX)
- **Toolchain:** GNURX GCC 8.3.0 (rx-elf-gcc)

## Native Build (Advanced)

For native toolchain installation (without Docker), see [BUILD.md](BUILD.md).

## Project Structure

```
star-rx72m-firmware/
├── CMakeLists.txt           # Main CMake configuration
├── cmake/
│   ├── toolchain-gnurx.cmake  # GNURX toolchain definition
│   └── rx72m.cmake           # RX72M-specific settings
├── src/
│   ├── main.c               # Application entry point
│   ├── hardware/            # Hardware initialization
│   ├── tasks/               # ThreadX tasks
│   └── drivers/             # RX72M peripheral drivers
├── include/                 # Public headers
├── lib/
│   ├── threadx/             # ThreadX submodule (git)
│   └── star_core/           # Shared STAR libraries (from ESP32 firmware)
└── linker/
    └── rx72m.ld             # Linker script
```

## ThreadX Tasks

1. **Motor Control Task** (Priority 6, 250Hz)
   - Read encoders (direct GPIO, no mux!)
   - Read current sensors (S12ADFa ADC with PWM sync)
   - Run PID controllers
   - Update MTU3a PWM

2. **SPI Communication Task** (Priority 4)
   - Receive Protocol Buffers from RPi5
   - Decode commands
   - Send telemetry

3. **Telemetry Task** (Priority 2, 10Hz)
   - Collect motor data
   - Encode Protocol Buffers
   - Queue for SPI transmission

4. **Temperature Monitor Task** (Priority 1, 1Hz)
   - Read DS18B20 sensor
   - Check thermal limits

## Debugging

### Using Renesas E2 Lite

1. Connect E2 Lite to RX72M board
2. In CLion: Run → Edit Configurations → Add Remote Debug
3. Set GDB server: `rx-elf-gdb`
4. Connect to E2 Lite on port 2345

### Using Segger J-Link

1. Install J-Link Software Pack
2. `JLinkGDBServer -device R5F572MN -if JTAG -speed 4000`
3. In CLion: Remote Debug on `localhost:2331`

## Differences from ESP32-S3 Firmware

| Feature | ESP32-S3 | RX72N |
|---------|----------|-------|
| **GPIO** | 35 usable (with mux/decoder) | **182 direct** |
| **Encoder Mux** | 74HC4052 (sequential read) | Direct (simultaneous) |
| **Motor CS** | 74HC238 decoder | Direct GPIO |
| **ADC** | SAR ADC, manual | S12ADFa, PWM-synced |
| **PWM** | MCPWM | MTU3a |
| **RTOS** | FreeRTOS | ThreadX |

## References

- [RX72N Datasheet](https://www.renesas.com/en/products/rx72n)
- [RX72M/RX72N Differences](https://www.renesas.com/en/document/apn/differences-between-rx72mrx72n-group-and-rx71m-group-rev130)
- [ThreadX Documentation](https://github.com/eclipse-threadx/rtos-docs)
- [GNURX Toolchain](https://llvm-gcc-renesas.com/)
- [CLion Embedded Development](https://www.jetbrains.com/help/clion/embedded-overview.html)
