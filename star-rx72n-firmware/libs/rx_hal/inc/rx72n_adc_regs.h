/**
 * @file rx72n_adc_regs.h
 * @brief RX72N S12AD 12-bit A/D Converter Register Definitions
 *
 * @details
 * This header provides comprehensive register definitions for the RX72N's
 * 12-bit Successive Approximation A/D Converter (S12ADFa). The S12AD is
 * essential for motor current sensing in the STAR project's DRV8263H H-bridge
 * motor control system.
 *
 * @par System Architecture Context:
 * @verbatim
 * +----------------------------------------------------------------------+
 * |                    STAR Motor Control Current Path                   |
 * +----------------------------------------------------------------------+
 * |                                                                      |
 * |   Motor Current --> DRV8263H IPROPI --> Sense Resistor --> S12AD     |
 * |                         |                    |               |       |
 * |                    202 uA/A             5100 ohm sense   12-bit ADC  |
 * |                    current mirror       (~3.2A range)    (0-3.3V)    |
 * |                                                                      |
 * |   I_motor = (ADC_value x 3.3V / 4095) / 1.0302                      |
 * |                                                                      |
 * +----------------------------------------------------------------------+
 * @endverbatim
 *
 * @par S12AD Unit Specifications:
 *
 * | Unit    | Base Address | Channels    | Features                         |
 * |---------|--------------|-------------|----------------------------------|
 * | S12AD0  | 0x00089000   | AN000-AN007 | 8ch, high-speed, motor sensing   |
 * | S12AD1  | 0x00089100   | AN100-AN120 | 21ch, temp sensor, internal ref  |
 *
 * @par Conversion Timing (@ 60 MHz PCLKB):
 *
 * | Mode           | Conversion Time | Throughput Rate |
 * |----------------|-----------------|-----------------|
 * | Single scan    | ~1.0 us/ch      | ~1 MSPS         |
 * | Group scan     | ~0.9 us/ch      | ~1.1 MSPS       |
 * | Continuous     | ~0.85 us/ch     | ~1.2 MSPS       |
 *
 * @par Motor Current Sensing Pin Mapping (144-pin LFQFP):
 *
 * | Motor | DRV8263H Pin | ADC Channel | RX72N Pin |
 * |-------|-------------|-------------|-----------|
 * | M0    | IPROPI      | AN007       | P47       |
 * | M1    | IPROPI      | AN006       | P46       |
 * | M2    | IPROPI      | AN005       | P45       |
 * | M3    | IPROPI      | AN004       | P44       |
 *
 * @par Hardware Requirements:
 * - CPU: Renesas RX72N @ 240 MHz (ICLK)
 * - Peripheral clock: PCLKB = 60 MHz (ADC clock source)
 * - ADC reference: AVCC0 = 3.3V, AVSS0 = GND
 * - Sample hold: Internal S&H capacitor charged during sampling phase
 *
 * @par Memory Usage:
 * - Register structure size: 48 bytes (0x30)
 * - No dynamic allocation required
 * - Direct memory-mapped register access
 *
 * @see docs/sections/03_hardware_pinout.tex GPIO pin assignments
 * @see lib/rx_hal/src/adc.c ADC implementation
 * @see RX72N Group User's Manual: Hardware, Chapter 56 (12-Bit A/D Converter)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: [OK] No goto, setjmp, or recursion in inline functions
 * - Rule 3: [OK] Zero dynamic allocation - all registers statically mapped
 * - Rule 8: [OK] All constants as C23 typed enums (base addresses, offsets)
 * - Rule 10: [OK] Compile with -Wall -Wextra -Werror, zero warnings
 *
 * @par SOLID Principles:
 * - S: Single responsibility - only ADC register definitions
 * - O: Open for extension via accessor functions, closed for modification
 * - I: Interface segregation - minimal required registers exposed
 * - D: Dependency inversion - hardware accessed via typed pointers
 *
 * @par Module Dependencies:
 * @dot
 * digraph adc_deps {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *   adc_regs [label="rx72n_adc_regs.h"];
 *   stdint [label="<stdint.h>"];
 *   stddef [label="<stddef.h>"];
 *   adc_impl [label="adc.c"];
 *   adc_regs -> stdint;
 *   adc_regs -> stddef;
 *   adc_impl -> adc_regs;
 * }
 * @enddot
 *
 * @par Verification Status:
 * [PASS] VERIFIED (2026-04-20) - All base addresses and register offsets verified
 * against RX72N Manual R01UH0824EJ0111 Table 5.1. Critical bug fixed: spurious
 * reserved5[2] field removed; ADDR0-7 corrected from 0x22-0x30 to 0x20-0x2E.
 *
 * @author Locked, Inc.
 * @date 2026-01-28
 * @version 1.0.0
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup rx_adc_regs RX72N S12AD Register Definitions
 * @brief 12-bit A/D Converter hardware register interface
 *
 * @details
 * Provides memory-mapped register access to the RX72N S12AD peripheral
 * for motor current sensing applications. All register addresses and
 * offsets are compile-time verified via static assertions.
 *
 * @par Quick Start Example:
 * @code{.c}
 * // Read motor 0 current from AN007 (P47)
 * volatile rx_s12ad_regs_t* adc = s12ad0();
 *
 * // Enable channel 7 for single scan
 * adc->adansa0 = (1U << 7);  // AN007 only
 *
 * // Configure: single scan, software trigger
 * adc->adcsr = 0x0000;
 *
 * // Start conversion
 * adc->adcsr |= 0x8000;  // ADST bit
 *
 * // Wait for completion
 * while (adc->adcsr & 0x8000) {}
 *
 * // Read 12-bit result (right-aligned)
 * uint16_t raw_value = adc->addr7;
 *
 * // Convert to voltage (3.3V reference)
 * static const float s_vref_volts = 3.3F;
 * static const float s_adc_full_scale_12bit = 4095.0F;
 * float voltage_v = (float)raw_value * s_vref_volts / s_adc_full_scale_12bit;
 * @endcode
 *
 * @{
 */

/* =============================================================================
 * 12-bit A/D Converter (S12ADFa) - For Current Sensing
 * =============================================================================
 */

/**
 * @enum rx_s12ad_addresses_t
 * @brief S12ADFa peripheral base addresses
 *
 * @details
 * Hardware base addresses for the two S12AD units on RX72N. Each unit
 * occupies 256 bytes (0x100) of address space in the peripheral area.
 *
 * Per RX72N Manual Ch56 and Ch04 Memory Map:
 * - S12AD0: 0x00089000 (Unit 0, 8 high-speed channels)
 * - S12AD1: 0x00089100 (Unit 1, 21 channels + temperature sensor)
 *
 * @par Address Space Layout:
 *
 * | Address Range         | Description                    |
 * |-----------------------|--------------------------------|
 * | 0x00089000 - 0x000890FF | S12AD0 registers (Unit 0)    |
 * | 0x00089100 - 0x000891FF | S12AD1 registers (Unit 1)    |
 *
 * @par Unit Selection Guidelines:
 * - Use S12AD0 for motor current sensing (lower latency, 8 channels)
 * - Use S12AD1 for temperature monitoring (has internal sensor)
 *
 * @invariant Base addresses are 256-byte aligned (bits [7:0] = 0)
 * @invariant Unit spacing is exactly 0x100 bytes
 *
 * @see s12ad0() Accessor function for Unit 0
 * @see s12ad1() Accessor function for Unit 1
 */
typedef enum : uintptr_t {
  /**
   * @brief S12AD Unit 0 base address
   * @details
   * 8-channel high-speed ADC optimized for motor current sensing.
   * Channels AN000-AN007 mapped to ports P40-P47.
   *
   * @par Usage: Motor current sensing (all 4 motors)
   * @par Throughput: Up to 1 MSPS in continuous scan mode
   */
  k_s12ad0_base_addr = 0x00089000,

  /**
   * @brief S12AD Unit 1 base address
   * @details
   * 21-channel ADC with additional features including temperature
   * sensor and internal voltage reference channels.
   * Channels AN100-AN120 mapped to various ports.
   *
   * @par Usage: Temperature monitoring, analog sensing
   * @par Special Channels: Temperature sensor (AN127), Ref voltage (AN128)
   */
  k_s12ad1_base_addr = 0x00089100,
} rx_s12ad_addresses_t;

/**
 * @enum s12ad_offsets_t
 * @brief S12ADFa register offsets from base address
 *
 * @details
 * Register byte offsets for accessing S12AD peripheral registers.
 * All offsets verified against RX72N Manual Ch56, section 56.2.
 *
 * @par Register Map Overview:
 *
 * | Offset | Register | Size | Description                    |
 * |--------|----------|------|--------------------------------|
 * | 0x00   | ADCSR    | 16   | Control/Status (start, mode)  |
 * | 0x04   | ADANSA0  | 16   | Channel select Group A (0-15) |
 * | 0x08   | ADADS0   | 16   | Addition mode select          |
 * | 0x0C   | ADADC    | 8    | Addition count (1, 2, 4, 16)  |
 * | 0x0E   | ADCER    | 16   | Control extended (alignment)  |
 * | 0x10   | ADSTRGR  | 16   | Trigger source select         |
 * | 0x14   | ADANSB0  | 16   | Channel select Group B        |
 * | 0x84   | ADDBLDR  | 16   | Double trigger data (not in struct; accessed directly) |
 * | 0x1E   | ADRD     | 16   | Self-diagnosis data           |
 * | 0x20   | ADDR0    | 16   | Conversion result channel 0   |
 * | ...    | ...      | ...  | (2-byte spacing per channel)  |
 * | 0x2E   | ADDR7    | 16   | Conversion result channel 7   |
 *
 * @note Offsets are 8-bit values (max 255), sufficient for S12AD register space
 *
 * @invariant Data registers ADDR0-ADDR7 are contiguous with 2-byte spacing
 * @invariant All 16-bit registers are halfword-aligned (even addresses)
 */
typedef enum : uint8_t {
  k_s12ad_offset_adcsr   = 0x00, /**< A/D Control Status: start, mode, interrupt control */
  k_s12ad_offset_adansa0 = 0x04, /**< Channel Select A0: channels 0-15 for group A scan */
  k_s12ad_offset_adads0  = 0x08, /**< Addition Select 0: channels 0-15 for averaging */
  k_s12ad_offset_adadc   = 0x0C, /**< Addition Count: 1x, 2x, 4x, or 16x oversampling */
  k_s12ad_offset_adcer   = 0x0E, /**< Control Extended: data alignment, self-diag mode */
  k_s12ad_offset_adstrgr = 0x10, /**< Trigger Select: software, MTU, GPT, external */
  k_s12ad_offset_adansb0 = 0x14, /**< Channel Select B0: channels for group B scan */
  k_s12ad_offset_addbldr =
    0x84, /**< Double Trigger Data: double-trigger mode result (not in struct) */
  k_s12ad_offset_adrd  = 0x1E, /**< Self-Diagnosis Data: internal reference result */
  k_s12ad_offset_addr0 = 0x20, /**< Data Register 0: AN000/AN100 conversion result */
  k_s12ad_offset_addr1 = 0x22, /**< Data Register 1: AN001/AN101 conversion result */
  k_s12ad_offset_addr2 = 0x24, /**< Data Register 2: AN002/AN102 conversion result */
  k_s12ad_offset_addr3 = 0x26, /**< Data Register 3: AN003/AN103 conversion result */
  k_s12ad_offset_addr4 = 0x28, /**< Data Register 4: AN004/AN104 conversion result */
  k_s12ad_offset_addr5 = 0x2A, /**< Data Register 5: AN005/AN105 conversion result */
  k_s12ad_offset_addr6 = 0x2C, /**< Data Register 6: AN006/AN106 conversion result */
  k_s12ad_offset_addr7 = 0x2E, /**< Data Register 7: AN007/AN107 conversion result */
} s12ad_offsets_t;

/**
 * @enum s12ad_reserved_sizes_t
 * @brief S12ADFa register reserved field sizes for structure padding
 *
 * @details
 * Defines byte sizes for reserved regions in the S12AD register map.
 * These ensure proper alignment and offset matching between the C
 * structure and actual hardware register locations.
 *
 * @warning Do not modify these values - they are derived from the
 *          RX72N hardware register layout specification.
 *
 * @note Reserved bytes must be declared as uint8_t arrays, not accessed
 */
typedef enum : uint8_t {
  k_s12ad_reserved_02_03 = 2, /**< Padding after ADCSR (16-bit) to ADANSA0 (32-bit aligned) */
  k_s12ad_reserved_0d    = 1, /**< Padding after ADADC (8-bit) to ADCER (16-bit aligned) */
  k_s12ad_reserved_12_13 = 2, /**< Padding after ADSTRGR to ADANSB0 */
  k_s12ad_reserved_16_1d = 8, /**< Padding after ADANSB0 to ADRD (ADDBLDR at 0x84, not in struct) */
} s12ad_reserved_sizes_t;

/**
 * @struct rx_s12ad_regs_t
 * @brief S12ADFa Register Map for 12-bit A/D Converter
 *
 * @details
 * Memory-mapped register structure for the S12AD peripheral. This structure
 * provides direct hardware access to ADC control and data registers for
 * motor current sensing and analog input measurement.
 *
 * @par Memory Layout (Total: 48 bytes = 0x30):
 *
 * | Offset | Size | Field     | Description                        |
 * |--------|------|-----------|------------------------------------|
 * | 0x00   | 2    | adcsr     | Control/Status (mode, start, IRQ)  |
 * | 0x02   | 2    | reserved  | Reserved padding                   |
 * | 0x04   | 2    | adansa0   | Channel select A (ch 0-15)         |
 * | 0x06   | 2    | adansa1   | Channel select A (ch 16-20, Unit1) |
 * | 0x08   | 2    | adads0    | Addition mode (ch 0-15)            |
 * | 0x0A   | 2    | adads1    | Addition mode (ch 16-20, Unit1)    |
 * | 0x0C   | 1    | adadc     | Addition count (1, 2, 4, 16)       |
 * | 0x0D   | 1    | reserved  | Reserved padding                   |
 * | 0x0E   | 2    | adcer     | Control extended (alignment)       |
 * | 0x10   | 2    | adstrgr   | Trigger source select              |
 * | 0x12   | 2    | reserved  | Reserved padding                   |
 * | 0x14   | 2    | adansb0   | Channel select B (group B scan)    |
 * | 0x16   | 8    | reserved  | Reserved (ADDBLDR at 0x84, not in struct) |
 * | 0x1E   | 2    | adrd      | Self-diagnosis data                |
 * | 0x20   | 2    | addr0     | Result register AN0/AN100          |
 * | 0x22   | 2    | addr1     | Result register AN1/AN101          |
 * | 0x24   | 2    | addr2     | Result register AN2/AN102          |
 * | 0x26   | 2    | addr3     | Result register AN3/AN103          |
 * | 0x28   | 2    | addr4     | Result register AN4/AN104          |
 * | 0x2A   | 2    | addr5     | Result register AN5/AN105          |
 * | 0x2C   | 2    | addr6     | Result register AN6/AN106          |
 * | 0x2E   | 2    | addr7     | Result register AN7/AN107          |
 *
 * @par Usage Example - Single Channel Conversion:
 * @code{.c}
 * // Initialize ADC for motor 0 current sensing (AN007, P47)
 * volatile rx_s12ad_regs_t* adc = s12ad0();
 *
 * // 1. Select channel AN007 for group A scan
 * adc->adansa0 = (1U << 7);  // Bit 7 = AN007
 *
 * // 2. Configure single scan mode, software trigger
 * adc->adcsr = 0x0000;  // ADCS[1:0]=00 (single), TRGE=0 (software)
 *
 * // 3. Set right-alignment (bits 11:0 contain result)
 * adc->adcer = 0x0000;  // ADRFMT=0 (right-align)
 *
 * // 4. Start conversion (set ADST bit)
 * adc->adcsr |= 0x8000;
 *
 * // 5. Wait for conversion complete (ADST clears)
 * while (adc->adcsr & 0x8000) {
 *     // ~1 us per conversion
 * }
 *
 * // 6. Read 12-bit result
 * uint16_t raw_count = adc->addr7 & 0x0FFF;
 *
 * // 7. Convert to current (using DRV8263H-Q1 IPROPI current sensing)
 * // IPROPI mirror: 202 uA/A, sense resistor: 5100 ohm
 * // V_IPROPI = I_motor * 202e-6 * 5100 = I_motor * 1.0302
 * // so I_motor = V_IPROPI / 1.0302
 * static const float s_avcc_volts             = 3.3F;
 * static const float s_adc_full_scale_divisor = 4095.0F;  // 2^12 - 1 (12-bit full scale)
 * static const float s_ipropi_mirror          = 202e-6F;  // 202 uA/A current mirror ratio
 * static const float s_ipropi_sense_ohm       = 5100.0F;  // 5.1k sense resistor
 * static const float s_ipropi_divisor         = 1.0302F;  // = 202e-6 * 5100
 * float voltage = (float)raw_count * s_avcc_volts / s_adc_full_scale_divisor;
 * float current_amps = voltage / s_ipropi_divisor;
 * @endcode
 *
 * @par Usage Example - Continuous 4-Channel Scan:
 * @code{.c}
 * volatile rx_s12ad_regs_t* adc = s12ad0();
 *
 * // Enable channels AN004-AN007 (all 4 motor currents)
 * adc->adansa0 = 0x00F0;  // Bits [7:4] = channels 4-7
 *
 * // Configure continuous scan mode
 * adc->adcsr = 0x4000;  // ADCS[1:0]=10 (continuous scan), bits[14:13]=10
 *
 * // Start continuous conversion
 * adc->adcsr |= 0x8000;
 *
 * // Read results any time (updated continuously)
 * uint16_t motor0_current = adc->addr7;  // AN007 = M0
 * uint16_t motor1_current = adc->addr6;  // AN006 = M1
 * uint16_t motor2_current = adc->addr5;  // AN005 = M2
 * uint16_t motor3_current = adc->addr4;  // AN004 = M3
 *
 * // Stop conversion when done
 * adc->adcsr &= ~0x8000;
 * @endcode
 *
 * @note This struct covers registers common to both S12AD0 and S12AD1.
 *       S12AD1 has additional registers (ADDR8-20, temp sensor, etc.)
 *       beyond offset 0x30 that are not included in this structure.
 *
 * @warning All registers are memory-mapped and must be accessed as volatile.
 *          Do not cache register values in local variables for repeated use.
 *
 * @warning Reserved fields must never be written. Reading returns undefined values.
 *
 * @invariant sizeof(rx_s12ad_regs_t) == 0x30 (48 bytes)
 * @invariant All register offsets match RX72N Manual Ch56 Table 56.1
 * @invariant Data registers ADDR0-ADDR7 have 2-byte spacing
 *
 * @see s12ad0() Accessor for S12AD Unit 0
 * @see s12ad1() Accessor for S12AD Unit 1
 * @see RX72N Manual Ch56 Section 56.2 for register bit definitions
 */
typedef struct {
  /**
   * @brief A/D Control/Status Register (ADCSR) @ offset 0x00
   *
   * @details
   * Controls conversion mode, trigger source, and interrupt enable.
   *
   * @par Bit Field Description:
   * | Bits    | Name   | R/W | Description                          |
   * |---------|--------|-----|--------------------------------------|
   * | [15]    | ADST   | R/W | Conversion start (1=start/running)   |
   * | [14:13] | ADCS   | R/W | Scan mode (00=single, 01=group, 10=continuous) |
   * | [12]    | ADIE   | R/W | Scan end interrupt enable            |
   * | [9]     | TRGE   | R/W | Trigger start enable (0=software only, 1=sync/async trigger) |
   * | [8]     | EXTRG  | R/W | Trigger select (0=synchronous, 1=asynchronous) |
   * | [7]     | DBLE   | R/W | Double trigger mode select           |
   * | [6]     | GBADIE | R/W | Group B scan end interrupt enable    |
   *
   * @par Common Configurations:
   * - 0x0000: Single scan, software trigger, no interrupt
   * - 0x4000: Continuous scan, software trigger
   * - 0x8000: Start conversion (write 1 to ADST)
   */
  volatile uint16_t adcsr;

  uint8_t reserved0[k_s12ad_reserved_02_03]; /**< Reserved @ 0x02-0x03 - do not access */

  /**
   * @brief A/D Channel Select Register A0 (ADANSA0) @ offset 0x04
   *
   * @details
   * Selects channels 0-15 for Group A scan. Each bit enables the
   * corresponding analog input channel.
   *
   * @par Channel Mapping (S12AD0, 144-pin LFQFP):
   * | Bit | Channel | Pin  | Usage |
   * |-----|---------|------|-------|
   * | 0-3 | AN000-3 | P40-43 | Available |
   * | 4   | AN004   | P44  | M3 current |
   * | 5   | AN005   | P45  | M2 current |
   * | 6   | AN006   | P46  | M1 current |
   * | 7   | AN007   | P47  | M0 current |
   *
   * @par Example: Select motor 0 and 1 current channels
   * @code
   * adc->adansa0 = 0x00C0;  // Enable AN007 (M0), AN006 (M1)
   * @endcode
   */
  volatile uint16_t adansa0;

  /**
   * @brief A/D Channel Select Register A1 (ADANSA1) @ offset 0x06
   *
   * @details
   * Selects channels 16-20 for Group A scan (S12AD1 only).
   * For S12AD0, this register exists but bits [7:0] are reserved.
   *
   * @note Only valid for S12AD1. S12AD0 has only 8 channels (0-7).
   */
  volatile uint16_t adansa1;

  /**
   * @brief A/D Addition Mode Select Register 0 (ADADS0) @ offset 0x08
   *
   * @details
   * Selects channels 0-15 for addition/averaging mode. When enabled,
   * multiple conversions are accumulated or averaged per the ADADC setting.
   *
   * @par Oversampling Benefit:
   * - 4x averaging: Reduces noise by ~6 dB (factor of 2)
   * - 16x averaging: Reduces noise by ~12 dB (factor of 4)
   */
  volatile uint16_t adads0;

  /**
   * @brief A/D Addition Mode Select Register 1 (ADADS1) @ offset 0x0A
   *
   * @details
   * Selects channels 16-20 for addition mode (S12AD1 only).
   */
  volatile uint16_t adads1;

  /**
   * @brief A/D Addition Count Select Register (ADADC) @ offset 0x0C
   *
   * @details
   * Sets the number of conversions to add/average for oversampling.
   *
   * @par Valid Values:
   * | Value | Additions | Effect on Data | Time Multiplier |
   * |-------|-----------|----------------|-----------------|
   * | 0x00  | 1x        | No addition    | 1x              |
   * | 0x01  | 2x        | Sum or average | 2x              |
   * | 0x02  | 3x        | Sum or average | 3x              |
   * | 0x03  | 4x        | Sum or average | 4x              |
   * | 0x05  | 16x       | Sum or average | 16x             |
   */
  volatile uint8_t adadc;

  uint8_t reserved1[k_s12ad_reserved_0d]; /**< Reserved @ 0x0D - do not access */

  /**
   * @brief A/D Control Extended Register (ADCER) @ offset 0x0E
   *
   * @details
   * Configures data alignment and self-diagnosis mode.
   *
   * @par Key Bits:
   * - Bit 15 (ADRFMT): 0=right-align (bits [11:0]), 1=left-align (bits [15:4])
   * - Bit 5 (ACE): Auto-clear enable for data registers
   *
   * @par Alignment Example:
   * - Right-align: Result in bits [11:0], bits [15:12] = 0
   * - Left-align: Result in bits [15:4], bits [3:0] = 0
   */
  volatile uint16_t adcer;

  /**
   * @brief A/D Start Trigger Select Register (ADSTRGR) @ offset 0x10
   *
   * @details
   * Selects the trigger source for starting A/D conversion when
   * hardware triggering is enabled (ADCSR.TRGE = 1).
   *
   * @par Trigger Sources (TRSA[5:0] at bits [13:8]; TRSB[5:0] at bits [5:0]):
   * | Value | Source           | Use Case                |
   * |-------|------------------|-------------------------|
   * | 0x00  | Software (ADST)  | Polled conversion       |
   * | 0x01  | MTU0 TGIA        | Motor PWM sync          |
   * | 0x02  | MTU0 TGIB        | Motor PWM sync          |
   * | 0x09  | MTU4 TADCORA     | MTU4 PWM sync           |
   */
  volatile uint16_t adstrgr;

  uint8_t reserved2[k_s12ad_reserved_12_13]; /**< Reserved @ 0x12-0x13 - do not access */

  /**
   * @brief A/D Channel Select Register B0 (ADANSB0) @ offset 0x14
   *
   * @details
   * Selects channels for Group B scan (dual-group scanning mode).
   * Group B runs in parallel with Group A for interleaved sampling.
   */
  volatile uint16_t adansb0;

  uint8_t
    reserved3[k_s12ad_reserved_16_1d]; /**< Reserved @ 0x16-0x1D (ADDBLDR at 0x84, not in struct) */

  /**
   * @brief A/D Self-Diagnosis Data Register (ADRD) @ offset 0x1E
   *
   * @details
   * Contains result of self-diagnosis conversion when ADCER.DIAGM = 1.
   * Used for ADC calibration and health monitoring.
   */
  volatile uint16_t adrd;

  /**
   * @brief A/D Data Register 0 (ADDR0) @ offset 0x20
   *
   * @details
   * 12-bit conversion result for channel AN000 (Unit 0) or AN100 (Unit 1).
   *
   * @par Data Format (ADCER.ADRFMT=0, right-aligned):
   * - Bits [11:0]: 12-bit result (0-4095)
   * - Bits [15:12]: Always 0
   *
   * @par Conversion Formula:
   * @f$ V_{in} = \frac{ADC_{result} \times V_{ref}}{4095} @f$
   *
   * where Vref = 3.3V (AVCC0)
   */
  volatile uint16_t addr0;

  volatile uint16_t addr1; /**< A/D Data Register 1 @ 0x22: AN001/AN101 */
  volatile uint16_t addr2; /**< A/D Data Register 2 @ 0x24: AN002/AN102 */
  volatile uint16_t addr3; /**< A/D Data Register 3 @ 0x26: AN003/AN103 */
  volatile uint16_t addr4; /**< A/D Data Register 4 @ 0x28: AN004/AN104 (Motor 3 current) */
  volatile uint16_t addr5; /**< A/D Data Register 5 @ 0x2A: AN005/AN105 (Motor 2 current) */
  volatile uint16_t addr6; /**< A/D Data Register 6 @ 0x2C: AN006/AN106 (Motor 1 current) */
  volatile uint16_t addr7; /**< A/D Data Register 7 @ 0x2E: AN007/AN107 (Motor 0 current) */
} rx_s12ad_regs_t;

/**
 * @brief Get pointer to S12AD0 (Unit 0) registers
 *
 * @details
 * Returns a volatile pointer to the S12AD Unit 0 register structure,
 * enabling direct hardware register access for ADC operations.
 * Unit 0 is optimized for motor current sensing with 8 high-speed channels.
 *
 * @return Volatile pointer to S12AD0 register structure at 0x00089000
 *
 * @pre Module stop state must be released (MSTPCRA.MSTPA17 = 0)
 * @pre AVCC0 and AVSS0 power supplies must be stable
 *
 * @post Pointer is immediately usable for register access
 * @post No hardware state is modified by this function
 *
 * @note Thread Safety: Function is reentrant and thread-safe (returns constant address)
 * @note Performance: Zero overhead - compiles to single load instruction
 *
 * @par Example - Motor Current Sensing (144-pin LFQFP):
 * @code{.c}
 * // Read all 4 motor currents (AN004-AN007 on P44-P47)
 * volatile rx_s12ad_regs_t* adc = s12ad0();
 *
 * // Configure for 4-channel scan
 * adc->adansa0 = 0x00F0;   // AN004-AN007
 * adc->adcsr = 0x0000;     // Single scan, software trigger
 * adc->adcer = 0x0000;     // Right-aligned data
 *
 * // Perform conversion
 * adc->adcsr |= 0x8000;    // Start
 * while (adc->adcsr & 0x8000) {}  // Wait (~4 us for 4 channels)
 *
 * // Read currents (M0=AN007, M1=AN006, M2=AN005, M3=AN004)
 * uint16_t motor_currents[4] = {
 *     adc->addr7, adc->addr6, adc->addr5, adc->addr4
 * };
 * @endcode
 *
 * @see s12ad1() For temperature sensing and additional channels
 * @see rx_s12ad_regs_t Register structure definition
 */
static inline volatile rx_s12ad_regs_t* s12ad0(void)
{
  return (volatile rx_s12ad_regs_t*)k_s12ad0_base_addr;
}

/**
 * @brief Get pointer to S12AD1 (Unit 1) registers
 *
 * @details
 * Returns a volatile pointer to the S12AD Unit 1 register structure.
 * Unit 1 provides 21 analog channels plus temperature sensor and
 * internal voltage reference for calibration and monitoring.
 *
 * @return Volatile pointer to S12AD1 register structure at 0x00089100
 *
 * @pre Module stop state must be released (MSTPCRA.MSTPA16 = 0)
 * @pre AVCC0 and AVSS0 power supplies must be stable
 *
 * @post Pointer is immediately usable for register access
 * @post No hardware state is modified by this function
 *
 * @note Thread Safety: Function is reentrant and thread-safe
 * @note Performance: Zero overhead - compiles to single load instruction
 *
 * @warning The rx_s12ad_regs_t structure only covers registers up to ADDR7.
 *          S12AD1 has additional registers (ADDR8-20, temperature sensor,
 *          internal reference) at higher offsets that require direct access.
 *
 * @par Example - Temperature Sensor Reading:
 * @code{.c}
 * volatile rx_s12ad_regs_t* adc = s12ad1();
 *
 * // Enable internal temperature sensor (AN127)
 * // Note: Requires accessing registers beyond rx_s12ad_regs_t
 * // See RX72N Manual Ch56 for temperature sensor configuration
 * @endcode
 *
 * @see s12ad0() For motor current sensing (Unit 0)
 * @see rx_s12ad_regs_t Register structure definition
 */
static inline volatile rx_s12ad_regs_t* s12ad1(void)
{
  return (volatile rx_s12ad_regs_t*)k_s12ad1_base_addr;
}

/* =============================================================================
 * Static Assertions - Verify Register Layout at Compile Time
 * =============================================================================
 */

/* Verify base addresses match Hardware Manual Ch56 */
static_assert(k_s12ad0_base_addr == 0x00089000, "S12AD0 base address incorrect");
static_assert(k_s12ad1_base_addr == 0x00089100, "S12AD1 base address incorrect");

/* Verify channel spacing (0x100 bytes between units) */
static_assert((k_s12ad1_base_addr - k_s12ad0_base_addr) == 0x100,
              "S12AD0 to S12AD1 spacing incorrect");

/* Verify register structure layout - offsets per RX72N Manual Ch56 */
static_assert(sizeof(rx_s12ad_regs_t) == 0x30,
              "S12AD register structure size incorrect (expected 0x30)");
static_assert(offsetof(rx_s12ad_regs_t, adcsr) == 0x00, "ADCSR offset incorrect (expected 0x00)");
static_assert(offsetof(rx_s12ad_regs_t, adansa0) == 0x04,
              "ADANSA0 offset incorrect (expected 0x04)");
static_assert(offsetof(rx_s12ad_regs_t, adansa1) == 0x06,
              "ADANSA1 offset incorrect (expected 0x06)");
static_assert(offsetof(rx_s12ad_regs_t, adads0) == 0x08, "ADADS0 offset incorrect (expected 0x08)");
static_assert(offsetof(rx_s12ad_regs_t, adads1) == 0x0A, "ADADS1 offset incorrect (expected 0x0A)");
static_assert(offsetof(rx_s12ad_regs_t, adadc) == 0x0C, "ADADC offset incorrect (expected 0x0C)");
static_assert(offsetof(rx_s12ad_regs_t, adcer) == 0x0E, "ADCER offset incorrect (expected 0x0E)");
static_assert(offsetof(rx_s12ad_regs_t, adstrgr) == 0x10,
              "ADSTRGR offset incorrect (expected 0x10)");
static_assert(offsetof(rx_s12ad_regs_t, adansb0) == 0x14,
              "ADANSB0 offset incorrect (expected 0x14)");
static_assert(offsetof(rx_s12ad_regs_t, adrd) == 0x1E, "ADRD offset incorrect (expected 0x1E)");
static_assert(offsetof(rx_s12ad_regs_t, addr0) == k_s12ad_offset_addr0, "ADDR0 offset incorrect");
static_assert(offsetof(rx_s12ad_regs_t, addr1) == k_s12ad_offset_addr1, "ADDR1 offset incorrect");
static_assert(offsetof(rx_s12ad_regs_t, addr2) == k_s12ad_offset_addr2, "ADDR2 offset incorrect");
static_assert(offsetof(rx_s12ad_regs_t, addr3) == k_s12ad_offset_addr3, "ADDR3 offset incorrect");
static_assert(offsetof(rx_s12ad_regs_t, addr4) == k_s12ad_offset_addr4, "ADDR4 offset incorrect");
static_assert(offsetof(rx_s12ad_regs_t, addr5) == k_s12ad_offset_addr5, "ADDR5 offset incorrect");
static_assert(offsetof(rx_s12ad_regs_t, addr6) == k_s12ad_offset_addr6, "ADDR6 offset incorrect");
static_assert(offsetof(rx_s12ad_regs_t, addr7) == k_s12ad_offset_addr7, "ADDR7 offset incorrect");

/** @} */ /* End of rx_adc_regs group */

#ifdef __cplusplus
}
#endif
