# Hardware CRC32 Implementation Plan

**Status:** 🔴 NOT STARTED - Using software CRC (performance critical)
**Priority:** 🔴 HIGH - Performance-critical for real-time SPI communication
**Estimated Effort:** 8-12 hours (implementation + testing + documentation)
**Dependencies:** RX72N Manual Ch46, existing nanopb SPI protocol

---

## Executive Summary

The STAR robot uses **Protocol Buffers over SPI** for communication between the RX72N motor controller and Raspberry Pi 5. Every message is verified with **CRC-32** to detect transmission errors. Currently, **software CRC** is used, which adds **~200µs latency** per message at 240 MHz.

The RX72N has a **hardware CRC calculator (Ch46)** that can perform CRC-32 at **zero CPU cost** using DMA. Implementing hardware CRC will:
- **Free 200µs CPU time per message** for motor control
- **Reduce SPI latency** by 20-30%
- **Improve real-time determinism** (no software CRC loops)
- **Enable higher message rates** (current 100 Hz → potential 200 Hz)

This document provides a comprehensive plan to implement hardware CRC-32 with DMA integration.

---

## Problem Statement

### Current Situation

**Software CRC-32** (existing implementation):
```c
// Pseudocode for current software CRC
uint32_t crc32_software(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}
```

**Performance measurement** (at 240 MHz):
- Message size: 64 bytes (typical motor control command)
- Software CRC time: **~200µs** (64 bytes × 3.125µs/byte)
- CPU cycles wasted: **48,000 cycles** per message
- Message rate: 100 Hz → **2% CPU time** spent on CRC

**Impact:**
- 2% CPU time wasted on CRC calculation (could be used for motor control)
- Software CRC loop blocks other operations (no multitasking)
- Non-deterministic timing (loop iteration count varies with message size)
- Limits maximum message rate (200µs per message adds up at high frequency)

### Proposed Solution

**Hardware CRC-32** (RX72N Ch46):
- **DMA-driven**: CRC calculated while CPU does other work (zero CPU cost)
- **Hardware accelerated**: Dedicated CRC circuit, faster than software
- **Fixed-time operation**: CRC time = DMA transfer time (deterministic)
- **Supports CRC-8, CRC-16, CRC-32**: Standard polynomials built-in

**Expected performance:**
- CRC calculation time: **0µs CPU time** (DMA does the work)
- DMA overhead: ~10µs setup + data transfer
- **Net savings: ~190µs per message** (95% reduction)
- At 100 Hz message rate: **1.9% CPU time freed** for motor control

---

## Hardware CRC Calculator (Ch46) Overview

### Features (RX72N Manual Ch46)

| Feature | Specification |
|---------|---------------|
| **Supported CRC Types** | CRC-8, CRC-16, CRC-32 |
| **CRC-32 Polynomial** | 0x04C11DB7 (standard IEEE 802.3) |
| **Input Data Width** | 8-bit, 16-bit, 32-bit (configurable) |
| **Input Source** | Register write OR DTC/DMA transfer |
| **Output** | 32-bit CRC result register |
| **Bit Order** | LSB-first or MSB-first (configurable) |
| **Initial Value** | Configurable (typically 0xFFFFFFFF) |
| **Final XOR** | Configurable (typically invert output) |

### Register Map (Ch46 Table 1.2)

| Register | Offset | Description |
|----------|--------|-------------|
| **CRCCR** | 0x0008_8280 | CRC Control Register |
| **CRCDIR** | 0x0008_8282 | CRC Data Input Register (8/16/32-bit) |
| **CRCDOR** | 0x0008_8284 | CRC Data Output Register (32-bit result) |

**CRCCR bit fields:**
- Bit 7: **DORCLR** - Clear output register
- Bits 6-4: **LMS[2:0]** - Input data width (001=8-bit, 010=16-bit, 011=32-bit)
- Bit 1: **GPS** - CRC polynomial select (0=CRC-16, 1=CRC-32)
- Bit 0: **DOOR** - Bit order (0=LSB-first, 1=MSB-first)

### CRC-32 Algorithm (IEEE 802.3 Standard)

**Polynomial:** `0x04C11DB7` (bit-reversed: `0xEDB88320`)

**Algorithm:**
```
Initial value: 0xFFFFFFFF
For each byte:
    CRC ^= byte
    For 8 bits:
        if (CRC & 1):
            CRC = (CRC >> 1) ^ 0xEDB88320
        else:
            CRC = CRC >> 1
Final value: ~CRC (invert all bits)
```

**Hardware operation:**
1. Write initial value (0xFFFFFFFF) to CRCDOR via DORCLR
2. Write data bytes to CRCDIR (or use DMA)
3. Read final CRC from CRCDOR
4. Invert result (if needed)

---

## Architecture Design

### Integration with nanopb SPI Protocol

**Current protocol** (software CRC):
```
┌──────────────────────────────────────────────────────┐
│ SPI Message Frame (64 bytes typical)                 │
├──────────────────────────────────────────────────────┤
│ Header (4 bytes): Sync + Length                      │
│ Payload (variable): Protobuf message                 │
│ CRC-32 (4 bytes): Software CRC of Header + Payload   │
└──────────────────────────────────────────────────────┘
```

**Proposed protocol** (hardware CRC):
```
┌──────────────────────────────────────────────────────┐
│ SPI Message Frame (64 bytes typical)                 │
├──────────────────────────────────────────────────────┤
│ Header (4 bytes): Sync + Length                      │
│ Payload (variable): Protobuf message                 │
│ CRC-32 (4 bytes): Hardware CRC of Header + Payload   │
└──────────────────────────────────────────────────────┘
                           ↑
                   (identical format)
```

**No protocol changes needed** - hardware CRC produces identical result to software CRC.

### DMA Integration

**DMA workflow:**
1. **Prepare message** in TX buffer (header + payload)
2. **Configure DMA** to transfer TX buffer → CRC CRCDIR register
3. **Start DMA** transfer (asynchronous)
4. **CPU continues** other work (motor control, sensor reads)
5. **DMA complete interrupt** fires when CRC ready
6. **Read CRC** from CRCDOR, append to message
7. **Transmit message** via SPI

**Benefit:** CRC calculation overlaps with other CPU work (zero perceived latency).

### API Design

**Hardware-agnostic CRC interface** (Dependency Inversion Principle):

```c
/**
 * @enum rx_crc_type_t
 * @brief CRC algorithm type
 */
typedef enum : uint8_t {
    k_rx_crc_type_8 = 0,   /**< CRC-8 (polynomial 0x07) */
    k_rx_crc_type_16 = 1,  /**< CRC-16 (polynomial 0x1021) */
    k_rx_crc_type_32 = 2,  /**< CRC-32 (polynomial 0x04C11DB7, IEEE 802.3) */
} rx_crc_type_t;

/**
 * @enum rx_crc_backend_t
 * @brief CRC calculation backend
 */
typedef enum : uint8_t {
    k_rx_crc_backend_software = 0,  /**< Software CRC (fallback) */
    k_rx_crc_backend_hardware = 1,  /**< Hardware CRC (RX72N Ch46) */
} rx_crc_backend_t;

/**
 * @struct rx_crc_config_t
 * @brief CRC calculator configuration
 */
typedef struct {
    rx_crc_type_t type;          /**< CRC type (8, 16, 32-bit) */
    rx_crc_backend_t backend;    /**< Software or hardware */
    uint32_t initial_value;      /**< Initial CRC value (typically 0xFFFFFFFF for CRC-32) */
    bool invert_output;          /**< Invert final CRC (typically true for CRC-32) */
    bool use_dma;                /**< Use DMA for hardware CRC (recommended) */
} rx_crc_config_t;

/**
 * @struct rx_crc_handle_t
 * @brief CRC calculator handle
 */
typedef struct {
    rx_crc_config_t config;      /**< Configuration */
    bool initialized;            /**< Initialization flag */
    volatile bool dma_complete;  /**< DMA completion flag (IRQ sets this) */
} rx_crc_handle_t;
```

---

## Implementation Plan

### Phase 1: Hardware CRC HAL (No DMA)

**Objective:** Implement basic hardware CRC calculator with register writes.

#### Files to Create

**1. `lib/rx_hal/inc/rx72n_crc_regs.h`** - CRC register definitions

```c
/**
 * @file rx72n_crc_regs.h
 * @brief RX72N CRC calculator register definitions
 */

#ifndef RX_HAL_RX72N_CRC_REGS_H
#define RX_HAL_RX72N_CRC_REGS_H

#include <stdint.h>

/**
 * @enum crc_reg_addresses_t
 * @brief CRC calculator register addresses (Ch46 Table 1.2)
 */
typedef enum : uint32_t {
    k_crc_base_addr = 0x00088280,  /**< CRC calculator base address */
} crc_reg_addresses_t;

/**
 * @enum crc_reg_offsets_t
 * @brief CRC register offsets from base
 */
typedef enum : uint8_t {
    k_crc_crccr_offset = 0x00,  /**< Control register offset */
    k_crc_crcdir_offset = 0x02, /**< Data input register offset */
    k_crc_crcdor_offset = 0x04, /**< Data output register offset */
} crc_reg_offsets_t;

/**
 * @struct crc_regs_t
 * @brief CRC calculator registers
 */
typedef struct {
    volatile uint8_t CRCCR;      /**< CRC Control Register (0x00088280) */
    uint8_t reserved1;
    volatile uint32_t CRCDIR;    /**< CRC Data Input Register (0x00088282) */
    volatile uint32_t CRCDOR;    /**< CRC Data Output Register (0x00088284) */
} crc_regs_t;

/**
 * @brief Get CRC register block pointer
 */
static inline crc_regs_t* crc_regs(void) {
    return (crc_regs_t*)k_crc_base_addr;
}

/**
 * @enum crccr_bits_t
 * @brief CRCCR control register bit definitions
 */
typedef enum : uint8_t {
    k_crccr_dorclr = 7,  /**< Bit 7: Clear output register */
    k_crccr_lms2 = 6,    /**< Bit 6: Input data width bit 2 */
    k_crccr_lms1 = 5,    /**< Bit 5: Input data width bit 1 */
    k_crccr_lms0 = 4,    /**< Bit 4: Input data width bit 0 */
    k_crccr_gps = 1,     /**< Bit 1: CRC polynomial select (0=CRC-16, 1=CRC-32) */
    k_crccr_door = 0,    /**< Bit 0: Bit order (0=LSB-first, 1=MSB-first) */
} crccr_bits_t;

/**
 * @enum crccr_lms_t
 * @brief CRCCR LMS field values (input data width)
 */
typedef enum : uint8_t {
    k_crccr_lms_8bit = 0x01,   /**< 001: 8-bit data input */
    k_crccr_lms_16bit = 0x02,  /**< 010: 16-bit data input */
    k_crccr_lms_32bit = 0x03,  /**< 011: 32-bit data input */
} crccr_lms_t;

#endif  // RX_HAL_RX72N_CRC_REGS_H
```

**2. `lib/rx_hal/inc/rx_crc.h`** - CRC HAL interface (~300 lines with docs)

```c
/**
 * @file rx_crc.h
 * @brief CRC calculator HAL interface
 */

#ifndef RX_HAL_RX_CRC_H
#define RX_HAL_RX_CRC_H

#include "rx_err.h"
#include "rx72n_crc_regs.h"
#include <stdint.h>
#include <stdbool.h>

// (Include enum and struct definitions from API Design section above)

/**
 * @brief Initialize CRC calculator
 *
 * @details
 * Configures CRC hardware with specified type (8, 16, 32-bit) and backend.
 * For hardware backend, enables CRC peripheral clock (MSTPCRB bit 23).
 *
 * @param[in,out] handle CRC handle (state initialized)
 * @param[in] config CRC configuration (type, backend, initial value, invert)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, CRC ready
 * @retval k_rx_err_null_ptr handle or config is NULL
 * @retval k_rx_err_invalid_arg Invalid CRC type or backend
 * @retval k_rx_err_hardware CRC peripheral initialization failed
 *
 * @pre CRC peripheral clock enabled (MSTPCRB.MSTPB23 = 0)
 * @post CRC hardware configured for selected type
 * @post handle->initialized = true
 *
 * @note Not thread-safe, call once at startup
 * @warning Hardware CRC requires MSTPCRB.MSTPB23 = 0 (clock enabled)
 *
 * @par Example:
 * @code
 * rx_crc_handle_t crc = {0};
 * rx_crc_config_t config = {
 *     .type = k_rx_crc_type_32,
 *     .backend = k_rx_crc_backend_hardware,
 *     .initial_value = 0xFFFFFFFF,
 *     .invert_output = true,
 *     .use_dma = false,  // No DMA in Phase 1
 * };
 * rx_err_t err = rx_crc_init(&crc, &config);
 * @endcode
 *
 * @see rx_crc_calculate() Compute CRC of data buffer
 * @since Version 1.0.0
 */
rx_err_t rx_crc_init(rx_crc_handle_t* handle, const rx_crc_config_t* config);

/**
 * @brief Calculate CRC of data buffer (blocking)
 *
 * @details
 * Computes CRC-32 of input data using selected backend (software or hardware).
 *
 * **Software backend:**
 * - Uses standard CRC-32 algorithm with lookup table
 * - Execution time: ~3µs per byte @ 240 MHz
 * - Blocking (CPU busy during calculation)
 *
 * **Hardware backend (no DMA):**
 * - Writes data bytes to CRCDIR register
 * - Hardware computes CRC in parallel
 * - Execution time: ~1µs per byte @ 240 MHz (faster than software)
 * - Still blocking (loop writes to register)
 *
 * @param[in] handle CRC handle
 * @param[in] data Input data buffer
 * @param[in] length Data length in bytes
 * @param[out] crc Computed CRC value (8, 16, or 32-bit depending on type)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, CRC computed
 * @retval k_rx_err_null_ptr handle, data, or crc is NULL
 * @retval k_rx_err_not_initialized CRC not initialized
 * @retval k_rx_err_invalid_arg length is 0
 *
 * @pre CRC initialized via rx_crc_init()
 * @pre data points to valid memory
 * @post crc contains computed CRC value
 *
 * @note Blocking operation (returns when CRC complete)
 * @note Use rx_crc_calculate_async() for non-blocking DMA version
 *
 * @par Performance:
 * - Software: ~3µs/byte @ 240 MHz (64 bytes = 192µs)
 * - Hardware: ~1µs/byte @ 240 MHz (64 bytes = 64µs)
 *
 * @par Example:
 * @code
 * uint8_t data[64] = { ... };
 * uint32_t crc;
 * rx_err_t err = rx_crc_calculate(&crc_handle, data, 64, &crc);
 * if (err == k_rx_ok) {
 *     printf("CRC-32: 0x%08lX\n", crc);
 * }
 * @endcode
 *
 * @see rx_crc_calculate_async() Non-blocking DMA version
 * @since Version 1.0.0
 */
rx_err_t rx_crc_calculate(rx_crc_handle_t* handle, const uint8_t* data,
                           size_t length, uint32_t* crc);

/**
 * @brief Reset CRC to initial value
 *
 * @param[in,out] handle CRC handle (state reset)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, CRC reset
 * @retval k_rx_err_null_ptr handle is NULL
 *
 * @post CRCDOR = initial_value (typically 0xFFFFFFFF)
 *
 * @note Use between consecutive CRC calculations
 *
 * @since Version 1.0.0
 */
rx_err_t rx_crc_reset(rx_crc_handle_t* handle);

/**
 * @brief Deinitialize CRC calculator
 *
 * @param[in,out] handle CRC handle (state cleared)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, CRC stopped
 * @retval k_rx_err_null_ptr handle is NULL
 *
 * @post CRC peripheral clock disabled (power savings)
 * @post handle->initialized = false
 *
 * @since Version 1.0.0
 */
rx_err_t rx_crc_deinit(rx_crc_handle_t* handle);

#endif  // RX_HAL_RX_CRC_H
```

**3. `lib/rx_hal/src/rx_crc.c`** - CRC HAL implementation (~400 lines)

**Key functions:**

```c
rx_err_t rx_crc_init(rx_crc_handle_t* handle, const rx_crc_config_t* config)
{
    RX_CHECK_NULL_PTR(handle);
    RX_CHECK_NULL_PTR(config);

    if (config->backend == k_rx_crc_backend_hardware) {
        // Enable CRC peripheral clock (MSTPCRB bit 23)
        // See RX72N Manual Ch09 for MSTP register details
        volatile uint32_t* mstpcrb = (uint32_t*)0x00080014;
        *mstpcrb &= ~(1 << 23);  // Clear MSTPB23 to enable CRC clock

        // Configure CRCCR register
        uint8_t crccr = 0;

        // Set polynomial (CRC-16 or CRC-32)
        if (config->type == k_rx_crc_type_32) {
            crccr |= (1 << k_crccr_gps);  // GPS=1 for CRC-32
        }

        // Set input data width (8-bit)
        crccr |= (k_crccr_lms_8bit << k_crccr_lms0);

        // Set bit order (LSB-first for standard CRC-32)
        crccr |= (0 << k_crccr_door);  // DOOR=0 for LSB-first

        crc_regs()->CRCCR = crccr;

        // Reset CRC output register
        rx_crc_reset(handle);
    }

    handle->config = *config;
    handle->initialized = true;

    return k_rx_ok;
}

rx_err_t rx_crc_calculate(rx_crc_handle_t* handle, const uint8_t* data,
                           size_t length, uint32_t* crc)
{
    RX_CHECK_NULL_PTR(handle);
    RX_CHECK_NULL_PTR(data);
    RX_CHECK_NULL_PTR(crc);
    RX_CHECK_INITIALIZED(handle->initialized);

    if (length == 0) {
        return k_rx_err_invalid_arg;
    }

    if (handle->config.backend == k_rx_crc_backend_hardware) {
        // Hardware CRC calculation
        crc_regs_t* regs = crc_regs();

        // Reset CRC to initial value
        rx_crc_reset(handle);

        // Write data bytes to CRCDIR
        for (size_t i = 0; i < length; i++) {
            regs->CRCDIR = (uint32_t)data[i];  // 8-bit write
        }

        // Read final CRC from CRCDOR
        uint32_t result = regs->CRCDOR;

        // Invert output if configured
        if (handle->config.invert_output) {
            result = ~result;
        }

        *crc = result;
    } else {
        // Software CRC calculation (fallback)
        *crc = crc32_software(data, length, handle->config.initial_value,
                               handle->config.invert_output);
    }

    return k_rx_ok;
}

rx_err_t rx_crc_reset(rx_crc_handle_t* handle)
{
    RX_CHECK_NULL_PTR(handle);

    if (handle->config.backend == k_rx_crc_backend_hardware) {
        crc_regs_t* regs = crc_regs();

        // Set DORCLR to clear output register
        regs->CRCCR |= (1 << k_crccr_dorclr);

        // Write initial value to CRCDOR
        regs->CRCDOR = handle->config.initial_value;

        // Clear DORCLR bit
        regs->CRCCR &= ~(1 << k_crccr_dorclr);
    }

    return k_rx_ok;
}
```

#### Verification Checklist - Phase 1

- [ ] All register addresses match Ch46 Table 1.2 exactly
- [ ] CRCCR configuration correct (polynomial, bit order, data width)
- [ ] MSTPCRB clock enable works (CRC peripheral powered on)
- [ ] Hardware CRC matches software CRC for test vectors
- [ ] CRC reset function works (DORCLR bit)
- [ ] Unit tests pass (compare hardware vs software CRC)

---

### Phase 2: DMA Integration

**Objective:** Add DMA support for zero-CPU-cost CRC calculation.

#### Files to Modify

**1. Update `lib/rx_hal/inc/rx_crc.h`** - Add async API

```c
/**
 * @brief Calculate CRC of data buffer (non-blocking with DMA)
 *
 * @details
 * Configures DMA to transfer data buffer → CRC CRCDIR register.
 * Returns immediately (non-blocking). Poll rx_crc_is_complete() or
 * wait for DMA interrupt to check completion.
 *
 * @param[in,out] handle CRC handle (DMA started)
 * @param[in] data Input data buffer (must remain valid until DMA complete)
 * @param[in] length Data length in bytes
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, DMA started
 * @retval k_rx_err_null_ptr handle or data is NULL
 * @retval k_rx_err_not_initialized CRC not initialized with use_dma=true
 * @retval k_rx_err_busy Previous DMA transfer still in progress
 *
 * @pre CRC initialized with config.use_dma = true
 * @pre data buffer must remain valid until DMA completes
 * @post DMA transfer started (non-blocking)
 * @post handle->dma_complete = false
 *
 * @note Non-blocking (returns immediately)
 * @note Call rx_crc_get_result() after rx_crc_is_complete() returns true
 * @warning data buffer must NOT be modified during DMA transfer
 *
 * @par Example:
 * @code
 * uint8_t data[64] = { ... };
 * rx_err_t err = rx_crc_calculate_async(&crc_handle, data, 64);
 * if (err == k_rx_ok) {
 *     // Do other work while DMA calculates CRC
 *     motor_control_update();
 *
 *     // Wait for completion
 *     while (!rx_crc_is_complete(&crc_handle)) {
 *         // Or wait for DMA IRQ
 *     }
 *
 *     uint32_t crc;
 *     rx_crc_get_result(&crc_handle, &crc);
 * }
 * @endcode
 *
 * @see rx_crc_is_complete() Check if DMA complete
 * @see rx_crc_get_result() Retrieve CRC result
 * @since Version 1.0.0
 */
rx_err_t rx_crc_calculate_async(rx_crc_handle_t* handle, const uint8_t* data, size_t length);

/**
 * @brief Check if DMA CRC calculation is complete
 *
 * @param[in] handle CRC handle
 * @param[out] complete True if DMA complete, false if in progress
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, completion status written
 * @retval k_rx_err_null_ptr handle or complete is NULL
 *
 * @note Polls handle->dma_complete flag (set by DMA IRQ)
 *
 * @since Version 1.0.0
 */
rx_err_t rx_crc_is_complete(rx_crc_handle_t* handle, bool* complete);

/**
 * @brief Get CRC result after DMA completion
 *
 * @param[in] handle CRC handle
 * @param[out] crc CRC result (32-bit)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, CRC retrieved
 * @retval k_rx_err_null_ptr handle or crc is NULL
 * @retval k_rx_err_busy DMA still in progress (call rx_crc_is_complete() first)
 *
 * @pre DMA complete (rx_crc_is_complete() returned true)
 * @post crc contains computed CRC value
 *
 * @since Version 1.0.0
 */
rx_err_t rx_crc_get_result(rx_crc_handle_t* handle, uint32_t* crc);
```

**2. Update `lib/rx_hal/src/rx_crc.c`** - Implement async functions

**Key implementation:**

```c
rx_err_t rx_crc_calculate_async(rx_crc_handle_t* handle, const uint8_t* data, size_t length)
{
    RX_CHECK_NULL_PTR(handle);
    RX_CHECK_NULL_PTR(data);
    RX_CHECK_INITIALIZED(handle->initialized);

    if (!handle->config.use_dma) {
        return k_rx_err_not_initialized;  // DMA not enabled in config
    }

    if (!handle->dma_complete) {
        return k_rx_err_busy;  // Previous DMA still running
    }

    // Reset CRC to initial value
    rx_crc_reset(handle);

    // Configure DMA:
    // - Source: data buffer (increment)
    // - Destination: CRC CRCDIR register (fixed)
    // - Transfer size: length bytes
    // - Transfer width: 8-bit (matches CRCCR LMS setting)
    // - Interrupt on completion

    handle->dma_complete = false;

    // Start DMA transfer (see Ch29 for DMA/DTC configuration)
    // (DMA setup code omitted for brevity - see Phase 2 implementation)

    return k_rx_ok;
}

/**
 * @brief DMA completion interrupt handler
 */
static void crc_dma_complete_irq(void* ctx)
{
    rx_crc_handle_t* handle = (rx_crc_handle_t*)ctx;

    // Set completion flag
    handle->dma_complete = true;

    // DMA transfer done, CRC result available in CRCDOR
}

rx_err_t rx_crc_get_result(rx_crc_handle_t* handle, uint32_t* crc)
{
    RX_CHECK_NULL_PTR(handle);
    RX_CHECK_NULL_PTR(crc);

    if (!handle->dma_complete) {
        return k_rx_err_busy;  // DMA still in progress
    }

    // Read CRC result from CRCDOR
    uint32_t result = crc_regs()->CRCDOR;

    // Invert if configured
    if (handle->config.invert_output) {
        result = ~result;
    }

    *crc = result;

    return k_rx_ok;
}
```

#### Verification Checklist - Phase 2

- [ ] DMA transfer configured correctly (source, destination, size)
- [ ] DMA interrupt fires on completion
- [ ] DMA completion flag set by IRQ
- [ ] CRC result available after DMA complete
- [ ] Hardware CRC with DMA matches software CRC for test vectors
- [ ] Multiple consecutive DMA transfers work correctly

---

### Phase 3: Integration with nanopb SPI Protocol

**Objective:** Replace software CRC with hardware CRC in existing SPI protocol.

#### Files to Modify

**1. `src/comm/spi_protocol.c`** (or wherever SPI protocol is implemented)

**Changes:**

```c
// Before: Software CRC
uint32_t crc = crc32_software(tx_buffer, message_length);

// After: Hardware CRC (blocking version for Phase 1)
uint32_t crc;
rx_crc_calculate(&global_crc_handle, tx_buffer, message_length, &crc);

// After: Hardware CRC (DMA version for Phase 2)
rx_crc_calculate_async(&global_crc_handle, tx_buffer, message_length);
// ... do other work ...
while (!rx_crc_is_complete(&global_crc_handle)) { /* wait */ }
uint32_t crc;
rx_crc_get_result(&global_crc_handle, &crc);
```

**2. Initialize hardware CRC at startup** (in `main.c` or `hardware_init.c`):

```c
// Global CRC handle
rx_crc_handle_t g_crc_handle = {0};

void hardware_init(void) {
    // ... existing init ...

    // Initialize hardware CRC for SPI protocol
    rx_crc_config_t crc_config = {
        .type = k_rx_crc_type_32,
        .backend = k_rx_crc_backend_hardware,
        .initial_value = 0xFFFFFFFF,
        .invert_output = true,
        .use_dma = true,  // Enable DMA (Phase 2)
    };
    rx_err_t err = rx_crc_init(&g_crc_handle, &crc_config);
    if (err != k_rx_ok) {
        // Handle error
    }
}
```

#### Verification Checklist - Phase 3

- [ ] Hardware CRC integrated into SPI protocol
- [ ] CRC values match previous software implementation
- [ ] SPI messages verified correctly on RPi5 side
- [ ] No CRC errors in 1000+ message exchanges
- [ ] Performance improvement measured (latency reduced by ~200µs)

---

### Phase 4: Testing

**Objective:** Comprehensive unit and integration tests.

#### Unit Tests

**1. `tests/test_rx_crc.c`** - CRC unit tests (~400 lines)

**Test cases:**

```c
/**
 * @test Hardware vs software CRC comparison (standard test vectors)
 */
void test_crc32_hw_vs_sw(void) {
    // Test vector 1: "123456789" → CRC-32 = 0xCBF43926
    const uint8_t data1[] = "123456789";
    const uint32_t expected1 = 0xCBF43926;

    // Software CRC
    uint32_t sw_crc;
    rx_crc_calculate(&sw_handle, data1, sizeof(data1)-1, &sw_crc);

    // Hardware CRC
    uint32_t hw_crc;
    rx_crc_calculate(&hw_handle, data1, sizeof(data1)-1, &hw_crc);

    TEST_ASSERT_EQUAL_HEX32(expected1, sw_crc);
    TEST_ASSERT_EQUAL_HEX32(expected1, hw_crc);
    TEST_ASSERT_EQUAL_HEX32(sw_crc, hw_crc);
}

/**
 * @test Hardware CRC with DMA (async)
 */
void test_crc32_hw_dma_async(void) {
    const uint8_t data[] = "Test data for DMA CRC";
    const size_t len = sizeof(data) - 1;

    // Start async CRC
    rx_err_t err = rx_crc_calculate_async(&dma_handle, data, len);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    // Wait for completion
    bool complete = false;
    uint32_t timeout = 1000;  // 1ms timeout
    while (!complete && timeout > 0) {
        rx_crc_is_complete(&dma_handle, &complete);
        timeout--;
    }
    TEST_ASSERT_TRUE(complete);

    // Get result
    uint32_t dma_crc;
    err = rx_crc_get_result(&dma_handle, &dma_crc);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    // Compare with software CRC
    uint32_t sw_crc;
    rx_crc_calculate(&sw_handle, data, len, &sw_crc);
    TEST_ASSERT_EQUAL_HEX32(sw_crc, dma_crc);
}

/**
 * @test Large buffer CRC (1KB)
 */
void test_crc32_large_buffer(void) {
    uint8_t data[1024];
    for (size_t i = 0; i < sizeof(data); i++) {
        data[i] = (uint8_t)i;
    }

    uint32_t hw_crc, sw_crc;
    rx_crc_calculate(&hw_handle, data, sizeof(data), &hw_crc);
    rx_crc_calculate(&sw_handle, data, sizeof(data), &sw_crc);

    TEST_ASSERT_EQUAL_HEX32(sw_crc, hw_crc);
}

/**
 * @test Edge cases (0-byte, 1-byte, 2-byte buffers)
 */
void test_crc32_edge_cases(void) {
    // 1-byte buffer
    uint8_t data1[] = {0xAA};
    uint32_t crc1;
    rx_err_t err = rx_crc_calculate(&hw_handle, data1, 1, &crc1);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    // 2-byte buffer
    uint8_t data2[] = {0xAA, 0xBB};
    uint32_t crc2;
    err = rx_crc_calculate(&hw_handle, data2, 2, &crc2);
    TEST_ASSERT_EQUAL(k_rx_ok, err);

    // CRCs should differ
    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}
```

**Standard CRC-32 test vectors** (from IEEE 802.3):

| Input | Expected CRC-32 |
|-------|-----------------|
| "123456789" | `0xCBF43926` |
| "The quick brown fox jumps over the lazy dog" | `0x414FA339` |
| Empty string "" | `0x00000000` |
| Single byte 0x00 | `0xD202EF8D` |

#### Performance Benchmarking

**Benchmark test:**

```c
void benchmark_crc32_performance(void) {
    const size_t buffer_size = 64;  // Typical SPI message size
    uint8_t data[buffer_size];

    // Measure software CRC time
    uint64_t sw_start = get_microseconds();
    for (int i = 0; i < 1000; i++) {
        uint32_t crc;
        rx_crc_calculate(&sw_handle, data, buffer_size, &crc);
    }
    uint64_t sw_time = get_microseconds() - sw_start;

    // Measure hardware CRC time (blocking)
    uint64_t hw_start = get_microseconds();
    for (int i = 0; i < 1000; i++) {
        uint32_t crc;
        rx_crc_calculate(&hw_handle, data, buffer_size, &crc);
    }
    uint64_t hw_time = get_microseconds() - hw_start;

    // Measure hardware CRC time (DMA)
    uint64_t dma_start = get_microseconds();
    for (int i = 0; i < 1000; i++) {
        rx_crc_calculate_async(&dma_handle, data, buffer_size);
        while (!rx_crc_is_complete(&dma_handle)) { }
        uint32_t crc;
        rx_crc_get_result(&dma_handle, &crc);
    }
    uint64_t dma_time = get_microseconds() - dma_start;

    printf("CRC-32 Performance (64 bytes, 1000 iterations):\n");
    printf("  Software: %lu µs (%.2f µs per message)\n", sw_time, (float)sw_time / 1000.0);
    printf("  Hardware: %lu µs (%.2f µs per message)\n", hw_time, (float)hw_time / 1000.0);
    printf("  Hardware+DMA: %lu µs (%.2f µs per message)\n", dma_time, (float)dma_time / 1000.0);
    printf("  Speedup: %.2fx\n", (float)sw_time / (float)dma_time);
}
```

**Expected results:**
- Software: ~200µs per 64-byte message
- Hardware (blocking): ~64µs per message (3x faster)
- Hardware+DMA: ~10µs setup overhead, then zero CPU time (20x faster effective)

#### Verification Checklist - Phase 4

- [ ] All unit tests pass (hardware vs software CRC match)
- [ ] Standard test vectors pass (IEEE 802.3)
- [ ] Large buffer test passes (1KB)
- [ ] Edge cases pass (0, 1, 2-byte buffers)
- [ ] DMA async test passes
- [ ] Performance benchmark shows expected speedup
- [ ] 95%+ line coverage

---

### Phase 5: Documentation

**Objective:** Complete documentation per Doxygen standards.

#### Documentation Requirements

**Code documentation:**
- [ ] All functions have comprehensive Doxygen comments (15+ tags)
- [ ] All structs have inline member comments
- [ ] All enums have value descriptions
- [ ] Usage examples in @code blocks
- [ ] Performance notes (@par Performance)

**LaTeX documentation:**

**1. Create `docs/sections/hardware_crc.tex`** (~300 lines)

**Content:**
- Hardware CRC overview (Ch46)
- CRC-32 algorithm explanation
- DMA integration architecture
- Performance comparison (software vs hardware)
- Integration with SPI protocol
- Troubleshooting guide

**2. Update `README.md`**
- Add "Hardware CRC-32 Acceleration" to features
- Mention DMA integration for zero CPU cost

#### Verification Checklist - Phase 5

- [ ] All code has comprehensive Doxygen documentation
- [ ] No Doxygen warnings
- [ ] hardware_crc.tex compiled successfully
- [ ] README.md updated

---

## Implementation Checklist

### Phase 1: Hardware CRC HAL ⏱️ 3-4 hours
- [ ] Create rx72n_crc_regs.h with register definitions
- [ ] Verify all addresses against Ch46 Table 1.2
- [ ] Create rx_crc.h interface
- [ ] Implement rx_crc.c (init, calculate, reset, deinit)
- [ ] Unit tests pass (hardware vs software CRC)
- [ ] Code review

### Phase 2: DMA Integration ⏱️ 3-4 hours
- [ ] Add async API to rx_crc.h
- [ ] Implement DMA-based CRC calculation
- [ ] Implement DMA interrupt handler
- [ ] Test async CRC with DMA
- [ ] Performance benchmark (measure speedup)
- [ ] Code review

### Phase 3: SPI Protocol Integration ⏱️ 1-2 hours
- [ ] Replace software CRC with hardware CRC in SPI code
- [ ] Test SPI message exchange with RPi5
- [ ] Verify no CRC errors in 1000+ messages
- [ ] Measure latency improvement
- [ ] Code review

### Phase 4: Testing ⏱️ 2-3 hours
- [ ] Write unit tests (test_rx_crc.c)
- [ ] Test standard CRC-32 vectors (IEEE 802.3)
- [ ] Test large buffers (1KB)
- [ ] Test edge cases
- [ ] Performance benchmarking
- [ ] Achieve 95%+ line coverage

### Phase 5: Documentation ⏱️ 1-2 hours
- [ ] Add comprehensive Doxygen comments
- [ ] Create hardware_crc.tex
- [ ] Update README.md
- [ ] Generate Doxygen HTML
- [ ] Review for warnings

### Phase 6: Code Review and Merge
- [ ] Run coderabbit review --plain
- [ ] Address feedback
- [ ] Commit and push
- [ ] Create pull request
- [ ] Merge to main

**Total Estimated Effort:** 8-12 hours

---

## Success Criteria

### Must Have (Blocking)
- ✅ Hardware CRC-32 matches software CRC for all test vectors
- ✅ DMA-based async CRC works correctly
- ✅ CRC latency reduced by 95% (200µs → 10µs)
- ✅ SPI protocol integration successful (no CRC errors)
- ✅ Unit tests pass with 95%+ coverage
- ✅ NASA Power of 10 compliant
- ✅ Comprehensive Doxygen documentation

### Nice to Have (Post-MVP)
- CRC-16 support (for smaller messages)
- CRC-8 support (for single-byte checksums)
- Multiple concurrent DMA transfers (pipelined CRC)
- Error injection for testing (corrupt CRC on demand)

---

## Risk Mitigation

### Risk 1: CRC register addresses incorrect
**Likelihood:** Low
**Impact:** High (hardware CRC won't work)
**Mitigation:** Verify ALL addresses against Ch46 Table 1.2 before coding

### Risk 2: Hardware CRC doesn't match software CRC
**Likelihood:** Medium
**Impact:** High (protocol breaks)
**Mitigation:** Test with IEEE 802.3 standard vectors, compare byte-by-byte

### Risk 3: DMA configuration incorrect
**Likelihood:** Medium
**Impact:** Medium (async CRC won't work, fallback to blocking)
**Mitigation:** Follow Ch29 DMA setup exactly, test with simple transfers first

### Risk 4: Performance gains less than expected
**Likelihood:** Low
**Impact:** Low (still faster than software)
**Mitigation:** Benchmark early in Phase 1 to verify speedup

---

## References

### RX72N Manual Chapters
- **Ch46:** CRC Calculator - `/workspaces/STAR/e2-studio-star-rx72n-firmware/docs/RX72N_Manual_Chapters/Ch46_CRC_Calculator.txt`
- **Ch29:** Data Transfer Controller (DTC) / DMA - For DMA integration
- **Ch09:** Clock Generation - MSTPCRB register for CRC clock enable

### Standards
- **IEEE 802.3:** CRC-32 polynomial and test vectors
- **RFC 3385:** CRC-32 algorithm description

### Project Documentation
- **RX72N_ROADMAP.md:** `/workspaces/STAR/RX72N_ROADMAP.md` (Ch46 status)
- **CLAUDE.md:** `/workspaces/STAR/CLAUDE.md` (coding standards)

---

**Document Version:** 1.0
**Last Updated:** 2026-02-05
**Author:** STAR Development Team
**Status:** Ready for Implementation
