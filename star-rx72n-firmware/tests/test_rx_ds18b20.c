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
  uint8_t rom[k_onewire_rom_bytes];
} mock_onewire_state_t;

static mock_onewire_state_t s_mock_state;

/** @brief OneWire ROM length constants */
typedef enum : uint8_t {
  k_onewire_rom_length    = k_onewire_rom_bytes,
  k_onewire_rom_crc_index = k_onewire_rom_length - 1,
} onewire_rom_constants_t;

/** @brief Mock OneWire constants */
typedef enum : uint8_t {
  k_mock_default_read_byte    = 0xFF,
  k_ds18b20_default_th_tl     = 0x00,
  k_ds18b20_reserved1_default = 0xFF,
  k_ds18b20_reserved2_default = 0x10,
  k_ds18b20_reserved3_default = 0x00,
  k_test_temp_25c_lsb         = 0x90,
  k_test_temp_25c_msb         = 0x01,
  k_test_temp_0c_lsb          = 0x00,
  k_test_temp_0c_msb          = 0x00,
  k_test_temp_minus_55c_lsb   = 0x90,
  k_test_temp_minus_55c_msb   = 0xFC,
  k_test_temp_125c_lsb        = 0xD0,
  k_test_temp_125c_msb        = 0x07,
  k_test_config_12bit         = 0x7F,
} ds18b20_mock_constants_t;

static const uint8_t s_ds18b20_family_code = 0x28U; /**< DS18B20 family code */

/** @brief ROM byte indices for default ROM population */
typedef enum : uint8_t {
  k_rom_idx_family   = 0,
  k_rom_idx_serial_0 = 1,
  k_rom_idx_serial_1 = 2,
  k_rom_idx_serial_2 = 3,
  k_rom_idx_serial_3 = 4,
  k_rom_idx_serial_4 = 5,
  k_rom_idx_serial_5 = 6,
} rom_byte_index_t;

typedef enum : uint8_t {
  k_mock_serial_byte_0 = 0x01,
  k_mock_serial_byte_1 = 0x02,
  k_mock_serial_byte_2 = 0x03,
  k_mock_serial_byte_3 = 0x04,
  k_mock_serial_byte_4 = 0x05,
  k_mock_serial_byte_5 = 0x06,
} mock_serial_bytes_t;

/** @brief Mocked DS18B20 device count returned by search */
enum {
  k_mock_ds18b20_device_count = 1
};
/** @brief Temperature comparison tolerance (°C) */
static const float s_temp_tolerance_c = 0.1F;

/* =============================================================================
 * Mock OneWire Bus Manager
 * =============================================================================
 */

static rx_bus_manager_t s_mock_bus_manager;
static const char*      s_test_bus_name = "test_onewire";

/* Mock bus_onewire functions - these replace the real implementations during testing */

/**
 * @brief Mock OneWire init
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if already init
 */
rx_err_t rx_bus_onewire_init(rx_bus_manager_t* manager, const char* bus_name)
{
  if ((manager == NULL) || (bus_name == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  s_mock_state.initialized = true;
  return k_rx_ok;
}

/**
 * @brief Mock OneWire reset
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[out] presence Presence detect flag
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_reset(rx_bus_manager_t* manager, const char* bus_name, bool* presence)
{
  if ((manager == NULL) || (bus_name == NULL) || (presence == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  *presence = s_mock_state.presence_response;
  return k_rx_ok;
}

/**
 * @brief Mock OneWire write byte
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[in] byte Byte to write (unused in mock)
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_write_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t byte)
{
  (void)byte;

  if ((manager == NULL) || (bus_name == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Mock OneWire read byte
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[out] byte Output byte
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_read_byte(rx_bus_manager_t* manager, const char* bus_name, uint8_t* byte)
{
  if ((manager == NULL) || (bus_name == NULL) || (byte == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  *byte = k_mock_default_read_byte;
  return k_rx_ok;
}

/**
 * @brief Mock OneWire read bit
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[out] bit Output bit
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_read_bit(rx_bus_manager_t* manager, const char* bus_name, bool* bit)
{
  if ((manager == NULL) || (bus_name == NULL) || (bit == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  *bit = s_mock_state.power_mode;
  return k_rx_ok;
}

/**
 * @brief Mock OneWire read block
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[out] data Output buffer
 * @param[in] length Requested length
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t
rx_bus_onewire_read(rx_bus_manager_t* manager, const char* bus_name, uint8_t* data, uint32_t length)
{
  if ((manager == NULL) || (bus_name == NULL) || (data == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (length > k_ds18b20_scratchpad_bytes) {
    length = k_ds18b20_scratchpad_bytes;
  }

  memcpy(data, s_mock_state.scratchpad, length);
  return k_rx_ok;
}

/**
 * @brief Mock OneWire write block
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[in] data Input buffer
 * @param[in] length Input length
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_write(rx_bus_manager_t* manager,
                              const char*       bus_name,
                              const uint8_t*    data,
                              uint32_t          length)
{
  if ((manager == NULL) || (bus_name == NULL) || (data == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  if (length == 0U) {
    return k_rx_ok;
  }

  return k_rx_ok;
}

/**
 * @brief Mock OneWire skip ROM
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_skip_rom(rx_bus_manager_t* manager, const char* bus_name)
{
  if ((manager == NULL) || (bus_name == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Mock OneWire match ROM
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[in] rom ROM code
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_match_rom(rx_bus_manager_t* manager,
                                  const char*       bus_name,
                                  const uint8_t     rom[k_onewire_rom_bytes])
{
  if ((manager == NULL) || (bus_name == NULL) || (rom == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Mock OneWire read ROM
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[out] rom ROM output buffer
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_read_rom(rx_bus_manager_t* manager,
                                 const char*       bus_name,
                                 uint8_t           rom[k_onewire_rom_bytes])
{
  if ((manager == NULL) || (bus_name == NULL) || (rom == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  memcpy(rom, s_mock_state.rom, k_onewire_rom_bytes);
  return k_rx_ok;
}

/**
 * @brief Mock OneWire search ROM
 *
 * @param[in] manager Bus manager instance
 * @param[in] bus_name Bus name string
 * @param[out] roms ROM output buffer
 * @param[in] max_devices Maximum devices to return
 * @param[out] num_devices Number of devices found
 * @return k_rx_ok on success, k_rx_err_null_ptr on NULL, k_rx_err_invalid_state if not init
 */
rx_err_t rx_bus_onewire_search(rx_bus_manager_t* manager,
                               const char*       bus_name,
                               uint8_t*          roms,
                               uint32_t          max_devices,
                               uint32_t*         num_devices)
{
  (void)roms;
  (void)max_devices;

  if ((manager == NULL) || (bus_name == NULL) || (num_devices == NULL)) {
    return k_rx_err_null_ptr;
  }

  if (!s_mock_state.initialized) {
    return k_rx_err_invalid_state;
  }

  *num_devices = k_mock_ds18b20_device_count;
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
 * @param[in] temp_raw Temperature raw bytes
 * @param[in] config Configuration register value
 */
typedef struct {
  uint8_t lsb; /**< Temperature LSB */
  uint8_t msb; /**< Temperature MSB */
} temp_raw_t;

static void internal_create_valid_scratchpad(uint8_t scratchpad[k_ds18b20_scratchpad_bytes],
                                             temp_raw_t temp_raw,
                                             uint8_t config)
{
  scratchpad[k_ds18b20_scratch_temp_lsb]  = temp_raw.lsb;
  scratchpad[k_ds18b20_scratch_temp_msb]  = temp_raw.msb;
  scratchpad[k_ds18b20_scratch_th_reg]    = k_ds18b20_default_th_tl;
  scratchpad[k_ds18b20_scratch_tl_reg]    = k_ds18b20_default_th_tl;
  scratchpad[k_ds18b20_scratch_config]    = config;
  scratchpad[k_ds18b20_scratch_reserved1] = k_ds18b20_reserved1_default;
  scratchpad[k_ds18b20_scratch_reserved2] = k_ds18b20_reserved2_default;
  scratchpad[k_ds18b20_scratch_reserved3] = k_ds18b20_reserved3_default;
  scratchpad[k_ds18b20_scratch_crc]       = rx_crc8_maxim(scratchpad, k_ds18b20_crc_bytes);
}

/**
 * @brief Reset mock state to defaults
 */
static void internal_reset_mock_state(void)
{
  memset(&s_mock_state, 0U, sizeof(s_mock_state));
  s_mock_state.presence_response = true;
  s_mock_state.power_mode        = true; /* External power */
  s_mock_state.initialized       = false;

  /* Default scratchpad: +25.0°C at 12-bit resolution */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_temp_25c_lsb, .msb = k_test_temp_25c_msb},
    k_test_config_12bit);

  /* Default ROM: DS18B20 family code */
  s_mock_state.rom[k_rom_idx_family]   = s_ds18b20_family_code;
  s_mock_state.rom[k_rom_idx_serial_0] = k_mock_serial_byte_0;
  s_mock_state.rom[k_rom_idx_serial_1] = k_mock_serial_byte_1;
  s_mock_state.rom[k_rom_idx_serial_2] = k_mock_serial_byte_2;
  s_mock_state.rom[k_rom_idx_serial_3] = k_mock_serial_byte_3;
  s_mock_state.rom[k_rom_idx_serial_4] = k_mock_serial_byte_4;
  s_mock_state.rom[k_rom_idx_serial_5] = k_mock_serial_byte_5;
  s_mock_state.rom[k_onewire_rom_crc_index] =
    rx_crc8_maxim(s_mock_state.rom, k_onewire_rom_crc_index);
}

/**
 * @brief Initialize handle to a known zero state
 */
static void internal_init_handle(rx_ds18b20_handle_t* handle)
{
  TEST_ASSERT_NOT_NULL(handle);
  memset(handle, 0U, sizeof(*handle));
}

/* =============================================================================
 * Unity Test Framework Setup/Teardown
 * =============================================================================
 */

void setUp(void)
{
  internal_reset_mock_state();
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

  internal_init_handle(&handle);

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

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
}

void test_ds18b20_init_null_config(void)
{
  rx_ds18b20_handle_t handle;

  internal_init_handle(&handle);

  rx_err_t err = rx_ds18b20_init(&handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

  /* Scratchpad for +25.0°C: 0x0190 = 400 decimal = 25.0°C */
  internal_create_valid_scratchpad(
    s_mock_state.scratchpad,
    (temp_raw_t){.lsb = k_test_temp_25c_lsb, .msb = k_test_temp_25c_msb},
    k_test_config_12bit);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, 25.0f, temp_c);
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

  internal_init_handle(&handle);

  /* 0°C: 0x0000 */
  internal_create_valid_scratchpad(s_mock_state.scratchpad,
                                   (temp_raw_t){.lsb = k_test_temp_0c_lsb,
                                                .msb = k_test_temp_0c_msb},
                                   k_test_config_12bit);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, 0.0f, temp_c);
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

  internal_init_handle(&handle);

  /* -55°C: 0xFC90 (two's complement) */
  internal_create_valid_scratchpad(s_mock_state.scratchpad,
                                   (temp_raw_t){.lsb = k_test_temp_minus_55c_lsb,
                                                .msb = k_test_temp_minus_55c_msb},
                                   k_test_config_12bit);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, -55.0f, temp_c);
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

  internal_init_handle(&handle);

  /* +125°C: 0x07D0 = 2000 decimal = 125.0°C */
  internal_create_valid_scratchpad(s_mock_state.scratchpad,
                                   (temp_raw_t){.lsb = k_test_temp_125c_lsb,
                                                .msb = k_test_temp_125c_msb},
                                   k_test_config_12bit);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(s_temp_tolerance_c, 125.0f, temp_c);
}

void test_ds18b20_read_temperature_not_initialized(void)
{
  rx_ds18b20_handle_t handle;
  float               temp_c = 0.0f;

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

  rx_ds18b20_init(&handle, &config);
  rx_err_t err = rx_ds18b20_read_temperature(&handle, NULL);

  TEST_ASSERT_EQUAL(k_rx_err_null_ptr, err);
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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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

  internal_init_handle(&handle);

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
