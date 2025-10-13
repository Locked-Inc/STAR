/* esp32-firmware/components/pynq_wifi_bridge/test/test_transport_mock.c */

/**
 * @file test_transport_mock.c
 * @brief Shared transport mock for all protocol handler tests
 *
 * This file provides a single __wrap_transport_send implementation that can be
 * used by all test files when linker wrapping is enabled (--wrap=transport_send).
 */

#include <string.h>
#include <stdint.h>

/* Mock transport state - shared across all tests */
static uint8_t  g_transport_buffer[8192];
static uint16_t g_transport_len   = 0;
static uint8_t  g_transport_error = 0;

/**
 * @brief Mock transport_send using linker wrapping (--wrap=transport_send)
 *
 * This mock captures all transport_send calls and stores them in a buffer
 * for verification by test cases.
 */
int32_t __wrap_transport_send(const uint8_t* data, uint16_t len)
{
  if (g_transport_error) {
    return -1;
  }
  if (len > sizeof(g_transport_buffer)) {
    return -1;
  }
  memcpy(g_transport_buffer, data, len);
  g_transport_len = len;
  return (int32_t)len;
}

/**
 * @brief Reset the transport mock state
 *
 * Call this before each test to start with a clean state.
 */
void transport_mock_reset(void)
{
  memset(g_transport_buffer, 0, sizeof(g_transport_buffer));
  g_transport_len   = 0;
  g_transport_error = 0;
}

/**
 * @brief Get the mock transport buffer
 */
const uint8_t* transport_mock_get_buffer(void)
{
  return g_transport_buffer;
}

/**
 * @brief Get the mock transport buffer length
 */
uint16_t transport_mock_get_length(void)
{
  return g_transport_len;
}

/**
 * @brief Set the mock transport error flag
 */
void transport_mock_set_error(uint8_t error)
{
  g_transport_error = error;
}
