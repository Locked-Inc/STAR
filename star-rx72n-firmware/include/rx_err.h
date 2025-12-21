/* include/rx_err.h */

/**
 * @file rx_err.h
 * @brief Error Code Definitions for RX72N Firmware
 *
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
 * Success (RX_OK) is always 0.
 */
typedef int32_t rx_err_t;

/* =============================================================================
 * Success Code
 * =============================================================================
 */

/**
 * @brief Success return code
 */
#define RX_OK (0)

/* =============================================================================
 * Generic Errors (0x100 - 0x1FF)
 * =============================================================================
 */

/**
 * @brief Generic failure
 */
#define RX_FAIL (0x101)

/**
 * @brief Out of memory
 */
#define RX_ERR_NO_MEM (0x102)

/**
 * @brief Invalid argument
 */
#define RX_ERR_INVALID_ARG (0x103)

/**
 * @brief Invalid state
 */
#define RX_ERR_INVALID_STATE (0x104)

/**
 * @brief Invalid size
 */
#define RX_ERR_INVALID_SIZE (0x105)

/**
 * @brief Not found
 */
#define RX_ERR_NOT_FOUND (0x106)

/**
 * @brief Not supported
 */
#define RX_ERR_NOT_SUPPORTED (0x107)

/**
 * @brief Timeout occurred
 */
#define RX_ERR_TIMEOUT (0x108)

/**
 * @brief Resource busy
 */
#define RX_ERR_BUSY (0x109)

/**
 * @brief Operation would block
 */
#define RX_ERR_WOULD_BLOCK (0x10A)

/**
 * @brief Resource already exists
 */
#define RX_ERR_EXISTS (0x10B)

/* =============================================================================
 * Hardware Errors (0x200 - 0x2FF)
 * =============================================================================
 */

/**
 * @brief Hardware initialization failed
 */
#define RX_ERR_HW_INIT_FAILED (0x201)

/**
 * @brief Hardware not ready
 */
#define RX_ERR_HW_NOT_READY (0x202)

/**
 * @brief Hardware timeout
 */
#define RX_ERR_HW_TIMEOUT (0x203)

/**
 * @brief Hardware error detected
 */
#define RX_ERR_HW_ERROR (0x204)

/**
 * @brief GPIO pin conflict
 */
#define RX_ERR_GPIO_CONFLICT (0x205)

/**
 * @brief Invalid GPIO port
 */
#define RX_ERR_GPIO_INVALID_PORT (0x206)

/**
 * @brief Invalid GPIO pin
 */
#define RX_ERR_GPIO_INVALID_PIN (0x207)

/* =============================================================================
 * RTOS Errors (0x300 - 0x3FF)
 * =============================================================================
 */

/**
 * @brief ThreadX error
 */
#define RX_ERR_RTOS_ERROR (0x301)

/**
 * @brief Thread creation failed
 */
#define RX_ERR_RTOS_THREAD_CREATE (0x302)

/**
 * @brief Semaphore error
 */
#define RX_ERR_RTOS_SEMAPHORE (0x303)

/**
 * @brief Mutex error
 */
#define RX_ERR_RTOS_MUTEX (0x304)

/**
 * @brief Queue error
 */
#define RX_ERR_RTOS_QUEUE (0x305)

/**
 * @brief Timer error
 */
#define RX_ERR_RTOS_TIMER (0x306)

/* =============================================================================
 * Communication Errors (0x400 - 0x4FF)
 * =============================================================================
 */

/**
 * @brief Communication error
 */
#define RX_ERR_COMM_ERROR (0x401)

/**
 * @brief SPI error
 */
#define RX_ERR_SPI_ERROR (0x402)

/**
 * @brief UART error
 */
#define RX_ERR_UART_ERROR (0x403)

/**
 * @brief I2C error
 */
#define RX_ERR_I2C_ERROR (0x404)

/**
 * @brief CRC mismatch
 */
#define RX_ERR_CRC_MISMATCH (0x405)

/**
 * @brief Protocol error
 */
#define RX_ERR_PROTOCOL_ERROR (0x406)

/* =============================================================================
 * Validation Errors (0x500 - 0x5FF)
 * =============================================================================
 */

/**
 * @brief Validation failed
 */
#define RX_ERR_VALIDATION_FAILED (0x501)

/**
 * @brief Checksum mismatch
 */
#define RX_ERR_CHECKSUM_MISMATCH (0x502)

/**
 * @brief Range check failed
 */
#define RX_ERR_RANGE_CHECK_FAILED (0x503)

/**
 * @brief Null pointer
 */
#define RX_ERR_NULL_POINTER (0x504)

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
  return (tx_status == 0) ? RX_OK : RX_ERR_RTOS_ERROR;
}

/**
 * @brief Check if error code represents success
 *
 * @param[in] err Error code to check
 * @return true if success, false if error
 */
static inline bool rx_err_is_ok(rx_err_t err)
{
  return (err == RX_OK);
}

/**
 * @brief Check if error code represents failure
 *
 * @param[in] err Error code to check
 * @return true if error, false if success
 */
static inline bool rx_err_is_error(rx_err_t err)
{
  return (err != RX_OK);
}

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX72N_ERR_H */
