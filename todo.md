# STAR Project - Future Work TODO

This document tracks implementation tasks and technical debt for the STAR robotics platform.

## Critical Priority (Must Complete Before Hardware Bring-Up)

### 1. USB CDC Debug Logging Fix
**Status:** ✅ COMPLETE - All implementation and documentation finished

**Summary:**
- All 34 USB0 registers verified against RX72N Manual Ch40
- 3 critical bulk transfer issues identified and fixed:
  1. ✅ 16-bit FIFO access (was byte-by-byte - root cause of ALL failures)
  2. ✅ BCLR sequence before writes (first packet integrity)
  3. ✅ Named constants for BEMP/BRDY (code readability)
- Complete debug logging backend with boot buffering and thread safety
- Comprehensive documentation (2,430 lines across 5 deliverables)

**Completed Action Items:**
- [x] Verified all 34 USB register addresses match Ch40 memory map exactly
- [x] Fixed bulk transfer implementation (16-bit FIFO word access)
- [x] Added BCLR sequence before all FIFO writes
- [x] Implemented comprehensive error handling and statistics
- [x] Created boot log buffering (512B ring buffer)
- [x] Added thread safety (ThreadX mutex)
- [x] Documented all fixes in USB_CDC_FINAL_SUMMARY.md

**Files Created/Modified:**
- `e2-studio-star-rx72n-firmware/libs/rx_core/src/rx_log_usb.c` (680 lines - NEW)
- `e2-studio-star-rx72n-firmware/libs/rx_core/inc/rx_log.h` (+68 lines)
- `e2-studio-star-rx72n-firmware/libs/rx_usb/src/rx_usb_hw.c` (51 lines modified)
- `e2-studio-star-rx72n-firmware/USB_CDC_FINAL_SUMMARY.md` (750 lines - NEW)
- `e2-studio-star-rx72n-firmware/USB_CDC_USER_GUIDE.md` (450 lines - NEW)
- `e2-studio-star-rx72n-firmware/USB_CDC_DEVELOPER_GUIDE.md` (550 lines - NEW)
- `docs/sections/09_usb_cdc_protocol.tex` (430 lines - NEW)

**Remaining:** Hardware testing only (blocked by equipment, 4 hours estimated)

**Documentation:** See `e2-studio-star-rx72n-firmware/USB_CDC_FINAL_SUMMARY.md` for complete project summary

**Tracking:** See `/workspaces/STAR/e2-studio-star-rx72n-firmware/USB_CDC_STATUS.md` for detailed status

---

### 2. TPU Encoder Driver Implementation
**Status:** 🔴 NOT STARTED - Rear wheel encoders not functional

**Problem:**
- Current firmware only supports MTU encoders (front wheels)
- Rear wheels use TPU1/5 and TPU2/4 - NO DRIVER EXISTS
- Rear encoders (Encoder 2, Encoder 3) cannot be read

**Technical Details:**
- **Primary Encoders (Front):** MTU1, MTU2 - 32-bit counters, fully implemented
- **Secondary Encoders (Rear):** TPU1/5, TPU2/4 - 16-bit counters, NOT implemented
- TPU has same phase counting mode as MTU (Ch28 Manual line 49-51)
- 16-bit overflow at 210 RPM = 13.7 seconds (adequate for 100Hz reads)

**Action Items:**
- [ ] Create TPU HAL (`rx_tpu.h`, `rx_tpu.c`) - mimic MTU HAL structure
- [ ] Implement `rx_encoder_tpu.c` backend using existing MTU implementation as template
- [ ] Add encoder backend selection to `rx_encoder_config_t`:
  ```c
  typedef enum : uint8_t {
    k_encoder_backend_mtu = 0,  // Primary (MTU1, MTU2)
    k_encoder_backend_tpu = 1,  // Secondary (TPU1, TPU2, TPU4, TPU5)
  } encoder_backend_t;
  ```
- [ ] Extend existing 45+ unit tests to cover TPU backend
- [ ] Test overflow handling at high speed
- [ ] Verify phase counting on hardware

**Files:**
- `lib/rx_hal/inc/rx_tpu.h` - NEW - TPU HAL interface
- `lib/rx_hal/src/rx_tpu.c` - NEW - TPU HAL implementation
- `lib/rx_hal/inc/rx72n_tpu_regs.h` - NEW - TPU register definitions
- `lib/rx_encoder/src/rx_encoder_tpu.c` - NEW - TPU encoder backend
- `lib/rx_encoder/inc/rx_encoder.h` - UPDATE - Add backend selection
- `tests/test_rx_encoder.c` - UPDATE - Add TPU tests

**Reference Implementation:**
- Existing MTU encoder: `/workspaces/STAR/star-rx72n-firmware/lib/rx_encoder/src/rx_mtu_encoder.c`
- TPU Manual: `/workspaces/STAR/star-rx72n-firmware/docs/RX72N_Manual_Chapters/Ch28_16-Bit_Timer_Pulse_Unit_TPUa_.txt`

**Documentation:**
- [ ] Create `docs/sections/encoder_architecture.tex` explaining MTU vs TPU design
- [ ] Add "Primary vs Secondary" encoder concept to pinout documentation
- [ ] Update `README.md` with encoder abstraction layer

**Tracking:** See `/workspaces/STAR/RX72N_ROADMAP.md` Ch28 status

---

### 3. Motor Fault Polling Implementation
**Status:** 🟡 DESIGN CHANGE - Switched from IRQ to polling

**Background:**
- Motor fault pins (nFAULT) were moved from P44-P47 (IRQ12-15) to PC4, PC5, P50, P51
- Ultrasonic sensors now use IRQ12-15 for precise ECHO timing
- Motor faults must now be polled instead of interrupt-driven

**Design Rationale:**
- Motor faults are less time-critical than ultrasonic ECHO timing (microseconds)
- DRV8243S has thermal shutdown - not instant fault
- 1kHz polling (1ms latency) is acceptable for fault detection

**Action Items:**
- [ ] Add motor fault polling to motor control task at 1kHz
- [ ] Implement debouncing (3 consecutive reads to confirm fault)
- [ ] Add fault status to motor state structure
- [ ] Update motor fault LED pattern (LED 1, 2 blinks)
- [ ] Test fault detection latency on hardware

**Polling Implementation:**
```c
// In motor control task (1kHz loop)
void motor_fault_poll(void) {
  const rx_port_pin_t fault_pins[] = {
    k_rx_pc_5,  // Front Left Motor 0
    k_rx_pc_4,  // Front Right Motor 1
    k_rx_p5_1,  // Rear Left Motor 2
    k_rx_p5_0,  // Rear Right Motor 3
  };

  for (uint8_t i = 0; i < 4; i++) {
    bool fault_active;
    gpio_read(fault_pins[i], &fault_active);

    if (!fault_active) {  // nFAULT is active LOW
      motor_fault_debounce[i]++;
      if (motor_fault_debounce[i] >= 3) {
        motor_handle_fault(i);
      }
    } else {
      motor_fault_debounce[i] = 0;
    }
  }
}
```

**Files:**
- `src/tasks/motor_control_task.c` - Add polling loop
- `include/tasks/motor_control_task.h` - Update API

---

## High Priority (Needed for Robot Operation)

### 4. LED Status Indicator Implementation
**Status:** 🟢 DESIGN COMPLETE - Ready for implementation

**Design:** See `/workspaces/STAR/docs/LED_USAGE_PLAN.md`

**Action Items:**
- [ ] Create `src/tasks/led_status_task.c` and `include/tasks/led_status_task.h`
- [ ] Implement LED state machine with 6 states (OFF, ON, BLINK_SLOW, BLINK_FAST, PWM, PULSE, PATTERN)
- [ ] Add GPIO LED initialization to `src/hardware_init.c`
- [ ] Create task at priority 20 with 512-byte stack
- [ ] Integrate with error detection (encoder errors, motor faults, comm timeouts)
- [ ] Implement heartbeat pattern (LED 0, 1 Hz)
- [ ] Implement error code patterns (LED 1, 1-5 blinks)
- [ ] Implement motor power feedback (LED 2, PWM brightness)
- [ ] Implement communication activity pulse (LED 3, 100ms)
- [ ] Implement ultrasonic ranging pulse (LED 4, variable width)
- [ ] Implement boot sequence (LED 5, 1-4 blinks)
- [ ] Add unit tests for LED patterns
- [ ] Test on hardware PCB

**LED Pin Assignments:**
- LED 0 (PD7, pin 79): System Heartbeat
- LED 1 (PD6, pin 80): Error Indicator
- LED 2 (PD5, pin 81): Motor Status
- LED 3 (PD4, pin 82): Communication Status
- LED 4 (PD0, pin 86): Ultrasonic Status
- LED 5 (P07, pin 98): Boot/Debug

---

### 5. Ultrasonic Sensor Driver (HC-SR04)
**Status:** 🔴 NOT STARTED - Sensors assigned but no driver

**Pin Assignments (CORRECTED with IRQ):**
- Front Left (Sonar 0): P55 (TRIG), P44/IRQ12 (ECHO) - pins 39, 90
- Front Right (Sonar 1): P54 (TRIG), P45/IRQ13 (ECHO) - pins 40, 89
- Rear Left (Sonar 2): P53 (TRIG), P46/IRQ14 (ECHO) - pins 41, 88
- Rear Right (Sonar 3): P52 (TRIG), P47/IRQ15 (ECHO) - pins 42, 87

**Implementation Requirements:**
- TRIG: 10µs pulse to start ranging
- ECHO: Pulse width = distance (58µs per cm, max 25ms)
- IRQ-based timing for accurate distance measurement
- 4 independent sonar channels
- Collision detection zones (front, rear, left, right)

**Action Items:**
- [ ] Create `lib/rx_hal/inc/rx_hc_sr04.h` and `lib/rx_hal/src/rx_hc_sr04.c`
- [ ] Implement IRQ handlers for ECHO timing (IRQ12-15)
- [ ] Add distance calculation (pulse width → cm)
- [ ] Implement timeout handling (no echo = max range)
- [ ] Add multi-sensor support (round-robin triggering to avoid crosstalk)
- [ ] Create test suite with mock timing
- [ ] Test ranging accuracy on hardware

**Algorithm:**
```c
1. Set TRIG GPIO HIGH for 10µs
2. Set TRIG GPIO LOW
3. Start IRQ timer on ECHO rising edge
4. Stop IRQ timer on ECHO falling edge
5. Calculate distance = (pulse_width_us / 58)  // cm
6. If pulse_width > 25ms, return max_range
```

---

### 6. Hardware CRC32 Implementation
**Status:** 🔴 NOT STARTED - Using software CRC (performance critical)

**Problem:**
- Current nanopb protocol uses software CRC32 calculation
- SPI messages verified with CRC32 on every receive
- Software CRC adds latency to real-time communication

**Solution:**
- RX72N has hardware CRC32 calculator (Ch46)
- Offload CRC calculation to hardware peripheral
- Free up CPU cycles for motor control

**Action Items:**
- [ ] Create `lib/rx_hal/inc/rx_crc.h` and `lib/rx_hal/src/rx_crc.c`
- [ ] Implement hardware CRC32 initialization
- [ ] Add CRC32 calculation function using DTC (Data Transfer Controller)
- [ ] Replace software CRC in nanopb handlers
- [ ] Benchmark performance improvement
- [ ] Verify CRC32 matches standard polynomial

**Files:**
- `lib/rx_hal/inc/rx_crc.h` - NEW
- `lib/rx_hal/src/rx_crc.c` - NEW
- `docs/RX72N_Manual_Chapters/Ch46_CRC_Calculator.txt` - Reference

**Tracking:** See `/workspaces/STAR/RX72N_ROADMAP.md` Ch46 status

---

## Medium Priority (Quality of Life Improvements)

### 7. SPI Driver to RPi5
**Status:** 🟡 PARTIAL - Peripheral initialized but no data transfer

**Current State:**
- RSPIA configured for 10 Mbps operation
- Pin assignments: PA4 (CS), PA5 (SCLK), PA6 (COPI), PA7 (CIPO)
- No SPI receive/transmit functions implemented

**Action Items:**
- [ ] Implement SPI transmit function in `lib/rx_hal/src/spi.c`
- [ ] Implement SPI receive function with DMA
- [ ] Add nanopb message serialization/deserialization
- [ ] Create communication task for RPi5 interface
- [ ] Implement request/response protocol
- [ ] Add timeout handling for lost messages
- [ ] Test with RPi5 SPI driver

---

### 8. I2C Driver for BMS (Battery Management System)
**Status:** 🔴 NOT STARTED - No BMS communication

**Pin Assignments:**
- P12 (SMBC0, pin 34): Clock
- P13 (SMBD0, pin 33): Data
- RIIC0 configured for FM+ mode (1 MHz)

**Action Items:**
- [ ] Implement I2C driver in `lib/rx_hal/src/i2c.c`
- [ ] Add BMS protocol handler (depends on BMS chip selection)
- [ ] Implement battery voltage/current reading
- [ ] Add low battery warning (integrate with LED 1)
- [ ] Test with actual BMS hardware

---

### 9. ADC Driver for Motor Current Sensing
**Status:** 🔴 NOT STARTED - No current monitoring

**Pin Assignments:**
- P40/AN000 (pin 95): Front Left Motor Current
- P41/AN001 (pin 93): Front Right Motor Current
- P42/AN002 (pin 92): Rear Left Motor Current
- P43/AN003 (pin 91): Rear Right Motor Current

**Action Items:**
- [ ] Implement ADC driver in `lib/rx_hal/src/adc.c`
- [ ] Configure simultaneous sampling mode for all 4 channels
- [ ] Add current measurement to motor control loop
- [ ] Implement overcurrent protection
- [ ] Add current limiting in PID controller
- [ ] Test with actual motor load

---

### 10. DS18B20 Temperature Sensor Driver
**Status:** 🔴 NOT STARTED - No temperature monitoring

**Pin Assignment:**
- P05 (pin 100): 1-Wire data (DQ)

**Use Cases:**
- Ambient temperature monitoring
- Motor driver thermal management
- Battery temperature monitoring

**Action Items:**
- [ ] Implement 1-Wire protocol driver
- [ ] Add DS18B20 device driver
- [ ] Read temperature at 1 Hz
- [ ] Add thermal throttling for motor control
- [ ] Integrate with LED warnings

---

## Documentation Tasks

### 11. RX72N Peripheral Verification
**Status:** 🟡 IN PROGRESS - See `RX72N_ROADMAP.md`

**Tracking Document:** `/workspaces/STAR/RX72N_ROADMAP.md`

**Priority Peripherals:**
- ✅ Clock Generation (Ch09) - Verified, needs documentation review
- ⚠️ USB (Ch40) - Critical bug needs fix
- 🔲 Hardware CRC (Ch46) - Implement for performance
- 🔲 TPU (Ch28) - Required for rear encoders
- 🔲 ADC (Ch56) - Required for current sensing

**Action Items:**
- [ ] Verify all register addresses match Ch04 memory map
- [ ] Cross-reference initialization sequences with manual
- [ ] Add register access verification in unit tests
- [ ] Document all peripheral errata and workarounds

---

### 12. Encoder Architecture Documentation
**Status:** 🔴 NOT STARTED - Design exists but not documented

**Action Items:**
- [ ] Create `docs/sections/encoder_architecture.tex`
- [ ] Explain MTU vs TPU hardware differences
- [ ] Document "Primary vs Secondary" encoder concept
- [ ] Add overflow period calculations
- [ ] Include performance comparison table
- [ ] Add state diagrams for phase counting mode
- [ ] Cross-reference with pinout documentation

**Content:**
1. Why two encoder backends (MTU 32-bit vs TPU 16-bit)
2. Primary encoders (Front wheels, MTU1/MTU2) - better overflow period
3. Secondary encoders (Rear wheels, TPU1/5, TPU2/4) - adequate at 210 RPM
4. Overflow handling strategy (100Hz reads, 13.7s overflow period)
5. Testing strategy (unified test suite for both backends)

---

### 13. LED Pattern Documentation
**Status:** ✅ COMPLETE - See `LED_USAGE_PLAN.md`

**Document:** `/workspaces/STAR/docs/LED_USAGE_PLAN.md`

No further action needed - ready for implementation.

---

## Testing Infrastructure

### 14. Hardware-in-the-Loop (HIL) Testing
**Status:** 🔴 NOT STARTED - No automated hardware testing

**Requirements:**
- Test fixture with 4 motors, 4 encoders, 4 sonars
- Automated encoder signal generation
- Motor current measurement
- Sonar echo simulation
- Automated regression testing

**Action Items:**
- [ ] Design HIL test fixture
- [ ] Create test automation scripts
- [ ] Add CI/CD integration for hardware tests
- [ ] Document test procedures

---

### 15. Simulator Mode Expansion
**Status:** 🟢 WORKING - Simulator builds functional

**Current State:**
- Simulator mode skips PLL/PPLL polling (prevents hangs)
- UART redirected to console
- Excellent for algorithm testing

**Improvements Needed:**
- [ ] Add mock encoder inputs for testing
- [ ] Add mock motor control outputs
- [ ] Simulate ADC readings
- [ ] Mock SPI communication with RPi5
- [ ] Add timing verification tests

---

## Code Quality Tasks

### 16. Doxygen Documentation
**Status:** 🟡 PARTIAL - Some files documented, many incomplete

**Tracking:** See `DOXYGEN_ROADMAP.md`

**Priority Files:**
- [ ] All encoder driver files
- [ ] All motor control files
- [ ] All GPIO/HAL files
- [ ] Task creation functions

**Requirements:** See `/workspaces/STAR/CLAUDE.md` Doxygen section for complete tag requirements.

---

### 17. NASA Power of 10 Compliance Review
**Status:** 🟡 PARTIAL - Most rules followed, needs audit

**Rules Needing Review:**
- Rule 2: Fixed loop bounds (verify all for loops)
- Rule 5: Assertions (verify 2+ per function)
- Rule 9: Pointer use (audit function pointers for DIP)

**Action Items:**
- [ ] Audit all for loops for fixed upper bounds
- [ ] Verify all functions have preconditions/postconditions
- [ ] Review function pointer usage (Dependency Inversion)
- [ ] Run static analysis tools

---

### 18. Unit Test Coverage
**Status:** 🟡 PARTIAL - Encoder tests excellent, others minimal

**Current Coverage:**
- ✅ Encoder driver: 45+ tests, excellent coverage
- ✅ GPIO HAL: Basic tests
- ⚠️ Motor control: Minimal tests
- ❌ Communication: No tests
- ❌ Tasks: No tests

**Action Items:**
- [ ] Add motor control unit tests
- [ ] Add communication protocol tests
- [ ] Add task creation tests
- [ ] Set up code coverage reporting (gcov/lcov)
- [ ] Target 80% line coverage

---

## Long-Term Improvements

### 19. Ethernet Support (Optional)
**Status:** ⚫ NOT NEEDED - 10 Mbps SPI sufficient

**Analysis:**
- Current SPI to RPi5 provides 62x headroom
- Ethernet would require PHY chip, 9+ pins, PCB complexity
- Decision: NOT NEEDED for current requirements

**Revisit if:**
- Need for remote debugging over network
- Additional high-bandwidth sensors added
- Multiple robots on same network

---

### 20. QSPI Support (Optional)
**Status:** ⚫ NOT NEEDED - Standard SPI sufficient

**Analysis:**
- RPi5 doesn't support quad-lane SPI natively
- Current 10 Mbps SPI has ample bandwidth
- QSPI would add PCB routing complexity

**Revisit if:**
- Need for high-speed external memory
- Large data logging requirements
- Real-time camera data streaming

---

## References

- **Main Pinout:** `/workspaces/STAR/docs/sections/03_hardware_pinout.tex`
- **Text Pinout:** `/workspaces/STAR/docs/RX72N_PINOUT.txt`
- **Peripheral Status:** `/workspaces/STAR/RX72N_ROADMAP.md`
- **Feature Checklist:** `/workspaces/STAR/star-rx72n-firmware/RX72N_FEATURE_CHECKLIST.md`
- **LED Plan:** `/workspaces/STAR/docs/LED_USAGE_PLAN.md`
- **Project Instructions:** `/workspaces/STAR/CLAUDE.md`

---

## Status Legend

- ✅ COMPLETE - Fully implemented and tested
- 🟢 DESIGN COMPLETE - Ready for implementation
- 🟡 PARTIAL - Some work done, needs completion
- 🔴 NOT STARTED - Not yet begun
- ⚠️ BLOCKED - Waiting on dependency or fix
- 🚨 CRITICAL - Blocking other work
- ⚫ NOT NEEDED - Deferred or not required

---

**Last Updated:** 2026-02-05
**Document Owner:** STAR Development Team
