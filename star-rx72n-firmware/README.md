# STAR RX72M Firmware

ThreadX-based motor control firmware for Renesas RX72M (R5F572MNDDFC#30).

## Architecture

- **MCU:** Renesas RX72N (R5F572NNHGFP#30)
  - 240 MHz RXv3 core
  - 4MB Flash, 1MB RAM, 32KB Data Flash
  - 100-pin LFQFP package (14×14mm)
  - 182 GPIO pins (no mux/decoder needed!)
- **RTOS:** ThreadX (Azure RTOS / Eclipse ThreadX)
- **Toolchain:** GNURX (GCC for Renesas RX)
- **IDE:** CLion (CMake-based)

## Development Environment Setup

### 1. Install GNURX Toolchain

1. Register (free) at https://llvm-gcc-renesas.com/
2. Download GNURX toolchain v14.2.0.202511 or later
3. Install to `/opt/gnurx` (or custom location)
4. Add to environment:
   ```bash
   export GNURX_ROOT=/opt/gnurx
   export PATH=$GNURX_ROOT/bin:$PATH
   ```

### 2. Install CLion

1. Download CLion from https://www.jetbrains.com/clion/
2. Install and activate (free for educational use, or 30-day trial)

### 3. Clone ThreadX Submodule

```bash
cd star-rx72m-firmware
git submodule add https://github.com/renesas/threadx.git lib/threadx
git submodule update --init --recursive
```

### 4. Open in CLion

1. Open CLion
2. File → Open → Select `star-rx72m-firmware` directory
3. CLion will detect CMakeLists.txt and configure project
4. Select toolchain: `cmake/toolchain-gnurx.cmake`

### 5. Build

```bash
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
make
```

Or use CLion's Build button.

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
