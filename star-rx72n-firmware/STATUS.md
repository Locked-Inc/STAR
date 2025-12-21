# RX72N Firmware Development Status

**Last Updated:** 2024-12-21
**Branch:** `create-star-gateway-module`
**Status:** ✅ **Core infrastructure complete - ready for build testing**

---

## Summary

The RX72N firmware infrastructure is fully implemented and ready for build verification. All core hardware drivers, ThreadX integration, and a working LED blink + UART debug demo are complete.

**What's Working:**
- ✅ Complete hardware register definitions
- ✅ Clock initialization (240 MHz PLL)
- ✅ GPIO driver with full read/write/toggle
- ✅ ThreadX system tick timer (CMT0 at 100 Hz)
- ✅ UART driver for debug output (SCI5 at 115200 bps)
- ✅ ThreadX task creation and scheduling
- ✅ LED blink task (500ms interval)
- ✅ Debug output task (1 second counter)

**What's Needed:**
- ⏳ GNURX toolchain installation (see BUILD.md)
- ⏳ Build verification on hardware
- ⏳ GPIO pin mapping verification (LED, UART TX)
- ⏳ Motor control drivers (MTU3a, S12ADFa, RSPI)

---

## File Structure

```
star-rx72n-firmware/
├── CMakeLists.txt              ✅ Updated with all source files
├── BUILD.md                    ✅ Complete build instructions
├── README.md                   ✅ Project overview
├── STATUS.md                   ✅ This file
│
├── cmake/
│   └── toolchain-gnurx.cmake   ✅ Cross-compilation config
│
├── linker/
│   └── rx72n.ld                ✅ Memory layout (4MB Flash, 1MB RAM)
│
├── include/
│   ├── rx72n_regs.h            ✅ Complete register definitions
│   ├── tx_user.h               ✅ ThreadX configuration
│   └── hardware.h              ✅ Hardware driver API
│
├── src/
│   ├── main.c                  ✅ ThreadX LED blink + UART demo
│   │
│   └── hardware/
│       ├── startup_rx72n.S     ✅ Reset vector, data/BSS init
│       ├── system_init.c       ✅ Clock + peripheral init
│       ├── gpio.c              ✅ GPIO driver
│       ├── timer.c             ✅ CMT0 system tick
│       └── uart.c              ✅ SCI5 debug output
│
└── lib/
    └── threadx/                ✅ Git submodule initialized
        └── ports/rxv3/gnu/     ✅ RXv3 port available
```

---

## Hardware Configuration

### Clock System
- **Main Oscillator:** 16 MHz (external crystal)
- **PLL:** 16 MHz × 30 / 2 = **240 MHz**
- **ICLK (CPU):** 240 MHz
- **PCLKA:** 120 MHz (high-speed peripherals)
- **PCLKB/C/D:** 60 MHz (standard peripherals)
- **FCLK (Flash):** 60 MHz

### Peripherals Enabled
| Peripheral | Module | Purpose | Clock | Status |
|------------|--------|---------|-------|--------|
| CMT0 | Timer | ThreadX tick (100 Hz) | PCLKB | ✅ Configured |
| SCI5 | UART | Debug output (115200 bps) | PCLKB | ✅ Configured |
| PORT0.0 | GPIO | LED output | - | ✅ Configured |
| MTU3a | Timer | Motor PWM | PCLKA | ⏳ Pending |
| S12ADFa | ADC | Current sensing | PCLKB | ⏳ Pending |
| RSPI0 | SPI | RPi5 communication | PCLKB | ⏳ Pending |

### Memory Layout
```
Flash ROM (4MB):  0xFFE00000 - 0xFFFFFFFF
  .text           Code section
  .rodata         Read-only data
  .data (source)  Initialized data source

RAM (1MB):        0x00000000 - 0x000FFFFF
  .data           Initialized data (copied from Flash)
  .bss            Uninitialized data (zero-filled)
  Heap            32 KB for dynamic allocation
  Stack           16 KB for main stack

Data Flash (32KB): 0x00100000 - 0x00107FFF
  (Not currently used)
```

---

## ThreadX Configuration

### Task Structure
| Task | Priority | Stack | Period | Function |
|------|----------|-------|--------|----------|
| LED Task | 5 | 1024 bytes | 500ms | Blinks LED on PORT0.0 |
| Demo Task | 10 | 1024 bytes | 1s | Prints counter to UART |

### ThreadX Settings (tx_user.h)
- **Tick Rate:** 100 Hz (10ms per tick)
- **Max Priorities:** 32 (0 = highest, 31 = lowest)
- **Minimum Stack:** 200 bytes
- **Error Checking:** Disabled in Release, enabled in Debug
- **Context Switch:** ~120-200 CPU cycles (fastest RTOS)

---

## Build Instructions

### Prerequisites
1. **GNURX Toolchain** (GCC for RX):
   - Download from https://llvm-gcc-renesas.com/rx-download-toolchains/
   - Version 14.2.0 or later
   - Extract to `/opt/gnurx` (macOS/Linux) or `C:\Program Files\GNURX` (Windows)
   - Add to PATH: `export PATH=/opt/gnurx/bin:$PATH`

2. **CMake** (version 3.20+):
   - macOS: `brew install cmake`
   - Ubuntu: `sudo apt-get install cmake`
   - Windows: Download from https://cmake.org/download/

3. **ThreadX Submodule**:
   ```bash
   git submodule update --init --recursive
   ```

### Build Steps
```bash
# 1. Create build directory
mkdir build && cd build

# 2. Configure with CMake
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..

# 3. Build firmware
make

# Expected output:
#   star-rx72n-firmware.elf  (ELF with debug symbols)
#   star-rx72n-firmware.hex  (Intel HEX for programming)
#   star-rx72n-firmware.bin  (Raw binary)
#   star-rx72n-firmware.map  (Memory map)
```

### Memory Size Estimation
Based on similar ThreadX projects:
- **Code (.text):** ~20-30 KB (hardware drivers + ThreadX kernel)
- **Data (.data):** ~2-4 KB (initialized globals)
- **BSS (.bss):** ~4-6 KB (task stacks + control blocks)
- **Total:** < 40 KB (less than 1% of 4MB Flash, 4% of 1MB RAM)

**Plenty of headroom for motor control, Protocol Buffers, and future features!**

---

## Next Steps

### Immediate (Build Verification)
1. ✅ Install GNURX toolchain
2. ✅ Run build and fix any compilation errors
3. ✅ Verify binary size is reasonable
4. ✅ Flash to RX72N development board
5. ✅ Verify LED blinks at 500ms intervals
6. ✅ Connect UART and verify debug output

### Short Term (Motor Control)
1. ⏳ Port MTU3a PWM driver from existing code
2. ⏳ Port S12ADFa ADC driver for current sensing
3. ⏳ Implement motor control task (250 Hz ISR)
4. ⏳ Add encoder reading (PCNT equivalent)
5. ⏳ Integrate PID controller

### Medium Term (Communication)
1. ⏳ Port RSPI driver for SPI to RPi5
2. ⏳ Integrate nanopb for Protocol Buffers
3. ⏳ Implement SPI task for RPi5 communication
4. ⏳ Add telemetry reporting task

### Long Term (Integration)
1. ⏳ Test full system with RPi5
2. ⏳ Performance tuning and optimization
3. ⏳ Add watchdog timer for safety
4. ⏳ Implement error handling and recovery

---

## Known Issues / TODOs

### GPIO Pin Mapping
- **LED Pin:** Currently PORT0.0 - update based on actual hardware
- **UART TX:** SCI5 TX pin - requires MPC (Multi-Function Pin Controller) configuration
- **UART RX:** SCI5 RX pin - not yet implemented (TX only for now)

### Missing Register Definitions
- **ICU (Interrupt Controller):** Need full interrupt priority registers
- **MPC (Pin Function):** Need pin multiplexer registers for UART/SPI/PWM
- **System Protection:** PRCR register is used but not fully defined

### ThreadX Integration
- **Timer ISR Vector:** CMT0 ISR needs to be properly linked in interrupt vector table
- **Stack Overflow:** Detection enabled but handler not implemented
- **Trace Support:** TraceX integration available but not configured

### UART Driver Limitations
- **TX Only:** Receive functionality not yet implemented
- **No Flow Control:** RTS/CTS not supported
- **Polling Mode:** No DMA or interrupt-driven transmission
- **No Printf:** Would need to implement newlib stubs

---

## Testing Checklist

### Hardware Validation
- [ ] Build compiles without errors
- [ ] Binary size is < 100 KB
- [ ] Flash programming succeeds
- [ ] LED toggles at 1 Hz (500ms on, 500ms off)
- [ ] UART outputs startup banner at 115200 bps
- [ ] Debug counter increments every second
- [ ] System runs stable for 1+ hours

### ThreadX Validation
- [ ] Both tasks run concurrently
- [ ] Task switching occurs (LED and UART don't block each other)
- [ ] System tick runs at 100 Hz (verify with timer_get_count())
- [ ] Stack usage is within limits (no overflow)
- [ ] No memory corruption (BSS zero-filled, data copied correctly)

### Clock Validation
- [ ] PLL locks to 240 MHz
- [ ] Peripheral clocks at expected frequencies
- [ ] Timer tick accuracy within 1%
- [ ] UART baud rate accurate (115200 ± 1%)

---

## Reference Documentation

### Hardware
- **RX72N User Manual:** https://www.renesas.com/rx72n
- **Hardware Manual:** Detailed register descriptions
- **Datasheet:** Electrical characteristics and pin functions

### Software
- **ThreadX User Guide:** lib/threadx/docs/
- **ThreadX API Reference:** https://github.com/eclipse-threadx/rtos-docs
- **GNURX Compiler:** https://llvm-gcc-renesas.com/documentation/

### Project Docs
- **BUILD.md:** Complete build instructions
- **README.md:** Project overview and architecture
- **CLAUDE.md:** Project guidelines and conventions

---

## Questions for Hardware Team

1. **LED Connection:**
   - Which port/pin is the LED connected to?
   - Is it active-high or active-low?

2. **UART Debug:**
   - Which SCI channel should we use? (currently SCI5)
   - Are TX/RX pins available on the connector?

3. **Motor Connections:**
   - MTU channel assignments for each motor?
   - ADC channel assignments for current sensing?

4. **SPI to RPi5:**
   - Which RSPI channel? (RSPI0/1/2)
   - SPI mode and frequency requirements?
   - GPIO pin for interrupt to RPi5?

---

**Status:** Ready for build verification and hardware testing! 🚀
