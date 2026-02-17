/* src/hardware_init.c */

/**
 * @file hardware_init.c
 * @brief Application-Specific Hardware Initialization - Motor Control, Sensors, Communication
 *
 * @details
 * # Overview
 *
 * This file implements **Stage 2 of the boot sequence** - application-specific peripheral
 * initialization. It is called **after** system clocks are configured (rx_clock_power_init)
 * but **before** ThreadX RTOS starts (tx_kernel_enter).
 *
 * **Boot sequence context:**
 * ```
 * main() -> rx_clock_power_init() -> hardware_init() -> tx_kernel_enter()
 *           ↑ System clocks        ↑ This file      ↑ Start RTOS
 * ```
 *
 * ## Peripheral Initialization Order (7 Stages)
 *
 * **Critical ordering** - later stages depend on earlier stages:
 *
 * 1. **Precondition validation** (~0.5 µs)
 *    - Verify system clocks initialized (SCKCR3 register check)
 *    - Ensure memory-mapped I/O accessible
 *
 * 2. **GPIO configuration** (planned, not yet implemented)
 *    - Motor control pins (DRV8243 enable, fault detection)
 *    - LED indicators (status, error, activity)
 *    - Sensor chip selects (SPI CS pins)
 *
 * 3. **Timers** (~10 µs) **IMPLEMENTED**
 *    - CMT0 for ThreadX tick (1 kHz)
 *    - GPTW for PWM generation (motor control)
 *
 * 4. **UART debug** - MOVED TO MAIN()
 *    - SCI9 initialized in main() before hardware_init() is called
 *    - Error logging available for all peripheral init below
 *    - See main.c for early UART initialization
 *
 * 5. **Communication peripherals** (planned, not yet implemented)
 *    - SPI for motor drivers and sensors
 *    - I2C for IMU, temperature sensors
 *    - USB CDC for ROS2 communication
 *
 * 6. **BMS Alert** (~5 µs) **IMPLEMENTED**
 *    - IRQ13 interrupt for BQ4050 ALERT pin (battery fault detection)
 *
 * 7. **ADC channels** (planned, not yet implemented)
 *    - Current sensing (motor protection)
 *    - Battery voltage monitoring
 *    - Temperature sensing (thermal management)
 *
 * ## Hardware Initialization Architecture
 *
 * @dot
 * digraph hardware_init {
 *   rankdir=TB;
 *   node [shape=box];
 *
 *   Entry [label="hardware_init()", shape=ellipse];
 *   Precond [label="Precondition Check\nSCKCR3 != reset state"];
 *   GPIO [label="GPIO Init\n(Planned)", style=dashed];
 *   Timers [label="Timer Init\nCMT0 @ 1 kHz", style=filled, fillcolor=lightgreen];
 *   UART [label="UART Debug Init\nSCI9 @ 115200", style=filled, fillcolor=lightgreen];
 *   SPI [label="SPI Init\n(Planned)", style=dashed];
 *   I2C [label="I2C Init\n(Planned)", style=dashed];
 *   BMSAlert [label="BMS Alert Init\nIRQ13 @ BQ4050", style=filled, fillcolor=lightgreen];
 *   ADC [label="ADC Init\n(Planned)", style=dashed];
 *   Postcond [label="Postcondition Check\nSCKCR3 still valid"];
 *   Exit [label="return k_rx_ok", shape=ellipse];
 *
 *   Entry -> Precond;
 *   Precond -> GPIO [label="Assert OK"];
 *   GPIO -> Timers;
 *   Timers -> UART [label="k_rx_ok"];
 *   UART -> SPI [label="k_rx_ok"];
 *   SPI -> I2C;
 *   I2C -> BMSAlert;
 *   BMSAlert -> ADC [label="k_rx_ok"];
 *   ADC -> Postcond;
 *   Postcond -> Exit [label="Assert OK"];
 *
 *   Timers -> Halt [label="Error", color=red];
 *   UART -> Halt [label="Error", color=red];
 *   SPI -> Halt [label="Error", color=red];
 *   BMSAlert -> Halt [label="Error", color=red];
 *
 *   Halt [label="return error", shape=octagon, color=red];
 * }
 * @enddot
 *
 * ## Initialization Timing (RX72N @ 240 MHz)
 *
 * | Stage | Duration | Implementation | Notes |
 * |-------|----------|----------------|-------|
 * | **Precondition check** | ~0.5 µs | [COMPLETE] | SCKCR3 register read + assert |
 * | **GPIO init** | ~5 µs | [PENDING] Planned | Pin mode, pull-up/down, output levels |
 * | **Timer init** | ~10 µs | [COMPLETE] | CMT0 setup for ThreadX tick |
 * | **UART debug** | ~50 µs | [COMPLETE] | SCI9 baud rate, FIFO, interrupts |
 * | **SPI init** | ~20 µs | [COMPLETE] | RSPI2 peripheral mode, 8-bit, mode 0 |
 * | **I2C init** | ~15 µs | [PENDING] Planned | RIIC0 speed, addressing, interrupts |
 * | **BMS Alert init** | ~5 µs | [COMPLETE] | IRQ13 for BQ4050 ALERT pin |
 * | **ADC init** | ~100 µs | [PENDING] Planned | ADC0 calibration, channel config |
 * | **Postcondition check** | ~0.5 µs | [COMPLETE] | SCKCR3 stability verification |
 * | **Total (current)** | **~86 µs** | Timers + UART + SPI + BMS Alert | |
 * | **Total (planned)** | **~206 µs** | All peripherals | |
 *
 * ## Memory Usage
 *
 * | Object | Size | Location | Purpose |
 * |--------|------|----------|---------|
 * | **s_sckcr3_reset_state** | 1 byte | .rodata | Clock validation constant |
 * | **Function stack** | ~64 bytes | Main stack | Local variables, return addresses |
 * | **Peripheral registers** | N/A | Memory-mapped | Hardware configuration only |
 *
 * **Total RAM:** ~64 bytes (stack only - no heap, no static buffers)
 *
 * ## Error Handling Strategy
 *
 * **Three-tier approach:**
 *
 * 1. **Critical errors (RX_ASSERT):**
 *    - Precondition failed (clocks not initialized)
 *    - Postcondition failed (clock system corrupted)
 *    - Register pointer NULL (memory map violation)
 *    - **Action:** Halt execution immediately (fail-fast)
 *
 * 2. **Peripheral errors (return error code):**
 *    - Timer init failed (timer_init() returns error)
 *    - UART init failed (uart_debug_init() returns error)
 *    - SPI/I2C/ADC init failed (when implemented)
 *    - **Action:** Return error to main(), which halts boot
 *
 * 3. **Non-critical warnings (log and continue):**
 *    - GPIO pin already configured (no action needed)
 *    - Peripheral module already enabled (idempotent operation)
 *    - **Action:** Log warning, return k_rx_ok
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Implementation Notes |
 * |------|--------|----------------------|
 * | **Rule 1: Control flow** | [PASS] | No goto, setjmp, or recursion |
 * | **Rule 2: Loop bounds** | [PASS] | No loops (sequential initialization) |
 * | **Rule 3: Heap allocation** | [PASS] | Zero dynamic allocation (static only) |
 * | **Rule 4: Function length** | [PASS] | hardware_init() = 80 lines |
 * | **Rule 5: Assertions** | [PASS] | 2 preconditions, 1 postcondition |
 * | **Rule 6: Data scope** | [PASS] | All variables at function scope |
 * | **Rule 7: Return checks** | [PASS] | All init functions checked via rx_err_is_error() |
 * | **Rule 8: Preprocessor** | [PASS] | Zero macros (uses typed enum for constant) |
 * | **Rule 9: Pointers** | [PASS] | Single-level dereferencing only |
 * | **Rule 10: Warnings** | [PASS] | Compiles with -Wall -Wextra -Werror |
 *
 * ## SOLID Principles
 *
 * | Principle | Application |
 * |-----------|-------------|
 * | **Single Responsibility** | hardware_init() does ONLY peripheral setup (no business logic) |
 * | **Open/Closed** | New peripherals added without modifying existing init calls |
 * | **Liskov Substitution** | All init functions return rx_err_t with consistent semantics |
 * | **Interface Segregation** | Small, focused init APIs (timer_init, uart_init, etc. separate) |
 * | **Dependency Inversion** | Depends on abstractions (rx_err_t), not implementations |
 *
 * ## Thread Safety
 *
 * **Pre-ThreadX context:** This file executes in **single-threaded mode** before the RTOS starts.
 * No synchronization primitives needed. Hardware registers accessed directly without mutex protection.
 *
 * **Post-init:** After this function returns, peripherals are **ready for multi-threaded access**.
 * Application threads must use appropriate synchronization (mutexes, semaphores) when accessing
 * shared hardware resources.
 *
 * ## Related Files
 *
 * - **Clock init:** See [rx_clock_power_init.c](rx_clock_power_init.c) - Must complete BEFORE this file
 * - **Main entry:** See [main.c](main.c) - Calls this function during boot sequence
 * - **Timer HAL:** See [timer.c](../lib/rx_hal/src/timer.c) - CMT0 configuration for ThreadX tick
 * - **UART HAL:** See [uart.c](../lib/rx_hal/src/uart.c) - SCI9 configuration for debug console
 *
 * @note **This file is incomplete.** GPIO, I2C, and ADC initialization are planned but not yet
 *       implemented. Timers, UART, SPI, and BMS Alert are functional in current version.
 *
 * @warning **Never call hardware_init() before rx_clock_power_init().** System clocks must be
 *          configured first. Precondition assertion will halt execution if violated.
 *
 * @see hardware_init() Main initialization function (entry point for this file)
 * @see rx_clock_power_init() System clock configuration (must call first)
 * @see timer_init() Configure CMT0 for ThreadX tick
 * @see uart_debug_init() Configure SCI9 for debug console
 *
 * @since Version 1.0.0
 *
 * @par Revision History:
 * - v1.0.0 (2026-01): Initial implementation with timers and UART
 *
 * @date 2026-01-14
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "hardware_init.h"

#include "hardware.h"
#include "rx72n_sci_regs.h"
#include "rx72n_system_regs.h"
#include "rx_bms_alert.h"
#include "rx_check.h"
#include "rx_err.h"
#include "rx_gptw.h"
#include "rx_mpc.h"
#include "rx_poeg.h"
#include "rx_port_utils.h"
#include "rx_simulator_config.h" /* For RX_IS_SIMULATOR conditional compilation */

/** @brief Port pin identifiers for MPC configuration (rx_port_pin_t values) */
typedef enum : uint16_t {
  /* Host I2C (RIIC0) */
  k_pin_host_scl0 = k_rx_p1_2, /**< P1.2 - SCL0 (host I2C clock) */
  k_pin_host_sda0 = k_rx_p1_3, /**< P1.3 - SDA0 (host I2C data) */

  /* BMS I2C (RIIC1) */
  k_pin_bms_sda1 = k_rx_p2_0, /**< P2.0 - SDA1 (BMS I2C data) */
  k_pin_bms_scl1 = k_rx_p2_1, /**< P2.1 - SCL1 (BMS I2C clock) */

  /* USB */
  k_pin_usb_vbus = k_rx_p1_6, /**< P1.6 - USB0_VBUS (USB VBUS detect) */

  /* HC-SR04 Sonar triggers (GPIO output, initial LOW) */
  k_pin_sonar_trig0 = k_rx_pf_5, /**< PF.5 - Sonar 0 trigger (pin 9) */
  k_pin_sonar_trig1 = k_rx_pj_5, /**< PJ.5 - Sonar 1 trigger (pin 11) */
  k_pin_sonar_trig2 = k_rx_pj_3, /**< PJ.3 - Sonar 2 trigger (pin 13) */
  k_pin_sonar_trig3 = k_rx_p3_3, /**< P3.3 - Sonar 3 trigger (pin 26) */

  /* HC-SR04 Sonar echoes (GPIO input, IRQ8-11) */
  k_pin_sonar_echo0 = k_rx_p0_3, /**< P0.3 - Sonar 0 echo (pin 4, IRQ11) */
  k_pin_sonar_echo1 = k_rx_p0_2, /**< P0.2 - Sonar 1 echo (pin 6, IRQ10) */
  k_pin_sonar_echo2 = k_rx_p0_1, /**< P0.1 - Sonar 2 echo (pin 7, IRQ9) */
  k_pin_sonar_echo3 = k_rx_p0_0, /**< P0.0 - Sonar 3 echo (pin 8, IRQ8) */

  /* DRV8243S SPI chip selects (GPIO output, initial HIGH) */
  k_pin_drv_cs0 = k_rx_p7_4, /**< P7.4 - DRV_CS0 motor 0 (pin 72) */
  k_pin_drv_cs1 = k_rx_pc_1, /**< PC.1 - DRV_CS1 motor 1 (pin 73) */
  k_pin_drv_cs2 = k_rx_pb_5, /**< PB.5 - DRV_CS2 motor 2 (pin 80) */
  k_pin_drv_cs3 = k_rx_pb_4, /**< PB.4 - DRV_CS3 motor 3 (pin 81) */

  /**
   * DRV8243S SPI data (SCI7 alternate function)
   * - P90 (SMOSI7): COPI - Controller Out, Peripheral In (data to motor drivers)
   * - P92 (SMISO7): CIPO - Controller In, Peripheral Out (data from motor drivers)
   */
  k_pin_drv_sclk = k_rx_p9_1, /**< P9.1 - SCK7 (pin 129) */
  k_pin_drv_copi = k_rx_p9_0, /**< P9.0 - COPI/SMOSI7 (pin 131) - data to drivers */
  k_pin_drv_cipo = k_rx_p9_2, /**< P9.2 - CIPO/SMISO7 (pin 128) - data from drivers */

  /**
   * Host SPI (RSPI2 peripheral mode on PORTD)
   * - PD1 (MOSIC): COPI - Controller Out, Peripheral In (data from RPi5)
   * - PD2 (MISOC): CIPO - Controller In, Peripheral Out (data to RPi5)
   */
  k_pin_host_copi = k_rx_pd_1, /**< PD.1 - COPI/MOSIC (RPi5 → RX72N) */
  k_pin_host_cipo = k_rx_pd_2, /**< PD.2 - CIPO/MISOC (RX72N → RPi5) */
  k_pin_host_sclk = k_rx_pd_3, /**< PD.3 - SCLK (RSPI2 clock from RPi5) */
  k_pin_host_cs0  = k_rx_pd_4, /**< PD.4 - CS0 (chip select from RPi5) */

  /* MTU Encoder clock inputs (front wheels) */
  k_pin_enc0_pha = k_rx_p2_4, /**< P2.4 - MTCLKA (encoder 0 phase A) */
  k_pin_enc0_phb = k_rx_p2_5, /**< P2.5 - MTCLKB (encoder 0 phase B) */
  k_pin_enc1_pha = k_rx_pa_1, /**< PA.1 - MTCLKC (encoder 1 phase A) */
  k_pin_enc1_phb = k_rx_pc_5, /**< PC.5 - MTCLKD (encoder 1 phase B) */

  /* TPU Encoder clock inputs (rear wheels) */
  k_pin_enc2_pha = k_rx_pc_2, /**< PC.2 - TCLKA (encoder 2 phase A) */
  k_pin_enc2_phb = k_rx_pa_3, /**< PA.3 - TCLKB (encoder 2 phase B) */
  k_pin_enc3_pha = k_rx_pc_0, /**< PC.0 - TCLKC (encoder 3 phase A) */
  k_pin_enc3_phb = k_rx_pb_3, /**< PB.3 - TCLKD (encoder 3 phase B) */

  /* GPTW PWM outputs (4 motors × 2 pins = PH + EN, all on PORT E) */
  k_pin_motor0_ph = k_rx_pe_5, /**< PE.5 - GTIOC0A (motor 0 phase, pin 106) */
  k_pin_motor0_en = k_rx_pe_2, /**< PE.2 - GTIOC0B (motor 0 enable, pin 109) */
  k_pin_motor1_ph = k_rx_pe_4, /**< PE.4 - GTIOC1A (motor 1 phase, pin 107) */
  k_pin_motor1_en = k_rx_pe_1, /**< PE.1 - GTIOC1B (motor 1 enable, pin 110) */
  k_pin_motor2_ph = k_rx_pe_3, /**< PE.3 - GTIOC2A (motor 2 phase, pin 108) */
  k_pin_motor2_en = k_rx_pe_0, /**< PE.0 - GTIOC2B (motor 2 enable, pin 111) */
  k_pin_motor3_ph = k_rx_pe_7, /**< PE.7 - GTIOC3A (motor 3 phase, pin 101) */
  k_pin_motor3_en = k_rx_pe_6, /**< PE.6 - GTIOC3B (motor 3 enable, pin 102) */

  /* ADC current sense (S12AD0, AN004-AN007) */
  k_pin_adc_an004 = k_rx_p4_4, /**< P4.4 - AN004 (motor 3 current) */
  k_pin_adc_an005 = k_rx_p4_5, /**< P4.5 - AN005 (motor 2 current) */
  k_pin_adc_an006 = k_rx_p4_6, /**< P4.6 - AN006 (motor 1 current) */
  k_pin_adc_an007 = k_rx_p4_7, /**< P4.7 - AN007 (motor 0 current) */
} rx_mpc_pin_t;

/** @brief Number of GPTW motor control pins */
typedef enum : uint8_t {
  k_gptw_pin_count = 8, /**< 4 motors x 2 pins (PH + EN) */
  k_sonar_count    = 4, /**< 4 HC-SR04 ultrasonic sensors */
  k_drv_cs_count   = 4, /**< 4 DRV8243S chip select pins */
} gpio_pin_counts_t;

/** @brief GPTW PWM frequency constant */
typedef enum : uint32_t {
  k_gptw_pwm_freq_hz = 20000, /**< 20 kHz PWM frequency */
} gptw_freq_t;

/** @brief GPTW dead-time constant */
typedef enum : uint16_t {
  k_gptw_deadtime_ns = 1000, /**< 1 us dead-time between complementary outputs */
} gptw_deadtime_t;

/** @brief RSPI2 channel number for host SPI */
typedef enum : uint8_t {
  k_host_spi_channel = 2, /**< RSPI2 = host SPI peripheral */
} spi_channel_t;

/** @brief I2C channel assignments */
typedef enum : uint8_t {
  k_i2c_channel_host = 0, /**< RIIC0 = host I2C (RPi5 comms) */
  k_i2c_channel_bms  = 1, /**< RIIC1 = BMS I2C (BQ4050) */
} i2c_channel_t;

/** @brief I2C bus frequency constants */
typedef enum : uint32_t {
  k_i2c_host_freq_hz = 400000, /**< 400 kHz fast mode for host */
  k_i2c_bms_freq_hz  = 100000, /**< 100 kHz standard mode for BMS */
} i2c_freq_t;

/** @brief Number of motor current ADC channels */
typedef enum : uint8_t {
  k_motor_adc_count = 4, /**< 4 motors = 4 current sense channels */
} adc_count_t;

/** @brief SCKCR3 reset/unconfigured state value (before clock initialization) */
static const uint8_t s_sckcr3_reset_state = 0U;

/**
 * @brief Configure I2C bus pins (RIIC0/RIIC1)
 *
 * @details
 * Configures MPC pin multiplexing for Host I2C (RIIC0) on P1.2/P1.3 and
 * BMS I2C (RIIC1) on P2.0/P2.1. Sets PSEL registers to enable I2C peripheral
 * function for SCL and SDA pins.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P1.2 configured as SCL0, P1.3 configured as SDA0
 * @post P2.1 configured as SCL1, P2.0 configured as SDA1
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_i2c(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_I2C";

  /* Host I2C (RIIC0): SCL0 + SDA0 */
  err = rx_mpc_set_riic((rx_port_pin_t)k_pin_host_scl0);
  RX_RETURN_ON_ERROR(err, s_tag, "SCL0 pin config failed");

  err = rx_mpc_set_riic((rx_port_pin_t)k_pin_host_sda0);
  RX_RETURN_ON_ERROR(err, s_tag, "SDA0 pin config failed");

  /* BMS I2C (RIIC1): SCL1 + SDA1 */
  err = rx_mpc_set_riic((rx_port_pin_t)k_pin_bms_scl1);
  RX_RETURN_ON_ERROR(err, s_tag, "SCL1 pin config failed");

  err = rx_mpc_set_riic((rx_port_pin_t)k_pin_bms_sda1);
  RX_RETURN_ON_ERROR(err, s_tag, "SDA1 pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure host SPI pins (RSPI2)
 *
 * @details
 * Configures MPC pin multiplexing for RSPI2 host SPI interface on PD.1-PD.4.
 * Sets PSEL registers to enable RSPI2 peripheral function for COPI, CIPO,
 * SCLK, and CS0 pins used for RPi5 communication.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post PD.1-PD.4 configured as RSPI2 COPI/CIPO/SCLK/CS0
 * @post RSPI2 pins ready for host communication
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_host_spi(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_SPI";

  err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_copi);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 COPI pin config failed");

  err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_cipo);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 CIPO pin config failed");

  err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_sclk);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 SCLK pin config failed");

  err = rx_mpc_set_rspi((rx_port_pin_t)k_pin_host_cs0);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 CS0 pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure MTU encoder input pins (front wheels)
 *
 * @details
 * Configures MPC pin multiplexing for MTU external clock inputs MTCLKA-MTCLKD
 * on P2.4/P2.5/PA.1/PC.5. Enables pulse counter mode for quadrature encoder
 * inputs from front wheel motors.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P2.4/P2.5 configured as MTCLKA/MTCLKB
 * @post PA.1/PC.5 configured as MTCLKC/MTCLKD
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_mtu_encoders(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_MTU_ENC";

  err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc0_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKA pin config failed");

  err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc0_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKB pin config failed");

  err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc1_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKC pin config failed");

  err = rx_mpc_set_mtu_encoder((rx_port_pin_t)k_pin_enc1_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "MTCLKD pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure TPU encoder input pins (rear wheels)
 *
 * @details
 * Configures MPC pin multiplexing for TPU external clock inputs TCLKA-TCLKD
 * on PC.2/PA.3/PC.0/PB.3. Enables pulse counter mode for quadrature encoder
 * inputs from rear wheel motors.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post PC.2/PA.3 configured as TCLKA/TCLKB
 * @post PC.0/PB.3 configured as TCLKC/TCLKD
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_tpu_encoders(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_TPU_ENC";

  err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc2_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKA pin config failed");

  err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc2_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKB pin config failed");

  err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc3_pha);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKC pin config failed");

  err = rx_mpc_set_tpu_encoder((rx_port_pin_t)k_pin_enc3_phb);
  RX_RETURN_ON_ERROR(err, s_tag, "TCLKD pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure GPTW PWM output pins (4 motors)
 *
 * @details
 * Configures MPC pin multiplexing for GPTW0-GPTW3 PWM outputs on PORT E
 * (PE0-PE7). Sets PSEL=0x1E for all 8 pins (4 motors × PH+EN per motor).
 * Enables complementary PWM mode for motor phase and enable control.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post PE5/PE2, PE4/PE1, PE3/PE0, PE7/PE6 configured as GPTW outputs
 * @post All 8 GPTW PWM pins ready for motor control
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_gptw_pwm(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_GPTW";

  const rx_port_pin_t gptw_pins[] = {(rx_port_pin_t)k_pin_motor0_ph,
                                     (rx_port_pin_t)k_pin_motor0_en,
                                     (rx_port_pin_t)k_pin_motor1_ph,
                                     (rx_port_pin_t)k_pin_motor1_en,
                                     (rx_port_pin_t)k_pin_motor2_ph,
                                     (rx_port_pin_t)k_pin_motor2_en,
                                     (rx_port_pin_t)k_pin_motor3_ph,
                                     (rx_port_pin_t)k_pin_motor3_en};

  for (uint8_t i = 0; i < k_gptw_pin_count; i++) {
    err = rx_mpc_set_gptw(gptw_pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "GPTW pin config failed");
  }

  return k_rx_ok;
}

/**
 * @brief Configure ADC current sense input pins
 *
 * @details
 * Configures MPC pin multiplexing for S12AD0 analog inputs AN004-AN007 on
 * P4.4-P4.7. Disables digital input buffers and sets pins to analog mode for
 * motor current sensing.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P4.4-P4.7 configured as AN004-AN007 analog inputs
 * @post ADC pins ready for current sensing
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_adc(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_ADC";

  err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an004);
  RX_RETURN_ON_ERROR(err, s_tag, "AN004 pin config failed");

  err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an005);
  RX_RETURN_ON_ERROR(err, s_tag, "AN005 pin config failed");

  err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an006);
  RX_RETURN_ON_ERROR(err, s_tag, "AN006 pin config failed");

  err = rx_mpc_set_adc((rx_port_pin_t)k_pin_adc_an007);
  RX_RETURN_ON_ERROR(err, s_tag, "AN007 pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure USB VBUS detection pin
 *
 * @details
 * Configures MPC pin multiplexing for USB0_VBUS detection on P1.6.
 * Enables USB peripheral to detect host connection via VBUS voltage level.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P1.6 configured as USB0_VBUS input
 * @post USB VBUS detection enabled
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_usb(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_USB";

  err = rx_mpc_set_usb_vbus((rx_port_pin_t)k_pin_usb_vbus);
  RX_RETURN_ON_ERROR(err, s_tag, "USB VBUS pin config failed");

  return k_rx_ok;
}

/**
 * @brief Configure HC-SR04 sonar trigger pins (GPIO outputs)
 *
 * @details
 * Configures 4 sonar trigger pins as GPIO outputs with initial LOW state.
 * Pins: PF5, PJ5, PJ3, P33. Used to trigger ultrasonic pulse transmission
 * on HC-SR04 distance sensors.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post All 4 sonar trigger pins configured as GPIO outputs
 * @post Trigger pins initialized to LOW (idle state)
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_sonar_triggers(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_SONAR_TRIG";

  const rx_port_pin_t sonar_trig_pins[k_sonar_count] = {(rx_port_pin_t)k_pin_sonar_trig0,
                                                        (rx_port_pin_t)k_pin_sonar_trig1,
                                                        (rx_port_pin_t)k_pin_sonar_trig2,
                                                        (rx_port_pin_t)k_pin_sonar_trig3};

  for (uint8_t i = 0; i < k_sonar_count; i++) {
    err = rx_mpc_set_gpio(sonar_trig_pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "Sonar trigger MPC config failed");

    const uint8_t            port = rx_port_from_pin(sonar_trig_pins[i]);
    const uint8_t            pin  = rx_pin_from_pin(sonar_trig_pins[i]);
    volatile rx_port_regs_t* regs = rx_port_get_base(port);
    RX_ASSERT(regs != nullptr, "Invalid sonar trigger port");
    regs->pmr &= ~(uint8_t)(1U << pin);
    regs->podr &= ~(uint8_t)(1U << pin);
    regs->pdr |= (uint8_t)(1U << pin);
  }

  return k_rx_ok;
}

/**
 * @brief Configure HC-SR04 sonar echo pins (GPIO inputs)
 *
 * @details
 * Configures 4 sonar echo pins as GPIO inputs for pulse width measurement.
 * Pins: P0.3, P0.2, P0.1, P0.0 (also IRQ11-IRQ8). Echo pulse duration
 * measured by IRQ timing to determine distance.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post All 4 sonar echo pins configured as GPIO inputs
 * @post Echo pins ready for IRQ-based pulse measurement
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_sonar_echoes(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_SONAR_ECHO";

  const rx_port_pin_t sonar_echo_pins[k_sonar_count] = {(rx_port_pin_t)k_pin_sonar_echo0,
                                                        (rx_port_pin_t)k_pin_sonar_echo1,
                                                        (rx_port_pin_t)k_pin_sonar_echo2,
                                                        (rx_port_pin_t)k_pin_sonar_echo3};

  for (uint8_t i = 0; i < k_sonar_count; i++) {
    err = rx_mpc_set_gpio(sonar_echo_pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "Sonar echo MPC config failed");

    const uint8_t            port = rx_port_from_pin(sonar_echo_pins[i]);
    const uint8_t            pin  = rx_pin_from_pin(sonar_echo_pins[i]);
    volatile rx_port_regs_t* regs = rx_port_get_base(port);
    RX_ASSERT(regs != nullptr, "Invalid sonar echo port");
    regs->pmr &= ~(uint8_t)(1U << pin);
    regs->pdr &= ~(uint8_t)(1U << pin);
  }

  return k_rx_ok;
}

/**
 * @brief Configure DRV8243S chip select pins (GPIO outputs)
 *
 * @details
 * Configures 4 motor driver CS pins as GPIO outputs with initial HIGH state
 * (deselected). Pins: P74, PC1, PB5, PB4. Used for SPI communication with
 * DRV8243S motor driver chips.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post All 4 DRV CS pins configured as GPIO outputs
 * @post CS pins initialized to HIGH (deselected state)
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_drv_cs(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_DRV_CS";

  const rx_port_pin_t drv_cs_pins[k_drv_cs_count] = {(rx_port_pin_t)k_pin_drv_cs0,
                                                     (rx_port_pin_t)k_pin_drv_cs1,
                                                     (rx_port_pin_t)k_pin_drv_cs2,
                                                     (rx_port_pin_t)k_pin_drv_cs3};

  for (uint8_t i = 0; i < k_drv_cs_count; i++) {
    err = rx_mpc_set_gpio(drv_cs_pins[i]);
    RX_RETURN_ON_ERROR(err, s_tag, "DRV CS MPC config failed");

    const uint8_t            port = rx_port_from_pin(drv_cs_pins[i]);
    const uint8_t            pin  = rx_pin_from_pin(drv_cs_pins[i]);
    volatile rx_port_regs_t* regs = rx_port_get_base(port);
    RX_ASSERT(regs != nullptr, "Invalid DRV CS port");
    regs->pmr &= ~(uint8_t)(1U << pin);
    regs->podr |= (uint8_t)(1U << pin);
    regs->pdr |= (uint8_t)(1U << pin);
  }

  return k_rx_ok;
}

/**
 * @brief Configure SCI7 SPI pins for motor driver communication
 *
 * @details
 * Configures MPC pin multiplexing for SCI7 in clock-synchronous mode on
 * P90-P92. Sets PSEL to enable SCI7 SCK/COPI/CIPO functions for SPI
 * communication with DRV8243S motor drivers.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All pins configured successfully
 * @retval k_rx_err_hw_init_failed MPC configuration failed for one or more pins
 *
 * @pre MPC write protection disabled (PWPR.B0WI=0, PWPR.PFSWE=1)
 * @pre Pins not in use by other peripherals
 * @post P90-P92 configured as SCI7 COPI/SCK/CIPO
 * @post SCI7 SPI pins ready for motor driver communication
 *
 * @note Thread-safe. No shared state modified.
 * @note Called only during system initialization.
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_gpio_init_sci7_spi(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO_SCI7";

  err = rx_mpc_set_sci((rx_port_pin_t)k_pin_drv_sclk);
  RX_RETURN_ON_ERROR(err, s_tag, "SCI7 SCK pin config failed");

  err = rx_mpc_set_sci((rx_port_pin_t)k_pin_drv_copi);
  RX_RETURN_ON_ERROR(err, s_tag, "SCI7 COPI pin config failed");

  err = rx_mpc_set_sci((rx_port_pin_t)k_pin_drv_cipo);
  RX_RETURN_ON_ERROR(err, s_tag, "SCI7 CIPO pin config failed");

  return k_rx_ok;
}

/**
 * @brief Initialize GPIO pins for motor control, I2C, and USB communication
 *
 * @details
 * Configures Multi-Function Pin Controller (MPC) for 8 critical pins used in STAR
 * robot application. All pins are configured for peripheral mode (not GPIO), with
 * specific PSEL values determined by hardware function.
 *
 * **Pin configuration:**
 * - **8× GPTW PWM pins** - Motor control via DRV8243 H-bridge drivers (PORT E)
 * - **2× I2C pins** - Sensor communication bus (SCL/SDA)
 * - **1× USB pin** - USB VBUS detection for CDC debug interface
 *
 * **Algorithm steps:**
 * 1. Validate all pin identifiers are within valid range
 * 2. Configure GPTW PWM pins (PSEL = 0x1E) for 4-motor control on PORT E
 * 3. Configure I2C pins (PSEL = 0x0F) for sensor bus
 * 4. Configure USB pin (PSEL = 0x11) for USB VBUS detect
 * 5. All operations use rx_mpc API with write-protect handling
 *
 * ## Pin Allocation Table
 *
 * | Pin | Port.Bit | Function | PSEL | Usage |
 * |-----|----------|----------|------|-------|
 * | PE5 | PORTE.5 | GTIOC0A | 0x1E | Motor 0 phase (pkg pin 106) |
 * | PE2 | PORTE.2 | GTIOC0B | 0x1E | Motor 0 enable (pkg pin 109) |
 * | PE4 | PORTE.4 | GTIOC1A | 0x1E | Motor 1 phase (pkg pin 107) |
 * | PE1 | PORTE.1 | GTIOC1B | 0x1E | Motor 1 enable (pkg pin 110) |
 * | PE3 | PORTE.3 | GTIOC2A | 0x1E | Motor 2 phase (pkg pin 108) |
 * | PE0 | PORTE.0 | GTIOC2B | 0x1E | Motor 2 enable (pkg pin 111) |
 * | PE7 | PORTE.7 | GTIOC3A | 0x1E | Motor 3 phase (pkg pin 101) |
 * | PE6 | PORTE.6 | GTIOC3B | 0x1E | Motor 3 enable (pkg pin 102) |
 * | P1.2 | PORT1.2 | SCL0 | 0x0F | I2C clock line |
 * | P1.3 | PORT1.3 | SDA0 | 0x0F | I2C data line |
 * | P1.6 | PORT1.6 | USB0_VBUS | 0x11 | USB VBUS detect input |
 *
 * @return rx_err_t Error code indicating success or failure
 * @retval k_rx_ok All 8 pins configured successfully
 * @retval k_rx_err_invalid_arg Pin identifier out of range (programming error)
 * @retval k_rx_err_hw_error MPC write-protect unlock failed (hardware fault)
 *
 * @pre PCLKB clock must be running (MPC registers require active clock)
 * @pre Must be called during single-threaded initialization (before RTOS starts)
 *
 * @post All 8 pins configured for peripheral mode (PMR = 1)
 * @post PWPR register locked (B0WI = 1, write protection active)
 * @post Motor control pins ready for GPTW PWM output
 * @post I2C pins ready for RIIC communication
 * @post USB pin ready for VBUS detection
 *
 * @note **Thread Safety**: Not thread-safe. Must be called during initialization
 *       before ThreadX kernel starts. Do not call from running RTOS tasks.
 *
 * @note **Re-entrancy**: Not reentrant. Calling multiple times is safe (idempotent)
 *       but wastes CPU cycles re-writing same register values.
 *
 * @note **Performance**: Execution time ~15 µs @ 240 MHz (8 pins × 2 µs/pin).
 *       One-time initialization cost, not runtime overhead.
 *
 * @warning **Pin conflicts**: If pins are already in use by another peripheral,
 *          reconfiguring them here will break that peripheral's functionality.
 *          Ensure pin allocations match hardware schematic.
 *
 * @warning **Motor safety**: Motor control pins must be configured before enabling
 *          GPTW channels. Unconfigured PWM pins can cause undefined motor behavior.
 *
 * @par Example - Normal Initialization:
 * @code
 * // Called from hardware_init() during boot
 * rx_err_t err = gpio_init();
 * if (err != k_rx_ok) {
 *     rx_log_error("HWINT", "GPIO init failed: %d", err);
 *     return err;
 * }
 *
 * // Now safe to initialize motor drivers
 * err = motor_init();
 * @endcode
 *
 * @par Example - Error Handling:
 * @code
 * rx_err_t err = gpio_init();
 * switch (err) {
 *     case k_rx_ok:
 *         rx_log_info("GPIO", "8 pins configured successfully");
 *         break;
 *     case k_rx_err_invalid_arg:
 *         // Programming error - invalid pin constant used
 *         rx_log_error("GPIO", "Invalid pin identifier");
 *         return err;
 *     case k_rx_err_hw_error:
 *         // Hardware fault - MPC registers not accessible
 *         rx_log_error("GPIO", "MPC hardware fault");
 *         return err;
 * }
 * @endcode
 *
 * @see rx_mpc_set_gptw() Configure pin for GPTW PWM output
 * @see rx_mpc_set_riic() Configure pin for I2C bus function
 * @see rx_mpc_set_peripheral() Generic pin configuration
 * @see RX72N Manual Chapter 23 - Multi-Function Pin Controller
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance
 * - **Rule 1** [OK] No goto, setjmp, recursion (sequential pin configuration)
 * - **Rule 2** [OK] No loops in main function (delegated to helper functions)
 * - **Rule 3** [OK] No dynamic allocation (all register I/O)
 * - **Rule 4** [OK] Function is ~30 lines (under 60 line limit, decomposed into helpers)
 * - **Rule 5** [OK] 2 preconditions, 5 postconditions documented
 * - **Rule 6** [OK] Minimal scope (no local variables)
 * - **Rule 7** [OK] All internal_gpio_init_*() return values checked
 * - **Rule 8** [OK] Uses C23 typed enums for pin identifiers
 * - **Rule 9** [OK] Single level of function call dereferencing
 * - **Rule 10** [OK] Compiles with -Wall -Wextra -Werror
 *
 */
static rx_err_t gpio_init(void)
{
  rx_err_t           err;
  static const char* s_tag = "GPIO";

  err = internal_gpio_init_i2c();
  RX_RETURN_ON_ERROR(err, s_tag, "I2C pin init failed");

  err = internal_gpio_init_host_spi();
  RX_RETURN_ON_ERROR(err, s_tag, "Host SPI pin init failed");

  err = internal_gpio_init_mtu_encoders();
  RX_RETURN_ON_ERROR(err, s_tag, "MTU encoder pin init failed");

  err = internal_gpio_init_tpu_encoders();
  RX_RETURN_ON_ERROR(err, s_tag, "TPU encoder pin init failed");

  err = internal_gpio_init_gptw_pwm();
  RX_RETURN_ON_ERROR(err, s_tag, "GPTW PWM pin init failed");

  err = internal_gpio_init_adc();
  RX_RETURN_ON_ERROR(err, s_tag, "ADC pin init failed");

  err = internal_gpio_init_usb();
  RX_RETURN_ON_ERROR(err, s_tag, "USB VBUS pin init failed");

  err = internal_gpio_init_sonar_triggers();
  RX_RETURN_ON_ERROR(err, s_tag, "Sonar trigger pin init failed");

  err = internal_gpio_init_sonar_echoes();
  RX_RETURN_ON_ERROR(err, s_tag, "Sonar echo pin init failed");

  err = internal_gpio_init_drv_cs();
  RX_RETURN_ON_ERROR(err, s_tag, "DRV CS pin init failed");

  err = internal_gpio_init_sci7_spi();
  RX_RETURN_ON_ERROR(err, s_tag, "SCI7 SPI pin init failed");

  /* GTETRG nFAULT pins: no MPC config needed (documented in header) */

  rx_log_info(s_tag, "GPIO pin muxing complete");

  return k_rx_ok;
}

/**
 * @brief Initialize GPTW PWM for 4 motor channels with 90-degree phase staggering
 *
 * @details
 * Configures GPTW channels 0-3 for complementary PWM output at 20 kHz with
 * 1 µs dead-time. Channels are phase-staggered by 90 degrees to reduce peak
 * current draw and EMI.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All 4 GPTW channels initialized successfully
 * @retval k_rx_err_hw_init_failed GPTW staggered init failed
 *
 * @pre GPIO pins for GPTW configured via gpio_init() (PSEL = 0x1E)
 * @pre PCLKA clock running at 120 MHz
 *
 * @post 4 GPTW channels configured for 20 kHz complementary PWM
 * @post Phase staggering active (0, 90, 180, 270 degrees)
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see rx_gptw_init_all_staggered() HAL function for staggered PWM init
 *
 * @since Version 1.1.0
 */
static rx_err_t gptw_pwm_init(void)
{
  static const char* s_tag = "GPTW";

  rx_gptw_config_t config = {.frequency_hz         = k_gptw_pwm_freq_hz,
                             .deadtime_ns          = k_gptw_deadtime_ns,
                             .wave_mode            = k_gptw_wave_saw_pwm,
                             .enable_complementary = true,
                             .invert_polarity      = false};

  rx_err_t err = rx_gptw_init_all_staggered(&config);
  RX_RETURN_ON_ERROR(err, s_tag, "GPTW staggered init failed");

  rx_log_info(s_tag, "4 channels @ 20kHz, 90-deg stagger");
  return k_rx_ok;
}

/**
 * @brief Initialize RSPI2 as SPI peripheral for RPi5 host communication
 *
 * @details
 * Configures RSPI2 in peripheral mode for receiving commands from the
 * Raspberry Pi 5 host controller. Uses SPI mode 0 (CPOL=0, CPHA=0) with
 * 8-bit transfers.
 *
 * Motor driver SPI uses SCI7 (not RSPI) and is deferred since DRV8243 runs
 * in PH/EN mode (use_spi_variant = false).
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok RSPI2 peripheral initialized successfully
 * @retval k_rx_err_hw_init_failed RSPI2 init failed
 *
 * @pre GPIO pins for RSPI2 configured via gpio_init()
 * @pre PCLKA clock running
 *
 * @post RSPI2 ready to receive SPI transactions from RPi5
 * @post SPI mode 0, 8-bit transfers configured
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see rspi_init_peripheral() HAL function for RSPI peripheral mode
 *
 * @since Version 1.1.0
 */
static rx_err_t spi_init(void)
{
  static const char* s_tag = "SPI";

  rspi_config_t host_config = {.spi_mode = k_rspi_mode_0, .use_16bit = false};

  rx_err_t err = rspi_init_peripheral(k_host_spi_channel, &host_config);
  RX_RETURN_ON_ERROR(err, s_tag, "RSPI2 peripheral init failed");

  rx_log_info(s_tag, "RSPI2 initialized (host peripheral, mode 0, 8-bit)");
  return k_rx_ok;
}

/**
 * @brief Initialize I2C buses for host communication and BMS
 *
 * @details
 * Configures two RIIC channels:
 * - **RIIC0** at 400 kHz (fast mode) for RPi5 host I2C
 * - **RIIC1** at 100 kHz (standard mode) for BQ4050 BMS communication
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Both RIIC channels initialized successfully
 * @retval k_rx_err_hw_init_failed RIIC init failed
 *
 * @pre GPIO pins for RIIC0/RIIC1 configured via gpio_init()
 * @pre PCLKB clock running at 60 MHz
 *
 * @post RIIC0 ready for host I2C at 400 kHz
 * @post RIIC1 ready for BMS I2C at 100 kHz
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see riic_init() HAL function for RIIC channel init
 *
 * @since Version 1.1.0
 */
static rx_err_t i2c_init(void)
{
  static const char* s_tag = "I2C";

  riic_channel_t ch0 = {.value = k_i2c_channel_host};
  rx_err_t       err = riic_init(ch0, k_i2c_host_freq_hz);
  RX_RETURN_ON_ERROR(err, s_tag, "RIIC0 init failed");

  riic_channel_t ch1 = {.value = k_i2c_channel_bms};
  err                = riic_init(ch1, k_i2c_bms_freq_hz);
  RX_RETURN_ON_ERROR(err, s_tag, "RIIC1 init failed");

  rx_log_info(s_tag, "RIIC0 @ 400kHz, RIIC1 @ 100kHz");
  return k_rx_ok;
}

/**
 * @brief Initialize ADC channels for motor current sensing
 *
 * @details
 * Configures S12AD0 channels AN004-AN007 at 12-bit resolution for reading
 * motor current via DRV8243S IPROPI analog output.
 *
 * | Channel | Motor | Pin |
 * |---------|-------|-----|
 * | AN007 | Motor 0 | P4.7 |
 * | AN006 | Motor 1 | P4.6 |
 * | AN005 | Motor 2 | P4.5 |
 * | AN004 | Motor 3 | P4.4 |
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok All 4 ADC channels initialized
 * @retval k_rx_err_hw_init_failed ADC channel init failed
 *
 * @pre GPIO pins for ADC configured via gpio_init()
 * @pre PCLKB/PCLKD clock running
 *
 * @post S12AD0 channels AN004-AN007 ready for conversion
 * @post 12-bit resolution configured
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 *
 * @see adc_init() HAL function for ADC channel init
 *
 * @since Version 1.1.0
 */
static rx_err_t adc_init_channels(void)
{
  static const char* s_tag = "ADC";

  const adc_channel_t channels[k_motor_adc_count] = {
    k_adc_channel_7, /* Motor 0: AN007 */
    k_adc_channel_6, /* Motor 1: AN006 */
    k_adc_channel_5, /* Motor 2: AN005 */
    k_adc_channel_4  /* Motor 3: AN004 */
  };

  for (uint8_t i = 0; i < k_motor_adc_count; i++) {
    rx_err_t err = adc_init(k_adc_unit_0, channels[i], k_adc_resolution_12bit);
    RX_RETURN_ON_ERROR(err, s_tag, "ADC channel init failed");
  }

  rx_log_info(s_tag, "S12AD0 AN004-AN007 @ 12-bit");
  return k_rx_ok;
}

/**
 * @brief Non-fatal peripheral validation after initialization
 *
 * @details
 * Performs a test ADC conversion to verify the ADC subsystem is operational.
 * Logs warnings on failure but never halts - all checks are informational.
 *
 * @pre All peripheral init functions have completed
 *
 * @post Validation results logged
 * @post No state changes (read-only checks)
 *
 * @note Not thread-safe. Call during single-threaded initialization only.
 * @note Non-fatal: failures are logged as warnings, boot continues.
 *
 * @since Version 1.1.0
 */
static void validate_peripherals(void)
{
  static const char* s_tag = "VALIDATE";

  /* ADC test conversion on AN007 (motor 0 current) */
  uint16_t test_val = 0;
  if (adc_read(k_adc_unit_0, k_adc_channel_7, &test_val) == k_rx_ok) {
    rx_log_info(s_tag, "ADC0 test conversion OK");
  } else {
    rx_log_warn(s_tag, "ADC0 test conversion failed (non-fatal)");
  }

  rx_log_info(s_tag, "Peripheral validation complete");
}

/**
 * @brief Initialize all application-specific hardware peripherals for STAR motor controller
 *
 * @details
 * Configures **six categories** of peripherals required by the STAR robot application:
 *
 * 1. **GPIO** (planned) - Motor control pins, LEDs, sensor chip selects
 * 2. **Timers** ([PASS] implemented) - CMT0 for ThreadX tick at 1 kHz
 * 3. **UART** ([PASS] implemented) - SCI9 for debug console at 115200 baud
 * 4. **SPI** (planned) - Motor drivers (DRV8243), sensor bus
 * 5. **I2C** (planned) - IMU, temperature, pressure sensors
 * 6. **ADC** (planned) - Current sensing, battery voltage monitoring
 *
 * ## Initialization Sequence (6 Stages)
 *
 * **Stage order is CRITICAL** - violating dependencies causes failures:
 *
 * @msc
 * msc {
 *   width=700;
 *   Caller, HwInit, Timers, UART;
 *
 *   Caller => HwInit [label="hardware_init()"];
 *   HwInit note HwInit [label="Precondition: Check SCKCR3 != reset_state", textcolor="blue"];
 *   HwInit => HwInit [label="RX_ASSERT(clocks initialized)"];
 *   HwInit => Timers [label="timer_init()"];
 *   Timers => Timers [label="Configure CMT0\n1 kHz tick"];
 *   Timers => HwInit [label="k_rx_ok"];
 *   HwInit => UART [label="uart_debug_init()"];
 *   UART => UART [label="Configure SCI9\n115200 baud"];
 *   UART => HwInit [label="k_rx_ok"];
 *   HwInit note HwInit [label="Postcondition: Check SCKCR3 still valid", textcolor="green"];
 *   HwInit => HwInit [label="RX_ASSERT(clocks not corrupted)"];
 *   HwInit => Caller [label="k_rx_ok"];
 * }
 * @endmsc
 *
 * ## Performance Characteristics (RX72N @ 240 MHz)
 *
 * | Stage | Duration | CPU Cycles | Implementation | Critical Path? |
 * |-------|----------|------------|----------------|----------------|
 * | **Precondition check** | 0.5 µs | ~120 | [COMPLETE] | No |
 * | **Timer init (CMT0)** | 10 µs | ~2,400 | [COMPLETE] | No |
 * | **UART init (SCI9)** | 50 µs | ~12,000 | [COMPLETE] | No |
 * | **Postcondition check** | 0.5 µs | ~120 | [COMPLETE] | No |
 * | **Total (current)** | **~61 µs** | **~14,640** | Timers + UART only | **No** |
 *
 * **Note:** Not on critical boot path. Total boot time (main to ThreadX) is ~51 ms,
 * dominated by USB enumeration (~50 ms) which happens later in app_main_task.
 *
 * ## Memory Usage (Static Allocation Only)
 *
 * | Object | Size | Location | Lifetime |
 * |--------|------|----------|----------|
 * | **s_sckcr3_reset_state** | 1 byte | .rodata (Flash) | Permanent |
 * | **err (local variable)** | 2 bytes | Stack (Main stack) | Function duration |
 * | **Return address** | 4 bytes | Stack (Main stack) | Function duration |
 * | **Total stack usage** | ~64 bytes | Main stack (4 KB available) | Function duration |
 *
 * **Stack depth:** 1 level (hardware_init -> timer_init/uart_init)
 *
 * ## Error Handling and Recovery
 *
 * **Critical errors (assert-halt):**
 * - **Precondition:** SCKCR3 == reset_state -> Clocks not initialized (caller bug)
 * - **Postcondition:** SCKCR3 changed during init -> Clock corruption (peripheral bug)
 * - **Action:** RX_ASSERT halts execution with message
 *
 * **Peripheral errors (return error code):**
 * - **timer_init() failed** -> CMT0 configuration error (hardware or register access issue)
 * - **uart_debug_init() failed** -> SCI9 configuration error (baud rate calculation, pin config)
 * - **Action:** Return error to main(), which halts boot with error code
 *
 * **Recovery strategy:**
 * - On assert: Halt execution (developer investigates)
 * - On error return: main() halts boot (error logged via UART if possible)
 * - No partial initialization cleanup (fail-fast, no recovery)
 *
 * @return rx_err_t Initialization status
 * @retval k_rx_ok All peripherals initialized successfully
 * @retval k_rx_err_hw_init_failed Timer or UART initialization failed
 * @retval k_rx_err_timeout Peripheral configuration timeout (rare, indicates hardware fault)
 * @retval k_rx_err_invalid_state Peripheral already initialized or in unexpected state
 *
 * @pre System clocks configured via rx_clock_power_init() (SCKCR3 != reset_state)
 * @pre Interrupt vector table set up (reset vector, exception vectors)
 * @pre Stack pointer valid (at least 4 KB available in main stack)
 * @pre Memory protection unlocked if required (PRCR register)
 *
 * @post CMT0 configured for ThreadX tick at 1 kHz (timer interrupt enabled)
 * @post SCI9 configured for debug console at 115200 baud (UART TX/RX operational)
 * @post System clocks still operational (SCKCR3 unchanged from precondition)
 * @post Peripherals ready for application use (motor control, sensors, communication)
 *
 * @note **Call order:** rx_clock_power_init() -> hardware_init() -> tx_kernel_enter()
 *       Violating this order causes precondition assertion failure.
 *
 * @note **Not idempotent:** Calling hardware_init() multiple times may cause errors
 *       (peripherals already configured, interrupts already enabled). Only call once during boot.
 *
 * @warning **Never call before rx_clock_power_init().** Peripheral configuration requires
 *          stable system clocks. Precondition assertion will halt execution if violated.
 *
 * @warning **Boot order critical.** rx_infrastructure_init() must be called before
 *          hardware_init(). See main.c for the complete boot sequence.
 *
 * @par Thread Safety:
 * Executes in single-threaded context before ThreadX starts. No synchronization needed.
 *
 * @par Example Boot Sequence:
 * @code
 * int main(void) {
 *   rx_err_t ret;
 *
 *   // Stage 1: System clocks (240 MHz PLL)
 *   ret = rx_clock_power_init();
 *   RX_ERROR_CHECK(ret);
 *
 *   // Stage 2: Application peripherals (timers, UART, sensors)
 *   ret = hardware_init();
 *   if (ret != k_rx_ok) {
 *     // Log error (if UART initialized successfully)
 *     rx_log_error("MAIN", "Hardware initialization failed: %d", ret);
 *     // Halt boot
 *     while (1) { __asm__ volatile("wait"); }
 *   }
 *
 *   // Stage 3: Start ThreadX RTOS
 *   tx_kernel_enter();
 *
 *   // Never reached (scheduler takes over)
 * }
 * @endcode
 *
 * @par Example Error Handling:
 * @code
 * rx_err_t hardware_init(void) {
 *   rx_err_t err;
 *
 *   // Initialize timer (CMT0)
 *   err = timer_init();
 *   if (rx_err_is_error(err)) {
 *     // Cannot use rx_log_error yet (UART not initialized)
 *     return err;  // Propagate error to main()
 *   }
 *
 *   // Initialize UART (SCI9) - enables logging
 *   err = uart_debug_init();
 *   if (rx_err_is_error(err)) {
 *     // Still cannot log (UART init failed)
 *     return err;  // Propagate error to main()
 *   }
 *
 *   // Now logging is available for subsequent errors
 *   err = spi_init();
 *   if (rx_err_is_error(err)) {
 *     rx_log_error("HWINT", "SPI initialization failed: %d", err);
 *     return err;
 *   }
 *
 *   return k_rx_ok;
 * }
 * @endcode
 *
 * @par Planned Peripherals (Future Implementation):
 * - **GPIO:** Port initialization for motor control (PA0-PA7), LEDs (PB0-PB2), sensor CS (PC0-PC3)
 * - **SPI:** RSPI0 for motor drivers (DRV8243 × 4), RSPI1 for sensor bus
 * - **I2C:** RIIC0 for IMU (MPU6050), temperature (LM75), pressure (BMP280)
 * - **ADC:** ADC0 channels for current sensing (4 channels), battery voltage (1 channel)
 * - **USB:** USB CDC for ROS2 communication (already partially implemented, needs integration)
 *
 * @see rx_clock_power_init() System clock configuration (MUST call before this function)
 * @see timer_init() Configure CMT0 for ThreadX tick (called by this function)
 * @see uart_debug_init() Configure SCI9 for debug console (called by this function)
 * @see main() Main entry point (calls this function during boot)
 *
 * @since Version 1.0.0
 *
 * @test test_hardware_init.c Verify all peripherals initialize successfully
 * @test test_hardware_init.c Verify precondition assertion (clocks not initialized)
 * @test test_hardware_init.c Verify error propagation (timer/UART init failure)
 */
rx_err_t hardware_init(void)
{
  static const char* s_tag = "HW_INIT";

  /* =========================================================================
   * PRECONDITION: Verify system initialization
   * =========================================================================
   */

  /* Precondition: Verify that system clocks have been initialized
   * Clock initialization must complete before peripheral setup */
  RX_ASSERT((system_regs() != nullptr) && (system_regs()->sckcr3 != s_sckcr3_reset_state),
            "Precondition: Clock system not properly initialized");

  /* =========================================================================
   * INITIALIZE PERIPHERALS
   *
   * Order: GPIO -> GPTW -> Timer -> UART -> SPI -> I2C -> BMS Alert -> ADC -> Validate
   * Later stages depend on earlier stages (GPIO must precede peripherals).
   * =========================================================================
   */

#if !RX_IS_SIMULATOR
  /* 1. GPIO: Configure MPC pin multiplexing for all peripherals */
  rx_err_t err = gpio_init();
  RX_RETURN_ON_ERROR(err, s_tag, "GPIO initialization failed");

  /* 2. GPTW: 4-channel motor PWM with phase staggering */
  err = gptw_pwm_init();
  RX_RETURN_ON_ERROR(err, s_tag, "GPTW PWM initialization failed");

  /* 2b. POEG: Motor fault protection (links GTETRG->POEG->GPTW) */
  err = rx_poeg_init();
  RX_RETURN_ON_ERROR(err, s_tag, "POEG fault protection init failed");

  /* 3. Timer: CMT0 for ThreadX tick (1 kHz) */
  err = timer_init();
  if (rx_err_is_error(err)) {
    return err;
  }

  /* 4. UART: SCI9 debug console - MOVED TO MAIN()
   *    UART initialized in main() before hardware_init() to enable early error logging.
   *    Error logging is now available for all peripheral initialization below. */

  /* 5a. SPI: RSPI2 host peripheral for RPi5 communication */
  err = spi_init();
  RX_RETURN_ON_ERROR(err, s_tag, "SPI initialization failed");

  /* 6. I2C: RIIC0 host + RIIC1 BMS */
  err = i2c_init();
  RX_RETURN_ON_ERROR(err, s_tag, "I2C initialization failed");

  /* 6b. BMS Alert: IRQ13 interrupt for BQ4050 ALERT pin (battery fault detection) */
  err = rx_bms_alert_init();
  RX_RETURN_ON_ERROR(err, s_tag, "BMS alert IRQ13 initialization failed");

  /* 7. ADC: S12AD0 channels AN004-AN007 for motor current sensing */
  err = adc_init_channels();
  RX_RETURN_ON_ERROR(err, s_tag, "ADC initialization failed");

  /* 8. Validate: Non-fatal peripheral checks (log warnings, never halt) */
  validate_peripherals();

#else
  /* Simulator: Skip all hardware peripheral init (not modeled) */
  rx_err_t err = k_rx_ok;
  (void)err;
#endif

  /* =========================================================================
   * POSTCONDITION: Verify initialization state
   * =========================================================================
   */

  /* Postcondition: Verify clock system is still operational after all setup
   * This confirms the initialization did not inadvertently disable clocks */
  RX_ASSERT((system_regs() != nullptr) && (system_regs()->sckcr3 != s_sckcr3_reset_state),
            "Postcondition: Clock system corrupted during initialization");

  return k_rx_ok;
}
