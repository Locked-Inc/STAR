/**
 * @file test_uart_hal.c
 * @brief Unit Tests for UART HAL Driver
 *
 * Tests UART HAL functionality including:
 * - Channel initialization and deinitialization
 * - TX operations (putc, puts, write)
 * - RX operations (getc, read, rx_available)
 * - Multi-channel isolation
 * - Error handling
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "hardware.h"
#include "unity.h"

/* Port constants */
#include "rx_port_constants.h"

/* Mock includes */
#include "mock_sci_regs.h"
#include "mock_uart_hw.h"

/* =============================================================================
 * Test Constants
 * =============================================================================
 */

/** @brief Default test pins for SCI9 (PB7/TXD9, PB6/RXD9) */
static const rx_port_pin_t k_test_tx_gpio = k_rx_pb_7; /**< PB7 TX pin (from rx_port_constants.h) */
static const rx_port_pin_t k_test_rx_gpio = k_rx_pb_6; /**< PB6 RX pin (from rx_port_constants.h) */

/** @brief UART test constants */
typedef enum : uint32_t {
  k_test_channel_sci9    = 9,
  k_test_channel_sci0    = 0,
  k_test_channel_invalid = 13,
  k_test_baudrate_115200 = 115200,
  k_test_baudrate_9600   = 9600,
} test_uart_constants_t;

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

void setUp(void)
{
  mock_uart_hw_init();
  mock_sci_regs_init();
}

void tearDown(void)
{
  mock_uart_hw_deinit();
  mock_sci_regs_clear();
}

/* =============================================================================
 * Channel Initialization Tests
 * =============================================================================
 */

void test_uart_init_channel_success(void)
{
  const uart_channel_config_t config = {
    .channel  = k_test_channel_sci9,
    .baudrate = k_test_baudrate_115200,
    .tx_gpio  = k_test_tx_gpio,
    .rx_gpio  = k_test_rx_gpio,
  };
  rx_err_t err = uart_init_channel(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_uart_hw_is_initialized(k_test_channel_sci9));
  TEST_ASSERT_EQUAL(k_test_baudrate_115200, mock_uart_hw_get_baudrate(k_test_channel_sci9));
}

void test_uart_init_channel_sci0(void)
{
  const uart_channel_config_t config = {
    .channel  = k_test_channel_sci0,
    .baudrate = k_test_baudrate_9600,
    .tx_gpio  = k_rx_p0_2,
    .rx_gpio  = k_rx_p0_1,
  };
  rx_err_t err = uart_init_channel(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_uart_hw_is_initialized(k_test_channel_sci0));
  TEST_ASSERT_EQUAL(k_test_baudrate_9600, mock_uart_hw_get_baudrate(k_test_channel_sci0));
}

void test_uart_init_channel_invalid(void)
{
  const uart_channel_config_t config = {
    .channel  = k_test_channel_invalid,
    .baudrate = k_test_baudrate_115200,
    .tx_gpio  = k_test_tx_gpio,
    .rx_gpio  = k_test_rx_gpio,
  };
  rx_err_t err = uart_init_channel(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_uart_init_channel_already_initialized(void)
{
  const uart_channel_config_t config = {
    .channel  = k_test_channel_sci9,
    .baudrate = k_test_baudrate_115200,
    .tx_gpio  = k_test_tx_gpio,
    .rx_gpio  = k_test_rx_gpio,
  };
  rx_err_t err = uart_init_channel(&config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Try to initialize again */
  err = uart_init_channel(&config);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_uart_deinit_channel_success(void)
{
  uart_channel_config_t cfg = {
    .channel  = k_test_channel_sci9,
    .baudrate = k_test_baudrate_115200,
    .tx_gpio  = k_test_tx_gpio,
    .rx_gpio  = k_test_rx_gpio,
  };
  rx_err_t init_err = uart_init_channel(&cfg);
  TEST_ASSERT_EQUAL(k_rx_ok, init_err);

  rx_err_t err = uart_deinit_channel(k_test_channel_sci9);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(mock_uart_hw_is_initialized(k_test_channel_sci9));
}

void test_uart_deinit_channel_invalid(void)
{
  rx_err_t err = uart_deinit_channel(k_test_channel_invalid);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * TX Tests
 * =============================================================================
 */

void test_uart_putc_channel_success(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  rx_err_t err = uart_putc_channel(k_test_channel_sci9, 'A');
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify data was transmitted */
  uint8_t  tx_data[8];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL('A', tx_data[0]);
}

void test_uart_putc_channel_not_initialized(void)
{
  rx_err_t err = uart_putc_channel(k_test_channel_sci9, 'A');
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_uart_putc_channel_invalid_channel(void)
{
  rx_err_t err = uart_putc_channel(k_test_channel_invalid, 'A');
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

void test_uart_putc_channel_tdre_timeout(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  /* Simulate TDRE flag never becoming ready (transmit buffer always full) */
  mock_sci_set_tdre(k_test_channel_sci9, false);

  /* This should timeout after k_uart_tx_timeout iterations */
  rx_err_t err = uart_putc_channel(k_test_channel_sci9, 'A');
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

void test_uart_puts_channel_success(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  rx_err_t err = uart_puts_channel(k_test_channel_sci9, "Hello");
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify data was transmitted */
  uint8_t  tx_data[32];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(5, count);
  TEST_ASSERT_EQUAL_STRING_LEN("Hello", tx_data, 5);
}

void test_uart_puts_channel_newline_conversion(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  rx_err_t err = uart_puts_channel(k_test_channel_sci9, "Hi\n");
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify \n was converted to \r\n */
  uint8_t  tx_data[32];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(4, count); /* "Hi\r\n" */
  TEST_ASSERT_EQUAL('H', tx_data[0]);
  TEST_ASSERT_EQUAL('i', tx_data[1]);
  TEST_ASSERT_EQUAL('\r', tx_data[2]);
  TEST_ASSERT_EQUAL('\n', tx_data[3]);
}

void test_uart_puts_channel_null_string(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  rx_err_t err = uart_puts_channel(k_test_channel_sci9, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_uart_puts_channel_not_initialized(void)
{
  rx_err_t err = uart_puts_channel(k_test_channel_sci9, "Test");
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_uart_write_channel_success(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  uint8_t  data[] = {0x01, 0x02, 0x03, 0x04};
  rx_err_t err    = uart_write_channel(k_test_channel_sci9, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify data was transmitted */
  uint8_t  tx_data[8];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(4, count);
  TEST_ASSERT_EQUAL_MEMORY(data, tx_data, 4);
}

void test_uart_write_channel_null_data(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  rx_err_t err = uart_write_channel(k_test_channel_sci9, NULL, 10);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * RX Tests
 * =============================================================================
 */

void test_uart_getc_channel_success(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  /* Inject RX data */
  uint8_t rx_byte = 'X';
  mock_uart_hw_inject_rx_data(k_test_channel_sci9, &rx_byte, 1);

  /* Read the data */
  char     received = '\0';
  rx_err_t err      = uart_getc_channel(k_test_channel_sci9, &received);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL('X', received);
}

void test_uart_getc_channel_no_data(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  char     received = '\0';
  rx_err_t err      = uart_getc_channel(k_test_channel_sci9, &received);
  TEST_ASSERT_EQUAL(k_rx_err_empty, err);
}

void test_uart_getc_channel_null_buffer(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  rx_err_t err = uart_getc_channel(k_test_channel_sci9, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_uart_getc_channel_not_initialized(void)
{
  char     received = '\0';
  rx_err_t err      = uart_getc_channel(k_test_channel_sci9, &received);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_uart_read_channel_success(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  /* Inject RX data */
  uint8_t rx_data[] = {0x11, 0x22, 0x33, 0x44};
  mock_uart_hw_inject_rx_data(k_test_channel_sci9, rx_data, sizeof(rx_data));

  /* Read the data */
  uint8_t  buffer[8];
  uint16_t bytes_read;
  rx_err_t err = uart_read_channel(k_test_channel_sci9, buffer, sizeof(buffer), &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(4, bytes_read);
  TEST_ASSERT_EQUAL_MEMORY(rx_data, buffer, 4);
}

void test_uart_read_channel_partial(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  /* Inject RX data */
  uint8_t rx_data[] = {0xAA, 0xBB, 0xCC};
  mock_uart_hw_inject_rx_data(k_test_channel_sci9, rx_data, sizeof(rx_data));

  /* Read only 2 bytes */
  uint8_t  buffer[8];
  uint16_t bytes_read;
  rx_err_t err = uart_read_channel(k_test_channel_sci9, buffer, 2, &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(2, bytes_read);
  TEST_ASSERT_EQUAL(0xAA, buffer[0]);
  TEST_ASSERT_EQUAL(0xBB, buffer[1]);

  /* Remaining byte should still be available */
  TEST_ASSERT_EQUAL(1, mock_uart_hw_rx_available(k_test_channel_sci9));
}

void test_uart_read_channel_null_buffer(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  uint16_t bytes_read;
  rx_err_t err = uart_read_channel(k_test_channel_sci9, NULL, 10, &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_uart_read_channel_null_bytes_read(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  uint8_t  buffer[8];
  rx_err_t err = uart_read_channel(k_test_channel_sci9, buffer, sizeof(buffer), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_uart_rx_available_success(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  bool     available;
  rx_err_t err = uart_rx_available(k_test_channel_sci9, &available);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(available);

  /* Inject data */
  uint8_t byte = 0x55;
  mock_uart_hw_inject_rx_data(k_test_channel_sci9, &byte, 1);

  err = uart_rx_available(k_test_channel_sci9, &available);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(available);
}

void test_uart_rx_available_null(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  rx_err_t err = uart_rx_available(k_test_channel_sci9, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

/* =============================================================================
 * Multi-Channel Isolation Tests
 * =============================================================================
 */

void test_uart_channel_isolation(void)
{
  /* Initialize channel 0 with config struct */
  uart_channel_config_t cfg0 = {.channel  = k_test_channel_sci0,
                                .baudrate = k_test_baudrate_9600,
                                .tx_gpio  = k_rx_p0_2,
                                .rx_gpio  = k_rx_p0_1};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg0));

  /* Initialize channel 9 */
  uart_channel_config_t cfg9 = {.channel  = k_test_channel_sci9,
                                .baudrate = k_test_baudrate_115200,
                                .tx_gpio  = k_test_tx_gpio,
                                .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg9));

  /* Write to channel 0 */
  uart_putc_channel(k_test_channel_sci0, 'A');

  /* Write to channel 9 */
  uart_putc_channel(k_test_channel_sci9, 'B');

  /* Verify channel 0 data */
  uint8_t  tx_data[8];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci0, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL('A', tx_data[0]);

  /* Verify channel 9 data */
  count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL('B', tx_data[0]);
}

void test_uart_rx_channel_isolation(void)
{
  /* Initialize channel 0 with config struct */
  uart_channel_config_t cfg0 = {.channel  = k_test_channel_sci0,
                                .baudrate = k_test_baudrate_9600,
                                .tx_gpio  = k_rx_p0_2,
                                .rx_gpio  = k_rx_p0_1};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg0));

  /* Initialize channel 9 */
  uart_channel_config_t cfg9 = {.channel  = k_test_channel_sci9,
                                .baudrate = k_test_baudrate_115200,
                                .tx_gpio  = k_test_tx_gpio,
                                .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg9));

  /* Inject data to channel 0 */
  uint8_t data0 = 'X';
  mock_uart_hw_inject_rx_data(k_test_channel_sci0, &data0, 1);

  /* Inject data to channel 9 */
  uint8_t data9 = 'Y';
  mock_uart_hw_inject_rx_data(k_test_channel_sci9, &data9, 1);

  /* Read from channel 0 */
  char received = '\0';
  uart_getc_channel(k_test_channel_sci0, &received);
  TEST_ASSERT_EQUAL('X', received);

  /* Read from channel 9 */
  uart_getc_channel(k_test_channel_sci9, &received);
  TEST_ASSERT_EQUAL('Y', received);
}

/* =============================================================================
 * Error Injection Tests
 * =============================================================================
 */

void test_uart_init_error_injection(void)
{
  mock_uart_hw_set_next_error(k_rx_err_hw_error);
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  rx_err_t              err = uart_init_channel(&cfg);
  TEST_ASSERT_EQUAL(k_rx_err_hw_error, err);
}

void test_uart_write_error_injection(void)
{
  uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                               .baudrate = k_test_baudrate_115200,
                               .tx_gpio  = k_test_tx_gpio,
                               .rx_gpio  = k_test_rx_gpio};
  TEST_ASSERT_EQUAL(k_rx_ok, uart_init_channel(&cfg));

  mock_uart_hw_set_next_error(k_rx_err_timeout);
  uint8_t  data[] = {0x01, 0x02};
  rx_err_t err    = uart_write_channel(k_test_channel_sci9, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_err_timeout, err);
}

/* =============================================================================
 * Legacy Debug UART Tests
 * =============================================================================
 */

void test_uart_init_legacy(void)
{
  rx_err_t err = uart_init();
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(mock_uart_hw_is_initialized(k_test_channel_sci9));
  TEST_ASSERT_EQUAL(k_test_baudrate_115200, mock_uart_hw_get_baudrate(k_test_channel_sci9));
}

void test_uart_putc_legacy(void)
{
  uart_init();
  uart_putc('Z');

  uint8_t  tx_data[8];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL('Z', tx_data[0]);
}

void test_uart_puts_legacy(void)
{
  uart_init();
  uart_puts("Test");

  uint8_t  tx_data[32];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(4, count);
  TEST_ASSERT_EQUAL_STRING_LEN("Test", tx_data, 4);
}

void test_uart_putint_positive(void)
{
  uart_init();
  uart_putint(12345);

  uint8_t  tx_data[32];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(5, count);
  TEST_ASSERT_EQUAL_STRING_LEN("12345", tx_data, 5);
}

void test_uart_putint_negative(void)
{
  uart_init();
  uart_putint(-42);

  uint8_t  tx_data[32];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(3, count);
  TEST_ASSERT_EQUAL_STRING_LEN("-42", tx_data, 3);
}

void test_uart_puthex(void)
{
  uart_init();
  uart_puthex(0xABCD, 4);

  uint8_t  tx_data[32];
  uint16_t count = mock_uart_hw_get_tx_data(k_test_channel_sci9, tx_data, sizeof(tx_data));
  TEST_ASSERT_EQUAL(6, count); /* "0xABCD" */
  TEST_ASSERT_EQUAL_STRING_LEN("0xABCD", tx_data, 6);
}

/* =============================================================================
 * Call History Tests
 * =============================================================================
 */

void test_uart_call_history(void)
{
  const uart_channel_config_t cfg = {.channel  = k_test_channel_sci9,
                                     .baudrate = k_test_baudrate_115200,
                                     .tx_gpio  = k_test_tx_gpio,
                                     .rx_gpio  = k_test_rx_gpio};
  uart_init_channel(&cfg);

  uart_putc_channel(k_test_channel_sci9, 'A');
  uart_deinit_channel(k_test_channel_sci9);

  TEST_ASSERT_EQUAL(3, mock_uart_hw_get_call_count());

  const mock_uart_call_t* call = mock_uart_hw_get_call(0);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL(k_mock_uart_call_init, call->type);
  TEST_ASSERT_EQUAL(k_test_channel_sci9, call->channel);
  TEST_ASSERT_EQUAL(k_test_baudrate_115200, call->param1);

  call = mock_uart_hw_get_call(1);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL(k_mock_uart_call_putc, call->type);
  TEST_ASSERT_EQUAL(k_test_channel_sci9, call->channel);
  TEST_ASSERT_EQUAL('A', call->param1);

  call = mock_uart_hw_get_call(2);
  TEST_ASSERT_NOT_NULL(call);
  TEST_ASSERT_EQUAL(k_mock_uart_call_deinit, call->type);
  TEST_ASSERT_EQUAL(k_test_channel_sci9, call->channel);
}

/* =============================================================================
 * Main Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_uart_init_channel_success);
  RUN_TEST(test_uart_init_channel_sci0);
  RUN_TEST(test_uart_init_channel_invalid);
  RUN_TEST(test_uart_init_channel_already_initialized);
  RUN_TEST(test_uart_deinit_channel_success);
  RUN_TEST(test_uart_deinit_channel_invalid);

  /* TX tests */
  RUN_TEST(test_uart_putc_channel_success);
  RUN_TEST(test_uart_putc_channel_not_initialized);
  RUN_TEST(test_uart_putc_channel_invalid_channel);
  RUN_TEST(test_uart_putc_channel_tdre_timeout);
  RUN_TEST(test_uart_puts_channel_success);
  RUN_TEST(test_uart_puts_channel_newline_conversion);
  RUN_TEST(test_uart_puts_channel_null_string);
  RUN_TEST(test_uart_puts_channel_not_initialized);
  RUN_TEST(test_uart_write_channel_success);
  RUN_TEST(test_uart_write_channel_null_data);

  /* RX tests */
  RUN_TEST(test_uart_getc_channel_success);
  RUN_TEST(test_uart_getc_channel_no_data);
  RUN_TEST(test_uart_getc_channel_null_buffer);
  RUN_TEST(test_uart_getc_channel_not_initialized);
  RUN_TEST(test_uart_read_channel_success);
  RUN_TEST(test_uart_read_channel_partial);
  RUN_TEST(test_uart_read_channel_null_buffer);
  RUN_TEST(test_uart_read_channel_null_bytes_read);
  RUN_TEST(test_uart_rx_available_success);
  RUN_TEST(test_uart_rx_available_null);

  /* Multi-channel tests */
  RUN_TEST(test_uart_channel_isolation);
  RUN_TEST(test_uart_rx_channel_isolation);

  /* Error injection tests */
  RUN_TEST(test_uart_init_error_injection);
  RUN_TEST(test_uart_write_error_injection);

  /* Legacy debug UART tests */
  RUN_TEST(test_uart_init_legacy);
  RUN_TEST(test_uart_putc_legacy);
  RUN_TEST(test_uart_puts_legacy);
  RUN_TEST(test_uart_putint_positive);
  RUN_TEST(test_uart_putint_negative);
  RUN_TEST(test_uart_puthex);

  /* Call history tests */
  RUN_TEST(test_uart_call_history);

  return UNITY_END();
}
