/* lib/rx_core/inc/rx_err.h */

/**
 * @file rx_err.h
 * @brief Error Code Definitions for RX72N Firmware
 * @details
 * Defines error codes and types for the RX72N firmware.
 * Follows ESP32 esp_err_t pattern for architectural consistency.
 *
 * Error codes are organized into categories:
 * - 0x000: Success
 * - 0x1xx: Generic errors
 * - 0x2xx: Hardware errors
 * - 0x3xx: RTOS errors
 * - 0x4xx: Communication errors
 * - 0x5xx: Validation errors
 *
 * @date 2026-01-01
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX72N_ERR_H
#define STAR_RX72N_ERR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Error Type Definition
 * =============================================================================
 */

/**
 * @brief Error code type for RX72N firmware
 *
 * Signed 32-bit integer allowing negative error codes.
 * Success (k_rx_ok) is always 0.
 */
typedef int32_t rx_err_t;

/* =============================================================================
 * Error Codes (Following Style Guide: Enums > Const > Macros)
 * =============================================================================
 */

/**
 * @brief Error codes for RX72N firmware
 *
 * Organized into categories:
 * - 0x000: Success
 * - 0x1xx: Generic errors
 * - 0x2xx: Hardware errors
 * - 0x3xx: RTOS errors
 * - 0x4xx: Communication errors
 * - 0x5xx: Validation errors
 */
typedef enum {
  k_rx_ok = 0, /**< Success */

  /* Generic Errors (0x100 - 0x1FF) */
  k_rx_fail                 = 0x101, /**< Generic failure */
  k_rx_err_no_mem           = 0x102, /**< Out of memory */
  k_rx_err_invalid_arg      = 0x103, /**< Invalid argument */
  k_rx_err_invalid_state    = 0x104, /**< Invalid state */
  k_rx_err_invalid_size     = 0x105, /**< Invalid size */
  k_rx_err_not_found        = 0x106, /**< Not found */
  k_rx_err_not_supported    = 0x107, /**< Not supported */
  k_rx_err_timeout          = 0x108, /**< Timeout */
  k_rx_err_busy             = 0x109, /**< Resource busy */
  k_rx_err_would_block      = 0x10A, /**< Operation would block */
  k_rx_err_exists           = 0x10B, /**< Already exists */
  k_rx_err_empty            = 0x10C, /**< Empty (no data available) */
  k_rx_err_cancelled        = 0x10D, /**< Operation cancelled */
  k_rx_err_not_initialized  = 0x10E, /**< Module not initialized */

  /* Hardware Errors (0x200 - 0x2FF) */
  k_rx_err_hw_init_failed    = 0x201, /**< Hardware initialization failed */
  k_rx_err_hw_not_ready      = 0x202, /**< Hardware not ready */
  k_rx_err_hw_timeout        = 0x203, /**< Hardware timeout */
  k_rx_err_hw_error          = 0x204, /**< Hardware error */
  k_rx_err_gpio_conflict     = 0x205, /**< GPIO pin conflict */
  k_rx_err_gpio_invalid_port = 0x206, /**< Invalid GPIO port */
  k_rx_err_gpio_invalid_pin  = 0x207, /**< Invalid GPIO pin */
  k_rx_err_out_of_range      = 0x208, /**< Sensor value out of range */

  /* RTOS Errors (0x300 - 0x3FF) */
  k_rx_err_rtos_error         = 0x301, /**< RTOS error */
  k_rx_err_threadx            = 0x301, /**< ThreadX error (alias) */
  k_rx_err_rtos_thread_create = 0x302, /**< Thread creation failed */
  k_rx_err_rtos_semaphore     = 0x303, /**< Semaphore error */
  k_rx_err_rtos_mutex         = 0x304, /**< Mutex error */
  k_rx_err_rtos_queue         = 0x305, /**< Queue error */
  k_rx_err_rtos_timer         = 0x306, /**< Timer error */

  /* Communication Errors (0x400 - 0x4FF) */
  k_rx_err_comm_error     = 0x401, /**< Communication error */
  k_rx_err_spi_error      = 0x402, /**< SPI error */
  k_rx_err_uart_error     = 0x403, /**< UART error */
  k_rx_err_i2c_error      = 0x404, /**< I2C error */
  k_rx_err_crc_mismatch   = 0x405, /**< CRC mismatch */
  k_rx_err_protocol_error = 0x406, /**< Protocol error */
  k_rx_err_nack           = 0x407, /**< NACK received */
  k_rx_err_conflict       = 0x408, /**< Resource conflict */

  /* Validation Errors (0x500 - 0x5FF) */
  k_rx_err_validation_failed  = 0x501, /**< Validation failed */
  k_rx_err_checksum_mismatch  = 0x502, /**< Checksum mismatch */
  k_rx_err_range_check_failed = 0x503, /**< Range check failed */
  k_rx_err_null_pointer       = 0x504, /**< Null pointer */
} rx_err_codes_t;

/* =============================================================================
 * Error Code Helpers
 * =============================================================================
 */

/**
 * @brief Convert ThreadX status code to rx_err_t
 *
 * @param[in] tx_status ThreadX status code (UINT)
 * @return rx_err_t equivalent error code
 */
static inline rx_err_t rx_err_from_threadx(uint32_t tx_status)
{
  return (tx_status == 0) ? k_rx_ok : k_rx_err_rtos_error;
}

/**
 * @brief Check if error code represents success
 *
 * @param[in] err Error code to check
 * @return true if success, false if error
 */
static inline bool rx_err_is_ok(rx_err_t err)
{
  return (err == k_rx_ok);
}

/**
 * @brief Check if error code represents failure
 *
 * @param[in] err Error code to check
 * @return true if error, false if success
 */
static inline bool rx_err_is_error(rx_err_t err)
{
  return (err != k_rx_ok);
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_ERR_H */
