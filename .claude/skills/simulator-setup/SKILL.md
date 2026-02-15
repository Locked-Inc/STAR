---
name: simulator-setup
description: Configure e² Studio simulator for RX72N firmware logic testing without hardware
disable-model-invocation: false
allowed-tools: [Read]
context: main
---

# e² Studio Simulator Setup

Configure the Renesas e² studio simulator for testing firmware logic without real hardware.

## Purpose

The Renesas e² studio simulator allows testing firmware **logic** without real hardware:
- ✅ Algorithm validation (PID controllers, state machines)
- ✅ Protocol parsing and encoding
- ✅ Error handling paths
- ✅ Interactive debugging (breakpoints, step-through, variable inspection)
- ❌ Timing behavior (not cycle-accurate)
- ❌ Hardware peripheral specifics (clocks, USB, SPI fully functional)

**WARNING**: Simulator builds are FOR LOGIC TESTING ONLY. Always validate critical paths on real hardware before deployment.

## Building for Simulator

### Option 1: e² studio (Interactive Debugging)

**Creating Simulator Build Configuration**:
1. In e² studio, right-click project "e2-studio-star-rx72n-firmware" → Properties
2. Navigate to: C/C++ Build → Manage Configurations
3. Click "New..." button
4. Name: **"Simulator Debug"**
5. Select "Copy settings from": **"Debug"**
6. Click OK

**Adding RX_SIMULATOR_MODE Define**:
1. Still in Properties, select configuration: **"Simulator Debug"** (top dropdown)
2. Navigate to: C/C++ Build → Settings
3. Expand: **"Compiler"** → click **"Preprocessor"**
4. In "Defined symbols (-D)" section, click "Add" (green + icon)
5. Enter: **`RX_SIMULATOR_MODE`** (no value needed)
6. Click OK → Apply → Close

**Building**:
1. Project → Build Configurations → Set Active → **"Simulator Debug"**
2. Project → Build Project (Ctrl+B)
3. Verify build succeeds with warning: "RX_SIMULATOR_MODE: This build is FOR SIMULATOR ONLY"

**Launching Simulator**:
1. Run → Debug As → Renesas GDB Hardware Debugging
2. Ensure "Simulator" is selected as target device (not hardware emulator)
3. Set breakpoints, step through code, inspect variables
4. Logs appear in Console view (Window → Show View → Console)

### Option 2: CMake (Automated Testing)

```bash
cd e2-studio-star-rx72n-firmware/tests
cmake .. -DCMAKE_BUILD_TYPE=Debug  # RX_SIMULATOR_MODE auto-enabled
make -j$(nproc)
ctest --output-on-failure
```

## What Works in Simulator

- **Control flow**: All branching, loops, function calls
- **Algorithms**: PID calculations, filtering, state machines
- **Protocol logic**: Parsing, encoding, CRC validation
- **Error paths**: Timeout handling, validation failures
- **Data structures**: Struct manipulation, array operations
- **Logging**: Output to console via stdout

## What Doesn't Work in Simulator

**Clock/Oscillator**:
- External 24 MHz crystal oscillation
- PLL/PPLL lock timing (flags never set → our fix skips polling)
- Precise frequency generation

**Peripherals**:
- **USB**: Enumeration, bulk transfers (limited or no support)
- **SPI**: External device communication (no physical devices)
- **UART**: Serial transmission (our fix redirects to console)
- **ADC**: Real sensor readings (could mock with constants)
- **Timers**: Precise timing, interrupt latency
- **DMA**: Transfer behavior, timing

**Timing**:
- Not cycle-accurate (instruction-level timing)
- Interrupt latency unpredictable
- DMA timing not modeled

## Use Hardware For

- Clock tree validation (actual 240 MHz operation)
- PLL lock timing measurements
- USB enumeration and bulk transfers
- SPI communication with real devices (DRV8243, sensors)
- UART communication (actual baud rates)
- Interrupt latency verification
- DMA transfer validation
- Real-time performance analysis
- Final integration testing

## Troubleshooting

**Problem**: Simulator still hangs in clock init
- **Solution**: Verify `RX_SIMULATOR_MODE` is defined
  - Check: Project Properties → C/C++ Build → Settings → Preprocessor
  - Should see: `RX_SIMULATOR_MODE` in defined symbols list
- **Alternative**: Build from wrong configuration (use "Simulator Debug", not "Debug")

**Problem**: No log output in simulator
- **Solution**: Open Console view (Window → Show View → Console)
- Logs use stdout, not UART hardware

**Problem**: Error "undefined reference to putchar"
- **Solution**: Ensure standard library is linked (should be automatic with GNURX)
- Check linker settings if issue persists

**Problem**: Simulator runs but functions don't behave as expected
- **Cause**: Simulator limitation (peripheral not modeled, timing issue)
- **Solution**: Test on hardware - simulator is for logic, not hardware behavior

## Implementation Details

**Simulator support added in 3 files**:
1. **`rx_simulator_config.h`**: Central configuration header with `RX_IS_SIMULATOR` macro
2. **`rx_clock_power_init.c`**: Skips PLL/PPLL polling loops (prevents 0x203 timeout)
3. **`rx_log.h`**: Redirects logging to stdout (console output)

**Hardware builds unaffected**: Conditional compilation (`#if RX_IS_SIMULATOR`) eliminates simulator code at compile time. Hardware builds produce identical binaries.

## When to Use Simulator

✅ **Good for**:
- Testing PID algorithm logic before hardware integration
- Validating protocol parsing (nanopb decoding)
- Checking error handling paths
- Interactive debugging with breakpoints
- Unit testing control flow

❌ **Not good for**:
- Performance measurement
- Timing-dependent code
- Hardware peripheral integration
- Real-time behavior validation
- Final system integration testing

## Best Practices

1. **Develop logic in simulator first** - Fast iteration without hardware setup
2. **Test algorithms thoroughly** - Use breakpoints and variable inspection
3. **Validate on hardware second** - Catch timing and peripheral issues
4. **Use CMake for automated tests** - CI/CD integration
5. **Document simulator limitations** - Note any skipped validation
