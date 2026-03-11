/**
 * @file mock_comm_transports.c
 * @brief Minimal transport and protocol stubs for comm_task unit tests
 *
 * @details
 * Provides stub implementations of transport init functions and protocol
 * helpers that comm_task.c references but that are not exercised by
 * comm_task_apply_system_config() tests.  All stubs immediately return
 * k_rx_ok (or void) without touching hardware.
 *
 * Covers three groups of symbols:
 * - Transport init: rx_usb_comm_init, rx_spi_comm_init, rx_spi_link_init,
 *   rx_i2c_comm_init, rx_uart_comm_init
 * - IWDT heartbeat: rx_iwdt_task_heartbeat
 * - Comm manager helpers absent from mock_rx_comm_manager.c:
 *   rx_comm_manager_process_retransmits, rx_comm_manager_set_auto_retransmit
 * - nanopb helpers absent from mock_rx_nanopb.c (called only from frame
 *   callbacks, never from comm_task_apply_system_config):
 *   rx_nanopb_decode_pid_gains_request,
 *   rx_nanopb_decode_retransmit_config_request,
 *   rx_nanopb_create_response_header,
 *   rx_nanopb_encode_velocity_response,
 *   rx_nanopb_encode_estop_response,
 *   rx_nanopb_encode_pid_gains_response
 *
 * @note The nanopb stubs use void* for struct parameters to avoid pulling in
 * generated protobuf headers.  On x86-64 System V ABI all pointer arguments
 * travel in integer registers, so the stubs are ABI-compatible with callers
 * that pass real struct pointers.  The stubs are never reached during
 * test_rx_system_config execution.
 *
 * @see comm_task.c  Production code that references these symbols
 * @see test_rx_system_config.c  Test that links this stub file
 *
 * @author Locked, Inc.
 * @date 2026-03-11
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "rx_err.h"

/* Transport init headers - needed for proper handle/config types */
#include <stdbool.h>
#include <stdint.h>

#include "rx_comm_manager.h"
#include "rx_i2c_comm.h"
#include "rx_iwdt.h"
#include "rx_spi_comm.h"
#include "rx_spi_link.h"
#include "rx_uart_comm.h"
#include "rx_usb_comm.h"

/* =============================================================================
 * Transport Init Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_usb_comm_init() -- always succeeds
 *
 * @param[out] handle Ignored
 * @param[in]  config Ignored
 * @return k_rx_ok
 */
rx_err_t rx_usb_comm_init(rx_usb_comm_handle_t* handle, const rx_usb_comm_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_spi_comm_init() -- always succeeds
 *
 * @param[out] handle Ignored
 * @param[in]  config Ignored
 * @return k_rx_ok
 */
rx_err_t rx_spi_comm_init(rx_spi_comm_handle_t* handle, const rx_spi_comm_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_spi_link_init() -- always succeeds
 *
 * @param[out] link   Ignored
 * @param[in]  config Ignored
 * @return k_rx_ok
 */
rx_err_t rx_spi_link_init(rx_spi_link_t* link, const rx_spi_link_config_t* config)
{
  (void)link;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_i2c_comm_init() -- always succeeds
 *
 * @param[out] handle Ignored
 * @param[in]  config Ignored
 * @return k_rx_ok
 */
rx_err_t rx_i2c_comm_init(rx_i2c_comm_handle_t* handle, const rx_i2c_comm_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_uart_comm_init() -- always succeeds
 *
 * @param[out] handle Ignored
 * @param[in]  config Ignored
 * @return k_rx_ok
 */
rx_err_t rx_uart_comm_init(rx_uart_comm_handle_t* handle, const rx_uart_comm_config_t* config)
{
  (void)handle;
  (void)config;
  return k_rx_ok;
}

/* =============================================================================
 * IWDT Stub
 * =============================================================================
 */

/**
 * @brief Stub for rx_iwdt_task_heartbeat() -- always succeeds
 *
 * @param[in] task_name Ignored
 * @return k_rx_ok
 */
rx_err_t rx_iwdt_task_heartbeat(const char* task_name)
{
  (void)task_name;
  return k_rx_ok;
}

/* =============================================================================
 * Comm Manager Stubs (absent from mock_rx_comm_manager.c)
 * =============================================================================
 */

/**
 * @brief Stub for rx_comm_manager_process_retransmits() -- no-op
 *
 * @param[in,out] mgr            Ignored
 * @param[in]     current_time_ms Ignored
 * @return k_rx_ok
 */
rx_err_t rx_comm_manager_process_retransmits(rx_comm_manager_t* mgr, uint32_t current_time_ms)
{
  (void)mgr;
  (void)current_time_ms;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_comm_manager_set_auto_retransmit() -- no-op
 *
 * @param[in,out] mgr     Ignored
 * @param[in]     enabled  Ignored
 * @param[in]     config   Ignored
 * @return k_rx_ok
 */
rx_err_t rx_comm_manager_set_auto_retransmit(rx_comm_manager_t*                     mgr,
                                             bool                                   enabled,
                                             const rx_spi_comm_retransmit_config_t* config)
{
  (void)mgr;
  (void)enabled;
  (void)config;
  return k_rx_ok;
}

/* =============================================================================
 * nanopb Stubs (absent from mock_rx_nanopb.c)
 *
 * Struct parameters use void* to avoid including generated protobuf headers.
 * These stubs are never called during test_rx_system_config execution.
 * =============================================================================
 */

/**
 * @brief Stub for rx_nanopb_decode_pid_gains_request() -- no-op
 * @return k_rx_ok
 */
rx_err_t rx_nanopb_decode_pid_gains_request(const void* buffer, uint32_t len, void* msg)
{
  (void)buffer;
  (void)len;
  (void)msg;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_decode_retransmit_config_request() -- no-op
 * @return k_rx_ok
 */
rx_err_t rx_nanopb_decode_retransmit_config_request(const void* buffer, uint32_t len, void* msg)
{
  (void)buffer;
  (void)len;
  (void)msg;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_create_response_header() -- no-op
 */
void rx_nanopb_create_response_header(void* header, uint32_t status, const char* request_id)
{
  (void)header;
  (void)status;
  (void)request_id;
}

/**
 * @brief Stub for rx_nanopb_encode_velocity_response() -- no-op
 * @return k_rx_ok
 */
rx_err_t rx_nanopb_encode_velocity_response(const void* msg,
                                            void*       buffer,
                                            uint32_t    buffer_size,
                                            uint32_t*   len)
{
  (void)msg;
  (void)buffer;
  (void)buffer_size;
  if (len != nullptr) {
    *len = 0u;
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_encode_estop_response() -- no-op
 * @return k_rx_ok
 */
rx_err_t
rx_nanopb_encode_estop_response(const void* msg, void* buffer, uint32_t buffer_size, uint32_t* len)
{
  (void)msg;
  (void)buffer;
  (void)buffer_size;
  if (len != nullptr) {
    *len = 0u;
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_encode_pid_gains_response() -- no-op
 * @return k_rx_ok
 */
rx_err_t rx_nanopb_encode_pid_gains_response(const void* msg,
                                             void*       buffer,
                                             uint32_t    buffer_size,
                                             uint32_t*   len)
{
  (void)msg;
  (void)buffer;
  (void)buffer_size;
  if (len != nullptr) {
    *len = 0u;
  }
  return k_rx_ok;
}

/* =============================================================================
 * mock_tx_api.c Dependency Stub
 * =============================================================================
 */

/**
 * @brief Stub for mock_tasks_reset() called by mock_tx_reset()
 *
 * @details
 * mock_tx_api.c references mock_tasks_reset() from mock_tasks.c.  Including
 * mock_tasks.c in test_rx_system_config would create a duplicate
 * comm_task_create() symbol (real comm_task.c is already linked).  This
 * no-op stub satisfies the linker without pulling in the full task mock.
 */
void mock_tasks_reset(void) {}
