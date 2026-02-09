/* tests/mocks/mock_rx_nanopb.c */

/**
 * @file mock_rx_nanopb.c
 * @brief Mock nanopb Implementation
 *
 * @details
 * Implements mock rx_nanopb functions for unit testing.
 * Tracks calls, stores parameters, and allows configurable return values.
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "mock_rx_nanopb.h"

#include <string.h>

/* =============================================================================
 * Static Variables - Return Values
 * =============================================================================
 */

/** @brief Return value for rx_nanopb_init() */
static rx_err_t s_init_return = k_rx_ok;

/** @brief Return value for rx_nanopb_decode_velocity_request() */
static rx_err_t s_decode_velocity_return = k_rx_ok;

/** @brief Return value for rx_nanopb_decode_estop_request() */
static rx_err_t s_decode_estop_return = k_rx_ok;

/** @brief Return value for rx_nanopb_encode_telemetry() */
static rx_err_t s_encode_telemetry_return = k_rx_ok;

/* =============================================================================
 * Static Variables - Call Counts
 * =============================================================================
 */

/** @brief Call count for rx_nanopb_init() */
static uint32_t s_init_count = 0;

/** @brief Call count for rx_nanopb_decode_velocity_request() */
static uint32_t s_decode_velocity_count = 0;

/** @brief Call count for rx_nanopb_decode_estop_request() */
static uint32_t s_decode_estop_count = 0;

/** @brief Call count for rx_nanopb_encode_telemetry() */
static uint32_t s_encode_telemetry_count = 0;

/* =============================================================================
 * Static Variables - Configured Output Values
 * =============================================================================
 */

/** @brief Configured velocity values for decode */
static float s_decoded_velocity[k_mock_nanopb_max_motors] = {0.0f};

/** @brief Configured encode length */
static uint32_t s_encode_length = 64;

/** @brief Last telemetry data encoded */
static star_v1_TelemetryData s_last_telemetry;

/** @brief Flag indicating telemetry was captured */
static bool s_telemetry_captured = false;

/** @brief Initialized state */
static bool s_initialized = false;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_nanopb_reset(void)
{
  uint8_t i;

  /* Reset return values to defaults */
  s_init_return             = k_rx_ok;
  s_decode_velocity_return  = k_rx_ok;
  s_decode_estop_return     = k_rx_ok;
  s_encode_telemetry_return = k_rx_ok;

  /* Reset call counts */
  s_init_count             = 0;
  s_decode_velocity_count  = 0;
  s_decode_estop_count     = 0;
  s_encode_telemetry_count = 0;

  /* Reset configured outputs */
  for (i = 0; i < k_mock_nanopb_max_motors; i++) {
    s_decoded_velocity[i] = 0.0f;
  }
  s_encode_length = 64;

  /* Reset captured telemetry */
  (void)memset(&s_last_telemetry, 0, sizeof(s_last_telemetry));
  s_telemetry_captured = false;

  /* Reset state */
  s_initialized = false;
}

void mock_nanopb_set_init_return(rx_err_t err)
{
  s_init_return = err;
}

void mock_nanopb_set_decode_velocity_return(rx_err_t err)
{
  s_decode_velocity_return = err;
}

void mock_nanopb_set_decoded_velocity(float fl, float fr, float bl, float br)
{
  s_decoded_velocity[0] = fl;
  s_decoded_velocity[1] = fr;
  s_decoded_velocity[2] = bl;
  s_decoded_velocity[3] = br;
}

void mock_nanopb_set_decode_estop_return(rx_err_t err)
{
  s_decode_estop_return = err;
}

void mock_nanopb_set_encode_telemetry_return(rx_err_t err)
{
  s_encode_telemetry_return = err;
}

void mock_nanopb_set_encode_length(uint32_t len)
{
  s_encode_length = len;
}

/* =============================================================================
 * Mock Query Functions
 * =============================================================================
 */

uint32_t mock_nanopb_get_init_count(void)
{
  return s_init_count;
}

uint32_t mock_nanopb_get_decode_velocity_count(void)
{
  return s_decode_velocity_count;
}

uint32_t mock_nanopb_get_decode_estop_count(void)
{
  return s_decode_estop_count;
}

uint32_t mock_nanopb_get_encode_telemetry_count(void)
{
  return s_encode_telemetry_count;
}

bool mock_nanopb_get_last_telemetry(star_v1_TelemetryData* out_telemetry)
{
  if (out_telemetry == NULL || !s_telemetry_captured) {
    return false;
  }

  (void)memcpy(out_telemetry, &s_last_telemetry, sizeof(*out_telemetry));
  return true;
}

bool mock_nanopb_was_initialized(void)
{
  return s_init_count > 0;
}

/* =============================================================================
 * Mock Implementation
 * =============================================================================
 */

rx_err_t rx_nanopb_init(void)
{
  s_init_count++;

  if (s_init_return == k_rx_ok) {
    s_initialized = true;
  }

  return s_init_return;
}

rx_err_t rx_nanopb_decode_velocity_request(const uint8_t*              buffer,
                                           uint32_t                    len,
                                           star_v1_SetVelocityRequest* msg)
{
  (void)buffer;
  (void)len;

  s_decode_velocity_count++;

  if (msg == NULL) {
    return k_rx_err_null_ptr;
  }

  if (s_decode_velocity_return == k_rx_ok) {
    msg->has_command                          = true;
    msg->command.front_left_velocity_mps      = (double)s_decoded_velocity[0];
    msg->command.front_right_velocity_mps     = (double)s_decoded_velocity[1];
    msg->command.back_left_velocity_mps       = (double)s_decoded_velocity[2];
    msg->command.back_right_velocity_mps      = (double)s_decoded_velocity[3];
    msg->command.sequence                     = s_decode_velocity_count;
  }

  return s_decode_velocity_return;
}

rx_err_t rx_nanopb_decode_estop_request(const uint8_t*                buffer,
                                        uint32_t                      len,
                                        star_v1_EmergencyStopRequest* msg)
{
  (void)buffer;
  (void)len;

  s_decode_estop_count++;

  if (msg == NULL) {
    return k_rx_err_null_ptr;
  }

  if (s_decode_estop_return == k_rx_ok) {
    msg->reason_present = true;
    msg->reason         = 1;  /* Manual e-stop */
  }

  return s_decode_estop_return;
}

rx_err_t rx_nanopb_encode_telemetry(const star_v1_TelemetryData* msg,
                                    uint8_t*                     buffer,
                                    uint32_t                     buffer_size,
                                    uint32_t*                    len)
{
  s_encode_telemetry_count++;

  if (msg == NULL || buffer == NULL || len == NULL) {
    return k_rx_err_null_ptr;
  }

  if (buffer_size < s_encode_length) {
    return k_rx_err_invalid_size;
  }

  if (s_encode_telemetry_return == k_rx_ok) {
    /* Capture telemetry for test verification */
    (void)memcpy(&s_last_telemetry, msg, sizeof(s_last_telemetry));
    s_telemetry_captured = true;

    /* Fill buffer with dummy data */
    (void)memset(buffer, 0xAA, s_encode_length);
    *len = s_encode_length;
  }

  return s_encode_telemetry_return;
}

void rx_nanopb_test_reset_state(void)
{
  s_initialized = false;
}
