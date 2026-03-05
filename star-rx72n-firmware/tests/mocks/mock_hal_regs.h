// SPDX-License-Identifier: MIT
/* tests/mocks/mock_hal_regs.h */

/**
 * @file mock_hal_regs.h
 * @brief Mock HAL Register Structures for Unit Testing
 *
 * @details
 * Provides mock register structures and accessor functions for testing
 * GPIO, ADC, RIIC, and System peripherals without actual hardware. This
 * module enables host-side unit testing by simulating RX72N hardware
 * registers with controllable behavior including error injection.
 *
 * @par Test Architecture
 * @dot
 * digraph mock_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_test {
 *     label="Unit Test Environment";
 *     style=filled;
 *     color=lightyellow;
 *     unity [label="Unity Test\nFramework"];
 *     test [label="test_*.c\nTest Cases"];
 *   }
 *
 *   subgraph cluster_mock {
 *     label="Mock HAL Layer (This Module)";
 *     style=filled;
 *     color=lightblue;
 *     state [label="g_mock_hal\nGlobal State"];
 *     port [label="mock_port_regs_t\nGPIO Registers"];
 *     adc [label="mock_adc_regs_t\nADC Registers"];
 *     riic [label="mock_riic_regs_t\nI2C Registers"];
 *     sys [label="mock_system_regs_t\nSystem Registers"];
 *   }
 *
 *   subgraph cluster_uut {
 *     label="Unit Under Test";
 *     style=filled;
 *     color=lightgreen;
 *     gpio [label="gpio.c"];
 *     adc_drv [label="adc.c"];
 *     riic_drv [label="riic.c"];
 *   }
 *
 *   test -> unity;
 *   unity -> test;
 *   test -> state [label="Configure mocks"];
 *   test -> gpio [label="Call UUT"];
 *   gpio -> port [label="Register access"];
 *   adc_drv -> adc;
 *   riic_drv -> riic;
 *   state -> port;
 *   state -> adc;
 *   state -> riic;
 *   state -> sys;
 * }
 * @enddot
 *
 * @par Mock Capabilities
 * | Peripheral | Registers Mocked | Simulated Behaviors |
 * |------------|------------------|---------------------|
 * | GPIO (PORT) | PDR, PODR, PIDR, PMR, PCR, DSCR | Input simulation |
 * | ADC (S12AD) | ADCSR, ADCER, ADANSA, ADDRx | Timeout, values |
 * | RIIC (I2C) | ICCR1/2, ICMRx, ICFERx, ICSRx | NACK, busy, timeout |
 * | System | PRCR, MSTPCRx | Module stop control |
 *
 * @par Error Injection Features
 * | Function | Effect | Use Case |
 * |----------|--------|----------|
 * | mock_hal_set_adc_timeout(true) | ADC conversion never completes | Timeout handling |
 * | mock_hal_set_riic_nack(true) | I2C device returns NACK | Address error |
 * | mock_hal_set_riic_busy(true) | I2C bus stays busy | Arbitration lost |
 *
 * @par Memory Layout
 * @verbatim
 * g_mock_hal (mock_hal_state_t):
 * +------------------+
 * | ports[20]        |  <- mock_port_regs_t x 20 (120 bytes)
 * +------------------+
 * | adc[2]           |  <- mock_adc_regs_t x 2 (48 bytes)
 * +------------------+
 * | riic[3]          |  <- mock_riic_regs_t x 3 (42 bytes)
 * +------------------+
 * | system           |  <- mock_system_regs_t (14 bytes)
 * +------------------+
 * | adc_initialized[2]|  <- bool x 2
 * +------------------+
 * | riic_initialized[3]| <- bool x 3
 * +------------------+
 * | simulation flags |  <- Error injection controls
 * +------------------+
 * Total: ~250 bytes
 * @endverbatim
 *
 * @par Test Setup Pattern
 * @code
 * void setUp(void) {
 *     mock_hal_init();  // Reset all mocks to default state
 * }
 *
 * void tearDown(void) {
 *     mock_hal_reset();  // Clean up between tests
 * }
 *
 * void test_gpio_read_pin_high(void) {
 *     // Arrange: Set port 5 pin 3 high
 *     mock_hal_set_port_input(5, 0x08);
 *
 *     // Act: Call function under test
 *     bool pin_value = gpio_read(5, 3);
 *
 *     // Assert: Verify correct behavior
 *     TEST_ASSERT_TRUE(pin_value);
 * }
 *
 * void test_i2c_device_nack(void) {
 *     // Arrange: Simulate device not responding
 *     mock_hal_set_riic_nack(true);
 *
 *     // Act & Assert: Expect NACK error
 *     rx_err_t err = riic_write(0, 0x50, data, 1);
 *     TEST_ASSERT_EQUAL(k_rx_err_nack, err);
 * }
 * @endcode
 *
 * @par Test Isolation
 * Each test case should:
 * 1. Call mock_hal_init() in setUp() to reset state
 * 2. Configure only the mocks needed for that test
 * 3. Verify mock state in assertions if needed
 * 4. Call mock_hal_reset() in tearDown() (optional)
 *
 * @par Performance (Host-Side Testing)
 * | Operation | Time | Notes |
 * |-----------|------|-------|
 * | mock_hal_init() | < 1 us | memset() of state |
 * | Register access | < 10 ns | Direct memory access |
 * | Typical test | < 100 us | Full test case |
 *
 * @par Supported Test Frameworks
 * - Unity (primary): Used in STAR project
 * - Compatible with CMock for auto-generated mocks
 * - Works with any C unit test framework
 *
 * @par Module Dependencies
 * - <stdbool.h>: bool type for simulation flags
 * - <stdint.h>: Fixed-width integer types
 *
 * @par NASA Power of 10 Compliance
 * - Rule 1: [OK] No goto, setjmp, or recursion
 * - Rule 2: [OK] All loops bounded by array sizes
 * - Rule 3: [OK] No dynamic memory allocation (static global state)
 * - Rule 4: [OK] Simple accessor functions
 * - Rule 5: [OK] Input validation (port/channel bounds checking)
 * - Rule 6: [OK] Variables at smallest scope
 * - Rule 7: [OK] All return values checked
 * - Rule 8: [OK] Minimal preprocessor (C23 typed enums)
 * - Rule 9: [OK] No function pointers in mock structures
 * - Rule 10: [OK] Compiled with -Wall -Wextra -Werror
 *
 * @par SOLID Principles
 * - **S**: Single Responsibility - Mock hardware registers only
 * - **O**: Open/Closed - Extensible via new register structures
 * - **L**: Liskov Substitution - Mocks substitute real HAL
 * - **I**: Interface Segregation - Separate helpers per peripheral
 * - **D**: Dependency Inversion - UUT depends on register interface
 *
 * @see mock_hal_regs.c Implementation with accessor functions
 * @see Unity Test framework: http://www.throwtheswitch.org/unity
 *
 * @version 1.0.0
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 Locked Inc.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum mock_hal_constants_t
 * @brief Mock HAL array dimension constants
 *
 * @details
 * Defines array sizes for mock register structures. Values match RX72N
 * hardware capabilities to ensure tests can cover all peripheral channels.
 *
 * @par RX72N Hardware Mapping
 * | Constant | Value | RX72N Equivalent |
 * |----------|-------|------------------|
 * | k_mock_hal_max_ports | 20 | PORT0-PORT9, PORTA-PORTJ |
 * | k_mock_hal_max_adc_unit | 2 | S12AD0, S12AD1 |
 * | k_mock_hal_max_adc_ch | 8 | AN000-AN007 per unit |
 * | k_mock_hal_max_riic_ch | 3 | RIIC0, RIIC1, RIIC2 |
 *
 * @note Using C23 typed enum with uint8_t to minimize memory usage
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_mock_hal_max_ports    = 20, /**< Maximum ports (PORT0-9, PORTA-J) */
  k_mock_hal_max_adc_ch   = 8,  /**< Maximum ADC channels per unit (AN000-AN007) */
  k_mock_hal_max_adc_unit = 2,  /**< Maximum ADC units (S12AD0, S12AD1) */
  k_mock_hal_max_riic_ch  = 3,  /**< Maximum RIIC channels (RIIC0-2) */
} mock_hal_constants_t;

/* =============================================================================
 * Mock PORT Register Structure (simplified for testing)
 * =============================================================================
 */

/**
 * @struct mock_port_regs_t
 * @brief Mock PORT register structure for GPIO testing
 *
 * @details
 * Simplified representation of RX72N PORT registers. Contains only the
 * essential registers needed for GPIO unit testing. Each instance represents
 * one I/O port (8 pins).
 *
 * @par Register Bit Mapping
 * Each register bit corresponds to one pin (bit 0 = pin 0, etc.):
 * - Write 1 to PDR bit: Configure pin as output
 * - Write 1 to PODR bit: Drive pin high (when output)
 * - Read PIDR bit: Get current pin state (input or output)
 *
 * @par Memory Layout
 * @verbatim
 * Offset  Size  Register  Description
 * 0x00    1     PDR       Port Direction (0=input, 1=output)
 * 0x01    1     PODR      Port Output Data
 * 0x02    1     PIDR      Port Input Data (read-only on hardware)
 * 0x03    1     PMR       Port Mode (0=GPIO, 1=peripheral)
 * 0x04    1     PCR       Pull-up Control (1=enable pullup)
 * 0x05    1     DSCR      Drive Strength (1=high drive)
 * Total: 6 bytes per port
 * @endverbatim
 *
 * @par Example - Testing GPIO Output
 * @code
 * mock_port_regs_t* port5 = mock_hal_get_port(5);
 * gpio_configure_output(5, 3);  // Configure port 5 pin 3 as output
 * TEST_ASSERT_BITS(0x08, 0x08, port5->pdr);  // Verify PDR bit 3 set
 * @endcode
 *
 * @see mock_hal_get_port() Get pointer to specific port
 * @see mock_hal_set_port_input() Configure simulated input values
 * @since Version 1.0.0
 */
typedef struct {
  uint8_t pdr;  /**< Port Direction Register (0=input, 1=output per pin) */
  uint8_t podr; /**< Port Output Data Register (output pin level) */
  uint8_t pidr; /**< Port Input Data Register (current pin state, simulate with
                   mock_hal_set_port_input) */
  uint8_t pmr;  /**< Port Mode Register (0=GPIO, 1=peripheral function) */
  uint8_t pcr;  /**< Pull-up Control Register (1=enable internal pullup) */
  uint8_t dscr; /**< Drive Capacity Control Register (1=high drive strength) */
} mock_port_regs_t;

/* =============================================================================
 * Mock ADC Register Structure
 * =============================================================================
 */

/**
 * @struct mock_adc_regs_t
 * @brief Mock S12AD (12-bit A/D Converter) register structure
 *
 * @details
 * Simplified representation of RX72N S12AD registers for ADC unit testing.
 * Includes control registers and 8 data registers for conversion results.
 *
 * @par ADC Resolution
 * - 12-bit resolution: Values 0-4095 (0x000-0xFFF)
 * - Voltage calculation: @f$ V = \frac{ADC_{value} \times V_{ref}}{4096} @f$
 * - With 3.3V reference: 1 LSB ~ 0.806 mV
 *
 * @par Memory Layout
 * @verbatim
 * Offset  Size  Register   Description
 * 0x00    2     ADCSR      Control/Status (start, mode, interrupt)
 * 0x02    2     ADCER      Extended (format, clearing, self-diag)
 * 0x04    2     ADANSA0    Channel Select A0 (channels 0-15)
 * 0x06    2     ADANSA1    Channel Select A1 (channels 16-20)
 * 0x08    2     ADDR0      Data Register channel 0
 * 0x0A    2     ADDR1      Data Register channel 1
 * ...     ...   ...        ...
 * 0x16    2     ADDR7      Data Register channel 7
 * Total: 24 bytes per ADC unit
 * @endverbatim
 *
 * @par Example - Testing ADC Read
 * @code
 * // Arrange: Set ADC channel 0 to return 2048 (1.65V at 3.3V ref)
 * mock_hal_set_adc_value(0, 0, 2048);
 *
 * // Act: Trigger ADC conversion and read
 * uint16_t result;
 * rx_err_t err = adc_read(0, 0, &result);
 *
 * // Assert: Verify correct value returned
 * TEST_ASSERT_EQUAL(k_rx_ok, err);
 * TEST_ASSERT_EQUAL(2048, result);
 * @endcode
 *
 * @see mock_hal_set_adc_value() Set simulated conversion results
 * @see mock_hal_set_adc_timeout() Simulate conversion timeout
 * @since Version 1.0.0
 */
typedef struct {
  uint16_t adcsr;   /**< A/D Control/Status Register (start conversion, mode) */
  uint16_t adcer;   /**< A/D Conversion Extended Register (data format) */
  uint16_t adansa0; /**< A/D Channel Select Register A0 (channels 0-15) */
  uint16_t adansa1; /**< A/D Channel Select Register A1 (channels 16-20) */
  uint16_t addr0;   /**< A/D Data Register 0 (12-bit result, 0-4095) */
  uint16_t addr1;   /**< A/D Data Register 1 */
  uint16_t addr2;   /**< A/D Data Register 2 */
  uint16_t addr3;   /**< A/D Data Register 3 */
  uint16_t addr4;   /**< A/D Data Register 4 */
  uint16_t addr5;   /**< A/D Data Register 5 */
  uint16_t addr6;   /**< A/D Data Register 6 */
  uint16_t addr7;   /**< A/D Data Register 7 */
} mock_adc_regs_t;

/* =============================================================================
 * Mock RIIC Register Structure
 * =============================================================================
 */

/**
 * @struct mock_riic_regs_t
 * @brief Mock RIIC (I2C) register structure for I2C testing
 *
 * @details
 * Representation of RX72N RIIC peripheral registers for I2C unit testing.
 * Includes control, mode, status, and data registers for simulating
 * I2C transactions.
 *
 * @par Key Status Bits (ICSR2)
 * | Bit | Name | Meaning |
 * |-----|------|---------|
 * | 7 | TDRE | Transmit Data Empty (ready for next byte) |
 * | 5 | RDRF | Receive Data Full (byte received) |
 * | 4 | NACKF | NACK Detection Flag |
 * | 1 | STOP | Stop Condition Detected |
 * | 0 | START | Start Condition Detected |
 *
 * @par Memory Layout
 * @verbatim
 * Offset  Size  Register  Description
 * 0x00    1     ICCR1     Control 1 (enable, reset)
 * 0x01    1     ICCR2     Control 2 (start, stop, ack)
 * 0x02    1     ICMR1     Mode 1 (clock divider)
 * 0x03    1     ICMR2     Mode 2 (timeout, SDA delay)
 * 0x04    1     ICMR3     Mode 3 (ack control)
 * 0x05    1     ICFER     Function Enable (filters)
 * 0x06    1     ICSER     Status Enable
 * 0x07    1     ICIER     Interrupt Enable
 * 0x08    1     ICSR1     Status 1 (arbitration)
 * 0x09    1     ICSR2     Status 2 (TDRE, RDRF, NACK)
 * 0x0A    1     ICBRL     Bit Rate Low
 * 0x0B    1     ICBRH     Bit Rate High
 * 0x0C    1     ICDRT     Transmit Data
 * 0x0D    1     ICDRR     Receive Data
 * Total: 14 bytes per RIIC channel
 * @endverbatim
 *
 * @par Example - Testing I2C NACK
 * @code
 * // Arrange: Simulate device not responding
 * mock_hal_set_riic_nack(true);
 *
 * // Act: Attempt I2C write
 * rx_err_t err = riic_write(0, 0x50, data, 1);
 *
 * // Assert: Expect NACK error
 * TEST_ASSERT_EQUAL(k_rx_err_nack, err);
 * @endcode
 *
 * @see mock_hal_set_riic_nack() Simulate NACK from device
 * @see mock_hal_set_riic_busy() Simulate bus busy condition
 * @since Version 1.0.0
 */
typedef struct {
  uint8_t iccr1; /**< I2C Bus Control Register 1 (ICE enable, IICRST reset) */
  uint8_t iccr2; /**< I2C Bus Control Register 2 (ST start, SP stop, RS
                    restart) */
  uint8_t icmr1; /**< I2C Bus Mode Register 1 (CKS clock select, BCWP protect) */
  uint8_t icmr2; /**< I2C Bus Mode Register 2 (TMOH timeout, SDDL delay) */
  uint8_t icmr3; /**< I2C Bus Mode Register 3 (ACKWP, ACKBT, ACKBR) */
  uint8_t icfer; /**< I2C Bus Function Enable Register (filters, timeout) */
  uint8_t icser; /**< I2C Bus Status Enable Register */
  uint8_t icier; /**< I2C Bus Interrupt Enable Register */
  uint8_t icsr1; /**< I2C Bus Status Register 1 (AAS, GCA, DID, HOA, AL) */
  uint8_t icsr2; /**< I2C Bus Status Register 2 (TDRE, RDRF, NACKF, STOP,
                    START) */
  uint8_t icbrl; /**< I2C Bus Bit Rate Low Register (SCL low period) */
  uint8_t icbrh; /**< I2C Bus Bit Rate High Register (SCL high period) */
  uint8_t icdrt; /**< I2C Bus Transmit Data Register (write to send) */
  uint8_t icdrr; /**< I2C Bus Receive Data Register (read received byte) */
} mock_riic_regs_t;

/* =============================================================================
 * Mock System Register Structure
 * =============================================================================
 */

/**
 * @struct mock_system_regs_t
 * @brief Mock System control register structure
 *
 * @details
 * Representation of RX72N System control registers for testing module
 * stop control and register protection. Used to verify that drivers
 * properly handle protected register access.
 *
 * @par PRCR Protection Register
 * Controls write access to protected registers:
 * - Write 0xA5xx to unlock (upper byte = 0xA5)
 * - Bit 0 (PRC0): Clock generation registers
 * - Bit 1 (PRC1): Operating modes, software reset
 * - Bit 3 (PRC3): Low voltage detection
 *
 * @par Module Stop Control
 * Each bit in MSTPCRx controls power to one peripheral:
 * - Write 1: Stop module clock (power down)
 * - Write 0: Supply module clock (enable)
 *
 * @par Example - Testing Module Enable
 * @code
 * mock_system_regs_t* sys = mock_hal_get_system();
 *
 * // Set initial state: ADC module stopped
 * sys->mstpcrb = 0x00020000;  // MSTPB17 = 1 (S12AD0 stopped)
 *
 * // Act: Enable ADC module
 * adc_module_enable(0);
 *
 * // Assert: MSTPB17 should be cleared
 * TEST_ASSERT_BITS_LOW(0x00020000, sys->mstpcrb);
 * @endcode
 *
 * @see mock_hal_get_system() Get pointer to system registers
 * @since Version 1.0.0
 */
typedef struct {
  uint16_t prcr;    /**< Protection Register (0xA5xx to unlock protected regs) */
  uint32_t mstpcra; /**< Module Stop Control Register A (peripheral clocks) */
  uint32_t mstpcrb; /**< Module Stop Control Register B (peripheral clocks) */
  uint32_t mstpcrc; /**< Module Stop Control Register C (peripheral clocks) */
} mock_system_regs_t;

/* =============================================================================
 * Mock HAL State Structure
 * =============================================================================
 */

/**
 * @struct mock_hal_state_t
 * @brief Global mock HAL state containing all simulated hardware
 *
 * @details
 * Central structure containing all mock register state and simulation control
 * flags. A single global instance (g_mock_hal) provides the mock hardware
 * state for all unit tests.
 *
 * @par State Categories
 * 1. **Register Arrays**: Mock PORT, ADC, RIIC, System registers
 * 2. **Initialization Tracking**: Per-peripheral init state
 * 3. **Behavior Control**: Error injection flags
 *
 * @par Initialization Behavior
 * mock_hal_init() resets all state to defaults:
 * - All registers zeroed
 * - All init flags false
 * - ADC timeout disabled
 * - RIIC NACK/busy disabled
 * - Timeout count set to 1 (fast completion)
 * - Pin validator disabled
 *
 * @par Memory Layout
 * @verbatim
 * mock_hal_state_t:
 * +------------------------------------------+
 * | ports[20] (mock_port_regs_t x 20)        | 120 bytes
 * +------------------------------------------+
 * | adc[2] (mock_adc_regs_t x 2)             | 48 bytes
 * +------------------------------------------+
 * | riic[3] (mock_riic_regs_t x 3)           | 42 bytes
 * +------------------------------------------+
 * | system (mock_system_regs_t)              | 14 bytes
 * +------------------------------------------+
 * | adc_initialized[2]                       | 2 bytes
 * +------------------------------------------+
 * | riic_initialized[3]                      | 3 bytes
 * +------------------------------------------+
 * | riic_simulated_timeout_count             | 4 bytes
 * +------------------------------------------+
 * | riic_simulate_nack                       | 1 byte
 * +------------------------------------------+
 * | riic_simulate_busy                       | 1 byte
 * +------------------------------------------+
 * | adc_simulate_timeout                     | 1 byte
 * +------------------------------------------+
 * | pin_validator_active                     | 1 byte
 * +------------------------------------------+
 * Total: ~237 bytes (plus alignment padding)
 * @endverbatim
 *
 * @par Thread Safety
 * Not thread-safe - designed for single-threaded unit test execution.
 * Each test should call mock_hal_init() in setUp() to ensure clean state.
 *
 * @invariant ports array size matches k_mock_hal_max_ports
 * @invariant adc array size matches k_mock_hal_max_adc_unit
 * @invariant riic array size matches k_mock_hal_max_riic_ch
 *
 * @see g_mock_hal Global instance of this structure
 * @see mock_hal_init() Reset all state to defaults
 * @since Version 1.0.0
 */
typedef struct {
  /** @brief Mock PORT register arrays for all ports (PORT0-9, PORTA-J) */
  mock_port_regs_t ports[k_mock_hal_max_ports];

  /** @brief Mock ADC register arrays (S12AD0, S12AD1) */
  mock_adc_regs_t adc[k_mock_hal_max_adc_unit];

  /** @brief Mock RIIC register arrays (RIIC0, RIIC1, RIIC2) */
  mock_riic_regs_t riic[k_mock_hal_max_riic_ch];

  /** @brief Mock System control registers */
  mock_system_regs_t system;

  /** @brief ADC initialization state per unit (true = initialized) */
  bool adc_initialized[k_mock_hal_max_adc_unit];

  /** @brief RIIC initialization state per channel (true = initialized) */
  bool riic_initialized[k_mock_hal_max_riic_ch];

  /** @brief Simulated RIIC timeout counter (iterations before timeout) */
  uint32_t riic_simulated_timeout_count;

  /** @brief Enable NACK simulation on RIIC (device not responding) */
  bool riic_simulate_nack;

  /** @brief Enable bus busy simulation on RIIC (arbitration lost) */
  bool riic_simulate_busy;

  /** @brief Enable ADC conversion timeout simulation */
  bool adc_simulate_timeout;

  /** @brief Enable pin validator mock (returns mock interface when true) */
  bool pin_validator_active;
} mock_hal_state_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/**
 * @var g_mock_hal
 * @brief Global mock HAL state instance
 *
 * @details
 * Single global instance containing all mock hardware state. Tests access
 * this directly or through helper functions. Reset via mock_hal_init().
 *
 * @warning Direct modification should be avoided; use helper functions
 * @see mock_hal_init() Reset state before each test
 */
extern mock_hal_state_t g_mock_hal;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize all mock HAL state to default values
 *
 * @details
 * Resets all mock registers and control flags to their default state.
 * Should be called in test setUp() function to ensure clean state.
 *
 * Default values after init:
 * - All registers: 0x00
 * - ADC/RIIC initialized: false
 * - ADC timeout simulation: disabled
 * - RIIC NACK simulation: disabled
 * - RIIC busy simulation: disabled
 * - RIIC timeout count: 1 (fast completion)
 * - Pin validator: disabled
 *
 * @pre None
 * @post All mock state reset to defaults
 *
 * @note Not thread-safe; call from single test thread
 *
 * @par Example:
 * @code
 * void setUp(void) {
 *     mock_hal_init();  // Clean state for each test
 * }
 * @endcode
 *
 * @see mock_hal_reset() Alias for this function
 * @since Version 1.0.0
 */
void mock_hal_init(void);

/**
 * @brief Reset mock HAL state (alias for mock_hal_init)
 *
 * @details
 * Provided for semantic clarity in tearDown() functions.
 * Functionally identical to mock_hal_init().
 *
 * @see mock_hal_init() Primary initialization function
 * @since Version 1.0.0
 */
void mock_hal_reset(void);

/* =============================================================================
 * Port Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock port registers for port number
 *
 * @details
 * Returns pointer to mock PORT register structure for direct register access.
 * Use for verifying register writes or complex test scenarios.
 *
 * @param[in] port Port number (0-19, maps to PORT0-9, PORTA-J)
 *   - 0-9: PORT0 through PORT9
 *   - 10-19: PORTA through PORTJ
 *
 * @return Pointer to mock port registers
 * @retval NULL if port >= k_mock_hal_max_ports
 * @retval Valid pointer to mock_port_regs_t on success
 *
 * @pre port < k_mock_hal_max_ports
 *
 * @par Example:
 * @code
 * mock_port_regs_t* port5 = mock_hal_get_port(5);
 * if (port5 != nullptr) {
 *     TEST_ASSERT_BITS(0x08, 0x08, port5->pdr);  // Check PDR bit 3
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
mock_port_regs_t* mock_hal_get_port(uint8_t port);

/**
 * @brief Set mock port input data (for gpio_read testing)
 *
 * @details
 * Sets the simulated input pin values for a port. Use to test GPIO read
 * operations by pre-setting the PIDR register value.
 *
 * @param[in] port Port number (0 to k_mock_hal_max_ports-1)
 * @param[in] pidr Value to set in PIDR register (bit per pin)
 *
 * @pre port < k_mock_hal_max_ports
 * @post g_mock_hal.ports[port].pidr == pidr
 *
 * @note Silently ignores invalid port numbers (no error)
 *
 * @par Example:
 * @code
 * // Simulate port 5 pins 0 and 3 HIGH, others LOW
 * mock_hal_set_port_input(5, 0x09);  // Binary: 0000_1001
 *
 * TEST_ASSERT_TRUE(gpio_read(5, 0));   // Pin 0 HIGH
 * TEST_ASSERT_FALSE(gpio_read(5, 1));  // Pin 1 LOW
 * TEST_ASSERT_TRUE(gpio_read(5, 3));   // Pin 3 HIGH
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_hal_set_port_input(uint8_t port, uint8_t pidr);

/**
 * @brief Get mock port output data
 *
 * @details
 * Returns the current PODR register value for verification of GPIO write
 * operations. Each bit represents one pin's output level.
 *
 * @param[in] port Port number (0 to k_mock_hal_max_ports-1)
 *
 * @return PODR register value (bit per pin)
 * @retval 0 if port invalid
 *
 * @par Example:
 * @code
 * gpio_write(5, 3, true);  // Set port 5 pin 3 HIGH
 * TEST_ASSERT_BITS(0x08, 0x08, mock_hal_get_port_output(5));
 * @endcode
 *
 * @since Version 1.0.0
 */
uint8_t mock_hal_get_port_output(uint8_t port);

/**
 * @brief Get mock port direction
 *
 * @details
 * Returns the current PDR register value for verification of GPIO
 * configuration. Each bit indicates pin direction.
 *
 * @param[in] port Port number (0 to k_mock_hal_max_ports-1)
 *
 * @return PDR register value
 * @retval 0x00 All pins input (or port invalid)
 * @retval bit=1 Pin configured as output
 * @retval bit=0 Pin configured as input
 *
 * @par Example:
 * @code
 * gpio_configure_output(5, 3);  // Configure port 5 pin 3 as output
 * TEST_ASSERT_BITS(0x08, 0x08, mock_hal_get_port_direction(5));
 * @endcode
 *
 * @since Version 1.0.0
 */
uint8_t mock_hal_get_port_direction(uint8_t port);

/* =============================================================================
 * ADC Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock ADC registers for unit
 *
 * @details
 * Returns pointer to mock S12AD register structure for direct register
 * access. Use for verifying register configuration or complex scenarios.
 *
 * @param[in] unit ADC unit number
 *   - 0: S12AD0 (primary ADC)
 *   - 1: S12AD1 (secondary ADC)
 *
 * @return Pointer to mock ADC registers
 * @retval NULL if unit >= k_mock_hal_max_adc_unit
 * @retval Valid pointer to mock_adc_regs_t on success
 *
 * @pre unit < k_mock_hal_max_adc_unit
 *
 * @par Example:
 * @code
 * mock_adc_regs_t* adc0 = mock_hal_get_adc(0);
 * TEST_ASSERT_NOT_NULL(adc0);
 * TEST_ASSERT_EQUAL(expected_adcsr, adc0->adcsr);
 * @endcode
 *
 * @since Version 1.0.0
 */
mock_adc_regs_t* mock_hal_get_adc(uint8_t unit);

/**
 * @brief Set mock ADC conversion result
 *
 * @details
 * Pre-sets the ADC data register value for a specific channel. When the
 * ADC driver reads from this channel, it will return the set value.
 *
 * @param[in] unit ADC unit (0 or 1)
 * @param[in] channel ADC channel (0-7 for AN000-AN007 or AN100-AN107)
 * @param[in] value ADC conversion value (12-bit: 0-4095)
 *
 * @pre unit < k_mock_hal_max_adc_unit
 * @pre channel < k_mock_hal_max_adc_ch
 * @post Corresponding ADDR register set to value
 *
 * @note Silently ignores invalid unit/channel combinations
 *
 * @par Voltage Calculation:
 * @f$ V = \frac{value \times V_{ref}}{4096} @f$
 *
 * @par Example:
 * @code
 * // Simulate 1.65V reading with 3.3V reference
 * // 1.65V / 3.3V x 4096 = 2048
 * mock_hal_set_adc_value(0, 0, 2048);
 *
 * uint16_t result;
 * adc_read(0, 0, &result);
 * TEST_ASSERT_EQUAL(2048, result);
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_hal_set_adc_value(uint8_t unit, uint8_t channel, uint16_t value);

/**
 * @brief Enable/disable ADC timeout simulation
 *
 * @details
 * When enabled, simulates ADC conversion never completing. Use to test
 * timeout handling in ADC driver code.
 *
 * @param[in] simulate
 *   - true: ADC conversion never completes (timeout)
 *   - false: ADC conversion completes immediately (default)
 *
 * @post g_mock_hal.adc_simulate_timeout set to simulate
 *
 * @par Example:
 * @code
 * // Test timeout handling
 * mock_hal_set_adc_timeout(true);
 *
 * uint16_t result;
 * rx_err_t err = adc_read(0, 0, &result);
 * TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
 *
 * // Restore normal behavior
 * mock_hal_set_adc_timeout(false);
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_hal_set_adc_timeout(bool simulate);

/* =============================================================================
 * RIIC Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock RIIC registers for channel
 *
 * @details
 * Returns pointer to mock RIIC register structure for direct register
 * access. Use for verifying I2C register configuration.
 *
 * @param[in] channel RIIC channel number
 *   - 0: RIIC0 (I2C channel 0)
 *   - 1: RIIC1 (sensor expansion)
 *   - 2: RIIC2 (reserved)
 *
 * @return Pointer to mock RIIC registers
 * @retval NULL if channel >= k_mock_hal_max_riic_ch
 * @retval Valid pointer to mock_riic_regs_t on success
 *
 * @pre channel < k_mock_hal_max_riic_ch
 *
 * @par Example:
 * @code
 * mock_riic_regs_t* riic0 = mock_hal_get_riic(0);
 * TEST_ASSERT_NOT_NULL(riic0);
 * // Verify I2C was enabled
 * TEST_ASSERT_BITS(0x80, 0x80, riic0->iccr1);  // ICE bit
 * @endcode
 *
 * @since Version 1.0.0
 */
mock_riic_regs_t* mock_hal_get_riic(uint8_t channel);

/**
 * @brief Simulate RIIC NACK response
 *
 * @details
 * When enabled, simulates peripheral device returning NACK (not acknowledging).
 * Use to test error handling when device doesn't respond or rejects data.
 *
 * @param[in] simulate
 *   - true: All I2C transactions receive NACK
 *   - false: Normal ACK behavior (default)
 *
 * @post g_mock_hal.riic_simulate_nack set to simulate
 *
 * @par Common NACK Causes:
 * - Device not present at address
 * - Device busy (internal operation)
 * - Device rejected write data
 *
 * @par Example:
 * @code
 * // Test device not responding
 * mock_hal_set_riic_nack(true);
 *
 * uint8_t data = 0x00;
 * rx_err_t err = riic_write(0, 0x50, &data, 1);
 * TEST_ASSERT_EQUAL(k_rx_err_nack, err);
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_hal_set_riic_nack(bool simulate);

/**
 * @brief Simulate RIIC bus busy
 *
 * @details
 * When enabled, simulates I2C bus being held by another controller or
 * stuck condition. Use to test bus arbitration and busy error handling.
 *
 * @param[in] simulate
 *   - true: Bus appears busy (SDA/SCL held low)
 *   - false: Bus available (default)
 *
 * @post g_mock_hal.riic_simulate_busy set to simulate
 *
 * @par Example:
 * @code
 * // Test bus busy handling
 * mock_hal_set_riic_busy(true);
 *
 * rx_err_t err = riic_write(0, 0x50, data, 1);
 * TEST_ASSERT_EQUAL(k_rx_err_busy, err);
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_hal_set_riic_busy(bool simulate);

/**
 * @brief Set RIIC simulated timeout counter
 *
 * @details
 * Controls how many polling iterations occur before simulated operation
 * completes. Use to test both timeout and normal completion paths.
 *
 * @param[in] count Number of iterations before completion
 *   - 0: Immediate timeout (no progress)
 *   - 1: Immediate completion (default after init)
 *   - N: Complete after N polling cycles
 *
 * @post g_mock_hal.riic_simulated_timeout_count set to count
 *
 * @par Example:
 * @code
 * // Test slow device (many retries before completion)
 * mock_hal_set_riic_timeout_count(100);
 *
 * // Test timeout path (never completes)
 * mock_hal_set_riic_timeout_count(0);
 * rx_err_t err = riic_write(0, 0x50, data, 1);
 * TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
 * @endcode
 *
 * @since Version 1.0.0
 */
void mock_hal_set_riic_timeout_count(uint32_t count);

/* =============================================================================
 * System Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock system registers
 *
 * @details
 * Returns pointer to mock System control register structure. Use for
 * verifying module stop control and register protection behavior.
 *
 * @return Pointer to mock system registers (never NULL)
 *
 * @note Always returns valid pointer (system is singleton, not array)
 *
 * @par Example:
 * @code
 * mock_system_regs_t* sys = mock_hal_get_system();
 *
 * // Verify protection was unlocked before writing
 * TEST_ASSERT_EQUAL(0xA501, sys->prcr);  // PRC0 unlocked
 *
 * // Verify ADC module was enabled (MSTPB17 cleared)
 * TEST_ASSERT_BITS_LOW(0x00020000, sys->mstpcrb);
 * @endcode
 *
 * @since Version 1.0.0
 */
mock_system_regs_t* mock_hal_get_system(void);

/**
 * @brief Enable/disable pin validator in mock
 *
 * @details
 * Controls whether the mock pin validator interface is returned by
 * rx_infrastructure_get_pin_interface(). Use to test pin conflict
 * detection and validation logic.
 *
 * @param[in] active
 *   - true: rx_infrastructure_get_pin_interface() returns mock interface
 *   - false: rx_infrastructure_get_pin_interface() returns NULL (default)
 *
 * @post g_mock_hal.pin_validator_active set to active
 *
 * @par Mock Pin Interface Behavior:
 * When active, the mock pin interface:
 * - reserve_pin(): Always returns k_rx_ok
 * - release_pin(): Always returns k_rx_ok
 *
 * @par Example:
 * @code
 * // Enable pin validation in tests
 * mock_hal_set_pin_validator_active(true);
 *
 * // Now drivers that check pins will get a valid interface
 * rx_pin_interface_t* iface = rx_infrastructure_get_pin_interface();
 * TEST_ASSERT_NOT_NULL(iface);
 *
 * // Disable for tests that shouldn't have validation
 * mock_hal_set_pin_validator_active(false);
 * @endcode
 *
 * @see rx_infrastructure_get_pin_interface() Returns mock when active
 * @since Version 1.0.0
 */
void mock_hal_set_pin_validator_active(bool active);

#ifdef __cplusplus
}
#endif
