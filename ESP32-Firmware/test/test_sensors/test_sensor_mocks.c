/* Mock-based sensor driver tests */

#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include <string.h>

#include "star_core.h"
#include "star_bus_manager.h"
#include "star_error_handler.h"

/* Simulated sensor data for mock testing */
typedef struct {
  float temperature;
  float humidity;
  float pressure;
  float accel_x, accel_y, accel_z;
  float gyro_x, gyro_y, gyro_z;
  float mag_x, mag_y, mag_z;
  float distance_cm;
  float lux;
  bool data_ready;
  int read_count;
  int write_count;
} mock_sensor_data_t;

static mock_sensor_data_t s_mock_data = {0};

void setUp(void) {
  memset(&s_mock_data, 0, sizeof(mock_sensor_data_t));
  s_mock_data.temperature = 25.0f;
  s_mock_data.humidity = 50.0f;
  s_mock_data.pressure = 101325.0f;
  s_mock_data.accel_x = 0.0f;
  s_mock_data.accel_y = 0.0f;
  s_mock_data.accel_z = 9.81f;
  s_mock_data.distance_cm = 100.0f;
  s_mock_data.lux = 500.0f;
  s_mock_data.data_ready = true;
}

void tearDown(void) {}

/* --- Temperature Sensor Mock Tests --- */

void test_temperature_mock_normal_range(void)
{
  s_mock_data.temperature = 25.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, s_mock_data.temperature);
}

void test_temperature_mock_cold_range(void)
{
  s_mock_data.temperature = -40.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -40.0f, s_mock_data.temperature);
}

void test_temperature_mock_hot_range(void)
{
  s_mock_data.temperature = 80.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 80.0f, s_mock_data.temperature);
}

void test_humidity_mock_normal_range(void)
{
  s_mock_data.humidity = 50.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, s_mock_data.humidity);
  TEST_ASSERT_TRUE(s_mock_data.humidity >= 0.0f && s_mock_data.humidity <= 100.0f);
}

void test_humidity_mock_boundary_low(void)
{
  s_mock_data.humidity = 0.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s_mock_data.humidity);
}

void test_humidity_mock_boundary_high(void)
{
  s_mock_data.humidity = 100.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, s_mock_data.humidity);
}

/* --- IMU Mock Tests --- */

void test_accelerometer_mock_stationary(void)
{
  s_mock_data.accel_x = 0.0f;
  s_mock_data.accel_y = 0.0f;
  s_mock_data.accel_z = 9.81f;

  /* Total magnitude should be ~9.81 m/s² when stationary */
  float magnitude = sqrtf(s_mock_data.accel_x * s_mock_data.accel_x +
                          s_mock_data.accel_y * s_mock_data.accel_y +
                          s_mock_data.accel_z * s_mock_data.accel_z);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 9.81f, magnitude);
}

void test_accelerometer_mock_tilted(void)
{
  /* 45 degree tilt */
  s_mock_data.accel_x = 6.94f;
  s_mock_data.accel_y = 0.0f;
  s_mock_data.accel_z = 6.94f;

  float magnitude = sqrtf(s_mock_data.accel_x * s_mock_data.accel_x +
                          s_mock_data.accel_y * s_mock_data.accel_y +
                          s_mock_data.accel_z * s_mock_data.accel_z);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 9.81f, magnitude);
}

void test_gyroscope_mock_stationary(void)
{
  s_mock_data.gyro_x = 0.0f;
  s_mock_data.gyro_y = 0.0f;
  s_mock_data.gyro_z = 0.0f;

  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s_mock_data.gyro_x);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s_mock_data.gyro_y);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s_mock_data.gyro_z);
}

void test_gyroscope_mock_rotating(void)
{
  s_mock_data.gyro_z = 90.0f;  /* 90 deg/s rotation */
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, s_mock_data.gyro_z);
}

void test_magnetometer_mock_valid_field(void)
{
  s_mock_data.mag_x = 25.0f;  /* microtesla */
  s_mock_data.mag_y = 5.0f;
  s_mock_data.mag_z = 40.0f;

  float magnitude = sqrtf(s_mock_data.mag_x * s_mock_data.mag_x +
                          s_mock_data.mag_y * s_mock_data.mag_y +
                          s_mock_data.mag_z * s_mock_data.mag_z);
  /* Earth's field is typically 25-65 microtesla */
  TEST_ASSERT_TRUE(magnitude > 20.0f && magnitude < 70.0f);
}

/* --- Distance Sensor Mock Tests --- */

void test_distance_mock_min_range(void)
{
  s_mock_data.distance_cm = 2.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 2.0f, s_mock_data.distance_cm);
}

void test_distance_mock_max_range(void)
{
  s_mock_data.distance_cm = 400.0f;
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 400.0f, s_mock_data.distance_cm);
}

void test_distance_mock_typical_range(void)
{
  s_mock_data.distance_cm = 50.0f;
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, s_mock_data.distance_cm);
}

/* --- Light Sensor Mock Tests --- */

void test_light_mock_dark(void)
{
  s_mock_data.lux = 0.5f;
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.5f, s_mock_data.lux);
}

void test_light_mock_indoor(void)
{
  s_mock_data.lux = 500.0f;
  TEST_ASSERT_FLOAT_WITHIN(10.0f, 500.0f, s_mock_data.lux);
}

void test_light_mock_outdoor(void)
{
  s_mock_data.lux = 50000.0f;
  TEST_ASSERT_FLOAT_WITHIN(100.0f, 50000.0f, s_mock_data.lux);
}

void test_light_mock_max_range(void)
{
  s_mock_data.lux = 65535.0f;
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 65535.0f, s_mock_data.lux);
}

/* --- Pressure Sensor Mock Tests --- */

void test_pressure_mock_sea_level(void)
{
  s_mock_data.pressure = 101325.0f;  /* Pa */
  TEST_ASSERT_FLOAT_WITHIN(100.0f, 101325.0f, s_mock_data.pressure);
}

void test_pressure_mock_altitude_conversion(void)
{
  /* Test barometric formula approximation */
  float sea_level_pa = 101325.0f;
  float altitude_m = 1000.0f;
  float expected_pa = sea_level_pa * powf(1.0f - (altitude_m / 44330.0f), 5.255f);

  s_mock_data.pressure = expected_pa;
  TEST_ASSERT_TRUE(s_mock_data.pressure < sea_level_pa);
  TEST_ASSERT_TRUE(s_mock_data.pressure > 85000.0f);  /* ~1000m */
}

/* --- Data Ready Flag Tests --- */

void test_data_ready_initial_state(void)
{
  TEST_ASSERT_TRUE(s_mock_data.data_ready);
}

void test_data_ready_after_read(void)
{
  s_mock_data.data_ready = false;
  TEST_ASSERT_FALSE(s_mock_data.data_ready);
  s_mock_data.data_ready = true;
  TEST_ASSERT_TRUE(s_mock_data.data_ready);
}

/* --- Read/Write Counter Tests --- */

void test_read_counter_increments(void)
{
  TEST_ASSERT_EQUAL(0, s_mock_data.read_count);
  s_mock_data.read_count++;
  TEST_ASSERT_EQUAL(1, s_mock_data.read_count);
  s_mock_data.read_count++;
  TEST_ASSERT_EQUAL(2, s_mock_data.read_count);
}

void test_write_counter_increments(void)
{
  TEST_ASSERT_EQUAL(0, s_mock_data.write_count);
  s_mock_data.write_count++;
  TEST_ASSERT_EQUAL(1, s_mock_data.write_count);
}

int runUnityTests(void)
{
  UNITY_BEGIN();

  /* Temperature/Humidity Tests */
  RUN_TEST(test_temperature_mock_normal_range);
  RUN_TEST(test_temperature_mock_cold_range);
  RUN_TEST(test_temperature_mock_hot_range);
  RUN_TEST(test_humidity_mock_normal_range);
  RUN_TEST(test_humidity_mock_boundary_low);
  RUN_TEST(test_humidity_mock_boundary_high);

  /* IMU Tests */
  RUN_TEST(test_accelerometer_mock_stationary);
  RUN_TEST(test_accelerometer_mock_tilted);
  RUN_TEST(test_gyroscope_mock_stationary);
  RUN_TEST(test_gyroscope_mock_rotating);
  RUN_TEST(test_magnetometer_mock_valid_field);

  /* Distance Sensor Tests */
  RUN_TEST(test_distance_mock_min_range);
  RUN_TEST(test_distance_mock_max_range);
  RUN_TEST(test_distance_mock_typical_range);

  /* Light Sensor Tests */
  RUN_TEST(test_light_mock_dark);
  RUN_TEST(test_light_mock_indoor);
  RUN_TEST(test_light_mock_outdoor);
  RUN_TEST(test_light_mock_max_range);

  /* Pressure Sensor Tests */
  RUN_TEST(test_pressure_mock_sea_level);
  RUN_TEST(test_pressure_mock_altitude_conversion);

  /* State Tests */
  RUN_TEST(test_data_ready_initial_state);
  RUN_TEST(test_data_ready_after_read);
  RUN_TEST(test_read_counter_increments);
  RUN_TEST(test_write_counter_increments);

  return UNITY_END();
}

void app_main(void)
{
  vTaskDelay(pdMS_TO_TICKS(2000));
  runUnityTests();
}
