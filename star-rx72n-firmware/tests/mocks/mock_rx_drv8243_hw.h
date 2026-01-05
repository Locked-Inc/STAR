/* tests/mocks/mock_rx_drv8243_hw.h */

/**
 * @file mock_rx_drv8243_hw.h
 * @brief Mock Hardware for DRV8243 Unit Testing
 * @details
 * Provides mock implementations for GPIO (fault pin) and ADC (current sensing)
 * hardware required by DRV8243 driver testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_DRV8243_HW_H
#define MOCK_RX_DRV8243_HW_H

#include <stdbool.h>
#include <stdint.h>

#include "rx_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Port Register Structure
 * =============================================================================
 */

/**
 * @brief Mock port register structure for GPIO simulation
 */
typedef struct {
  volatile uint8_t pdr;  /**< Port Direction Register */
  volatile uint8_t podr; /**< Port Output Data Register */
  volatile uint8_t pidr; /**< Port Input Data Register */
  volatile uint8_t pcr;  /**< Pull-up Control Register */
} mock_rx_port_regs_t;

/* =============================================================================
 * Mock Test Helpers
 * =============================================================================
 */

/**
 * @brief Reset all mock hardware state
 *
 * Call before each test to ensure clean state.
 */
void mock_drv8243_hw_reset(void);

/**
 * @brief Set fault pin state
 *
 * @param[in] fault_active true = fault (pin LOW), false = no fault (pin HIGH)
 */
void mock_drv8243_set_fault(bool fault_active);

/**
 * @brief Get whether fault is set
 *
 * @return true if fault condition is simulated
 */
bool mock_drv8243_get_fault(void);

/**
 * @brief Set ADC voltage reading in millivolts
 *
 * @param[in] voltage_mv Voltage in millivolts
 */
void mock_drv8243_set_adc_voltage(uint32_t voltage_mv);

/**
 * @brief Get current ADC voltage setting
 *
 * @return Voltage in millivolts
 */
uint32_t mock_drv8243_get_adc_voltage(void);

/**
 * @brief Set error to return from ADC read
 *
 * @param[in] err Error code to return
 */
void mock_drv8243_set_adc_error(rx_err_t err);

/**
 * @brief Get mock port register base for testing
 *
 * @param[in] port Port number
 *
 * @return Pointer to mock port registers, or NULL if invalid
 */
mock_rx_port_regs_t* mock_drv8243_get_port(uint8_t port);

/**
 * @brief Check if fault pin was configured as input
 *
 * @return true if configured correctly
 */
bool mock_drv8243_fault_pin_configured(void);

/**
 * @brief Check if pull-up was enabled on fault pin
 *
 * @return true if pull-up enabled
 */
bool mock_drv8243_pullup_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_RX_DRV8243_HW_H */
