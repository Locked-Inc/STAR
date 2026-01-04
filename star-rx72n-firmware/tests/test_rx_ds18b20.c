/* tests/test_rx_ds18b20.c */

/**
 * @file test_rx_ds18b20.c
 * @brief Unit Tests for DS18B20 Temperature Sensor Driver
 *
 * @details
 * Tests the DS18B20 1-Wire digital temperature sensor driver using
 * mocked bus operations (Dependency Inversion Principle).
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "unity.h"
#include "rx_ds18b20.h"
#include "rx_crc.h"
#include "mock_rx_bus_onewire.h"
#include <string.h>

/* =============================================================================
 * Test Fixtures
 * =============================================================================
 */

static rx_bus_manager_t s_bus_manager;
static rx_ds18b20_handle_t s_sensor;

void setUp(void)
{
  /* Reset mock state before each test */
  mock_onewire_reset();

  /* Clear sensor handle */
  memset(&s_sensor, 0, sizeof(s_sensor));
  memset(&s_bus_manager, 0, sizeof(s_bus_manager));
}

void tearDown(void)
{
  /* Nothing to tear down */
}

/* =============================================================================
 * Helper Functions
 * =============================================================================
 */

/**
 * @brief Create valid scratchpad with temperature and CRC
 *
 * @param[out] scratchpad 9-byte scratchpad buffer
 * @param[in] temp_lsb Temperature LSB
 * @param[in] temp_msb Temperature MSB
 * @param[in] config Configuration byte
 */
static void create_valid_scratchpad(uint8_t  scratchpad[9],
                                     uint8_t  temp_lsb,
                                     uint8_t  temp_msb,
                                     uint8_t  config)
{
  scratchpad[0] = temp_lsb;
  scratchpad[1] = temp_msb;
  scratchpad[2] = 0x4B; /* TH register */
  scratchpad[3] = 0x46; /* TL register */
  scratchpad[4] = config;
  scratchpad[5] = 0xFF; /* Reserved */
  scratchpad[6] = 0x0C; /* Reserved */
  scratchpad[7] = 0x10; /* Reserved */
  scratchpad[8] = rx_crc8_maxim(scratchpad, 8); /* Calculate CRC */
}

/**
 * @brief Create valid ROM with family code and CRC
 *
 * @param[out] rom 8-byte ROM buffer
 */
static void create_valid_rom(uint8_t rom[8])
{
  rom[0] = k_ds18b20_family_code; /* Family code */
  rom[1] = 0xFF;
  rom[2] = 0x12;
  rom[3] = 0x34;
  rom[4] = 0x56;
  rom[5] = 0x78;
  rom[6] = 0x9A;
  rom[7] = rx_crc8_maxim(rom, 7); /* Calculate CRC */
}

/* =============================================================================
 * Initialization Tests
 * =============================================================================
 */

/**
 * @brief Test DS18B20 initialization with NULL handle
 */
void test_ds18b20_init_null_handle(void)
{
  rx_err_t err = rx_ds18b20_init(NULL, &s_bus_manager, "temp_bus", NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test DS18B20 initialization with NULL bus manager
 */
void test_ds18b20_init_null_bus_manager(void)
{
  rx_err_t err = rx_ds18b20_init(&s_sensor, NULL, "temp_bus", NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test DS18B20 initialization with NULL bus name
 */
void test_ds18b20_init_null_bus_name(void)
{
  rx_err_t err = rx_ds18b20_init(&s_sensor, &s_bus_manager, NULL, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test DS18B20 initialization with no device present
 */
void test_ds18b20_init_no_device(void)
{
  mock_onewire_set_device_present(false);
  rx_err_t err = rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);
  TEST_ASSERT_EQUAL(k_rx_err_not_found, err);
}

/**
 * @brief Test DS18B20 initialization success (Skip ROM mode)
 */
void test_ds18b20_init_success_skip_rom(void)
{
  /* Setup mock: 12-bit resolution, 25.0625C */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);

  rx_err_t err = rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_sensor.initialized);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_12bit, s_sensor.resolution_bits);
  TEST_ASSERT_FALSE(s_sensor.use_rom_match);
}

/**
 * @brief Test DS18B20 initialization with specific ROM (Match ROM mode)
 */
void test_ds18b20_init_success_match_rom(void)
{
  /* Setup mock scratchpad */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x50, 0x05, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);

  /* Create ROM address */
  uint8_t rom[8];
  create_valid_rom(rom);

  rx_err_t err = rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", rom);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_sensor.initialized);
  TEST_ASSERT_TRUE(s_sensor.use_rom_match);
  TEST_ASSERT_EQUAL_UINT8(k_ds18b20_family_code, s_sensor.rom[0]);
}

/**
 * @brief Test DS18B20 initialization with wrong family code
 */
void test_ds18b20_init_wrong_family_code(void)
{
  /* Setup mock scratchpad */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x50, 0x05, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);

  /* Create ROM with wrong family code */
  uint8_t rom[8] = {0x10, 0xFF, 0x12, 0x34, 0x56, 0x78, 0x9A, 0x00}; /* DS18S20 */
  rom[7] = rx_crc8_maxim(rom, 7);

  rx_err_t err = rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", rom);

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/* =============================================================================
 * Temperature Reading Tests
 * =============================================================================
 */

/**
 * @brief Test reading temperature at 25.0625C
 */
void test_ds18b20_read_temperature_25c(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F); /* 0x0191 = 401 = 25.0625C */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0625f, temp_c);
}

/**
 * @brief Test reading temperature at 0C
 */
void test_ds18b20_read_temperature_0c(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x00, 0x00, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, temp_c);
}

/**
 * @brief Test reading temperature at -55C (minimum)
 */
void test_ds18b20_read_temperature_minus_55c(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x90, 0xFC, 0x7F); /* 0xFC90 = -880 = -55C */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -55.0f, temp_c);
}

/**
 * @brief Test reading temperature at +125C (maximum)
 */
void test_ds18b20_read_temperature_125c(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0xD0, 0x07, 0x7F); /* 0x07D0 = 2000 = 125C */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 125.0f, temp_c);
}

/**
 * @brief Test reading temperature in Fahrenheit
 */
void test_ds18b20_read_temperature_fahrenheit(void)
{
  /* Initialize sensor at 25C */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x90, 0x01, 0x7F); /* 25C */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature in Fahrenheit */
  float temp_f = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature_f(&s_sensor, &temp_f);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* 25C = 77F */
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 77.0f, temp_f);
}

/**
 * @brief Test reading temperature with uninitialized handle
 */
void test_ds18b20_read_temperature_not_initialized(void)
{
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/**
 * @brief Test reading temperature with NULL pointer
 */
void test_ds18b20_read_temperature_null_pointer(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, NULL);
  TEST_ASSERT_EQUAL(k_rx_err_null_pointer, err);
}

/**
 * @brief Test reading temperature with corrupted CRC
 */
void test_ds18b20_read_temperature_bad_crc(void)
{
  /* Initialize sensor with valid scratchpad */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Now corrupt the scratchpad for the next read */
  scratchpad[8] = 0x00; /* Corrupt CRC */
  mock_onewire_set_scratchpad(scratchpad);

  /* Try to read - should fail CRC check */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);
  TEST_ASSERT_EQUAL(k_rx_err_crc_mismatch, err);
}

/**
 * @brief Test reading temperature at 9-bit resolution (25C with undefined bits set)
 *
 * At 9-bit resolution, bits 0-2 are undefined. The driver should mask these bits.
 * Raw value with junk: 0x0197 (407) would give 25.4375C if not masked
 * Masked value: 0x0190 (400) gives correct 25.0C
 */
void test_ds18b20_read_temperature_9bit_resolution(void)
{
  /* Initialize sensor at 9-bit */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x97, 0x01, 0x1F); /* 9-bit, junk in bits 0-2 */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature - should mask bits 0-2 */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* After masking 0x0197 & 0xFFF8 = 0x0190 = 400 * 0.0625 = 25.0C */
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, temp_c);
}

/**
 * @brief Test reading temperature at 10-bit resolution (25C with undefined bits set)
 *
 * At 10-bit resolution, bits 0-1 are undefined. The driver should mask these bits.
 * Raw value with junk: 0x0193 (403) would give 25.1875C if not masked
 * Masked value: 0x0190 (400) gives correct 25.0C
 */
void test_ds18b20_read_temperature_10bit_resolution(void)
{
  /* Initialize sensor at 10-bit */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x93, 0x01, 0x3F); /* 10-bit, junk in bits 0-1 */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature - should mask bits 0-1 */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* After masking 0x0193 & 0xFFFC = 0x0190 = 400 * 0.0625 = 25.0C */
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, temp_c);
}

/**
 * @brief Test reading temperature at 11-bit resolution (25C with undefined bit set)
 *
 * At 11-bit resolution, bit 0 is undefined. The driver should mask this bit.
 * Raw value with junk: 0x0191 (401) would give 25.0625C if not masked
 * Masked value: 0x0190 (400) gives correct 25.0C
 */
void test_ds18b20_read_temperature_11bit_resolution(void)
{
  /* Initialize sensor at 11-bit */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x5F); /* 11-bit, junk in bit 0 */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Read temperature - should mask bit 0 */
  float temp_c = 0.0f;
  rx_err_t err = rx_ds18b20_read_temperature(&s_sensor, &temp_c);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  /* After masking 0x0191 & 0xFFFE = 0x0190 = 400 * 0.0625 = 25.0C */
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, temp_c);
}

/* =============================================================================
 * Resolution Configuration Tests
 * =============================================================================
 */

/**
 * @brief Test setting resolution to 9-bit
 */
void test_ds18b20_set_resolution_9bit(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Set resolution to 9-bit */
  rx_err_t err = rx_ds18b20_set_resolution(&s_sensor, k_ds18b20_resolution_9bit);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_9bit, s_sensor.resolution_bits);
}

/**
 * @brief Test setting resolution to 12-bit
 */
void test_ds18b20_set_resolution_12bit(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x1F); /* 9-bit initially */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Set resolution to 12-bit */
  rx_err_t err = rx_ds18b20_set_resolution(&s_sensor, k_ds18b20_resolution_12bit);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_12bit, s_sensor.resolution_bits);

  /* Verify write scratchpad command was sent (check write buffer) */
  uint8_t buffer[32];
  uint32_t length = 0;
  mock_onewire_get_write_buffer(buffer, &length);
  TEST_ASSERT_GREATER_THAN(0, length);

  /* Find the write scratchpad command in the buffer */
  bool found_write_cmd = false;
  for (uint32_t i = 0; i < length; ++i) {
    if (buffer[i] == k_ds18b20_cmd_write_scratchpad) {
      found_write_cmd = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(found_write_cmd);
}

/**
 * @brief Test setting invalid resolution
 */
void test_ds18b20_set_resolution_invalid(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Try to set invalid resolution */
  rx_err_t err = rx_ds18b20_set_resolution(&s_sensor, 8); /* Invalid */

  TEST_ASSERT_EQUAL(k_rx_err_invalid_arg, err);
}

/**
 * @brief Test getting resolution
 */
void test_ds18b20_get_resolution(void)
{
  /* Initialize sensor with 12-bit */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Get resolution */
  uint8_t resolution = 0;
  rx_err_t err = rx_ds18b20_get_resolution(&s_sensor, &resolution);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL(k_ds18b20_resolution_12bit, resolution);
}

/**
 * @brief Test getting conversion time for 9-bit
 */
void test_ds18b20_get_conversion_time_9bit(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x1F); /* 9-bit */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Get conversion time */
  uint32_t time_ms = 0;
  rx_err_t err = rx_ds18b20_get_conversion_time(&s_sensor, &time_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_9bit_ms, time_ms);
}

/**
 * @brief Test getting conversion time for 12-bit
 */
void test_ds18b20_get_conversion_time_12bit(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F); /* 12-bit */
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Get conversion time */
  uint32_t time_ms = 0;
  rx_err_t err = rx_ds18b20_get_conversion_time(&s_sensor, &time_ms);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(k_ds18b20_conv_time_12bit_ms, time_ms);
}

/* =============================================================================
 * Start Conversion Tests
 * =============================================================================
 */

/**
 * @brief Test starting temperature conversion
 */
void test_ds18b20_start_conversion(void)
{
  /* Initialize sensor */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Start conversion */
  rx_err_t err = rx_ds18b20_start_conversion(&s_sensor);

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  /* Verify convert command was sent */
  uint8_t cmd = 0;
  mock_onewire_get_last_command(&cmd);
  TEST_ASSERT_EQUAL_HEX8(k_ds18b20_cmd_convert_t, cmd);
}

/**
 * @brief Test starting conversion with uninitialized handle
 */
void test_ds18b20_start_conversion_not_initialized(void)
{
  rx_err_t err = rx_ds18b20_start_conversion(&s_sensor);
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
}

/* =============================================================================
 * Multi-Sensor Support Tests
 * =============================================================================
 */

/**
 * @brief Test searching for devices
 */
void test_ds18b20_search_devices(void)
{
  /* Setup mock ROM */
  uint8_t expected_rom[8];
  create_valid_rom(expected_rom);
  mock_onewire_set_rom(expected_rom);

  /* Search for devices */
  uint8_t roms[16 * 8]; /* Space for 16 devices */
  uint32_t num_devices = 0;
  rx_err_t err = rx_ds18b20_search_devices(&s_bus_manager, "temp_bus", roms, 16, &num_devices);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_UINT32(1, num_devices);
  TEST_ASSERT_EQUAL_HEX8(k_ds18b20_family_code, roms[0]);
}

/**
 * @brief Test reading ROM (single device)
 */
void test_ds18b20_read_rom(void)
{
  /* Setup mock ROM */
  uint8_t expected_rom[8];
  create_valid_rom(expected_rom);
  mock_onewire_set_rom(expected_rom);

  /* Read ROM */
  uint8_t rom[8];
  rx_err_t err = rx_ds18b20_read_rom(&s_bus_manager, "temp_bus", rom);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_rom, rom, 8);
}

/* =============================================================================
 * Power Supply Tests
 * =============================================================================
 */

/**
 * @brief Test detecting parasitic power mode
 */
void test_ds18b20_is_parasitic_power_true(void)
{
  /* Initialize sensor with parasitic power */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  mock_onewire_set_parasitic_power(true);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Check power mode */
  bool parasitic = false;
  rx_err_t err = rx_ds18b20_is_parasitic_power(&s_sensor, &parasitic);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(parasitic);
}

/**
 * @brief Test detecting external power mode
 */
void test_ds18b20_is_parasitic_power_false(void)
{
  /* Initialize sensor with external power */
  uint8_t scratchpad[9];
  create_valid_scratchpad(scratchpad, 0x91, 0x01, 0x7F);
  mock_onewire_set_scratchpad(scratchpad);
  mock_onewire_set_parasitic_power(false);
  rx_ds18b20_init(&s_sensor, &s_bus_manager, "temp_bus", NULL);

  /* Check power mode */
  bool parasitic = true;
  rx_err_t err = rx_ds18b20_is_parasitic_power(&s_sensor, &parasitic);

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_FALSE(parasitic);
}

/* =============================================================================
 * Main
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Initialization tests */
  RUN_TEST(test_ds18b20_init_null_handle);
  RUN_TEST(test_ds18b20_init_null_bus_manager);
  RUN_TEST(test_ds18b20_init_null_bus_name);
  RUN_TEST(test_ds18b20_init_no_device);
  RUN_TEST(test_ds18b20_init_success_skip_rom);
  RUN_TEST(test_ds18b20_init_success_match_rom);
  RUN_TEST(test_ds18b20_init_wrong_family_code);

  /* Temperature reading tests */
  RUN_TEST(test_ds18b20_read_temperature_25c);
  RUN_TEST(test_ds18b20_read_temperature_0c);
  RUN_TEST(test_ds18b20_read_temperature_minus_55c);
  RUN_TEST(test_ds18b20_read_temperature_125c);
  RUN_TEST(test_ds18b20_read_temperature_fahrenheit);
  RUN_TEST(test_ds18b20_read_temperature_not_initialized);
  RUN_TEST(test_ds18b20_read_temperature_null_pointer);
  RUN_TEST(test_ds18b20_read_temperature_bad_crc);
  RUN_TEST(test_ds18b20_read_temperature_9bit_resolution);
  RUN_TEST(test_ds18b20_read_temperature_10bit_resolution);
  RUN_TEST(test_ds18b20_read_temperature_11bit_resolution);

  /* Resolution configuration tests */
  RUN_TEST(test_ds18b20_set_resolution_9bit);
  RUN_TEST(test_ds18b20_set_resolution_12bit);
  RUN_TEST(test_ds18b20_set_resolution_invalid);
  RUN_TEST(test_ds18b20_get_resolution);
  RUN_TEST(test_ds18b20_get_conversion_time_9bit);
  RUN_TEST(test_ds18b20_get_conversion_time_12bit);

  /* Start conversion tests */
  RUN_TEST(test_ds18b20_start_conversion);
  RUN_TEST(test_ds18b20_start_conversion_not_initialized);

  /* Multi-sensor support tests */
  RUN_TEST(test_ds18b20_search_devices);
  RUN_TEST(test_ds18b20_read_rom);

  /* Power supply tests */
  RUN_TEST(test_ds18b20_is_parasitic_power_true);
  RUN_TEST(test_ds18b20_is_parasitic_power_false);

  return UNITY_END();
}
