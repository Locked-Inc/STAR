/* tests/mocks/mock_hal_regs.c */

/**
 * @file mock_hal_regs.c
 * @brief Mock HAL Register Implementation for Unit Testing
 *
 * Provides mock implementations of hardware register accessor functions
 * and helper functions for test setup and verification.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "mock_hal_regs.h"

#include <string.h>

/* =============================================================================
 * Global State
 * =============================================================================
 */

mock_hal_state_t g_mock_hal;

/* =============================================================================
 * Initialization Functions
 * =============================================================================
 */

void mock_hal_init(void)
{
  memset(&g_mock_hal, 0, sizeof(g_mock_hal));

  /* Set default ADC behavior - conversion completes immediately */
  g_mock_hal.adc_simulate_timeout = false;

  /* Set default RIIC behavior - no NACK, not busy */
  g_mock_hal.riic_simulate_nack = false;
  g_mock_hal.riic_simulate_busy = false;

  /* Set reasonable default for timeout count (simulates fast completion) */
  g_mock_hal.riic_simulated_timeout_count = 1;

  /* Pin validator disabled by default (tests control this) */
  g_mock_hal.pin_validator_active = false;
}

void mock_hal_reset(void)
{
  mock_hal_init();
}

/* =============================================================================
 * Port Mock Helpers
 * =============================================================================
 */

mock_port_regs_t* mock_hal_get_port(uint8_t port)
{
  if (port >= k_mock_hal_max_ports) {
    return NULL;
  }
  return &g_mock_hal.ports[port];
}

void mock_hal_set_port_input(uint8_t port, uint8_t pidr)
{
  if (port < k_mock_hal_max_ports) {
    g_mock_hal.ports[port].pidr = pidr;
  }
}

uint8_t mock_hal_get_port_output(uint8_t port)
{
  if (port < k_mock_hal_max_ports) {
    return g_mock_hal.ports[port].podr;
  }
  return 0;
}

uint8_t mock_hal_get_port_direction(uint8_t port)
{
  if (port < k_mock_hal_max_ports) {
    return g_mock_hal.ports[port].pdr;
  }
  return 0;
}

/* =============================================================================
 * ADC Mock Helpers
 * =============================================================================
 */

mock_adc_regs_t* mock_hal_get_adc(uint8_t unit)
{
  if (unit >= k_mock_hal_max_adc_unit) {
    return NULL;
  }
  return &g_mock_hal.adc[unit];
}

void mock_hal_set_adc_value(uint8_t unit, uint8_t channel, uint16_t value)
{
  if (unit >= k_mock_hal_max_adc_unit || channel >= k_mock_hal_max_adc_ch) {
    return;
  }

  mock_adc_regs_t* adc = &g_mock_hal.adc[unit];
  switch (channel) {
    case 0:
      adc->addr0 = value;
      break;
    case 1:
      adc->addr1 = value;
      break;
    case 2:
      adc->addr2 = value;
      break;
    case 3:
      adc->addr3 = value;
      break;
    case 4:
      adc->addr4 = value;
      break;
    case 5:
      adc->addr5 = value;
      break;
    case 6:
      adc->addr6 = value;
      break;
    case 7:
      adc->addr7 = value;
      break;
    default:
      break;
  }
}

void mock_hal_set_adc_timeout(bool simulate)
{
  g_mock_hal.adc_simulate_timeout = simulate;
}

/* =============================================================================
 * RIIC Mock Helpers
 * =============================================================================
 */

mock_riic_regs_t* mock_hal_get_riic(uint8_t channel)
{
  if (channel >= k_mock_hal_max_riic_ch) {
    return NULL;
  }
  return &g_mock_hal.riic[channel];
}

void mock_hal_set_riic_nack(bool simulate)
{
  g_mock_hal.riic_simulate_nack = simulate;
}

void mock_hal_set_riic_busy(bool simulate)
{
  g_mock_hal.riic_simulate_busy = simulate;
}

void mock_hal_set_riic_timeout_count(uint32_t count)
{
  g_mock_hal.riic_simulated_timeout_count = count;
}

/* =============================================================================
 * System Mock Helpers
 * =============================================================================
 */

mock_system_regs_t* mock_hal_get_system(void)
{
  return &g_mock_hal.system;
}

void mock_hal_set_pin_validator_active(bool active)
{
  g_mock_hal.pin_validator_active = active;
}

/* =============================================================================
 * HAL Function Mocks
 *
 * These functions replace the real HAL register accessors during testing.
 * They return pointers to mock register structures.
 * =============================================================================
 */

#include "rx_err.h"
#include "rx_port_constants.h"

/* Forward declaration for infrastructure interface mock */
typedef struct rx_pin_interface rx_pin_interface_t;

/**
 * @brief Mock pin reserve function (always succeeds)
 */
static rx_err_t mock_pin_reserve(void* ctx, uint8_t port, uint8_t pin, const char* function)
{
  (void)ctx;
  (void)port;
  (void)pin;
  (void)function;
  return k_rx_ok;
}

/**
 * @brief Mock pin release function (always succeeds)
 */
static rx_err_t mock_pin_release(void* ctx, uint8_t port, uint8_t pin)
{
  (void)ctx;
  (void)port;
  (void)pin;
  return k_rx_ok;
}

/**
 * @brief Mock pin interface structure
 */
static struct {
  rx_err_t (*reserve_pin)(void* ctx, uint8_t port, uint8_t pin, const char* function);
  rx_err_t (*release_pin)(void* ctx, uint8_t port, uint8_t pin);
  void*    ctx;
} s_mock_pin_interface = {
  .reserve_pin = mock_pin_reserve,
  .release_pin = mock_pin_release,
  .ctx = NULL
};

/**
 * @brief Get mock pin interface
 */
rx_pin_interface_t* rx_infrastructure_get_pin_interface(void)
{
  if (g_mock_hal.pin_validator_active) {
    return (rx_pin_interface_t*)&s_mock_pin_interface;
  }
  return NULL;
}

/* =============================================================================
 * Logging Stubs (for tests that don't include mock_rx_log.c)
 * =============================================================================
 */

/* These are weak symbols - if mock_rx_log.c is linked, its versions win */
__attribute__((weak)) void uart_putc(char data)
{
  (void)data;
}

__attribute__((weak)) void uart_puts(const char* str)
{
  (void)str;
}

__attribute__((weak)) void uart_putint(int32_t value)
{
  (void)value;
}

__attribute__((weak)) void uart_puthex(uint32_t value, uint8_t digits)
{
  (void)value;
  (void)digits;
}
