/**
 * @file mock_rx_hcsr04_irq.c
 * @brief Mock implementations for HC-SR04 IRQ mode functions (for unit testing)
 */

#include <stdint.h>

#include "rx_err.h"
#include "rx_hcsr04_icu.h"
#include "rx_hcsr04_isr.h"
#include "rx_port_constants.h"

/* Mock implementations - these are stubs for unit testing */

rx_err_t rx_mpc_set_gpio(rx_port_pin_t pin)
{
  (void)pin;
  return k_rx_ok;
}

rx_err_t rx_mpc_set_irq(rx_port_pin_t pin)
{
  (void)pin;
  return k_rx_ok;
}

rx_err_t rx_hcsr04_icu_configure(uint8_t irq_num, uint8_t priority)
{
  (void)irq_num;
  (void)priority;
  return k_rx_ok;
}

rx_err_t rx_hcsr04_icu_disable(uint8_t irq_num)
{
  (void)irq_num;
  return k_rx_ok;
}

rx_err_t rx_hcsr04_isr_register(uint8_t irq_num, uint8_t sensor_index)
{
  (void)irq_num;
  (void)sensor_index;
  return k_rx_ok;
}

void rx_hcsr04_isr_start(uint8_t irq_num)
{
  (void)irq_num;
}

rx_err_t rx_hcsr04_isr_get_duration(uint8_t irq_num, uint32_t* duration_us)
{
  (void)irq_num;
  (void)duration_us;
  return k_rx_err_timeout; /* Not implemented in tests */
}
