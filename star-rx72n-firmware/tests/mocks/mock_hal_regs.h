/* tests/mocks/mock_hal_regs.h */

/**
 * @file mock_hal_regs.h
 * @brief Mock HAL Register Structures for Unit Testing
 *
 * Provides mock register structures and accessor functions for testing
 * GPIO, ADC, RIIC, and System peripherals without actual hardware.
 *
 * Features:
 * - Mock PORT registers for GPIO testing
 * - Mock ADC registers for ADC testing
 * - Mock RIIC registers for I2C testing
 * - Mock System registers for clock/module control
 * - Infrastructure interface mocking
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_HAL_REGS_H
#define MOCK_HAL_REGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Mock HAL register constants */
typedef enum {
  k_mock_hal_max_ports    = 20, /**< Maximum number of ports */
  k_mock_hal_max_adc_ch   = 8,  /**< Maximum ADC channels per unit */
  k_mock_hal_max_adc_unit = 2,  /**< Maximum ADC units */
  k_mock_hal_max_riic_ch  = 3,  /**< Maximum RIIC channels */
} mock_hal_constants_t;

/* =============================================================================
 * Mock PORT Register Structure (simplified for testing)
 * =============================================================================
 */

/**
 * @brief Mock PORT register structure
 */
typedef struct {
  uint8_t pdr;  /**< Port Direction Register */
  uint8_t podr; /**< Port Output Data Register */
  uint8_t pidr; /**< Port Input Data Register */
  uint8_t pmr;  /**< Port Mode Register */
  uint8_t pcr;  /**< Pull-up Control Register */
  uint8_t dscr; /**< Drive Capacity Control Register */
} mock_port_regs_t;

/* =============================================================================
 * Mock ADC Register Structure
 * =============================================================================
 */

/**
 * @brief Mock S12AD register structure
 */
typedef struct {
  uint16_t adcsr;  /**< A/D Control/Status Register */
  uint16_t adcer;  /**< A/D Conversion Extended Register */
  uint16_t adansa0; /**< A/D Channel Select Register A0 */
  uint16_t adansa1; /**< A/D Channel Select Register A1 */
  uint16_t addr0;  /**< A/D Data Register 0 */
  uint16_t addr1;  /**< A/D Data Register 1 */
  uint16_t addr2;  /**< A/D Data Register 2 */
  uint16_t addr3;  /**< A/D Data Register 3 */
  uint16_t addr4;  /**< A/D Data Register 4 */
  uint16_t addr5;  /**< A/D Data Register 5 */
  uint16_t addr6;  /**< A/D Data Register 6 */
  uint16_t addr7;  /**< A/D Data Register 7 */
} mock_adc_regs_t;

/* =============================================================================
 * Mock RIIC Register Structure
 * =============================================================================
 */

/**
 * @brief Mock RIIC register structure
 */
typedef struct {
  uint8_t iccr1; /**< I2C Bus Control Register 1 */
  uint8_t iccr2; /**< I2C Bus Control Register 2 */
  uint8_t icmr1; /**< I2C Bus Mode Register 1 */
  uint8_t icmr2; /**< I2C Bus Mode Register 2 */
  uint8_t icmr3; /**< I2C Bus Mode Register 3 */
  uint8_t icfer; /**< I2C Bus Function Enable Register */
  uint8_t icser; /**< I2C Bus Status Enable Register */
  uint8_t icier; /**< I2C Bus Interrupt Enable Register */
  uint8_t icsr1; /**< I2C Bus Status Register 1 */
  uint8_t icsr2; /**< I2C Bus Status Register 2 */
  uint8_t icbrl; /**< I2C Bus Bit Rate Register L */
  uint8_t icbrh; /**< I2C Bus Bit Rate Register H */
  uint8_t icdrt; /**< I2C Bus Transmit Data Register */
  uint8_t icdrr; /**< I2C Bus Receive Data Register */
} mock_riic_regs_t;

/* =============================================================================
 * Mock System Register Structure
 * =============================================================================
 */

/**
 * @brief Mock System register structure
 */
typedef struct {
  uint16_t prcr;    /**< Protection Register */
  uint32_t mstpcra; /**< Module Stop Control Register A */
  uint32_t mstpcrb; /**< Module Stop Control Register B */
  uint32_t mstpcrc; /**< Module Stop Control Register C */
} mock_system_regs_t;

/* =============================================================================
 * Mock HAL State Structure
 * =============================================================================
 */

/**
 * @brief Global mock HAL state
 */
typedef struct {
  mock_port_regs_t   ports[k_mock_hal_max_ports];    /**< Port registers */
  mock_adc_regs_t    adc[k_mock_hal_max_adc_unit];   /**< ADC registers */
  mock_riic_regs_t   riic[k_mock_hal_max_riic_ch];   /**< RIIC registers */
  mock_system_regs_t system;                         /**< System registers */
  bool               adc_initialized[k_mock_hal_max_adc_unit]; /**< ADC init state */
  bool               riic_initialized[k_mock_hal_max_riic_ch]; /**< RIIC init state */
  uint32_t           riic_simulated_timeout_count;   /**< Simulated timeout loops */
  bool               riic_simulate_nack;             /**< Simulate NACK response */
  bool               riic_simulate_busy;             /**< Simulate bus busy */
  bool               adc_simulate_timeout;           /**< Simulate ADC timeout */
  bool               pin_validator_active;           /**< Whether pin validator is active */
} mock_hal_state_t;

/* =============================================================================
 * Global State
 * =============================================================================
 */

/** @brief Global mock HAL state instance */
extern mock_hal_state_t g_mock_hal;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

/**
 * @brief Initialize all mock HAL state
 */
void mock_hal_init(void);

/**
 * @brief Reset mock HAL state (alias for init)
 */
void mock_hal_reset(void);

/* =============================================================================
 * Port Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock port registers for port number
 *
 * @param[in] port Port number
 *
 * @return Pointer to mock port registers, or NULL if invalid
 */
mock_port_regs_t* mock_hal_get_port(uint8_t port);

/**
 * @brief Set mock port input data (for gpio_read testing)
 *
 * @param[in] port Port number
 * @param[in] pidr Value to set in PIDR register
 */
void mock_hal_set_port_input(uint8_t port, uint8_t pidr);

/**
 * @brief Get mock port output data
 *
 * @param[in] port Port number
 *
 * @return PODR register value
 */
uint8_t mock_hal_get_port_output(uint8_t port);

/**
 * @brief Get mock port direction
 *
 * @param[in] port Port number
 *
 * @return PDR register value
 */
uint8_t mock_hal_get_port_direction(uint8_t port);

/* =============================================================================
 * ADC Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock ADC registers for unit
 *
 * @param[in] unit ADC unit (0 or 1)
 *
 * @return Pointer to mock ADC registers, or NULL if invalid
 */
mock_adc_regs_t* mock_hal_get_adc(uint8_t unit);

/**
 * @brief Set mock ADC conversion result
 *
 * @param[in] unit ADC unit
 * @param[in] channel ADC channel (0-7)
 * @param[in] value ADC conversion value
 */
void mock_hal_set_adc_value(uint8_t unit, uint8_t channel, uint16_t value);

/**
 * @brief Enable/disable ADC timeout simulation
 *
 * @param[in] simulate True to simulate timeout
 */
void mock_hal_set_adc_timeout(bool simulate);

/* =============================================================================
 * RIIC Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock RIIC registers for channel
 *
 * @param[in] channel RIIC channel (0-2)
 *
 * @return Pointer to mock RIIC registers, or NULL if invalid
 */
mock_riic_regs_t* mock_hal_get_riic(uint8_t channel);

/**
 * @brief Simulate RIIC NACK response
 *
 * @param[in] simulate True to simulate NACK
 */
void mock_hal_set_riic_nack(bool simulate);

/**
 * @brief Simulate RIIC bus busy
 *
 * @param[in] simulate True to simulate bus busy
 */
void mock_hal_set_riic_busy(bool simulate);

/**
 * @brief Set RIIC simulated timeout counter
 *
 * @param[in] count Number of iterations before simulated timeout completes
 */
void mock_hal_set_riic_timeout_count(uint32_t count);

/* =============================================================================
 * System Mock Helpers
 * =============================================================================
 */

/**
 * @brief Get mock system registers
 *
 * @return Pointer to mock system registers
 */
mock_system_regs_t* mock_hal_get_system(void);

/**
 * @brief Enable/disable pin validator in mock
 *
 * @param[in] active True to enable pin validator
 */
void mock_hal_set_pin_validator_active(bool active);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_HAL_REGS_H */
