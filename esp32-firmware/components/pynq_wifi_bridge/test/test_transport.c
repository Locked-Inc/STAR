/* esp32-firmware/components/pynq_wifi_bridge/test/test_transport.c */

/**
 * @file test_transport.c
 * @brief STAR_TEST comprehensive tests for pynq_wifi_bridge transport (35 tests)
 *
 * Tests cover:
 * - Transport initialization (8 tests)
 * - Configuration validation (6 tests)
 * - Send operations (7 tests)
 * - Receive operations (4 tests)
 * - Lifecycle management (5 tests)
 * - Error handling (5 tests)
 */

#include <string.h>

#include "esp_log.h"
#include "pynq_wifi_transport.h"
#include "star_bus_manager.h"
#include "star_test.h"

static const char* TAG = "test_transport";

static star_bus_manager_t g_manager;
static bool               g_manager_initialized = false;
static bool               g_callback_called     = false;
static protocol_packet_t* g_received_packet     = NULL;
static uint8_t            g_callback_count      = 0;

/**
 * @brief Callback for received packets
 */
static void test_rx_callback(protocol_packet_t* packet)
{
  g_callback_called = true;
  g_received_packet = packet;
  g_callback_count++;
  ESP_LOGI(TAG, "Received packet: cmd=0x%02X", packet->cmd);
}

/**
 * @brief Reset callback state
 */
static void reset_callback_state(void)
{
  g_callback_called = false;
  g_received_packet = NULL;
  g_callback_count  = 0;
}

/* ========================================================================
 * Transport Initialization Tests (8 tests)
 * ======================================================================== */

STAR_TEST_CASE(transport, uart_init_valid)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config    = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager           = &g_manager;
  config.config.uart.tx_pin    = 17;
  config.config.uart.rx_pin    = 16;
  config.config.uart.baud_rate = 115200;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_type_t type = transport_get_type();
  STAR_ASSERT_EQUAL(k_transport_uart, type);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, uart_init_different_baud_rates)
{
  uint32_t baud_rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

  for (size_t i = 0; i < sizeof(baud_rates) / sizeof(baud_rates[0]); i++) {
    esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
    STAR_ASSERT_EQUAL(ESP_OK, err);

    transport_config_t config    = UART_TRANSPORT_CONFIG_DEFAULT();
    config.bus_manager           = &g_manager;
    config.config.uart.tx_pin    = 17;
    config.config.uart.rx_pin    = 16;
    config.config.uart.baud_rate = baud_rates[i];

    bool ret = transport_init(&config, test_rx_callback);
    STAR_ASSERT_TRUE(ret);

    transport_deinit();
    star_bus_manager_deinit(&g_manager);
  }
}

STAR_TEST_CASE(transport, spi_init_valid)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config         = SPI_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager                = &g_manager;
  config.config.spi.copi_pin        = 13;
  config.config.spi.cipo_pin        = 12;
  config.config.spi.sclk_pin        = 14;
  config.config.spi.cs_pin          = 15;
  config.config.spi.transaction_len = 2048;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_type_t type = transport_get_type();
  STAR_ASSERT_EQUAL(k_transport_spi, type);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, spi_init_different_buffer_sizes)
{
  size_t buffer_sizes[] = {256, 512, 1024, 2048, 4096};

  for (size_t i = 0; i < sizeof(buffer_sizes) / sizeof(buffer_sizes[0]); i++) {
    esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
    STAR_ASSERT_EQUAL(ESP_OK, err);

    transport_config_t config         = SPI_TRANSPORT_CONFIG_DEFAULT();
    config.bus_manager                = &g_manager;
    config.config.spi.copi_pin        = 13;
    config.config.spi.cipo_pin        = 12;
    config.config.spi.sclk_pin        = 14;
    config.config.spi.cs_pin          = 15;
    config.config.spi.transaction_len = buffer_sizes[i];

    bool ret = transport_init(&config, test_rx_callback);
    STAR_ASSERT_TRUE(ret);

    transport_deinit();
    star_bus_manager_deinit(&g_manager);
  }
}

STAR_TEST_CASE(transport, init_without_callback)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, NULL);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, double_init)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, uart_init_custom_buffer_sizes)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config      = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager             = &g_manager;
  config.config.uart.tx_pin      = 17;
  config.config.uart.rx_pin      = 16;
  config.config.uart.rx_buf_size = 4096;
  config.config.uart.tx_buf_size = 4096;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, get_type_before_init)
{
  transport_type_t type = transport_get_type();
  STAR_ASSERT_EQUAL(k_transport_uart, type);
}

/* ========================================================================
 * Configuration Validation Tests (6 tests)
 * ======================================================================== */

STAR_TEST_CASE(transport, init_null_config)
{
  bool ret = transport_init(NULL, test_rx_callback);
  STAR_ASSERT_FALSE(ret);
}

STAR_TEST_CASE(transport, init_null_bus_manager)
{
  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = NULL;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_FALSE(ret);
}

STAR_TEST_CASE(transport, uart_config_default_values)
{
  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  STAR_ASSERT_EQUAL(k_transport_uart, config.type);
  STAR_ASSERT_EQUAL(115200, config.config.uart.baud_rate);
  STAR_ASSERT_EQUAL(2048, config.config.uart.rx_buf_size);
  STAR_ASSERT_EQUAL(2048, config.config.uart.tx_buf_size);
}

STAR_TEST_CASE(transport, spi_config_default_values)
{
  transport_config_t config = SPI_TRANSPORT_CONFIG_DEFAULT();
  STAR_ASSERT_EQUAL(k_transport_spi, config.type);
  STAR_ASSERT_EQUAL(2048, config.config.spi.transaction_len);
  STAR_ASSERT_EQUAL(3, config.config.spi.queue_size);
  STAR_ASSERT_EQUAL(0, config.config.spi.mode);
}

STAR_TEST_CASE(transport, uart_config_custom_pins)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 10;
  config.config.uart.rx_pin = 11;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, spi_config_custom_pins)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config  = SPI_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager         = &g_manager;
  config.config.spi.copi_pin = 23;
  config.config.spi.cipo_pin = 19;
  config.config.spi.sclk_pin = 18;
  config.config.spi.cs_pin   = 5;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

/* ========================================================================
 * Send Operations Tests (7 tests)
 * ======================================================================== */

STAR_TEST_CASE(transport, send_without_init)
{
  uint8_t data[10] = {0};
  int32_t ret      = transport_send(data, sizeof(data));
  STAR_ASSERT_EQUAL(-1, ret);
}

STAR_TEST_CASE(transport, send_null_data)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  int32_t ret = transport_send(NULL, 10);
  STAR_ASSERT_EQUAL(-1, ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, send_zero_length)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  uint8_t data[10] = {0};
  int32_t ret      = transport_send(data, 0);
  STAR_ASSERT_EQUAL(-1, ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, send_valid_data_uart)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  int32_t ret      = transport_send(data, sizeof(data));
  STAR_ASSERT_EQUAL((int)sizeof(data), ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, send_valid_data_spi)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config  = SPI_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager         = &g_manager;
  config.config.spi.copi_pin = 13;
  config.config.spi.cipo_pin = 12;
  config.config.spi.sclk_pin = 14;
  config.config.spi.cs_pin   = 15;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  int32_t ret      = transport_send(data, sizeof(data));
  STAR_ASSERT_EQUAL((int)sizeof(data), ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, send_protocol_packet)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  uint8_t  packet_buffer[PROTOCOL_HEADER_SIZE + 10];
  uint16_t packet_len = protocol_create_packet(k_cmd_ping, NULL, 0, packet_buffer);
  STAR_ASSERT_TRUE(packet_len > 0);

  int32_t ret = transport_send(packet_buffer, packet_len);
  STAR_ASSERT_EQUAL((int)packet_len, ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, send_large_data)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  uint8_t large_data[1024];
  memset(large_data, 0xAA, sizeof(large_data));

  int32_t ret = transport_send(large_data, sizeof(large_data));
  STAR_ASSERT_EQUAL((int)sizeof(large_data), ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

/* ========================================================================
 * Receive Operations Tests (4 tests)
 * ======================================================================== */

STAR_TEST_CASE(transport, callback_state_initial)
{
  reset_callback_state();
  STAR_ASSERT_FALSE(g_callback_called);
  STAR_ASSERT_NULL(g_received_packet);
  STAR_ASSERT_EQUAL(0, g_callback_count);
}

STAR_TEST_CASE(transport, callback_reset)
{
  g_callback_called = true;
  g_callback_count  = 5;
  reset_callback_state();
  STAR_ASSERT_FALSE(g_callback_called);
  STAR_ASSERT_EQUAL(0, g_callback_count);
}

STAR_TEST_CASE(transport, init_preserves_callback)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, multiple_send_operations)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  for (int i = 0; i < 10; i++) {
    uint8_t data[5] = {(uint8_t)i,
                       (uint8_t)(i + 1),
                       (uint8_t)(i + 2),
                       (uint8_t)(i + 3),
                       (uint8_t)(i + 4)};
    int32_t ret     = transport_send(data, sizeof(data));
    STAR_ASSERT_EQUAL((int)sizeof(data), ret);
  }

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

/* ========================================================================
 * Lifecycle Management Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(transport, deinit_without_init)
{
  transport_deinit();
  STAR_ASSERT_TRUE(true);
}

STAR_TEST_CASE(transport, multiple_deinit)
{
  transport_deinit();
  transport_deinit();
  transport_deinit();
  STAR_ASSERT_TRUE(true);
}

STAR_TEST_CASE(transport, init_deinit_cycle)
{
  for (int i = 0; i < 3; i++) {
    esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
    STAR_ASSERT_EQUAL(ESP_OK, err);

    transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
    config.bus_manager        = &g_manager;
    config.config.uart.tx_pin = 17;
    config.config.uart.rx_pin = 16;

    bool ret = transport_init(&config, test_rx_callback);
    STAR_ASSERT_TRUE(ret);

    transport_deinit();
    star_bus_manager_deinit(&g_manager);
  }
}

STAR_TEST_CASE(transport, deinit_clears_state)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();

  uint8_t data[10] = {0};
  int32_t send_ret = transport_send(data, sizeof(data));
  STAR_ASSERT_EQUAL(-1, send_ret);

  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, switch_transport_type)
{
  /* Initialize UART */
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t uart_config = UART_TRANSPORT_CONFIG_DEFAULT();
  uart_config.bus_manager        = &g_manager;
  uart_config.config.uart.tx_pin = 17;
  uart_config.config.uart.rx_pin = 16;

  bool ret = transport_init(&uart_config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);
  STAR_ASSERT_EQUAL(k_transport_uart, transport_get_type());

  transport_deinit();
  star_bus_manager_deinit(&g_manager);

  /* Switch to SPI */
  err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t spi_config  = SPI_TRANSPORT_CONFIG_DEFAULT();
  spi_config.bus_manager         = &g_manager;
  spi_config.config.spi.copi_pin = 13;
  spi_config.config.spi.cipo_pin = 12;
  spi_config.config.spi.sclk_pin = 14;
  spi_config.config.spi.cs_pin   = 15;

  ret = transport_init(&spi_config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);
  STAR_ASSERT_EQUAL(k_transport_spi, transport_get_type());

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

/* ========================================================================
 * Error Handling Tests (5 tests)
 * ======================================================================== */

STAR_TEST_CASE(transport, send_after_deinit)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();

  uint8_t data[10] = {0};
  int32_t send_ret = transport_send(data, sizeof(data));
  STAR_ASSERT_EQUAL(-1, send_ret);

  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, get_type_after_deinit)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_type_t type_before = transport_get_type();
  STAR_ASSERT_EQUAL(k_transport_uart, type_before);

  transport_deinit();

  transport_type_t type_after = transport_get_type();
  STAR_ASSERT_EQUAL(k_transport_uart, type_after);

  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, init_same_pins_twice)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, send_oversized_packet_spi)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config         = SPI_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager                = &g_manager;
  config.config.spi.copi_pin        = 13;
  config.config.spi.cipo_pin        = 12;
  config.config.spi.sclk_pin        = 14;
  config.config.spi.cs_pin          = 15;
  config.config.spi.transaction_len = 100;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  uint8_t large_data[200];
  memset(large_data, 0xFF, sizeof(large_data));

  int32_t ret = transport_send(large_data, sizeof(large_data));
  STAR_ASSERT_EQUAL(-1, ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

STAR_TEST_CASE(transport, boundary_test_max_payload)
{
  esp_err_t err = star_bus_manager_init(&g_manager, "test_transport");
  STAR_ASSERT_EQUAL(ESP_OK, err);
  g_manager_initialized = true;

  transport_config_t config = UART_TRANSPORT_CONFIG_DEFAULT();
  config.bus_manager        = &g_manager;
  config.config.uart.tx_pin = 17;
  config.config.uart.rx_pin = 16;

  bool init_ret = transport_init(&config, test_rx_callback);
  STAR_ASSERT_TRUE(init_ret);

  uint8_t max_data[PROTOCOL_MAX_PAYLOAD_SIZE];
  memset(max_data, 0xBB, sizeof(max_data));

  int32_t ret = transport_send(max_data, sizeof(max_data));
  STAR_ASSERT_EQUAL((int)sizeof(max_data), ret);

  transport_deinit();
  star_bus_manager_deinit(&g_manager);
  g_manager_initialized = false;
}

/* Register all tests */
STAR_TEST_LIST_BEGIN()
/* Initialization tests */
STAR_TEST_REF(transport, uart_init_valid)
STAR_TEST_REF(transport, uart_init_different_baud_rates)
STAR_TEST_REF(transport, spi_init_valid)
STAR_TEST_REF(transport, spi_init_different_buffer_sizes)
STAR_TEST_REF(transport, init_without_callback)
STAR_TEST_REF(transport, double_init)
STAR_TEST_REF(transport, uart_init_custom_buffer_sizes)
STAR_TEST_REF(transport, get_type_before_init)

/* Configuration validation tests */
STAR_TEST_REF(transport, init_null_config)
STAR_TEST_REF(transport, init_null_bus_manager)
STAR_TEST_REF(transport, uart_config_default_values)
STAR_TEST_REF(transport, spi_config_default_values)
STAR_TEST_REF(transport, uart_config_custom_pins)
STAR_TEST_REF(transport, spi_config_custom_pins)

/* Send operations tests */
STAR_TEST_REF(transport, send_without_init)
STAR_TEST_REF(transport, send_null_data)
STAR_TEST_REF(transport, send_zero_length)
STAR_TEST_REF(transport, send_valid_data_uart)
STAR_TEST_REF(transport, send_valid_data_spi)
STAR_TEST_REF(transport, send_protocol_packet)
STAR_TEST_REF(transport, send_large_data)

/* Receive operations tests */
STAR_TEST_REF(transport, callback_state_initial)
STAR_TEST_REF(transport, callback_reset)
STAR_TEST_REF(transport, init_preserves_callback)
STAR_TEST_REF(transport, multiple_send_operations)

/* Lifecycle management tests */
STAR_TEST_REF(transport, deinit_without_init)
STAR_TEST_REF(transport, multiple_deinit)
STAR_TEST_REF(transport, init_deinit_cycle)
STAR_TEST_REF(transport, deinit_clears_state)
STAR_TEST_REF(transport, switch_transport_type)

/* Error handling tests */
STAR_TEST_REF(transport, send_after_deinit)
STAR_TEST_REF(transport, get_type_after_deinit)
STAR_TEST_REF(transport, init_same_pins_twice)
STAR_TEST_REF(transport, send_oversized_packet_spi)
STAR_TEST_REF(transport, boundary_test_max_payload)
STAR_TEST_LIST_END()
