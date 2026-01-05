/* tests/test_rx_bus_uart.c */

/**
 * @file test_rx_bus_uart.c
 * @brief Unit Tests for rx_bus_uart Bus Abstraction Layer
 *
 * Tests the UART bus abstraction layer using mocked UART HAL functions
 * and real bus manager infrastructure.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"

#include <string.h>

#include "hardware_pinout.h"
#include "mock_uart_hw.h"
#include "rx_bus_config.h"
#include "rx_bus_manager.h"
#include "rx_bus_uart.h"
#include "rx_err.h"

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

/** @brief Static bus manager for tests */
static rx_bus_manager_t s_test_manager;

/** @brief Static UART bus config for SCI9 (debug UART) */
static rx_bus_config_t s_uart_config;

/** @brief Test bus name */
static const char* s_test_bus_name = "test_uart";

/**
 * @brief Set up test fixtures before each test
 */
void setUp(void)
{
  /* Initialize mock UART hardware */
  mock_uart_hw_init();

  /* Initialize bus manager */
  rx_err_t err = rx_bus_manager_init(&s_test_manager, "TEST", NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Create UART bus config for SCI9 (PB7/TXD9, PB6/RXD9) */
  err = rx_bus_config_init_uart(&s_uart_config,
                                s_test_bus_name,
                                9,            /* SCI9 */
                                k_gpio_pb7,   /* TX: Port B, Pin 7 */
                                k_gpio_pb6,   /* RX: Port B, Pin 6 */
                                115200);      /* 115200 baud */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Add bus to manager */
  err = rx_bus_manager_add_bus(&s_test_manager, &s_uart_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

/**
 * @brief Tear down test fixtures after each test
 */
void tearDown(void)
{
  /* Deinitialize bus manager */
  rx_bus_manager_deinit(&s_test_manager);

  /* Clean up mock state */
  mock_uart_hw_deinit();
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test successful UART bus initialization
 */
void test_rx_bus_uart_init_success(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify channel was initialized */
  TEST_ASSERT_TRUE(mock_uart_hw_is_initialized(9));
  TEST_ASSERT_EQUAL(115200, mock_uart_hw_get_baudrate(9));
}

/**
 * @brief Test UART init with NULL manager
 */
void test_rx_bus_uart_init_null_manager(void)
{
  rx_err_t err = rx_bus_uart_init(NULL, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test UART init with NULL bus name
 */
void test_rx_bus_uart_init_null_bus_name(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test UART init with non-existent bus
 */
void test_rx_bus_uart_init_bus_not_found(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, "nonexistent_bus");
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test UART init with hardware error
 */
void test_rx_bus_uart_init_hw_error(void)
{
  mock_uart_hw_set_next_error(k_rx_err_hw_error);

  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_err_hw_init_failed, err);
}

/* =============================================================================
 * Write/TX Tests
 * =============================================================================
 */

/**
 * @brief Test successful UART write
 */
void test_rx_bus_uart_write_success(void)
{
  /* Initialize bus first */
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Clear TX FIFO (logging during init pollutes it) */
  mock_uart_hw_clear_tx(9);

  /* Write data */
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  err = rx_bus_uart_write(&s_test_manager, s_test_bus_name, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify data was transmitted */
  uint8_t tx_buf[16];
  uint16_t tx_count = mock_uart_hw_get_tx_data(9, tx_buf, sizeof(tx_buf));
  TEST_ASSERT_EQUAL(5, tx_count);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(data, tx_buf, 5);
}

/**
 * @brief Test UART write with NULL data pointer
 */
void test_rx_bus_uart_write_null_data(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_uart_write(&s_test_manager, s_test_bus_name, NULL, 10);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test UART write on uninitialized bus
 */
void test_rx_bus_uart_write_not_initialized(void)
{
  /* Don't initialize bus - just try to write */
  uint8_t data[] = {0x01, 0x02};
  rx_err_t err = rx_bus_uart_write(&s_test_manager, s_test_bus_name, data, sizeof(data));
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test successful UART putc
 */
void test_rx_bus_uart_putc_success(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Clear TX FIFO (logging during init pollutes it) */
  mock_uart_hw_clear_tx(9);

  err = rx_bus_uart_putc(&s_test_manager, s_test_bus_name, 'A');
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify character was transmitted */
  uint8_t tx_buf[4];
  uint16_t tx_count = mock_uart_hw_get_tx_data(9, tx_buf, sizeof(tx_buf));
  TEST_ASSERT_EQUAL(1, tx_count);
  TEST_ASSERT_EQUAL('A', tx_buf[0]);
}

/**
 * @brief Test successful UART puts
 */
void test_rx_bus_uart_puts_success(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Clear TX FIFO (logging during init pollutes it) */
  mock_uart_hw_clear_tx(9);

  err = rx_bus_uart_puts(&s_test_manager, s_test_bus_name, "Hello");
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify string was transmitted */
  uint8_t tx_buf[32];
  uint16_t tx_count = mock_uart_hw_get_tx_data(9, tx_buf, sizeof(tx_buf));
  TEST_ASSERT_EQUAL(5, tx_count);
  TEST_ASSERT_EQUAL_MEMORY("Hello", tx_buf, 5);
}

/**
 * @brief Test UART puts with newline conversion
 */
void test_rx_bus_uart_puts_newline_conversion(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Clear TX FIFO (logging during init pollutes it) */
  mock_uart_hw_clear_tx(9);

  err = rx_bus_uart_puts(&s_test_manager, s_test_bus_name, "Hi\n");
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify newline was converted to \r\n */
  uint8_t tx_buf[32];
  uint16_t tx_count = mock_uart_hw_get_tx_data(9, tx_buf, sizeof(tx_buf));
  TEST_ASSERT_EQUAL(4, tx_count); /* "Hi" + \r + \n */
  TEST_ASSERT_EQUAL('H', tx_buf[0]);
  TEST_ASSERT_EQUAL('i', tx_buf[1]);
  TEST_ASSERT_EQUAL('\r', tx_buf[2]);
  TEST_ASSERT_EQUAL('\n', tx_buf[3]);
}

/**
 * @brief Test UART puts with NULL string
 */
void test_rx_bus_uart_puts_null_string(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_uart_puts(&s_test_manager, s_test_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/* =============================================================================
 * Read/RX Tests
 * =============================================================================
 */

/**
 * @brief Test successful UART read
 */
void test_rx_bus_uart_read_success(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject RX data */
  uint8_t inject_data[] = {0x10, 0x20, 0x30};
  mock_uart_hw_inject_rx_data(9, inject_data, sizeof(inject_data));

  /* Read data */
  uint8_t rx_buf[16];
  uint16_t bytes_read = 0;
  err = rx_bus_uart_read(&s_test_manager, s_test_bus_name, rx_buf, sizeof(rx_buf), &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(3, bytes_read);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(inject_data, rx_buf, 3);
}

/**
 * @brief Test UART read with no data available
 */
void test_rx_bus_uart_read_no_data(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Don't inject any data - just try to read */
  uint8_t rx_buf[16];
  uint16_t bytes_read = 99; /* Set to non-zero to verify it gets set to 0 */
  err = rx_bus_uart_read(&s_test_manager, s_test_bus_name, rx_buf, sizeof(rx_buf), &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(0, bytes_read);
}

/**
 * @brief Test UART read partial data (less than buffer size)
 */
void test_rx_bus_uart_read_partial(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject less data than buffer can hold */
  uint8_t inject_data[] = {0xAA, 0xBB};
  mock_uart_hw_inject_rx_data(9, inject_data, sizeof(inject_data));

  /* Request more bytes than available */
  uint8_t rx_buf[100];
  uint16_t bytes_read = 0;
  err = rx_bus_uart_read(&s_test_manager, s_test_bus_name, rx_buf, sizeof(rx_buf), &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(2, bytes_read);
}

/**
 * @brief Test UART read with NULL buffer
 */
void test_rx_bus_uart_read_null_buffer(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint16_t bytes_read;
  err = rx_bus_uart_read(&s_test_manager, s_test_bus_name, NULL, 10, &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test UART read with NULL bytes_read pointer
 */
void test_rx_bus_uart_read_null_bytes_read(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  uint8_t rx_buf[16];
  err = rx_bus_uart_read(&s_test_manager, s_test_bus_name, rx_buf, sizeof(rx_buf), NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test UART read on uninitialized bus
 */
void test_rx_bus_uart_read_not_initialized(void)
{
  uint8_t rx_buf[16];
  uint16_t bytes_read;
  rx_err_t err =
      rx_bus_uart_read(&s_test_manager, s_test_bus_name, rx_buf, sizeof(rx_buf), &bytes_read);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test successful UART getc
 */
void test_rx_bus_uart_getc_success(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject a character */
  uint8_t data = 'X';
  mock_uart_hw_inject_rx_data(9, &data, 1);

  /* Read character */
  char c = '\0';
  err = rx_bus_uart_getc(&s_test_manager, s_test_bus_name, &c);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL('X', c);
}

/**
 * @brief Test UART getc with no data
 */
void test_rx_bus_uart_getc_no_data(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Don't inject any data */
  char c = 'Z';
  err = rx_bus_uart_getc(&s_test_manager, s_test_bus_name, &c);
  TEST_ASSERT_EQUAL(k_rx_err_empty, err);
}

/**
 * @brief Test UART getc with NULL pointer
 */
void test_rx_bus_uart_getc_null_pointer(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_uart_getc(&s_test_manager, s_test_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/* =============================================================================
 * RX Available Tests
 * =============================================================================
 */

/**
 * @brief Test rx_available when data is available
 */
void test_rx_bus_uart_rx_available_true(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Inject some data */
  uint8_t data = 0x42;
  mock_uart_hw_inject_rx_data(9, &data, 1);

  /* Check availability */
  bool available = false;
  err = rx_bus_uart_rx_available(&s_test_manager, s_test_bus_name, &available);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(available);
}

/**
 * @brief Test rx_available when no data is available
 */
void test_rx_bus_uart_rx_available_false(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Don't inject any data */
  bool available = true;
  err = rx_bus_uart_rx_available(&s_test_manager, s_test_bus_name, &available);
  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(available);
}

/**
 * @brief Test rx_available with NULL pointer
 */
void test_rx_bus_uart_rx_available_null_pointer(void)
{
  rx_err_t err = rx_bus_uart_init(&s_test_manager, s_test_bus_name);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_uart_rx_available(&s_test_manager, s_test_bus_name, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test rx_available on uninitialized bus
 */
void test_rx_bus_uart_rx_available_not_initialized(void)
{
  bool available;
  rx_err_t err = rx_bus_uart_rx_available(&s_test_manager, s_test_bus_name, &available);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Multi-Channel Tests
 * =============================================================================
 */

/**
 * @brief Test multiple UART channels operate independently
 */
void test_rx_bus_uart_multi_channel_isolation(void)
{
  /* Create second bus for SCI0 */
  static rx_bus_config_t uart0_config;
  rx_err_t err = rx_bus_config_init_uart(&uart0_config,
                                          "uart0",
                                          0,           /* SCI0 */
                                          k_gpio_p17,  /* TX: Port 1, Pin 7 */
                                          k_gpio_p16,  /* RX: Port 1, Pin 6 */
                                          9600);       /* 9600 baud */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_manager_add_bus(&s_test_manager, &uart0_config);
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Initialize both channels */
  err = rx_bus_uart_init(&s_test_manager, s_test_bus_name); /* SCI9 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = rx_bus_uart_init(&s_test_manager, "uart0"); /* SCI0 */
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify both are initialized with correct baud rates */
  TEST_ASSERT_TRUE(mock_uart_hw_is_initialized(9));
  TEST_ASSERT_EQUAL(115200, mock_uart_hw_get_baudrate(9));

  TEST_ASSERT_TRUE(mock_uart_hw_is_initialized(0));
  TEST_ASSERT_EQUAL(9600, mock_uart_hw_get_baudrate(0));

  /* Clear TX FIFOs after init (init logging pollutes them) */
  mock_uart_hw_clear_tx(9);
  mock_uart_hw_clear_tx(0);

  /* Write to channel 9 */
  err = rx_bus_uart_putc(&s_test_manager, s_test_bus_name, 'A');
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Write to channel 0 */
  err = rx_bus_uart_putc(&s_test_manager, "uart0", 'B');
  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify data went to correct channels */
  uint8_t tx_buf[4];
  uint16_t count;

  count = mock_uart_hw_get_tx_data(9, tx_buf, sizeof(tx_buf));
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL('A', tx_buf[0]);

  count = mock_uart_hw_get_tx_data(0, tx_buf, sizeof(tx_buf));
  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL('B', tx_buf[0]);
}

/* =============================================================================
 * Config Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test UART config init with invalid channel
 */
void test_rx_bus_config_init_uart_invalid_channel(void)
{
  rx_bus_config_t config;
  rx_err_t err = rx_bus_config_init_uart(&config, "bad_uart", 13, k_gpio_pb7, k_gpio_pb6, 115200);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test UART config init with invalid port
 *
 * @note With gpio_pin_t, invalid port values are encoded in the enum.
 *       This test uses a hand-crafted invalid gpio_pin_t value (port 0x11).
 */
void test_rx_bus_config_init_uart_invalid_port(void)
{
  rx_bus_config_t config;
  /* Create invalid gpio_pin_t: port 0x11 (beyond G), pin 0 */
  gpio_pin_t invalid_pin = (gpio_pin_t)0x1100;
  rx_err_t err = rx_bus_config_init_uart(&config, "bad_uart", 9, invalid_pin, k_gpio_pb6, 115200);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test UART config init with invalid pin
 *
 * @note With gpio_pin_t, invalid pin values are encoded in the enum.
 *       This test uses a hand-crafted invalid gpio_pin_t value (pin 8).
 */
void test_rx_bus_config_init_uart_invalid_pin(void)
{
  rx_bus_config_t config;
  /* Create invalid gpio_pin_t: port 0, pin 8 (beyond 0-7 range) */
  gpio_pin_t invalid_pin = (gpio_pin_t)0x0008;
  rx_err_t err = rx_bus_config_init_uart(&config, "bad_uart", 9, invalid_pin, k_gpio_pb6, 115200);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test UART config init with zero baud rate
 */
void test_rx_bus_config_init_uart_zero_baudrate(void)
{
  rx_bus_config_t config;
  rx_err_t err = rx_bus_config_init_uart(&config, "bad_uart", 9, k_gpio_pb7, k_gpio_pb6, 0);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test UART config init with NULL config
 */
void test_rx_bus_config_init_uart_null_config(void)
{
  rx_err_t err = rx_bus_config_init_uart(NULL, "uart", 9, k_gpio_pb7, k_gpio_pb6, 115200);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test UART config init with NULL name
 */
void test_rx_bus_config_init_uart_null_name(void)
{
  rx_bus_config_t config;
  rx_err_t err = rx_bus_config_init_uart(&config, NULL, 9, k_gpio_pb7, k_gpio_pb6, 115200);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization Tests */
  RUN_TEST(test_rx_bus_uart_init_success);
  RUN_TEST(test_rx_bus_uart_init_null_manager);
  RUN_TEST(test_rx_bus_uart_init_null_bus_name);
  RUN_TEST(test_rx_bus_uart_init_bus_not_found);
  RUN_TEST(test_rx_bus_uart_init_hw_error);

  /* Write/TX Tests */
  RUN_TEST(test_rx_bus_uart_write_success);
  RUN_TEST(test_rx_bus_uart_write_null_data);
  RUN_TEST(test_rx_bus_uart_write_not_initialized);
  RUN_TEST(test_rx_bus_uart_putc_success);
  RUN_TEST(test_rx_bus_uart_puts_success);
  RUN_TEST(test_rx_bus_uart_puts_newline_conversion);
  RUN_TEST(test_rx_bus_uart_puts_null_string);

  /* Read/RX Tests */
  RUN_TEST(test_rx_bus_uart_read_success);
  RUN_TEST(test_rx_bus_uart_read_no_data);
  RUN_TEST(test_rx_bus_uart_read_partial);
  RUN_TEST(test_rx_bus_uart_read_null_buffer);
  RUN_TEST(test_rx_bus_uart_read_null_bytes_read);
  RUN_TEST(test_rx_bus_uart_read_not_initialized);
  RUN_TEST(test_rx_bus_uart_getc_success);
  RUN_TEST(test_rx_bus_uart_getc_no_data);
  RUN_TEST(test_rx_bus_uart_getc_null_pointer);

  /* RX Available Tests */
  RUN_TEST(test_rx_bus_uart_rx_available_true);
  RUN_TEST(test_rx_bus_uart_rx_available_false);
  RUN_TEST(test_rx_bus_uart_rx_available_null_pointer);
  RUN_TEST(test_rx_bus_uart_rx_available_not_initialized);

  /* Multi-Channel Tests */
  RUN_TEST(test_rx_bus_uart_multi_channel_isolation);

  /* Config Initialization Tests */
  RUN_TEST(test_rx_bus_config_init_uart_invalid_channel);
  RUN_TEST(test_rx_bus_config_init_uart_invalid_port);
  RUN_TEST(test_rx_bus_config_init_uart_invalid_pin);
  RUN_TEST(test_rx_bus_config_init_uart_zero_baudrate);
  RUN_TEST(test_rx_bus_config_init_uart_null_config);
  RUN_TEST(test_rx_bus_config_init_uart_null_name);

  return UNITY_END();
}
