/* Test SPI bus functionality with Unity */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <string.h>

#include "star_bus_manager.h"
#include "star_bus_config.h"
#include "star_bus_types.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"
#include "star_error_interface.h"
#include "star_pin_interface.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

/* Test globals */
static star_bus_manager_t *s_manager = NULL;

void setUp(void)
{
  s_manager = malloc(sizeof(star_bus_manager_t));
  if (s_manager) {
    star_bus_manager_init(s_manager, "SPITestMgr", NULL, NULL);
  }
}

void tearDown(void)
{
  if (s_manager) {
    star_bus_manager_deinit(s_manager);
    free(s_manager);
    s_manager = NULL;
  }
  star_free_pin_validator();
}

/* ==========================================================================
 * DEVICE CONFIGURATION TESTS
 * ========================================================================== */

void test_spi_config_create_null_name(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    NULL,
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NULL(config);
}

void test_spi_config_create_null_dev_cfg(void)
{
  star_bus_config_t * config = star_bus_config_create_spi_device(
    "TestSPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    NULL
  );

  TEST_ASSERT_NULL(config);
}

void test_spi_config_create_valid_params(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "ValidSPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_EQUAL_STRING("ValidSPI", config->name);
  TEST_ASSERT_EQUAL(k_star_bus_type_spi, config->type);
  TEST_ASSERT_EQUAL(SPI2_HOST, config->proto.spi.host);
  TEST_ASSERT_EQUAL(GPIO_NUM_23, config->proto.spi.copi_pin);
  TEST_ASSERT_EQUAL(GPIO_NUM_19, config->proto.spi.cipo_pin);
  TEST_ASSERT_EQUAL(GPIO_NUM_18, config->proto.spi.sclk_pin);
  TEST_ASSERT_EQUAL(SPI_DMA_CH_AUTO, config->proto.spi.dma_channel);
  TEST_ASSERT_FALSE(config->initialized);

  star_bus_config_destroy(config);
}

void test_spi_config_invalid_host(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "InvalidHostSPI",
    (spi_host_device_t)99,  /* Invalid host */
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  /* Implementation should reject invalid host or return NULL */
  TEST_ASSERT_NULL(config);
}

void test_spi_config_invalid_dma_channel(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "InvalidDMASPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    (spi_dma_chan_t)255,  /* Invalid DMA channel */
    &dev_cfg
  );

  /* Implementation should reject invalid DMA channel or return NULL */
  TEST_ASSERT_NULL(config);
}

/* ==========================================================================
 * TRANSFER TESTS
 * ========================================================================== */

void test_spi_transmit_null_manager(void)
{
  uint8_t tx_buffer[8] = {0x01, 0x02, 0x03, 0x04};

  esp_err_t result = star_bus_manager_spi_transmit(NULL, "TestSPI", tx_buffer, sizeof(tx_buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_spi_transmit_null_name(void)
{
  uint8_t tx_buffer[8] = {0x01, 0x02, 0x03, 0x04};

  esp_err_t result = star_bus_manager_spi_transmit(s_manager, NULL, tx_buffer, sizeof(tx_buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_spi_transmit_null_buffer(void)
{
  esp_err_t result = star_bus_manager_spi_transmit(s_manager, "TestSPI", NULL, 8);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_spi_transmit_zero_length(void)
{
  uint8_t tx_buffer[8] = {0x01, 0x02, 0x03, 0x04};

  esp_err_t result = star_bus_manager_spi_transmit(s_manager, "TestSPI", tx_buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_spi_transmit_nonexistent_bus(void)
{
  uint8_t tx_buffer[8] = {0x01, 0x02, 0x03, 0x04};

  esp_err_t result = star_bus_manager_spi_transmit(s_manager, "NonexistentSPI", tx_buffer, sizeof(tx_buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, result);
}

void test_spi_receive_null_buffer(void)
{
  esp_err_t result = star_bus_manager_spi_receive(s_manager, "TestSPI", NULL, 8);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_spi_receive_zero_length(void)
{
  uint8_t rx_buffer[8];

  esp_err_t result = star_bus_manager_spi_receive(s_manager, "TestSPI", rx_buffer, 0);
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_spi_transfer_full_duplex(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "FullDuplexSPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config);
  star_bus_manager_add_bus(s_manager, config);

  uint8_t tx_buffer[8] = {0xAA, 0xBB, 0xCC, 0xDD};
  uint8_t rx_buffer[8] = {0};

  /* Transfer should fail since device is not initialized on real hardware */
  esp_err_t result = star_bus_manager_spi_transfer(s_manager, "FullDuplexSPI",
                                                    tx_buffer, rx_buffer, sizeof(tx_buffer));
  /* Expect either success (mocked) or not initialized error */
  TEST_ASSERT_TRUE(result == ESP_OK || result == ESP_ERR_INVALID_STATE);

  star_bus_manager_deinit(s_manager);
  s_manager = NULL;
}

/* ==========================================================================
 * MULTI-DEVICE TESTS
 * ========================================================================== */

void test_spi_multiple_devices_same_host(void)
{
  spi_device_interface_config_t dev_cfg1 = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  spi_device_interface_config_t dev_cfg2 = {
    .clock_speed_hz = 2000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_15,
    .queue_size = 7,
  };

  star_bus_config_t * config1 = star_bus_config_create_spi_device(
    "SPI_Device1",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg1
  );

  star_bus_config_t * config2 = star_bus_config_create_spi_device(
    "SPI_Device2",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg2
  );

  TEST_ASSERT_NOT_NULL(config1);
  TEST_ASSERT_NOT_NULL(config2);

  esp_err_t result1 = star_bus_manager_add_bus(s_manager, config1);
  esp_err_t result2 = star_bus_manager_add_bus(s_manager, config2);

  TEST_ASSERT_EQUAL(ESP_OK, result1);
  TEST_ASSERT_EQUAL(ESP_OK, result2);

  /* Verify both devices can be found */
  star_bus_config_t * found1 = star_bus_manager_find_bus(s_manager, "SPI_Device1");
  star_bus_config_t * found2 = star_bus_manager_find_bus(s_manager, "SPI_Device2");

  TEST_ASSERT_NOT_NULL(found1);
  TEST_ASSERT_NOT_NULL(found2);
  TEST_ASSERT_NOT_EQUAL(found1, found2);
}

void test_spi_devices_different_cs_pins(void)
{
  spi_device_interface_config_t dev_cfg1 = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  spi_device_interface_config_t dev_cfg2 = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_15,
    .queue_size = 7,
  };

  spi_device_interface_config_t dev_cfg3 = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_16,
    .queue_size = 7,
  };

  star_bus_config_t * config1 = star_bus_config_create_spi_device(
    "CS_Device1", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18,
    SPI_DMA_CH_AUTO, &dev_cfg1);

  star_bus_config_t * config2 = star_bus_config_create_spi_device(
    "CS_Device2", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18,
    SPI_DMA_CH_AUTO, &dev_cfg2);

  star_bus_config_t * config3 = star_bus_config_create_spi_device(
    "CS_Device3", SPI2_HOST, GPIO_NUM_23, GPIO_NUM_19, GPIO_NUM_18,
    SPI_DMA_CH_AUTO, &dev_cfg3);

  TEST_ASSERT_NOT_NULL(config1);
  TEST_ASSERT_NOT_NULL(config2);
  TEST_ASSERT_NOT_NULL(config3);

  /* Verify CS pins are different */
  TEST_ASSERT_NOT_EQUAL(config1->proto.spi.dev_cfg.spics_io_num,
                        config2->proto.spi.dev_cfg.spics_io_num);
  TEST_ASSERT_NOT_EQUAL(config2->proto.spi.dev_cfg.spics_io_num,
                        config3->proto.spi.dev_cfg.spics_io_num);

  star_bus_config_destroy(config1);
  star_bus_config_destroy(config2);
  star_bus_config_destroy(config3);
}

void test_spi_duplicate_device_name(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config1 = star_bus_config_create_spi_device(
    "DuplicateName",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  star_bus_config_t * config2 = star_bus_config_create_spi_device(
    "DuplicateName",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config1);
  TEST_ASSERT_NOT_NULL(config2);

  esp_err_t result1 = star_bus_manager_add_bus(s_manager, config1);
  TEST_ASSERT_EQUAL(ESP_OK, result1);

  /* Adding duplicate name should fail */
  esp_err_t result2 = star_bus_manager_add_bus(s_manager, config2);
  TEST_ASSERT_NOT_EQUAL(ESP_OK, result2);

  star_bus_config_destroy(config2);
}

/* ==========================================================================
 * DMA TESTS
 * ========================================================================== */

void test_spi_dma_buffer_allocation(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 10000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "DMA_Buffer_Test",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_EQUAL(SPI_DMA_CH_AUTO, config->proto.spi.dma_channel);

  star_bus_config_destroy(config);
}

void test_spi_dma_buffer_alignment(void)
{
  /* DMA buffers must be 4-byte aligned on ESP32 */
  uint8_t * dma_buffer = heap_caps_malloc(64, MALLOC_CAP_DMA);

  TEST_ASSERT_NOT_NULL(dma_buffer);
  TEST_ASSERT_EQUAL(0, ((uintptr_t)dma_buffer) % 4);  /* Check 4-byte alignment */

  heap_caps_free(dma_buffer);
}

void test_spi_dma_capability_check(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 10000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  /* Create config with DMA enabled */
  star_bus_config_t * config_dma = star_bus_config_create_spi_device(
    "DMA_Capable",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config_dma);
  TEST_ASSERT_NOT_EQUAL(SPI_DMA_DISABLED, config_dma->proto.spi.dma_channel);

  star_bus_config_destroy(config_dma);
}

void test_spi_dma_fallback_to_cpu(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  /* Create config with DMA disabled */
  star_bus_config_t * config_cpu = star_bus_config_create_spi_device(
    "CPU_Transfer",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_DISABLED,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config_cpu);
  TEST_ASSERT_EQUAL(SPI_DMA_DISABLED, config_cpu->proto.spi.dma_channel);

  star_bus_config_destroy(config_cpu);
}

/* ==========================================================================
 * ERROR HANDLING TESTS
 * ========================================================================== */

void test_spi_wrong_bus_type(void)
{
  /* Create an I2C bus and try to use SPI operations on it */
  star_bus_config_t * i2c_config = star_bus_config_create_i2c(
    "I2C_Not_SPI",
    I2C_NUM_0,
    0x50,
    GPIO_NUM_21,
    GPIO_NUM_22,
    100000
  );

  TEST_ASSERT_NOT_NULL(i2c_config);
  star_bus_manager_add_bus(s_manager, i2c_config);

  uint8_t tx_buffer[8] = {0x01, 0x02, 0x03, 0x04};

  /* Trying SPI transmit on I2C bus should fail with wrong type error */
  esp_err_t result = star_bus_manager_spi_transmit(s_manager, "I2C_Not_SPI",
                                                    tx_buffer, sizeof(tx_buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, result);
}

void test_spi_uninitialized_device(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "UninitSPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config);
  TEST_ASSERT_FALSE(config->initialized);

  star_bus_manager_add_bus(s_manager, config);

  uint8_t tx_buffer[8] = {0x01, 0x02, 0x03, 0x04};

  /* Transmit on uninitialized device should fail */
  esp_err_t result = star_bus_manager_spi_transmit(s_manager, "UninitSPI",
                                                    tx_buffer, sizeof(tx_buffer));
  TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, result);
}

void test_spi_missing_ops_function(void)
{
  spi_device_interface_config_t dev_cfg = {
    .clock_speed_hz = 1000000,
    .mode = 0,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 7,
  };

  star_bus_config_t * config = star_bus_config_create_spi_device(
    "NoOpsSPI",
    SPI2_HOST,
    GPIO_NUM_23,
    GPIO_NUM_19,
    GPIO_NUM_18,
    SPI_DMA_CH_AUTO,
    &dev_cfg
  );

  TEST_ASSERT_NOT_NULL(config);

  /* Clear ops to simulate missing function pointers */
  config->ops = NULL;

  star_bus_manager_add_bus(s_manager, config);

  uint8_t tx_buffer[8] = {0x01, 0x02, 0x03, 0x04};

  /* Should fail due to missing ops */
  esp_err_t result = star_bus_manager_spi_transmit(s_manager, "NoOpsSPI",
                                                    tx_buffer, sizeof(tx_buffer));
  TEST_ASSERT_NOT_EQUAL(ESP_OK, result);
}

/* ==========================================================================
 * TEST RUNNER
 * ========================================================================== */

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* Device Configuration Tests */
  RUN_TEST(test_spi_config_create_null_name);
  RUN_TEST(test_spi_config_create_null_dev_cfg);
  RUN_TEST(test_spi_config_create_valid_params);
  RUN_TEST(test_spi_config_invalid_host);
  RUN_TEST(test_spi_config_invalid_dma_channel);

  /* Transfer Tests */
  RUN_TEST(test_spi_transmit_null_manager);
  RUN_TEST(test_spi_transmit_null_name);
  RUN_TEST(test_spi_transmit_null_buffer);
  RUN_TEST(test_spi_transmit_zero_length);
  RUN_TEST(test_spi_transmit_nonexistent_bus);
  RUN_TEST(test_spi_receive_null_buffer);
  RUN_TEST(test_spi_receive_zero_length);
  RUN_TEST(test_spi_transfer_full_duplex);

  /* Multi-Device Tests */
  RUN_TEST(test_spi_multiple_devices_same_host);
  RUN_TEST(test_spi_devices_different_cs_pins);
  RUN_TEST(test_spi_duplicate_device_name);

  /* DMA Tests */
  RUN_TEST(test_spi_dma_buffer_allocation);
  RUN_TEST(test_spi_dma_buffer_alignment);
  RUN_TEST(test_spi_dma_capability_check);
  RUN_TEST(test_spi_dma_fallback_to_cpu);

  /* Error Handling Tests */
  RUN_TEST(test_spi_wrong_bus_type);
  RUN_TEST(test_spi_uninitialized_device);
  RUN_TEST(test_spi_missing_ops_function);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
