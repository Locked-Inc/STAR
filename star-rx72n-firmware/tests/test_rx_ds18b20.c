/* tests/test_rx_ds18b20.c */

/**
 * @file test_rx_ds18b20.c
 * @brief Unit Tests for DS18B20 1-Wire Temperature Sensor Driver
 *
 * Tests the DS18B20 driver with mock OneWire bus implementation.
 * Validates temperature reading, resolution control, and error handling.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#include <string.h>

#include "rx_crc.h"
#include "rx_ds18b20.h"
#include "unity.h"

/* =============================================================================
 * Mock OneWire Bus State
 * =============================================================================
 */

typedef struct {
  bool    presence_response;
  uint8_t scratchpad[k_ds18b20_scratchpad_bytes];
  uint8_t power_mode;
  bool    initialized;
  uint8_t rom[8];
} mock_onewire_state_t;

static mock_onewire_state_t s_mock_state;

/* =============================================================================
 * Mock OneWire Bus Manager
 * =============================================================================
 */

static rx_bus_manager_t s_mock_bus_manager;
static const char*      s_test_bus_name = "test_onewire";

/* Mock bus_onewire functions - these replace the real implementations during testing */

rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name)
{
  (void)manager;
  (void)bus_name;
  s_mock_state.initialized = true;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence)
{
  (void)manager;
  (void)bus_name;

  if (presence == NULL) {
    return k_rx_err_null_pointer;
  }

  *presence = s_mock_state.presence_response;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte)
{
  (void)manager;
  (void)bus_name;
  (void)byte;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte)
{
  (void)manager;
  (void)bus_name;

  if (byte == NULL) {
    return k_rx_err_null_pointer;
  }

  *byte = 0xFF;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit)
{
  (void)manager;
  (void)bus_name;

  if (bit == NULL) {
    return k_rx_err_null_pointer;
  }

  *bit = s_mock_state.power_mode;
  return k_rx_ok;
}

rx_err_t
rx_bus_onewire_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint32_t length)
{
  (void)manager;
  (void)bus_name;

  if (data == NULL) {
    return k_rx_err_null_pointer;
  }

  if (length > k_ds18b20_scratchpad_bytes) {
    length = k_ds18b20_scratchpad_bytes;
  }

  memcpy(data, s_mock_state.scratchpad, length);
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
                              const char*       bus_name,
                              const uint8_t*    data,
                              uint32_t          length)
{
  (void)manager;
  (void)bus_name;
  (void)data;
  (void)length;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name)
{
  (void)manager;
  (void)bus_name;
  return k_rx_ok;
}

rx_err_t
rx_bus_onewire_match_rom(rx_bus_manager_t* manager, const char* bus_name, const uint8_t rom[8])
{
  (void)manager;
  (void)bus_name;
  (void)rom;
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager, const char* bus_name, uint8_t rom[8])
{
  (void)manager;
  (void)bus_name;

  if (rom == NULL) {
    return k_rx_err_null_pointer;
  }

  memcpy(rom, s_mock_state.rom, 8);
  return k_rx_ok;
}

rx_err_t rx_bus_onewire_search(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               uint8_t*          roms,
                               uint32_t          max_devices,
                               uint32_t*         num_devices)
{
  (void)manager;
  (void)bus_name;
  (void)roms;
  (void)max_devices;

  if (num_devices != NULL) {
    *num_devices = 1;
  }
  return k_rx_ok;
}

/* =============================================================================
 * Test Helper Functions
 * =============================================================================
 */

/**
 * @brief Create valid scratchpad with CRC
 *
 * @param[out] scratchpad 9-byte scratchpad buffer
 * @param[in] temp_lsb Temperature LSB
 * @param[in] temp_msb Temperature MSB
 * @param[in] config Configuration register value
 */
static void create_valid_scratchpad(uint8_t scratchpad[k_ds18b20_scratchpad_bytes],
                                    uint8_t temp_lsb,
                                    uint8_t temp_msb,
                                    uint8_t config)
{
  scratchpad[k_ds18b20_scratch_temp_lsb]  = temp_lsb;
  scratchpad[k_ds18b20_scratch_temp_msb]  = temp_msb;
  scratchpad[k_ds18b20_scratch_th_reg]    = 0x00;
  scratchpad[k_ds18b20_scratch_tl_reg]    = 0x00;
  scratchpad[k_ds18b20_scratch_config]    = config;
  scratchpad[k_ds18b20_scratch_reserved1] = 0xFF;
  scratchpad[k_ds18b20_scratch_reserved2] = 0x10;
  scratchpad[k_ds18b20_scratch_reserved3] = 0x00;
  scratchpad[k_ds18b20_scratch_crc]       = rx_crc8_maxim(scratchpad, k_ds18b20_crc_bytes);
}

/**
 * @brief Reset mock state to defaults
 */
static void reset_mock_state(void)
{
  memset(&s_mock_state, 0, sizeof(s_mock_state));
  s_mock_state.presence_response = true;
  s_mock_state.power_mode        = true; /* External power */
  s_mock_state.initialized       = false;

  /* Default scratchpad: +25.0°C at 12-bit resolution */
  create_valid_scratchpad(s_mock_state.scratchpad, 0x90, 0x01, 0x7F);

  /* Default ROM: DS18B20 family code */
  s_mock_state.rom[0] = k_ds18b20_family_code;
  s_mock_state.rom[1] = 0x01;
  s_mock_state.rom[2] = 0x02;
  s_mock_state.rom[3] = 0x03;
  s_mock_state.rom[4] = 0x04;
  s_mock_state.rom[5] = 0x05;
  s_mock_state.rom[6] = 0x06;
  s_mock_state.rom[7] = rx_crc8_maxim(s_mock_state.rom, 7);
}

/* =============================================================================
 * Unity Test Framework Setup/Teardown
 * =============================================================================
 */

void setUp(void)
{
  reset_mock_state();
}

void tearDown(void)
{
  /* Nothing to tear down */
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

void test_ds18b20_init_success(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(handle.initialized);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_12bit, handle.resolution);
}

void test_ds18b20_init_null_handle(void)
{
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_err_t err = rx_ds18b20_init(NULL, &config);

  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_ds18b20_init_null_config(void)
{
  rx_ds18b20_handle_t handle;

  rx_err_t err = rx_ds18b20_init(&handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

void test_ds18b20_init_no_device_present(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  s_mock_state.presence_response = false;

  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_FALSE(handle.initialized);
}

void test_ds18b20_init_already_initialized(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_init_invalid_resolution(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = (ds18b20_resolution_t)99,
    .use_rom_matching = false,
  };

  rx_err_t err = rx_ds18b20_init(&handle, &config);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Temperature Reading Tests
 * =============================================================================
 */

void test_ds18b20_read_temperature_25c(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0f;

  /* Scratchpad for +25.0°C: 0x0190 = 400 decimal = 25.0°C */
  create_valid_scratchpad(s_mock_state.scratchpad, 0x90, 0x01, 0x7F);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, temp_c);
}

void test_ds18b20_read_temperature_0c(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0f;

  /* 0°C: 0x0000 */
  create_valid_scratchpad(s_mock_state.scratchpad, 0x00, 0x00, 0x7F);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, temp_c);
}

void test_ds18b20_read_temperature_minus_55c(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0f;

  /* -55°C: 0xFC90 (two's complement) */
  create_valid_scratchpad(s_mock_state.scratchpad, 0x90, 0xFC, 0x7F);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -55.0f, temp_c);
}

void test_ds18b20_read_temperature_125c(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  float temp_c = 0.0f;

  /* +125°C: 0x07D0 = 2000 decimal = 125.0°C */
  create_valid_scratchpad(s_mock_state.scratchpad, 0xD0, 0x07, 0x7F);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 125.0f, temp_c);
}

void test_ds18b20_read_temperature_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  float               temp_c = 0.0f;

  memset(&handle, 0, sizeof(handle));

  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

void test_ds18b20_read_temperature_null_output(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/* =============================================================================
 * Resolution Tests
 * =============================================================================
 */

void test_ds18b20_set_resolution_9bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_set_resolution(&handle, k_ds18b20_resolution_9bit);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_9bit, handle.resolution);
}

void test_ds18b20_get_resolution(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_11bit,
    .use_rom_matching = false,
  };
  ds18b20_resolution_t resolution;

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_get_resolution(&handle, &resolution);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_11bit, resolution);
}

void test_ds18b20_get_conversion_time_12bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(&handle);

  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_12bit_ms, time_ms);
}

void test_ds18b20_get_conversion_time_9bit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_9bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  uint32_t time_ms = rx_ds18b20_get_conversion_time_ms(&handle);

  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_9bit_ms, time_ms);
}

/* =============================================================================
 * Power Mode Tests
 * =============================================================================
 */

void test_ds18b20_read_power_mode_external(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = false;

  s_mock_state.power_mode = true;

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_power_mode(&handle, &external_power);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(external_power);
}

void test_ds18b20_read_power_mode_parasitic(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };
  bool external_power = true;

  s_mock_state.power_mode = false;

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_power_mode(&handle, &external_power);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(external_power);
}

/* =============================================================================
 * Conversion Tests
 * =============================================================================
 */

void test_ds18b20_trigger_conversion(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_trigger_conversion(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
}

void test_ds18b20_trigger_conversion_no_device(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  s_mock_state.presence_response = false;

  rx_err_t err = rx_ds18b20_trigger_conversion(&handle);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Deinitialization Tests
 * =============================================================================
 */

void test_ds18b20_deinit(void)
{
  rx_ds18b20_handle_t handle;
  rx_ds18b20_config_t config = {
    .bus_manager      = &s_mock_bus_manager,
    .bus_name         = s_test_bus_name,
    .resolution       = k_ds18b20_resolution_12bit,
    .use_rom_matching = false,
  };

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_deinit(&handle);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(handle.initialized);
}

/* =============================================================================
 * Unity Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_ds18b20_init_success);
  RUN_TEST(test_ds18b20_init_null_handle);
  RUN_TEST(test_ds18b20_init_null_config);
  RUN_TEST(test_ds18b20_init_no_device_present);
  RUN_TEST(test_ds18b20_init_already_initialized);
  RUN_TEST(test_ds18b20_init_invalid_resolution);

  /* Temperature reading tests */
  RUN_TEST(test_ds18b20_read_temperature_25c);
  RUN_TEST(test_ds18b20_read_temperature_0c);
  RUN_TEST(test_ds18b20_read_temperature_minus_55c);
  RUN_TEST(test_ds18b20_read_temperature_125c);
  RUN_TEST(test_ds18b20_read_temperature_not_initialized);
  RUN_TEST(test_ds18b20_read_temperature_null_output);

  /* Resolution tests */
  RUN_TEST(test_ds18b20_set_resolution_9bit);
  RUN_TEST(test_ds18b20_get_resolution);
  RUN_TEST(test_ds18b20_get_conversion_time_12bit);
  RUN_TEST(test_ds18b20_get_conversion_time_9bit);

  /* Power mode tests */
  RUN_TEST(test_ds18b20_read_power_mode_external);
  RUN_TEST(test_ds18b20_read_power_mode_parasitic);

  /* Conversion tests */
  RUN_TEST(test_ds18b20_trigger_conversion);
  RUN_TEST(test_ds18b20_trigger_conversion_no_device);

  /* Deinitialization tests */
  RUN_TEST(test_ds18b20_deinit);

  return UNITY_END();
}
