/* include/rx_bus_manager.h */

/**
 * @file rx_bus_manager.h
 * @brief Bus Manager Skeleton (Future Implementation)
 *
 * This is a skeleton/stub file demonstrating how the bus manager will use
 * the Dependency Inversion Principle (DIP) interfaces.
 *
 * The bus manager will:
 * - Accept error_interface and pin_interface as dependencies (DIP)
 * - Manage communication buses (SPI, I2C, UART, etc.)
 * - Use pin interface to reserve/validate GPIO pins
 * - Use error interface to report bus errors and retry logic
 *
 * This pattern follows the ESP32 star_bus_manager architecture:
 * - Interfaces injected at initialization
 * - No direct dependencies on concrete implementations
 * - Testable via mock interfaces
 *
 * Example usage (future implementation):
 *   // 1. Create dependencies
 *   static error_handler_t s_error_handler;
 *   static pin_validator_t s_pin_validator;
 *   error_handler_init(&s_error_handler, 3, 100, 5000);
 *   pin_validator_init(&s_pin_validator);
 *
 *   // 2. Get interfaces
 *   rx_error_interface_t error_iface;
 *   rx_pin_interface_t pin_iface;
 *   error_handler_get_interface(&error_iface, &s_error_handler);
 *   pin_validator_get_interface(&pin_iface, &s_pin_validator);
 *
 *   // 3. Initialize bus manager with interfaces
 *   static bus_manager_t s_bus_manager;
 *   bus_manager_init(&s_bus_manager, &error_iface, &pin_iface);
 *
 *   // 4. Use bus manager
 *   rx_err_t err = bus_manager_create_spi(&s_bus_manager, "SPI0", ...);
 *   if (err != RX_OK) {
 *       // Bus manager will have used error interface to report details
 *   }
 */

#ifndef STAR_RX72N_BUS_MANAGER_H
#define STAR_RX72N_BUS_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rx_err.h"
#include "rx_error_interface.h"
#include "rx_pin_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Bus Manager Structure (Skeleton)
 * =============================================================================
 */

/**
 * @brief Bus manager instance (skeleton for future implementation)
 *
 * This structure will hold:
 * - Injected interfaces (error, pin)
 * - Bus instance tracking (SPI, I2C, UART)
 * - Bus configuration and state
 */
typedef struct {
  /**
   * @brief Error handler interface (injected dependency)
   */
  rx_error_interface_t* error_iface;

  /**
   * @brief Pin validator interface (injected dependency)
   */
  rx_pin_interface_t* pin_iface;

  /**
   * @brief Is the bus manager initialized?
   */
  bool initialized;

  /* Future: Bus instance tracking arrays will go here */
} bus_manager_t;

/* =============================================================================
 * Public API (Skeleton)
 * =============================================================================
 */

/**
 * @brief Initialize bus manager with injected dependencies
 *
 * @param[in,out] manager Bus manager instance to initialize
 * @param[in] error_iface Error handler interface (DIP)
 * @param[in] pin_iface Pin validator interface (DIP)
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if any parameter is NULL,
 *         RX_ERR_INVALID_STATE if interfaces are invalid
 *
 * @note This is a skeleton implementation demonstrating DIP pattern.
 *       Full bus management functionality will be added in future commits.
 */
rx_err_t bus_manager_init(bus_manager_t* manager, rx_error_interface_t* error_iface,
                          rx_pin_interface_t* pin_iface);

/**
 * @brief Deinitialize bus manager
 *
 * @param[in,out] manager Bus manager instance
 *
 * @return RX_OK on success,
 *         RX_ERR_NULL_POINTER if manager is NULL
 */
rx_err_t bus_manager_deinit(bus_manager_t* manager);

/* =============================================================================
 * Future API (Placeholders)
 * =============================================================================
 */

/*
 * Future functions will include:
 *
 * rx_err_t bus_manager_create_spi(bus_manager_t* manager, const char* name,
 *                                 uint8_t mosi_port, uint8_t mosi_pin,
 *                                 uint8_t miso_port, uint8_t miso_pin,
 *                                 uint8_t sck_port, uint8_t sck_pin,
 *                                 uint8_t cs_port, uint8_t cs_pin,
 *                                 uint32_t frequency_hz);
 *
 * rx_err_t bus_manager_create_i2c(bus_manager_t* manager, const char* name,
 *                                 uint8_t sda_port, uint8_t sda_pin,
 *                                 uint8_t scl_port, uint8_t scl_pin,
 *                                 uint32_t frequency_hz);
 *
 * rx_err_t bus_manager_create_uart(bus_manager_t* manager, const char* name,
 *                                  uint8_t tx_port, uint8_t tx_pin,
 *                                  uint8_t rx_port, uint8_t rx_pin,
 *                                  uint32_t baudrate);
 *
 * rx_err_t bus_manager_spi_transfer(bus_manager_t* manager, const char* name,
 *                                   const uint8_t* tx_data, uint8_t* rx_data,
 *                                   size_t len, uint32_t timeout_ms);
 *
 * ... etc ...
 */

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_BUS_MANAGER_H */
