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
#include <stdint.h>

#include "rx_comm_manager.h"
#include "rx_i2c_comm.h"
#include "rx_iwdt.h"
#include "rx_spi_comm.h"
#include "rx_spi_link.h"
#include "rx_uart_comm.h"

/* =============================================================================
 * Transport Init Stubs
 * =============================================================================
 */

/**
 * @brief Stub for rx_spi_comm_init() -- always succeeds
 *
 * @details
 * No-op test stub that immediately returns k_rx_ok without touching hardware
 * or initializing any transport state. Satisfies the linker for test targets
 * that link comm_task.c but do not exercise SPI transport initialization.
 *
 * @param[out] handle Ignored (may be NULL)
 * @param[in]  config Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No SPI hardware or handle state modified
 * @post Returns k_rx_ok unconditionally
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
 * @details
 * No-op test stub that immediately returns k_rx_ok without touching hardware
 * or initializing any link state. Satisfies the linker for test targets that
 * link comm_task.c but do not exercise SPI link layer initialization.
 *
 * @param[out] link   Ignored (may be NULL)
 * @param[in]  config Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No SPI link hardware or link state modified
 * @post Returns k_rx_ok unconditionally
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
 * @details
 * No-op test stub that immediately returns k_rx_ok without touching hardware
 * or initializing any transport state. Satisfies the linker for test targets
 * that link comm_task.c but do not exercise I2C transport initialization.
 *
 * @param[out] handle Ignored (may be NULL)
 * @param[in]  config Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No I2C hardware or handle state modified
 * @post Returns k_rx_ok unconditionally
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
 * @details
 * No-op test stub that immediately returns k_rx_ok without touching hardware
 * or initializing any transport state. Satisfies the linker for test targets
 * that link comm_task.c but do not exercise UART transport initialization.
 *
 * @param[out] handle Ignored (may be NULL)
 * @param[in]  config Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No UART hardware or handle state modified
 * @post Returns k_rx_ok unconditionally
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
 * @details
 * No-op test stub that immediately returns k_rx_ok without touching the
 * Independent Watchdog Timer or any hardware register. Satisfies the linker
 * for test targets that link comm_task.c but do not exercise the IWDT
 * heartbeat path.
 *
 * @param[in] task_name Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No IWDT hardware or watchdog state modified
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
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
 * @details
 * No-op test stub that immediately returns k_rx_ok without processing any
 * pending retransmit entries. Satisfies the linker for test targets that
 * link comm_task.c but do not exercise the retransmit processing path.
 *
 * @param[in,out] mgr            Ignored (may be NULL)
 * @param[in]     current_time_ms Ignored
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No comm manager retransmit state modified
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
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
 * @details
 * No-op test stub that immediately returns k_rx_ok without modifying any
 * retransmit configuration. Satisfies the linker for test targets that
 * link comm_task.c but do not exercise the auto-retransmit configuration
 * path.
 *
 * @param[in,out] mgr    Ignored (may be NULL)
 * @param[in]     enabled Ignored
 * @param[in]     config  Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No comm manager auto-retransmit state modified
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
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
 * Uses the exact parameter types declared in rx_nanopb.h so each stub is
 * ABI-compatible with its production counterpart and the linker can verify
 * the signatures at link time. These stubs are never called during
 * test_rx_system_config execution.
 * =============================================================================
 */
#include "rx_nanopb.h"

/**
 * @brief Stub for rx_nanopb_decode_pid_gains_request() -- no-op
 *
 * @details
 * No-op test stub that immediately returns k_rx_ok without decoding any
 * protobuf data. Satisfies the linker for test targets that link comm_task.c
 * but do not exercise PID gains request decoding.
 *
 * @param[in]  buffer Ignored (may be NULL)
 * @param[in]  len    Ignored
 * @param[out] msg    Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No protobuf state or output message modified
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
 */
rx_err_t rx_nanopb_decode_pid_gains_request(const uint8_t*              buffer,
                                            uint32_t                    len,
                                            star_v1_SetPIDGainsRequest* msg)
{
  (void)buffer;
  (void)len;
  (void)msg;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_decode_retransmit_config_request() -- no-op
 *
 * @details
 * No-op test stub that immediately returns k_rx_ok without decoding any
 * protobuf data. Satisfies the linker for test targets that link comm_task.c
 * but do not exercise retransmit config request decoding.
 *
 * @param[in]  buffer Ignored (may be NULL)
 * @param[in]  len    Ignored
 * @param[out] msg    Ignored (may be NULL)
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No protobuf state or output message modified
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
 */
rx_err_t rx_nanopb_decode_retransmit_config_request(const uint8_t*                      buffer,
                                                    uint32_t                            len,
                                                    star_v1_SetRetransmitConfigRequest* msg)
{
  (void)buffer;
  (void)len;
  (void)msg;
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_create_response_header() -- no-op
 *
 * @details
 * No-op test stub that returns immediately without populating any response
 * header fields. Satisfies the linker for test targets that link comm_task.c
 * but do not exercise response header creation.
 *
 * @param[out] header     Ignored (may be NULL)
 * @param[in]  status     Ignored
 * @param[in]  request_id Ignored (may be NULL)
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post No header fields populated; pointed-to memory unchanged
 * @post Function returns with no observable side effects
 *
 * @since Version 1.0.0
 */
void rx_nanopb_create_response_header(star_v1_ResponseHeader* header,
                                      star_v1_Status          status,
                                      const char*             request_id)
{
  (void)header;
  (void)status;
  (void)request_id;
}

/* =============================================================================
 * Encoded length sentinel used by nanopb encode stubs
 * =============================================================================
 */

/**
 * @enum nanopb_stub_len_t
 * @brief Sentinel value written to the output length pointer by nanopb encode stubs
 *
 * @details
 * Stub encode functions write k_encoded_len_none (zero) to indicate that
 * no bytes were encoded. Using a named constant avoids magic literals.
 *
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_encoded_len_none = 0U, /**< Zero bytes encoded; stubs always produce no output */
} nanopb_stub_len_t;

/**
 * @brief Stub for rx_nanopb_encode_velocity_response() -- no-op
 *
 * @details
 * No-op test stub that writes k_encoded_len_none to len (if non-NULL) and
 * returns k_rx_ok. Satisfies the linker for test targets that link
 * comm_task.c but do not exercise velocity response encoding.
 *
 * @param[in]  msg         Ignored (may be NULL)
 * @param[out] buffer      Ignored (may be NULL)
 * @param[in]  buffer_size Ignored
 * @param[out] len         Set to k_encoded_len_none if non-NULL
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post *len set to k_encoded_len_none if len is non-NULL
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_nanopb_encode_velocity_response(const star_v1_SetVelocityResponse* msg,
                                   uint8_t*  buffer, /* NOLINT(readability-non-const-parameter) */
                                   uint32_t  buffer_size,
                                   uint32_t* len)
{
  (void)msg;
  (void)buffer;
  (void)buffer_size;
  if (len != nullptr) {
    *len = k_encoded_len_none;
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_encode_estop_response() -- no-op
 *
 * @details
 * No-op test stub that writes k_encoded_len_none to len (if non-NULL) and
 * returns k_rx_ok. Satisfies the linker for test targets that link
 * comm_task.c but do not exercise e-stop response encoding.
 *
 * @param[in]  msg         Ignored (may be NULL)
 * @param[out] buffer      Ignored (may be NULL)
 * @param[in]  buffer_size Ignored
 * @param[out] len         Set to k_encoded_len_none if non-NULL
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post *len set to k_encoded_len_none if len is non-NULL
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_nanopb_encode_estop_response(const star_v1_EmergencyStopResponse* msg,
                                uint8_t*  buffer, /* NOLINT(readability-non-const-parameter) */
                                uint32_t  buffer_size,
                                uint32_t* len)
{
  (void)msg;
  (void)buffer;
  (void)buffer_size;
  if (len != nullptr) {
    *len = k_encoded_len_none;
  }
  return k_rx_ok;
}

/**
 * @brief Stub for rx_nanopb_encode_pid_gains_response() -- no-op
 *
 * @details
 * No-op test stub that writes k_encoded_len_none to len (if non-NULL) and
 * returns k_rx_ok. Satisfies the linker for test targets that link
 * comm_task.c but do not exercise PID gains response encoding.
 *
 * @param[in]  msg         Ignored (may be NULL)
 * @param[out] buffer      Ignored (may be NULL)
 * @param[in]  buffer_size Ignored
 * @param[out] len         Set to k_encoded_len_none if non-NULL
 *
 * @retval k_rx_ok Always; stub never fails
 *
 * @pre  None -- stub accepts any pointer including NULL
 * @pre  Caller context is single-threaded test execution
 * @post *len set to k_encoded_len_none if len is non-NULL
 * @post Returns k_rx_ok unconditionally
 *
 * @since Version 1.0.0
 */
rx_err_t
rx_nanopb_encode_pid_gains_response(const star_v1_SetPIDGainsResponse* msg,
                                    uint8_t*  buffer, /* NOLINT(readability-non-const-parameter) */
                                    uint32_t  buffer_size,
                                    uint32_t* len)
{
  (void)msg;
  (void)buffer;
  (void)buffer_size;
  if (len != nullptr) {
    *len = k_encoded_len_none;
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
 *
 * @pre  None -- may be called from any test context
 * @pre  Caller context is single-threaded test execution
 * @post No task mock state modified
 * @post Function returns with no observable side effects
 *
 * @since Version 1.0.0
 */
void mock_tasks_reset(void) {}
