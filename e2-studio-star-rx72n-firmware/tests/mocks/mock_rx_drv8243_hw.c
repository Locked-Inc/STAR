/* tests/mocks/mock_rx_drv8243_hw.c */

/**
 * @file mock_rx_drv8243_hw.c
 * @brief Mock Hardware Implementation for DRV8243 Unit Testing
 * @details
 * Provides mock implementations for GPIO (fault pin) and ADC (current sensing)
 * hardware required by DRV8243 driver testing.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_rx_drv8243_hw.h"

#include <string.h>

/* =============================================================================
 * Constants
 * =============================================================================
 */

/** @brief Number of mock ports to simulate */
typedef enum : uint8_t {
  k_mock_max_ports = 32, /**< Support ports 0-31 */
} mock_port_limits_t;

/** @brief Pin state constants */
typedef enum : uint8_t {
  k_pin_high = 1, /**< Pin is HIGH */
  k_pin_low  = 0, /**< Pin is LOW */
} mock_pin_state_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

static mock_rx_port_regs_t s_ports[k_mock_max_ports];
static bool                s_fault_active         = false;
static uint32_t            s_adc_voltage_mv       = 0;
static rx_err_t            s_adc_error            = k_rx_ok;
static uint8_t             s_fault_port           = 0;
static uint8_t             s_fault_pin            = 0;
static bool                s_fault_pin_configured = false;
static bool                s_pullup_enabled       = false;

/* =============================================================================
 * Mock Test Helpers
 * =============================================================================
 */

void mock_drv8243_hw_reset(void)
{
  memset(s_ports, 0, sizeof(s_ports));

  /* Default: all pins HIGH (no fault) */
  for (uint8_t i = 0; i < k_mock_max_ports; i++) {
    s_ports[i].pidr = 0xFF; /* All pins high by default */
  }

  s_fault_active         = false;
  s_adc_voltage_mv       = 0;
  s_adc_error            = k_rx_ok;
  s_fault_port           = 0;
  s_fault_pin            = 0;
  s_fault_pin_configured = false;
  s_pullup_enabled       = false;
}

void mock_drv8243_set_fault(bool fault_active)
{
  s_fault_active = fault_active;

  /* Update the PIDR register for the fault pin */
  /* nFAULT is active LOW: fault = LOW, no fault = HIGH */
  if (fault_active) {
    s_ports[s_fault_port].pidr &= ~(1 << s_fault_pin);
  } else {
    s_ports[s_fault_port].pidr |= (1 << s_fault_pin);
  }
}

bool mock_drv8243_get_fault(void)
{
  return s_fault_active;
}

void mock_drv8243_set_adc_voltage(uint32_t voltage_mv)
{
  s_adc_voltage_mv = voltage_mv;
}

uint32_t mock_drv8243_get_adc_voltage(void)
{
  return s_adc_voltage_mv;
}

void mock_drv8243_set_adc_error(rx_err_t err)
{
  s_adc_error = err;
}

mock_rx_port_regs_t* mock_drv8243_get_port(uint8_t port)
{
  if (port >= k_mock_max_ports) {
    return nullptr;
  }
  return &s_ports[port];
}

bool mock_drv8243_fault_pin_configured(void)
{
  return s_fault_pin_configured;
}

bool mock_drv8243_pullup_enabled(void)
{
  return s_pullup_enabled;
}

/* =============================================================================
 * Mock rx_port_get_base Implementation
 *
 * This replaces the inline function from rx_port_utils.h for testing.
 * The test file redefines rx_port_regs_t to use mock_rx_port_regs_t.
 * =============================================================================
 */

/**
 * @brief Internal function to track fault pin configuration
 *
 * Called by the test when rx_drv8243 configures the fault pin.
 */
void mock_drv8243_track_fault_config(uint8_t port, uint8_t pin)
{
  s_fault_port = port;
  s_fault_pin  = pin;
}

/* =============================================================================
 * Mock ADC Bus Read Implementation
 * =============================================================================
 */

/**
 * @brief Mock ADC voltage read for bus manager
 */
rx_err_t mock_bus_adc_read_voltage_mv(uint32_t* voltage_mv)
{
  if (voltage_mv == nullptr) {
    return k_rx_err_null_ptr;
  }

  if (s_adc_error != k_rx_ok) {
    return s_adc_error;
  }

  *voltage_mv = s_adc_voltage_mv;
  return k_rx_ok;
}

/* =============================================================================
 * Mock Port Register Write Tracking
 *
 * These functions intercept writes to port registers to verify configuration.
 * =============================================================================
 */

/**
 * @brief Called when PDR register is modified (direction)
 */
void mock_drv8243_pdr_write(uint8_t port, uint8_t value)
{
  if (port < k_mock_max_ports) {
    uint8_t old_value = s_ports[port].pdr;
    s_ports[port].pdr = value;

    /* Check if fault pin was configured as input (bit cleared) */
    if (port == s_fault_port) {
      uint8_t pin_mask = (1 << s_fault_pin);
      if ((old_value & pin_mask) && !(value & pin_mask)) {
        s_fault_pin_configured = true;
      }
    }
  }
}

/**
 * @brief Called when PCR register is modified (pull-up)
 */
void mock_drv8243_pcr_write(uint8_t port, uint8_t value)
{
  if (port < k_mock_max_ports) {
    s_ports[port].pcr = value;

    /* Check if pull-up was enabled on fault pin */
    if (port == s_fault_port) {
      uint8_t pin_mask = (1 << s_fault_pin);
      if (value & pin_mask) {
        s_pullup_enabled = true;
      }
    }
  }
}
