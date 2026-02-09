# TPU Encoder Driver Implementation Plan

**Status:** 🔴 NOT STARTED - Rear wheel encoders not functional
**Priority:** 🚨 CRITICAL - Blocks rear wheel odometry and motor control
**Estimated Effort:** 16-20 hours (implementation + testing + documentation)
**Dependencies:** Existing MTU encoder implementation, RX72N Manual Ch28

---

## Executive Summary

The STAR robot uses 4 quadrature encoders (341 PPR Hall effect) for odometry and closed-loop motor control. Currently, only the **front two encoders work** because they use MTU (Multi-Function Timer Pulse Unit) channels which have a complete driver implementation. The **rear two encoders use TPU (16-bit Timer Pulse Unit)** channels which have NO driver code, making rear wheel odometry completely non-functional.

This document provides a comprehensive plan to implement the TPU encoder backend and create a hardware-agnostic encoder abstraction layer.

---

## Problem Statement

### Current Situation

| Wheel Position | Encoder ID | Hardware | Counter Width | Pin Assignment | Status |
|---------------|------------|----------|---------------|----------------|--------|
| Front Left | Encoder 0 | **MTU1** | 32-bit | P24/P25 (pins 26, 25) | ✅ **Working** |
| Front Right | Encoder 1 | **MTU2** | 32-bit | P22/PA3 (pins 28, 67) | ✅ **Working** |
| Rear Left | Encoder 2 | **TPU1/5** | 16-bit | P14/P15 (pins 31, 32) | ❌ **NOT FUNCTIONAL** |
| Rear Right | Encoder 3 | **TPU2/4** | 16-bit | PC1/PC0 (pins 45, 49) | ❌ **NOT FUNCTIONAL** |

### Impact

Without rear encoder functionality:
- ❌ No rear wheel odometry (position/velocity unknown)
- ❌ Cannot implement closed-loop rear motor control
- ❌ Robot navigation relies only on front wheels (inaccurate)
- ❌ Cannot detect rear wheel slippage
- ❌ Cannot perform 4-wheel skid-steer maneuvers
- ❌ HIL testing incomplete (only 50% of encoders tested)

### Root Cause

**TPU driver does NOT exist.** The firmware has:
- ✅ Complete MTU encoder implementation: `lib/rx_encoder/src/rx_mtu_encoder.c` (320 lines)
- ✅ MTU HAL with register definitions: `lib/rx_hal/src/rx_mtu.c`
- ✅ 45+ unit tests for MTU encoders: `tests/test_rx_encoder.c`
- ❌ **ZERO TPU code** - no HAL, no encoder backend, no tests

---

## Technical Background

### MTU vs TPU Comparison

Both peripherals support **phase counting mode** for quadrature encoders, but with key differences:

| Feature | MTU (Multi-Function Timer) | TPU (16-bit Timer Pulse Unit) |
|---------|---------------------------|------------------------------|
| **Counter Width** | 32-bit (TCNTn) | 16-bit (TCNTn) |
| **Max Count** | 4,294,967,295 | 65,535 |
| **Overflow Period @ 210 RPM** | 150 hours | **13.7 seconds** |
| **Phase Counting Mode** | Yes (Ch18 Manual) | Yes (Ch28 Manual line 49-51) |
| **Input Pins** | 2 per channel (MTIOC) | 2 per channel (TIOC) |
| **Channels Available** | MTU1, MTU2, MTU3, MTU4 | TPU0-5 (6 channels) |
| **Register Base Address** | 0x000C_1290 (MTU1) | 0x0008_8100 (TPU0) |

### Why Use Both?

**Design Rationale:**
1. **Limited MTU channels**: Only 4 MTU channels exist, and MTU3/MTU4 are reserved for PWM motor control
2. **TPU adequate for 210 RPM**: 13.7 second overflow period is plenty when reading at 100 Hz (10ms intervals)
3. **Pin availability**: Rear encoder pins (P14, P15, PC0, PC1) only connect to TPU channels
4. **Resource allocation**: MTU reserved for time-critical PWM, TPU for lower-priority phase counting

**Primary vs Secondary Encoder Concept:**
- **Primary (MTU)**: Front wheels - longer overflow period, better for high-speed operation
- **Secondary (TPU)**: Rear wheels - adequate at robot's max speed (210 RPM), saves MTU channels

### Overflow Handling

**TPU 16-bit overflow calculations** (341 PPR encoder @ 210 RPM):
```
Pulses per revolution: 341 × 4 (quadrature edges) = 1364 pulses/rev
Max RPM: 210
Pulses per second: 1364 × (210 / 60) = 4774 pulses/sec
Overflow period: 65535 / 4774 = 13.7 seconds
```

**Mitigation strategy:**
- Read encoders at **100 Hz** (10ms intervals)
- Each read period: max 47.7 pulses (< 65535)
- Track software overflow counter: 32-bit accumulator
- Detect overflow: compare current vs previous 16-bit value
- Overflow handling identical to MTU implementation

---

## Architecture Design

### Encoder Abstraction Layer

**Goal:** Unified API that works with both MTU and TPU backends transparently.

**Current Structure** (MTU-only):
```
lib/rx_encoder/
  inc/
    rx_encoder.h              # Public API (MTU-only)
  src/
    rx_mtu_encoder.c          # MTU implementation (320 lines)
  tests/
    test_rx_encoder.c         # 45+ unit tests (MTU-only)
```

**Proposed Structure** (MTU + TPU abstraction):
```
lib/rx_encoder/
  inc/
    rx_encoder.h              # Public API (hardware-agnostic)
    rx_encoder_config.h       # Config structs with backend selection
  src/
    rx_encoder.c              # Common logic + backend dispatch (NEW)
    rx_encoder_mtu.c          # MTU backend (refactored from rx_mtu_encoder.c)
    rx_encoder_tpu.c          # TPU backend (NEW, mirrors MTU)
  tests/
    test_rx_encoder.c         # Unified tests for both backends (EXTENDED)
```

### Backend Selection

**Configuration structure** with compile-time backend selection:

```c
/**
 * @enum encoder_backend_t
 * @brief Encoder hardware backend selector
 */
typedef enum : uint8_t {
    k_encoder_backend_mtu = 0,  /**< Primary: MTU1/MTU2 (32-bit, front wheels) */
    k_encoder_backend_tpu = 1,  /**< Secondary: TPU1/5, TPU2/4 (16-bit, rear wheels) */
} encoder_backend_t;

/**
 * @enum rx_tpu_channel_t
 * @brief TPU channel identifiers for encoder phase counting
 */
typedef enum : uint8_t {
    k_rx_tpu0 = 0,
    k_rx_tpu1 = 1,  /**< Used by Encoder 2 (Rear Left) */
    k_rx_tpu2 = 2,  /**< Used by Encoder 3 (Rear Right) */
    k_rx_tpu3 = 3,
    k_rx_tpu4 = 4,  /**< Used by Encoder 3 (Rear Right) - phase B */
    k_rx_tpu5 = 5,  /**< Used by Encoder 2 (Rear Left) - phase B */
} rx_tpu_channel_t;

/**
 * @struct rx_encoder_config_t
 * @brief Encoder configuration with backend selection
 */
typedef struct {
    encoder_backend_t backend;  /**< Hardware backend (MTU or TPU) */

    union {
        rx_mtu_channel_t mtu_channel;  /**< MTU channel if backend == MTU */
        struct {
            rx_tpu_channel_t tpu_channel_a;  /**< TPU channel for phase A */
            rx_tpu_channel_t tpu_channel_b;  /**< TPU channel for phase B */
        } tpu;
    };

    uint16_t counts_per_rev;    /**< Encoder pulses per revolution (typically 341 × 4) */
    bool invert_direction;      /**< Swap phase A/B to reverse direction */
} rx_encoder_config_t;
```

**Example configuration** (Rear Left encoder using TPU1/5):
```c
rx_encoder_config_t rear_left_config = {
    .backend = k_encoder_backend_tpu,
    .tpu = {
        .tpu_channel_a = k_rx_tpu1,  // P14 (pin 31)
        .tpu_channel_b = k_rx_tpu5,  // P15 (pin 32)
    },
    .counts_per_rev = 1364,  // 341 PPR × 4 edges
    .invert_direction = false,
};
```

### Backend Dispatch

**Function pointer interface** for hardware-specific operations:

```c
/**
 * @struct encoder_backend_ops_t
 * @brief Backend operation function pointers (Dependency Inversion)
 */
typedef struct {
    rx_err_t (*init)(rx_encoder_handle_t* handle, const rx_encoder_config_t* config);
    rx_err_t (*read_count)(rx_encoder_handle_t* handle, int32_t* count);
    rx_err_t (*reset)(rx_encoder_handle_t* handle);
    rx_err_t (*deinit)(rx_encoder_handle_t* handle);
} encoder_backend_ops_t;

/**
 * @struct rx_encoder_handle_t
 * @brief Encoder handle with backend dispatch
 */
typedef struct {
    encoder_backend_t backend;           /**< Active backend */
    const encoder_backend_ops_t* ops;    /**< Backend function pointers */

    union {
        rx_mtu_encoder_state_t mtu_state;  /**< MTU-specific state */
        rx_tpu_encoder_state_t tpu_state;  /**< TPU-specific state */
    };

    int32_t accumulated_count;  /**< Software 32-bit accumulator (shared) */
    uint16_t prev_count;        /**< Previous 16-bit hardware count (for overflow) */
    uint16_t counts_per_rev;    /**< Encoder resolution */
    bool initialized;           /**< Initialization flag */
} rx_encoder_handle_t;
```

**Public API** remains unchanged (hardware-agnostic):
```c
// Initialize encoder (backend selected via config)
rx_err_t rx_encoder_init(rx_encoder_handle_t* handle, const rx_encoder_config_t* config);

// Read accumulated count (32-bit software accumulator)
rx_err_t rx_encoder_read_count(rx_encoder_handle_t* handle, int32_t* count);

// Read velocity (counts per second → RPS)
rx_err_t rx_encoder_read_velocity(rx_encoder_handle_t* handle, float dt_sec, float* rps);

// Reset count to zero
rx_err_t rx_encoder_reset(rx_encoder_handle_t* handle);

// Deinitialize encoder
rx_err_t rx_encoder_deinit(rx_encoder_handle_t* handle);
```

---

## Implementation Plan

### Phase 1: TPU HAL (Hardware Abstraction Layer)

**Objective:** Create low-level TPU register access layer (mirrors existing MTU HAL).

#### Files to Create

**1. `lib/rx_hal/inc/rx72n_tpu_regs.h`** - TPU register definitions
```c
/**
 * @file rx72n_tpu_regs.h
 * @brief RX72N TPU register definitions
 */

typedef enum : uint32_t {
    k_tpu0_base_addr = 0x00088100,  /**< TPU0 base address (Ch28 Table 1.2) */
    k_tpu1_base_addr = 0x00088110,  /**< TPU1 base address */
    k_tpu2_base_addr = 0x00088120,  /**< TPU2 base address */
    k_tpu3_base_addr = 0x00088130,  /**< TPU3 base address */
    k_tpu4_base_addr = 0x00088140,  /**< TPU4 base address */
    k_tpu5_base_addr = 0x00088150,  /**< TPU5 base address */

    k_tpua_base_addr = 0x00088100,  /**< TPUA (channels 0-3) control registers */
    k_tpub_base_addr = 0x00088180,  /**< TPUB (channels 4-5) control registers */
} tpu_base_addresses_t;

/**
 * @struct tpu_channel_regs_t
 * @brief TPU channel registers (per channel)
 */
typedef struct {
    volatile uint8_t TCR;     /**< Timer Control Register */
    volatile uint8_t TMDR;    /**< Timer Mode Register */
    volatile uint8_t TIORH;   /**< Timer I/O Control Register H */
    volatile uint8_t TIORL;   /**< Timer I/O Control Register L */
    volatile uint8_t TIER;    /**< Timer Interrupt Enable Register */
    volatile uint8_t TSR;     /**< Timer Status Register */
    volatile uint16_t TCNT;   /**< Timer Counter (16-bit) */
    volatile uint16_t TGRA;   /**< Timer General Register A */
    volatile uint16_t TGRB;   /**< Timer General Register B */
    volatile uint16_t TGRC;   /**< Timer General Register C */
    volatile uint16_t TGRD;   /**< Timer General Register D */
} tpu_channel_regs_t;

/**
 * @struct tpu_control_regs_t
 * @brief TPU shared control registers
 */
typedef struct {
    volatile uint8_t TSTR;    /**< Timer Start Register */
    volatile uint8_t TSYR;    /**< Timer Synchronous Register */
} tpu_control_regs_t;
```

**2. `lib/rx_hal/inc/rx_tpu.h`** - TPU HAL interface
```c
/**
 * @file rx_tpu.h
 * @brief TPU HAL interface for phase counting mode
 */

/**
 * @enum rx_tpu_phase_mode_t
 * @brief TPU phase counting mode configuration
 */
typedef enum : uint8_t {
    k_tpu_phase_mode_1x = 0,  /**< Count on phase A only (1x resolution) */
    k_tpu_phase_mode_2x = 1,  /**< Count on both edges of phase A (2x resolution) */
    k_tpu_phase_mode_4x = 2,  /**< Count on both phases A and B (4x resolution, RECOMMENDED) */
} rx_tpu_phase_mode_t;

/**
 * @struct rx_tpu_phase_config_t
 * @brief TPU phase counting configuration
 */
typedef struct {
    rx_tpu_channel_t channel_a;  /**< Channel for phase A input */
    rx_tpu_channel_t channel_b;  /**< Channel for phase B input */
    rx_tpu_phase_mode_t mode;    /**< Phase counting mode (1x, 2x, 4x) */
    bool invert_direction;       /**< Swap phase A/B to reverse count direction */
} rx_tpu_phase_config_t;

/**
 * @brief Initialize TPU channel for phase counting mode
 *
 * @details
 * Configures two TPU channels for quadrature encoder phase counting.
 * Channel A and Channel B must be from the same unit (TPUA or TPUB).
 *
 * Valid channel pairs:
 * - TPU0 (A) + TPU3 (B) - TPUA unit
 * - TPU1 (A) + TPU5 (B) - TPUA/TPUB cascade (Rear Left encoder)
 * - TPU2 (A) + TPU4 (B) - TPUB unit (Rear Right encoder)
 *
 * @param[in] config Phase counting configuration
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success
 * @retval k_rx_err_null_ptr config is NULL
 * @retval k_rx_err_invalid_arg Invalid channel pair
 *
 * @pre TPU module clock enabled via MSTP (Ch09)
 * @post TPU channels configured for phase counting, counter running
 *
 * @note Thread-safe if different channels accessed
 * @warning Do not call for channels already initialized
 *
 * @see RX72N Manual Ch28 Section 1.2.8 "Phase Counting Mode"
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_init_phase_counting(const rx_tpu_phase_config_t* config);

/**
 * @brief Read TPU counter value (16-bit)
 *
 * @param[in] channel TPU channel to read
 * @param[out] count Current 16-bit counter value
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, count written
 * @retval k_rx_err_null_ptr count is NULL
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_not_initialized Channel not initialized
 *
 * @pre Channel initialized via rx_tpu_init_phase_counting()
 * @post count contains current 16-bit TCNT value
 *
 * @note Thread-safe for different channels
 * @note Read operation is non-blocking
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_read_count(rx_tpu_channel_t channel, uint16_t* count);

/**
 * @brief Reset TPU counter to zero
 *
 * @param[in] channel TPU channel to reset
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, counter reset
 * @retval k_rx_err_invalid_arg Invalid channel
 * @retval k_rx_err_not_initialized Channel not initialized
 *
 * @pre Channel initialized via rx_tpu_init_phase_counting()
 * @post TCNT register = 0
 *
 * @note Thread-safe for different channels
 * @warning Resets hardware counter only, does not affect software accumulator
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_reset_count(rx_tpu_channel_t channel);

/**
 * @brief Deinitialize TPU channel (stop counting)
 *
 * @param[in] channel TPU channel to deinitialize
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, channel stopped
 * @retval k_rx_err_invalid_arg Invalid channel
 *
 * @post Timer stopped, registers reset to defaults
 *
 * @since Version 1.0.0
 */
rx_err_t rx_tpu_deinit(rx_tpu_channel_t channel);
```

**3. `lib/rx_hal/src/rx_tpu.c`** - TPU HAL implementation (~250 lines)

**Key implementation notes:**
- Verify ALL register addresses against Ch28 Table 1.2 memory map
- TPU channels 0-3 use TPUA control registers, channels 4-5 use TPUB
- Phase counting mode requires cascaded channels (A and B from specific pairs)
- Set TMDR to phase counting mode (0x04 for 4x mode)
- Configure TIORH/TIORL for input capture on both edges
- Start timer via TSTR register (set channel bit)

#### Verification Checklist - Phase 1

- [ ] All register base addresses match Ch28 Table 1.2 exactly
- [ ] Register offset calculations verified against manual
- [ ] Phase counting mode configuration tested (TMDR register)
- [ ] TCNT read function atomic (no race condition)
- [ ] TSTR register manipulation correct (start/stop timer)
- [ ] Channel pair validation (e.g., TPU1+TPU5, TPU2+TPU4)
- [ ] Unit tests pass (mock register reads/writes)

---

### Phase 2: TPU Encoder Backend

**Objective:** Implement encoder driver using TPU HAL (mirrors MTU encoder backend).

#### Files to Create

**1. `lib/rx_encoder/src/rx_encoder_tpu.c`** - TPU encoder backend (~300 lines)

**Key functions:**

```c
/**
 * @brief Initialize TPU encoder backend
 *
 * @details
 * Configures TPU channels for quadrature encoder phase counting.
 * Initializes 32-bit software accumulator and overflow tracking.
 *
 * Algorithm:
 * 1. Validate config (backend == TPU, valid channel pair)
 * 2. Initialize TPU HAL for phase counting mode
 * 3. Reset software accumulator to 0
 * 4. Read initial hardware counter value
 * 5. Set initialized flag
 *
 * @param[in,out] handle Encoder handle (state updated)
 * @param[in] config Encoder configuration (backend, channels, counts_per_rev)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, TPU encoder ready
 * @retval k_rx_err_null_ptr handle or config is NULL
 * @retval k_rx_err_invalid_arg Invalid backend or channel pair
 * @retval k_rx_err_hardware TPU initialization failed
 *
 * @pre TPU clock enabled (MSTP)
 * @pre handle and config not NULL
 * @post TPU configured for phase counting
 * @post handle->initialized = true
 * @post Software accumulator = 0
 *
 * @note Not thread-safe, call once per encoder at startup
 * @warning Do not call multiple times for same handle
 *
 * @see rx_tpu_init_phase_counting() Underlying HAL function
 * @since Version 1.0.0
 */
static rx_err_t rx_encoder_tpu_init(rx_encoder_handle_t* handle,
                                     const rx_encoder_config_t* config);

/**
 * @brief Read TPU encoder count (32-bit accumulator)
 *
 * @details
 * Reads 16-bit hardware counter and detects overflow by comparing
 * with previous value. Updates 32-bit software accumulator.
 *
 * Overflow detection algorithm:
 * 1. Read current 16-bit TCNT
 * 2. Calculate delta = current - previous (wraps correctly)
 * 3. Add delta to 32-bit accumulator
 * 4. Store current value as previous for next read
 *
 * Example: Overflow from 65535 → 0
 * - Previous = 65535
 * - Current = 10
 * - Delta = 10 - 65535 = -65525 (uint16_t wraps to 11)
 * - Accumulator += 11 (correct increment)
 *
 * @param[in,out] handle Encoder handle (state updated)
 * @param[out] count 32-bit accumulated count
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, count written
 * @retval k_rx_err_null_ptr handle or count is NULL
 * @retval k_rx_err_not_initialized Encoder not initialized
 * @retval k_rx_err_hardware TPU read failed
 *
 * @pre Encoder initialized via rx_encoder_tpu_init()
 * @post Accumulator updated with new delta
 * @post Previous count updated
 *
 * @note Thread-safe for different encoders
 * @note Call at >= 100 Hz to prevent overflow loss (13.7s period)
 *
 * @warning Unsigned arithmetic used - ensure delta calculation correct
 *
 * @par Performance:
 * Execution time: ~1.5 µs @ 240 MHz (HAL read + arithmetic)
 *
 * @since Version 1.0.0
 */
static rx_err_t rx_encoder_tpu_read_count(rx_encoder_handle_t* handle, int32_t* count);

/**
 * @brief Reset TPU encoder count to zero
 *
 * @param[in,out] handle Encoder handle (state reset)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, count reset
 * @retval k_rx_err_null_ptr handle is NULL
 * @retval k_rx_err_not_initialized Encoder not initialized
 *
 * @pre Encoder initialized
 * @post Software accumulator = 0
 * @post Hardware counter = 0
 * @post Previous count = 0
 *
 * @since Version 1.0.0
 */
static rx_err_t rx_encoder_tpu_reset(rx_encoder_handle_t* handle);

/**
 * @brief Deinitialize TPU encoder backend
 *
 * @param[in,out] handle Encoder handle (state cleared)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, encoder stopped
 * @retval k_rx_err_null_ptr handle is NULL
 *
 * @post TPU channels stopped
 * @post handle->initialized = false
 *
 * @since Version 1.0.0
 */
static rx_err_t rx_encoder_tpu_deinit(rx_encoder_handle_t* handle);

/**
 * @brief TPU backend operation function pointers
 */
static const encoder_backend_ops_t tpu_backend_ops = {
    .init = rx_encoder_tpu_init,
    .read_count = rx_encoder_tpu_read_count,
    .reset = rx_encoder_tpu_reset,
    .deinit = rx_encoder_tpu_deinit,
};
```

#### Verification Checklist - Phase 2

- [ ] Overflow detection works correctly (tested with simulated wrap-around)
- [ ] 32-bit accumulator increments/decrements correctly
- [ ] Direction reversal works (invert_direction flag)
- [ ] Reset function clears both hardware and software counters
- [ ] All error cases handled (NULL pointers, not initialized, hardware failures)
- [ ] Matches MTU backend behavior exactly (drop-in replacement)

---

### Phase 3: Encoder Abstraction Layer

**Objective:** Refactor existing code to support backend dispatch.

#### Files to Modify

**1. `lib/rx_encoder/inc/rx_encoder.h`** - Update API to be hardware-agnostic

**Changes:**
- Add `encoder_backend_t` enum
- Add backend selection to `rx_encoder_config_t`
- Update documentation to mention MTU vs TPU
- Keep public API unchanged (add backend internally)

**2. `lib/rx_encoder/src/rx_encoder_mtu.c`** - Refactor existing MTU code

**Changes:**
- Rename `rx_mtu_encoder.c` → `rx_encoder_mtu.c`
- Change function names: `rx_encoder_init()` → `rx_encoder_mtu_init()` (static)
- Add `mtu_backend_ops` function pointer table
- Keep all logic identical (pure refactor)

**3. `lib/rx_encoder/src/rx_encoder.c`** - NEW common dispatch layer

```c
/**
 * @file rx_encoder.c
 * @brief Encoder abstraction layer with backend dispatch
 */

rx_err_t rx_encoder_init(rx_encoder_handle_t* handle, const rx_encoder_config_t* config)
{
    RX_CHECK_NULL_PTR(handle);
    RX_CHECK_NULL_PTR(config);

    // Select backend operations based on config
    switch (config->backend) {
        case k_encoder_backend_mtu:
            handle->ops = &mtu_backend_ops;
            break;
        case k_encoder_backend_tpu:
            handle->ops = &tpu_backend_ops;
            break;
        default:
            return k_rx_err_invalid_arg;
    }

    // Dispatch to backend-specific init
    return handle->ops->init(handle, config);
}

rx_err_t rx_encoder_read_count(rx_encoder_handle_t* handle, int32_t* count)
{
    RX_CHECK_NULL_PTR(handle);
    RX_CHECK_NULL_PTR(count);
    RX_CHECK_INITIALIZED(handle->initialized);

    // Dispatch to backend-specific read
    return handle->ops->read_count(handle, count);
}

// Similar dispatch for reset, deinit, etc.
```

#### Verification Checklist - Phase 3

- [ ] Existing MTU encoder tests still pass (regression check)
- [ ] Backend dispatch selects correct function pointers
- [ ] Invalid backend returns error
- [ ] Public API unchanged (backward compatible)
- [ ] No performance regression (dispatch overhead < 0.1 µs)

---

### Phase 4: Testing

**Objective:** Extend existing test suite to cover TPU backend.

#### Files to Modify

**1. `tests/test_rx_encoder.c`** - Add TPU tests (~200 lines of new tests)

**Test strategy:**
- Duplicate all existing MTU tests for TPU backend
- Use mock TPU HAL (same pattern as mock MTU HAL)
- Test overflow handling specifically (16-bit TPU vs 32-bit MTU)

**New test cases:**

```c
/**
 * @test TPU encoder initialization
 */
void test_tpu_encoder_init_success(void) {
    rx_encoder_handle_t handle = {0};
    rx_encoder_config_t config = {
        .backend = k_encoder_backend_tpu,
        .tpu = {
            .tpu_channel_a = k_rx_tpu1,
            .tpu_channel_b = k_rx_tpu5,
        },
        .counts_per_rev = 1364,
        .invert_direction = false,
    };

    // Mock TPU HAL to return success
    mock_tpu_init_return = k_rx_ok;

    rx_err_t err = rx_encoder_init(&handle, &config);

    TEST_ASSERT_EQUAL(k_rx_ok, err);
    TEST_ASSERT_TRUE(handle.initialized);
    TEST_ASSERT_EQUAL(k_encoder_backend_tpu, handle.backend);
}

/**
 * @test TPU encoder 16-bit overflow detection
 */
void test_tpu_encoder_overflow_detection(void) {
    rx_encoder_handle_t handle = {0};
    // ... init ...

    // Simulate counter near overflow
    mock_tpu_count = 65530;
    int32_t count1;
    rx_encoder_read_count(&handle, &count1);
    TEST_ASSERT_EQUAL(65530, count1);

    // Simulate overflow: 65535 → 10
    mock_tpu_count = 10;
    int32_t count2;
    rx_encoder_read_count(&handle, &count2);

    // Delta should be: 10 - 65530 = -65520 (wraps to 16)
    // Accumulator should be: 65530 + 16 = 65546
    TEST_ASSERT_EQUAL(65546, count2);
}

/**
 * @test TPU encoder direction reversal
 */
void test_tpu_encoder_direction_invert(void) {
    rx_encoder_handle_t handle = {0};
    rx_encoder_config_t config = {
        .backend = k_encoder_backend_tpu,
        .tpu = {
            .tpu_channel_a = k_rx_tpu2,
            .tpu_channel_b = k_rx_tpu4,
        },
        .counts_per_rev = 1364,
        .invert_direction = true,  // Reverse direction
    };

    // Mock TPU to count up
    mock_tpu_count = 100;
    int32_t count;
    rx_encoder_read_count(&handle, &count);

    // With invert_direction = true, should negate
    TEST_ASSERT_EQUAL(-100, count);
}

/**
 * @test TPU vs MTU parity (both backends return same results)
 */
void test_backend_parity(void) {
    // Initialize MTU encoder
    rx_encoder_handle_t mtu_handle = {0};
    rx_encoder_config_t mtu_config = {
        .backend = k_encoder_backend_mtu,
        .mtu_channel = k_rx_mtu1,
        .counts_per_rev = 1364,
    };
    rx_encoder_init(&mtu_handle, &mtu_config);

    // Initialize TPU encoder
    rx_encoder_handle_t tpu_handle = {0};
    rx_encoder_config_t tpu_config = {
        .backend = k_encoder_backend_tpu,
        .tpu = { .tpu_channel_a = k_rx_tpu1, .tpu_channel_b = k_rx_tpu5 },
        .counts_per_rev = 1364,
    };
    rx_encoder_init(&tpu_handle, &tpu_config);

    // Mock both to same count
    mock_mtu_count = 1000;
    mock_tpu_count = 1000;

    int32_t mtu_count, tpu_count;
    rx_encoder_read_count(&mtu_handle, &mtu_count);
    rx_encoder_read_count(&tpu_handle, &tpu_count);

    // Both should return same result
    TEST_ASSERT_EQUAL(mtu_count, tpu_count);
}
```

**Test coverage goals:**
- Line coverage: 95%+ for TPU backend
- Branch coverage: 100% for error paths
- Overflow cases: 10+ test scenarios

#### Verification Checklist - Phase 4

- [ ] All existing MTU tests pass (no regression)
- [ ] All new TPU tests pass
- [ ] Overflow detection tested with 16-bit wrap-around
- [ ] Direction inversion tested
- [ ] Backend parity verified (MTU and TPU return same results)
- [ ] Edge cases covered (NULL pointers, not initialized, etc.)
- [ ] Test code follows NASA Rule 5 (all error paths tested)

---

### Phase 5: Integration and Hardware Testing

**Objective:** Test TPU encoders on actual RX72N hardware with real encoders.

#### Hardware Test Procedure

**Equipment needed:**
- RX72N PCB with rear encoders connected (Encoder 2, Encoder 3)
- E2 emulator or USB debug
- Oscilloscope (verify phase A/B signals)
- Motor driver power supply
- Serial console for log output

**Test 1: Static count verification**
1. Initialize TPU encoder drivers for Encoder 2 and 3
2. Manually rotate rear wheels slowly (10 RPM)
3. Read encoder count every 100ms
4. Verify count increments/decrements correctly
5. Compare with front wheel MTU encoders (should match)

**Test 2: Overflow handling**
1. Rotate rear wheel rapidly (150+ RPM)
2. Verify counter continues past 65535 (no reset to 0)
3. Monitor for >1 minute (should exceed 13.7 sec overflow period)
4. Compare accumulated count with expected value

**Test 3: Direction detection**
1. Rotate rear wheel clockwise
2. Verify count increments (positive)
3. Rotate rear wheel counter-clockwise
4. Verify count decrements (negative)
5. Test `invert_direction` flag (reverses behavior)

**Test 4: High-speed operation**
1. Run motor at full speed (210 RPM)
2. Read encoder at 100 Hz for 5 minutes
3. Verify no count loss
4. Check for timing violations (read duration < 10ms)

**Test 5: Four-wheel odometry**
1. Initialize all 4 encoders (2 MTU + 2 TPU)
2. Drive robot in straight line (1 meter)
3. All 4 encoders should show same count (within 5%)
4. Drive robot in circle
5. Left/right encoders should differ (verify skid-steer math)

#### Verification Checklist - Phase 5

- [ ] TPU encoders count correctly on hardware
- [ ] Overflow handling works at high speed (210 RPM)
- [ ] Direction detection correct (CW/CCW)
- [ ] Read latency < 10ms at 100 Hz
- [ ] All 4 encoders (2 MTU + 2 TPU) operational simultaneously
- [ ] Rear wheel odometry matches front wheel odometry (straight line test)
- [ ] No count loss over extended operation (5+ minutes)

---

## Documentation Requirements

### Code Documentation

**Doxygen requirements** (see DOXYGEN_ROADMAP.md):
- [ ] All functions have complete documentation (15+ tags)
- [ ] All structs have inline member comments (`/**<`)
- [ ] All enums have value descriptions
- [ ] State diagrams for initialization sequence
- [ ] Usage examples in @code blocks
- [ ] Cross-references between MTU and TPU backends

### System Documentation

**LaTeX documentation** to create:

**1. `docs/sections/encoder_architecture.tex`** (NEW)

**Content:**
1. MTU vs TPU hardware comparison table
2. Primary vs Secondary encoder concept
3. Overflow period calculations
4. Backend selection rationale
5. Abstraction layer architecture diagram
6. Testing strategy

**2. Update `docs/sections/03_hardware_pinout.tex`**

**Changes:**
- Add note: "Primary (MTU)" next to Encoder 0, 1
- Add note: "Secondary (TPU)" next to Encoder 2, 3
- Reference encoder_architecture.tex for details

**3. Update `README.md`**

**Changes:**
- Add "Encoder Abstraction Layer" to features list
- Mention 4 encoders supported (2 MTU + 2 TPU)

### Verification Checklist - Documentation

- [ ] All code has comprehensive Doxygen comments
- [ ] encoder_architecture.tex created and compiled
- [ ] Pinout documentation updated
- [ ] README.md updated with new feature
- [ ] No Doxygen warnings (`doxygen 2>&1 | grep warning`)

---

## Implementation Checklist

### Phase 1: TPU HAL ⏱️ 4-5 hours
- [ ] Create `rx72n_tpu_regs.h` with register definitions
- [ ] Verify all addresses against Ch28 Table 1.2
- [ ] Create `rx_tpu.h` with HAL interface
- [ ] Implement `rx_tpu.c` HAL functions
- [ ] Test HAL with unit tests (mock registers)
- [ ] Code review (coderabbit)

### Phase 2: TPU Encoder Backend ⏱️ 4-5 hours
- [ ] Create `rx_encoder_tpu.c` backend implementation
- [ ] Implement initialization function
- [ ] Implement count read with overflow handling
- [ ] Implement reset and deinit functions
- [ ] Add backend operation function pointer table
- [ ] Unit test TPU backend (mock HAL)
- [ ] Code review

### Phase 3: Abstraction Layer ⏱️ 2-3 hours
- [ ] Refactor `rx_mtu_encoder.c` → `rx_encoder_mtu.c`
- [ ] Create `rx_encoder.c` dispatch layer
- [ ] Update `rx_encoder.h` API
- [ ] Add backend selection to config
- [ ] Verify existing MTU tests pass (no regression)
- [ ] Code review

### Phase 4: Testing ⏱️ 3-4 hours
- [ ] Add TPU-specific unit tests
- [ ] Test overflow detection
- [ ] Test direction inversion
- [ ] Test backend parity (MTU vs TPU)
- [ ] Achieve 95%+ line coverage
- [ ] All tests pass

### Phase 5: Hardware Testing ⏱️ 2-3 hours
- [ ] Test static count on hardware
- [ ] Test overflow at high speed
- [ ] Test direction detection
- [ ] Test all 4 encoders simultaneously
- [ ] Verify odometry accuracy (straight line, circle)
- [ ] Log any issues for bug fixes

### Phase 6: Documentation ⏱️ 1-2 hours
- [ ] Add comprehensive Doxygen comments
- [ ] Create `encoder_architecture.tex`
- [ ] Update pinout documentation
- [ ] Update README.md
- [ ] Generate Doxygen HTML
- [ ] Review for warnings

### Phase 7: Code Review and Merge
- [ ] Run `coderabbit review --plain`
- [ ] Address all feedback
- [ ] Run clang-format
- [ ] Commit with detailed message
- [ ] Create pull request
- [ ] Merge to main

**Total Estimated Effort:** 16-20 hours

---

## Success Criteria

### Must Have (Blocking)
- ✅ All 4 encoders (2 MTU + 2 TPU) operational on hardware
- ✅ Rear wheel encoders count correctly at 210 RPM
- ✅ Overflow handling works (13.7 sec period)
- ✅ Direction detection accurate (CW/CCW)
- ✅ No count loss over 5+ minutes of operation
- ✅ Unit tests pass with 95%+ coverage
- ✅ All code has comprehensive Doxygen documentation
- ✅ NASA Power of 10 compliant

### Nice to Have (Post-MVP)
- Velocity calculation optimization (reduce floating-point math)
- Hardware interrupt on overflow (instead of software detection)
- DMA for counter reads (reduce CPU load)
- Encoder diagnostics (detect disconnected encoder)

---

## Risk Mitigation

### Risk 1: TPU register addresses incorrect
**Likelihood:** Medium
**Impact:** High (driver won't work)
**Mitigation:** Verify ALL addresses against Ch28 Table 1.2 before coding

### Risk 2: Overflow detection fails at high speed
**Likelihood:** Low
**Impact:** High (count loss)
**Mitigation:** Extensive unit tests with simulated overflow scenarios

### Risk 3: MTU refactor breaks existing encoders
**Likelihood:** Medium
**Impact:** Critical (front wheels stop working)
**Mitigation:** Run full regression test suite before merging

### Risk 4: TPU channels incorrectly paired
**Likelihood:** Medium
**Impact:** High (phase counting won't work)
**Mitigation:** Consult Ch28 Manual for valid channel pairs (TPU1+TPU5, TPU2+TPU4)

---

## References

### RX72N Manual Chapters
- **Ch28:** 16-Bit Timer Pulse Unit (TPUa) - `/workspaces/STAR/e2-studio-star-rx72n-firmware/docs/RX72N_Manual_Chapters/Ch28_16-Bit_Timer_Pulse_Unit_TPUa_.txt`
- **Ch04:** Memory Map - Register address verification
- **Ch09:** Clock Generation - TPU module stop control

### Existing Implementations
- **MTU Encoder:** `/workspaces/STAR/star-rx72n-firmware/lib/rx_encoder/src/rx_mtu_encoder.c`
- **MTU HAL:** `/workspaces/STAR/star-rx72n-firmware/lib/rx_hal/src/rx_mtu.c`
- **Encoder Tests:** `/workspaces/STAR/star-rx72n-firmware/tests/test_rx_encoder.c`

### Project Documentation
- **RX72N_ROADMAP.md:** `/workspaces/STAR/RX72N_ROADMAP.md` (Ch28 status)
- **DOXYGEN_ROADMAP.md:** `/workspaces/STAR/DOXYGEN_ROADMAP.md` (documentation requirements)
- **CLAUDE.md:** `/workspaces/STAR/CLAUDE.md` (coding standards)

---

**Document Version:** 1.0
**Last Updated:** 2026-02-05
**Author:** STAR Development Team
**Status:** Ready for Implementation
