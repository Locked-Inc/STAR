/* esp32-firmware/components/pynq_wifi_bridge/test/test_transport_mock.h */

#ifndef TEST_TRANSPORT_MOCK_H
#define TEST_TRANSPORT_MOCK_H

#include <stdint.h>

/**
 * @brief Reset the transport mock state
 *
 * Call this before each test to start with a clean state.
 */
void transport_mock_reset(void);

/**
 * @brief Get the mock transport buffer
 * @return Pointer to the internal transport buffer
 */
const uint8_t* transport_mock_get_buffer(void);

/**
 * @brief Get the mock transport buffer length
 * @return Number of bytes in the transport buffer
 */
uint16_t transport_mock_get_length(void);

/**
 * @brief Set the mock transport error flag
 * @param error 1 to enable error mode, 0 to disable
 */
void transport_mock_set_error(uint8_t error);

#endif /* TEST_TRANSPORT_MOCK_H */
