# RX72N Firmware Enum Duplicate Analysis

## Executive Summary

Analyzed 345 enum definitions across 138 production files in the RX72N firmware codebase.

**Key Findings:**
- **40 unique values** have duplicates across multiple enums
- **349 total duplicate instances** identified
- Most duplicates (85) occur for value `0`, representing "first item" or "disabled" states
- Several **hardware-specific constants** are duplicated and should be centralized

## Critical Duplicates Requiring Centralization

### 1. Protection Register Unlock Code (0xA50B / 0xA50F)

**Value: 0xA50B (42251)** - Found in **8 enums**

This is the **RX72N PRCR unlock sequence** for module stop registers. Currently duplicated across:

```c
// lib/rx_hal/src/rx_cmt.c
k_cmt_prcr_unlock = 0xA50B

// lib/rx_hal/src/riic.c
k_riic_prcr_unlock = 0xA50B

// lib/rx_hal/src/rspi.c
k_rspi_prcr_unlock = 0xA50B

// lib/rx_hal/src/adc.c
k_adc_prcr_unlock = 0xA50B

// lib/rx_hal/src/rx_mtu3a.c
k_mtu_prcr_unlock = 0xA50B

// lib/rx_hal/src/rx_gptw.c
k_gptw_prcr_unlock = 0xA50B

// lib/rx_bus/src/rx_bus_onewire.c
k_onewire_prcr_unlock = 0xA50B

// lib/rx_usb/src/rx_usb_hw.c
k_prcr_unlock = 0xA50B
```

**Value: 0xA50F (42255)** - Found in **2 enums**

```c
// lib/rx_hal/src/system_init.c
k_prcr_unlock = 0xA50F

// lib/rx_hal/src/uart.c
k_uart_prcr_unlock = 0xA50F
```

**Recommendation:**
Create `lib/rx_core/inc/rx_register_protection.h`:
```c
typedef enum {
    k_rx_prcr_unlock_prc1_prc3 = 0xA50B,  // PRC1 and PRC3 unlock
    k_rx_prcr_unlock_all       = 0xA50F,  // All protection bits unlock
    k_rx_prcr_lock             = 0xA500,  // Lock protection
} rx_prcr_values_t;
```

**Rationale:**
- Hardware-specific magic number (from RX72N datasheet)
- Used across 10 different files
- Single source of truth ensures consistency
- Self-documenting (0xA50B vs k_rx_prcr_unlock_prc1_prc3)

---

### 2. ThreadX Tick Rate (100 Hz)

**Value: 100** - Found in **8 enums**

```c
// lib/rx_bus/src/rx_bus_manager.c
k_threadx_ticks_per_second = 100

// lib/rx_obstacle_detect/src/rx_obstacle_detect.c
k_ticks_per_second = 100

// lib/rx_usb/src/rx_usb_hw.c
k_threadx_tick_rate_hz = 100

// lib/rx_usb/src/rx_usb.c
k_threadx_tick_rate_hz = 100

// lib/rx_spi_comm/src/rx_spi_comm.c
k_threadx_tick_rate_hz = 100

// lib/rx_usb_comm/src/rx_usb_comm.c
k_max_receive_iterations = 100  (different meaning!)
```

**Recommendation:**
Create `lib/rx_core/inc/rx_threadx_config.h`:
```c
typedef enum {
    k_rx_threadx_tick_rate_hz = 100,  // 10ms tick (from tx_user.h)
} rx_threadx_timing_t;
```

**Rationale:**
- System-wide constant defined in `tx_user.h`
- Used for time conversions (ms to ticks)
- Should match `TX_TIMER_TICKS_PER_SECOND` from ThreadX config
- Prevents desynchronization between RTOS config and application code

---

### 3. Milliseconds Per Second (1000)

**Value: 1000** - Found in **7 enums**

```c
// lib/rx_bus/inc/rx_bus_types.h
k_bus_manager_mutex_timeout_ms = 1000

// lib/rx_motor/src/rx_motor.c
k_motor_min_pwm_freq = 1000

// lib/rx_hal/src/adc.c
k_adc_timeout_multiplier = 1000

// lib/rx_hal/src/uart.c
k_uart_bit_time_delay_cycles = 1000

// lib/rx_bus/src/rx_bus_manager.c
k_ms_per_second = 1000

// lib/rx_usb/src/rx_usb_hw.c
k_usb_fifo_timeout_iterations = 1000

// lib/rx_spi_comm/src/rx_spi_comm.c
k_max_poll_iterations = 1000
```

**Recommendation:**
Create `lib/rx_core/inc/rx_time_constants.h`:
```c
typedef enum {
    k_rx_ms_per_second = 1000,
    k_rx_us_per_ms     = 1000,
    k_rx_ns_per_us     = 1000,
} rx_time_conversion_t;
```

**Rationale:**
- Universal time conversion constant
- Used for timeout calculations across the codebase
- Prevents magic number `1000` appearing everywhere
- Makes time conversions explicit and searchable

---

### 4. Bits Per Byte (8)

**Value: 8** - Found in **20 enums**

```c
// lib/rx_core/inc/rx_port_constants.h
k_port_shift = 8

// lib/rx_core/inc/rx_gpio_constants.h
k_pins_per_port = 8

// lib/rx_fec/src/rx_fec.c
k_fec_bits_per_byte = 8

// lib/rx_hal/src/rx_cmt.c
k_cmt_ier_bits_per_reg = 8

// lib/rx_hal/src/timer.c
k_cmt0_ier_bits_per_reg = 8

// lib/rx_hal/src/uart.c
k_uart_hex_max_digits = 8

// lib/rx_encoder/src/rx_mtu_encoder.c
PRC1 = 8  (unrelated - protection register bit!)

// lib/rx_usb/src/rx_usb_hw.c
k_icu_bits_per_ier_register = 8

// lib/rx_hcsr04/src/rx_hcsr04_hal_hw.c
k_cmt2_divider = 8

// lib/rx_harq/src/rx_harq.c
k_bits_per_byte = 8
```

**Recommendation:**
Create `lib/rx_core/inc/rx_bit_constants.h`:
```c
typedef enum {
    k_rx_bits_per_byte   = 8,
    k_rx_bits_per_word16 = 16,
    k_rx_bits_per_word32 = 32,
} rx_bit_sizes_t;
```

**Rationale:**
- Fundamental constant used across protocol, encoding, and hardware layers
- Multiple unrelated enums define this same value
- Centralization improves searchability and consistency
- Note: Some `8` values are hardware-specific and should NOT be centralized (e.g., `k_cmt2_divider`)

---

### 5. Port/Pin Constants

**Port Numbers (0-18)** - Multiple duplicates

Currently duplicated in:
- `lib/rx_core/inc/rx_port_constants.h` - `rx_port_number_t` (uses letters J=0, A=1, etc.)
- `lib/rx_hal/inc/rx_mpc.h` - `rx_port_t` (uses hex: 0x00, 0x01, ... 0x12)
- `lib/rx_hal/src/rx_mpc.c` - `mpc_port_number_t` (duplicates values)

**Example conflict:**
```c
// rx_port_number_t
J = 0,
A = 1,
// ...

// rx_port_t
k_port_0 = 0,
k_port_1 = 1,
// ...
k_port_j = 0x12,
```

**Recommendation:**
**DO NOT CENTRALIZE** - This is by design per CLAUDE.md policy:
- `lib/rx_core/inc/rx_port_constants.h` is the **single source of truth**
- Application code uses constants from this file
- Hardware register files (`rx72n_*_regs.h`) may duplicate for register access
- See CLAUDE.md section "Port/Pin Constants Policy"

---

### 6. Maximum Channel Counts

**Value: 3** - Found in **3 enums** (RIIC/RSPI channel counts)

```c
// lib/rx_bus/inc/rx_bus_types.h
k_riic_channel_count = 3
k_rspi_channel_count = 3

// lib/rx_hal/src/riic.c
k_riic_max_channels = 3

// lib/rx_hal/src/rspi.c
k_rspi_max_channels = 3
```

**Recommendation:**
**KEEP SEPARATE** - These are **hardware configuration constants**:
- `rx_bus_types.h` defines bus manager limits
- `riic.c` / `rspi.c` define HAL driver limits
- Different layers may have different limits (e.g., bus manager might support fewer channels)
- Consolidation would create tight coupling between layers

---

### 7. ADC Unit Count

**Value: 2** - Found in **2 enums**

```c
// lib/rx_bus/inc/rx_bus_types.h
k_adc_unit_count = 2

// lib/rx_hal/src/adc.c
k_adc_max_units = 2
```

**Recommendation:**
Move to `lib/rx_hal/inc/rx72n_adc_regs.h`:
```c
typedef enum {
    k_rx72n_adc_unit_count = 2,  // RX72N has 2 S12AD units
} rx72n_adc_limits_t;
```

**Rationale:**
- Hardware-specific constant (RX72N has 2 ADC units)
- Both enums represent the same hardware limit
- Should be defined once in hardware register header

---

## Legitimate Duplicates (DO NOT CENTRALIZE)

### Enum Index Values (0, 1, 2, 3...)

**85 enums** use value `0` for their first member. This is **intentional and correct**:

```c
// All legitimate - different semantic meanings
k_mtu_channel_0 = 0,           // Hardware channel 0
k_port_0 = 0,                   // Hardware port 0
k_rx_ok = 0,                    // Success return code
k_log_none = 0,                 // Logging disabled
k_cmt_channel_0 = 0,            // Timer channel 0
```

**Why NOT to centralize:**
- Each enum represents a different domain
- Value `0` has different semantic meaning in each context
- Type safety requires separate enums (can't mix `rx_err_t` with `rx_port_t`)
- Follows C enum convention (first member = 0 unless explicitly set)

### Hardware Channel Numbers

Multiple peripherals use channels 0-7:
```c
k_mtu_channel_0 = 0, ..., k_mtu_channel_7 = 7  // MTU timer channels
k_cmt_channel_0 = 0, ..., k_cmt_channel_3 = 3  // CMT timer channels
k_gptw_channel_0 = 0, ..., k_gptw_channel_3 = 3  // GPTW timer channels
```

**Why NOT to centralize:**
- Each peripheral has different channel counts
- Channel numbers map to hardware register offsets
- Type safety prevents accidentally using MTU channel ID with CMT

### USB Descriptor Constants

Multiple USB-related constants share values but have different meanings:
```c
k_usb_desc_type_device = 0x01           // Descriptor type
k_usb_desc_type_configuration = 0x02    // Different descriptor type
k_usb_class_cdc = 0x02                  // Class code (same value!)
```

**Why NOT to centralize:**
- Different namespaces in USB specification
- Type safety requires separate enums
- USB spec defines these values (can't change them)

---

## Recommendations Summary

### High Priority - Should Centralize

1. **Protection register unlock codes** (0xA50B, 0xA50F)
   - Create: `lib/rx_core/inc/rx_register_protection.h`
   - Impact: 10 files

2. **ThreadX tick rate** (100 Hz)
   - Create: `lib/rx_core/inc/rx_threadx_config.h`
   - Impact: 5 files

3. **Time conversion constants** (1000 ms/s)
   - Create: `lib/rx_core/inc/rx_time_constants.h`
   - Impact: 7 files

4. **Bits per byte** (8)
   - Create: `lib/rx_core/inc/rx_bit_constants.h`
   - Impact: 10+ files (be selective - only protocol layer)

### Medium Priority - Consider Centralizing

5. **ADC unit count** (2)
   - Move to: `lib/rx_hal/inc/rx72n_adc_regs.h`
   - Impact: 2 files

### Do NOT Centralize

- Enum index values (0, 1, 2, ...)
- Hardware channel numbers (peripheral-specific)
- USB specification constants (different namespaces)
- Port/pin numbers (already centralized per policy)

---

## Implementation Notes

### Naming Convention for Centralized Constants

```c
// GOOD: Platform prefix for hardware constants
k_rx72n_prcr_unlock_all = 0xA50F    // RX72N-specific
k_rx_bits_per_byte = 8              // Universal constant

// AVOID: Generic names
k_prcr_unlock = 0xA50F              // Unclear which platform
k_byte_size = 8                     // Ambiguous
```

### File Organization

```
lib/rx_core/inc/
├── rx_err.h                    # Error codes (existing)
├── rx_register_protection.h    # NEW: PRCR unlock codes
├── rx_threadx_config.h         # NEW: ThreadX constants
├── rx_time_constants.h         # NEW: Time conversions
└── rx_bit_constants.h          # NEW: Bit sizes
```

### Migration Strategy

1. Create new header files in `lib/rx_core/inc/`
2. Add enum definitions with Doxygen documentation
3. Update existing files to include and use new constants
4. Remove old local enum definitions
5. Run `./scripts/format_code.sh` to maintain consistency
6. Build and test to ensure no regressions

---

## Statistics

| Category | Count | Notes |
|----------|-------|-------|
| Total enums analyzed | 345 | Excluding lib/threadx and tests |
| Total enum members | 470 | Across all enums |
| Unique values with duplicates | 40 | Values appearing in 2+ enums |
| Total duplicate instances | 349 | Sum of all occurrences |
| High-priority duplicates | 4 | Should be centralized |
| Legitimate duplicates | 85+ | Index 0, channel numbers, etc. |

---

## Appendix: Full Duplicate Value List

See `enum_analysis_report.txt` for complete list of all 40 duplicate values and their occurrences.

Key duplicate values:
- **-1**: 2 occurrences (error indicators)
- **0**: 85 occurrences (first index, disabled state)
- **1-18**: Channel/port numbers (mostly legitimate)
- **100**: 8 occurrences (ThreadX tick rate + unrelated)
- **1000**: 7 occurrences (ms/s conversion)
- **0xA50B**: 8 occurrences (PRCR unlock - HIGH PRIORITY)
- **0xA50F**: 2 occurrences (PRCR unlock - HIGH PRIORITY)

---

**Generated:** 2026-01-07
**Tool:** Python enum analysis script (analyze_enums.py)
**Scope:** Production code only (excludes lib/threadx/, tests/, build/)
