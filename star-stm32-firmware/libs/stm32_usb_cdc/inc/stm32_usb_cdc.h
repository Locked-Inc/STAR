/**
 * @file stm32_usb_cdc.h
 * @brief USB CDC ACM transport layer via TinyUSB.
 *
 * @details
 * Provides byte-stream send/receive over USB CDC ACM. The STM32 enumerates
 * as a USB CDC device (/dev/ttyACM0 on Linux). The Go gateway connects using
 * its existing CDC transport with no changes required.
 *
 * **Architecture:**
 * - `vUsbDeviceTask` calls `tud_task()` continuously and drains CDC RX into
 *   a FreeRTOS StreamBuffer.
 * - `stm32_usb_cdc_receive()` reads from that StreamBuffer, not directly from
 *   TinyUSB, decoupling USB timing from frame parsing.
 * - `stm32_usb_cdc_send()` writes to the TinyUSB CDC TX FIFO and flushes.
 *
 * **USB disconnect fast-path:**
 * `tud_cdc_line_state_cb()` is registered internally. When DTR goes low
 * (host disconnected), estop is set immediately via stm32_shared_data --
 * no need to wait for the 500 ms watchdog.
 *
 * @note Not thread-safe for concurrent senders. Only vCommTask calls send;
 *       only vTelemetryTask calls send for telemetry. Callers must coordinate.
 *
 * @see Sources/tusb_config.h TinyUSB configuration
 * @see Sources/usb_descriptors.c USB device descriptors
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "stm32_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the USB CDC transport and TinyUSB stack.
 *
 * @details
 * Configures the STM32 USB OTG HS peripheral, sets NVIC priority to
 * configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY, enables the OTG_HS IRQ,
 * and calls tusb_init(). Must be called after clock init and before
 * vTaskStartScheduler().
 *
 * @return stm32_err_t
 * @retval k_stm32_err_ok Initialization successful
 * @retval k_stm32_err_hardware TinyUSB or peripheral init failed
 *
 * @pre System clock at 216 MHz and USB 48 MHz must be configured
 * @pre FreeRTOS StreamBuffer for RX must be created before this call
 * @post USB OTG HS IRQ enabled at correct FreeRTOS-safe priority
 * @post TinyUSB task can begin processing via tud_task()
 *
 * @note Call once from main() before vTaskStartScheduler()
 *
 * @since Version 1.0.0
 */
stm32_err_t stm32_usb_cdc_init(void);

/**
 * @brief Send bytes over USB CDC.
 *
 * @details
 * Writes data to the TinyUSB CDC TX FIFO and calls tud_cdc_write_flush().
 * Blocks up to 20 ms waiting for TX FIFO space before returning timeout.
 * Never drops bytes silently.
 *
 * @param[in] data  Buffer to transmit
 * @param[in] len   Number of bytes to send
 *
 * @return stm32_err_t
 * @retval k_stm32_err_ok All bytes sent
 * @retval k_stm32_err_null_ptr data is NULL
 * @retval k_stm32_err_timeout TX FIFO did not drain within 20 ms
 *
 * @pre stm32_usb_cdc_init() must have been called
 * @pre data must point to a valid buffer of at least len bytes
 * @post Bytes queued for USB transmission and flush triggered
 *
 * @note Not re-entrant; only one task should call send at a time
 *
 * @since Version 1.0.0
 */
stm32_err_t stm32_usb_cdc_send(const uint8_t* data, uint32_t len);

/**
 * @brief Receive bytes from the USB CDC RX StreamBuffer.
 *
 * @details
 * Reads up to max_len bytes from the FreeRTOS StreamBuffer populated by
 * vUsbDeviceTask. Returns immediately with however many bytes are available
 * (non-blocking). Sets *out_len = 0 if no data is available.
 *
 * @param[out] buf      Destination buffer
 * @param[in]  max_len  Maximum bytes to read
 * @param[out] out_len  Actual bytes read
 *
 * @return stm32_err_t
 * @retval k_stm32_err_ok Read succeeded (out_len may be 0 if no data)
 * @retval k_stm32_err_null_ptr buf or out_len is NULL
 *
 * @pre stm32_usb_cdc_init() must have been called
 * @pre buf must point to a valid buffer of at least max_len bytes
 * @post *out_len set to number of bytes written to buf
 *
 * @note Non-blocking; caller must poll or use vTaskDelay between calls
 *
 * @since Version 1.0.0
 */
stm32_err_t stm32_usb_cdc_receive(uint8_t* buf, uint32_t max_len,
                                                 uint32_t* out_len);

/**
 * @brief Check whether a CDC host is connected (DTR asserted).
 *
 * @return bool true if host has opened the CDC port, false otherwise
 *
 * @pre stm32_usb_cdc_init() must have been called
 * @post No side effects
 *
 * @since Version 1.0.0
 */
bool stm32_usb_cdc_is_connected(void);

#ifdef __cplusplus
}
#endif
