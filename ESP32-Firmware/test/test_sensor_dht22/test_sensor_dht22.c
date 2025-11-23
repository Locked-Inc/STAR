/* DHT22 Temperature/Humidity Sensor Tests */

#include "unity.h"
#include "esp_err.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* DHT22 constants */
#define DHT22_MIN_INTERVAL_MS       (2000)
#define DHT22_TEMP_MIN              (-40.0f)
#define DHT22_TEMP_MAX              (80.0f)
#define DHT22_HUMIDITY_MIN          (0.0f)
#define DHT22_HUMIDITY_MAX          (100.0f)
#define DHT22_GPIO_MAX              (39)

/* DHT22 error codes */
typedef enum {
    DHT22_OK = 0,
    DHT22_ERR_INVALID_ARG,
    DHT22_ERR_TIMEOUT,
    DHT22_ERR_CHECKSUM,
    DHT22_ERR_NOT_READY,
    DHT22_ERR_OUT_OF_RANGE
} dht22_err_t;

/* DHT22 handle structure */
typedef struct {
    uint8_t gpio_pin;
    float temperature;
    float humidity;
    float temp_compensation;
    uint32_t last_read_time;
    bool initialized;
    bool has_valid_reading;
} dht22_handle_t;

/* Mock state */
static struct {
    bool gpio_init_success;
    bool read_success;
    uint8_t raw_data[5];
    uint32_t current_time_ms;
    int timeout_count;
} s_s_mock_state;

static dht22_handle_t s_s_test_handle;

/* Mock functions */
static uint32_t priv_mock_get_time_ms(void) {
    return s_s_mock_state.current_time_ms;
}

static void priv_mock_set_raw_data(uint8_t rh_high, uint8_t rh_low, uint8_t t_high, uint8_t t_low) {
    s_s_mock_state.raw_data[0] = rh_high;
    s_s_mock_state.raw_data[1] = rh_low;
    s_s_mock_state.raw_data[2] = t_high;
    s_s_mock_state.raw_data[3] = t_low;
    s_s_mock_state.raw_data[4] = (rh_high + rh_low + t_high + t_low) & 0xFF;
}

static void priv_mock_set_raw_data_with_checksum(uint8_t rh_high, uint8_t rh_low,
                                            uint8_t t_high, uint8_t t_low, uint8_t checksum) {
    s_s_mock_state.raw_data[0] = rh_high;
    s_s_mock_state.raw_data[1] = rh_low;
    s_s_mock_state.raw_data[2] = t_high;
    s_s_mock_state.raw_data[3] = t_low;
    s_s_mock_state.raw_data[4] = checksum;
}

/* Simulated driver functions */
static dht22_err_t dht22_init(dht22_handle_t *handle, uint8_t gpio_pin) {
    if (handle == NULL) {
        return DHT22_ERR_INVALID_ARG;
    }
    if (gpio_pin > DHT22_GPIO_MAX) {
        return DHT22_ERR_INVALID_ARG;
    }
    if (!s_mock_state.gpio_init_success) {
        return DHT22_ERR_INVALID_ARG;
    }

    memset(handle, 0, sizeof(dht22_handle_t));
    handle->gpio_pin = gpio_pin;
    handle->temp_compensation = 0.0f;
    handle->initialized = true;
    handle->last_read_time = 0;
    handle->has_valid_reading = false;

    return DHT22_OK;
}

static dht22_err_t dht22_set_temp_compensation(dht22_handle_t *handle, float compensation) {
    if (handle == NULL || !handle->initialized) {
        return DHT22_ERR_INVALID_ARG;
    }
    handle->temp_compensation = compensation;
    return DHT22_OK;
}

static bool dht22_can_read(dht22_handle_t *handle) {
    if (handle == NULL || !handle->initialized) {
        return false;
    }
    uint32_t elapsed = priv_mock_get_time_ms() - handle->last_read_time;
    return elapsed >= DHT22_MIN_INTERVAL_MS;
}

static uint8_t priv_calculate_checksum(uint8_t *data) {
    return (data[0] + data[1] + data[2] + data[3]) & 0xFF;
}

static dht22_err_t dht22_read(dht22_handle_t *handle, float *temperature, float *humidity) {
    if (handle == NULL || !handle->initialized) {
        return DHT22_ERR_INVALID_ARG;
    }
    if (temperature == NULL || humidity == NULL) {
        return DHT22_ERR_INVALID_ARG;
    }

    /* Check minimum interval */
    if (!dht22_can_read(handle)) {
        return DHT22_ERR_NOT_READY;
    }

    /* Simulate timeout */
    if (s_mock_state.timeout_count > 0) {
        s_mock_state.timeout_count--;
        return DHT22_ERR_TIMEOUT;
    }

    /* Check read success */
    if (!s_mock_state.read_success) {
        return DHT22_ERR_TIMEOUT;
    }

    /* Verify checksum */
    uint8_t expected_checksum = priv_calculate_checksum(s_mock_state.raw_data);
    if (s_mock_state.raw_data[4] != expected_checksum) {
        return DHT22_ERR_CHECKSUM;
    }

    /* Parse humidity (0.1% resolution) */
    uint16_t raw_humidity = (s_mock_state.raw_data[0] << 8) | s_mock_state.raw_data[1];
    *humidity = raw_humidity / 10.0f;

    /* Parse temperature (0.1C resolution, bit 15 = sign) */
    uint16_t raw_temp = (s_mock_state.raw_data[2] << 8) | s_mock_state.raw_data[3];
    if (raw_temp & 0x8000) {
        *temperature = -((raw_temp & 0x7FFF) / 10.0f);
    } else {
        *temperature = raw_temp / 10.0f;
    }

    /* Apply compensation */
    *temperature += handle->temp_compensation;

    /* Validate ranges */
    if (*temperature < DHT22_TEMP_MIN || *temperature > DHT22_TEMP_MAX) {
        return DHT22_ERR_OUT_OF_RANGE;
    }
    if (*humidity < DHT22_HUMIDITY_MIN || *humidity > DHT22_HUMIDITY_MAX) {
        return DHT22_ERR_OUT_OF_RANGE;
    }

    /* Update state */
    handle->last_read_time = priv_mock_get_time_ms();
    handle->temperature = *temperature;
    handle->humidity = *humidity;
    handle->has_valid_reading = true;

    return DHT22_OK;
}

static dht22_err_t dht22_get_last_reading(dht22_handle_t *handle, float *temperature, float *humidity) {
    if (handle == NULL || !handle->initialized) {
        return DHT22_ERR_INVALID_ARG;
    }
    if (!handle->has_valid_reading) {
        return DHT22_ERR_NOT_READY;
    }
    if (temperature != NULL) *temperature = handle->temperature;
    if (humidity != NULL) *humidity = handle->humidity;
    return DHT22_OK;
}

/* Test setup/teardown */
void setUp(void) {
    memset(&s_mock_state, 0, sizeof(s_mock_state));
    memset(&s_test_handle, 0, sizeof(s_test_handle));
    s_mock_state.gpio_init_success = true;
    s_mock_state.read_success = true;
    s_mock_state.current_time_ms = 5000;
    priv_mock_set_raw_data(0x02, 0x8A, 0x01, 0x02);  /* 65.0% RH, 25.8C */
}

void tearDown(void) {
}

/* === Initialization Tests === */

void test_init_null_handle(void) {
    dht22_err_t err = dht22_init(NULL, 4);
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, err);
}

void test_init_invalid_gpio(void) {
    dht22_err_t err = dht22_init(&s_test_handle, 50);
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, err);
}

void test_init_gpio_boundary(void) {
    dht22_err_t err = dht22_init(&s_test_handle, DHT22_GPIO_MAX);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_EQUAL(DHT22_GPIO_MAX, s_test_handle.gpio_pin);
}

void test_init_success(void) {
    dht22_err_t err = dht22_init(&s_test_handle, 4);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_TRUE(s_test_handle.initialized);
    TEST_ASSERT_EQUAL(4, s_test_handle.gpio_pin);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s_test_handle.temp_compensation);
}

void test_init_gpio_failure(void) {
    s_mock_state.gpio_init_success = false;
    dht22_err_t err = dht22_init(&s_test_handle, 4);
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, err);
}

/* === Configuration Tests === */

void test_config_temp_compensation_positive(void) {
    dht22_init(&s_test_handle, 4);
    dht22_err_t err = dht22_set_temp_compensation(&s_test_handle, 2.5f);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, s_test_handle.temp_compensation);
}

void test_config_temp_compensation_negative(void) {
    dht22_init(&s_test_handle, 4);
    dht22_err_t err = dht22_set_temp_compensation(&s_test_handle, -1.5f);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.5f, s_test_handle.temp_compensation);
}

void test_config_null_handle(void) {
    dht22_err_t err = dht22_set_temp_compensation(NULL, 1.0f);
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, err);
}

void test_config_uninitialized_handle(void) {
    dht22_err_t err = dht22_set_temp_compensation(&s_test_handle, 1.0f);
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, err);
}

/* === Reading Tests === */

void test_read_timing_violation(void) {
    dht22_init(&s_test_handle, 4);

    /* First read succeeds */
    float temp, hum;
    s_test_handle.last_read_time = s_mock_state.current_time_ms - DHT22_MIN_INTERVAL_MS;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);

    /* Immediate second read fails (timing violation) */
    s_mock_state.current_time_ms += 100;  /* Only 100ms later */
    err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_NOT_READY, err);
}

void test_read_after_min_interval(void) {
    dht22_init(&s_test_handle, 4);

    float temp, hum;
    s_test_handle.last_read_time = 0;
    s_mock_state.current_time_ms = DHT22_MIN_INTERVAL_MS;

    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
}

void test_read_checksum_failure(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    priv_mock_set_raw_data_with_checksum(0x02, 0x8A, 0x01, 0x02, 0xFF);  /* Bad checksum */

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_CHECKSUM, err);
}

void test_read_out_of_range_temperature_high(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    /* Set temperature to 90C (out of range) */
    priv_mock_set_raw_data(0x02, 0x00, 0x03, 0x84);  /* 90.0C */

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_OUT_OF_RANGE, err);
}

void test_read_out_of_range_humidity(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    /* Set humidity to 110% (out of range) */
    priv_mock_set_raw_data(0x04, 0x4C, 0x00, 0xFA);  /* 110.0% */

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_OUT_OF_RANGE, err);
}

void test_read_normal_values(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    priv_mock_set_raw_data(0x02, 0x8A, 0x01, 0x02);  /* 65.0% RH, 25.8C */

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.8f, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 65.0f, hum);
}

void test_read_with_compensation(void) {
    dht22_init(&s_test_handle, 4);
    dht22_set_temp_compensation(&s_test_handle, -2.0f);
    s_test_handle.last_read_time = 0;

    priv_mock_set_raw_data(0x02, 0x8A, 0x01, 0x02);  /* 65.0% RH, 25.8C */

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 23.8f, temp);  /* 25.8 - 2.0 */
}

/* === Edge Cases === */

void test_can_read_check(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 3000;
    s_mock_state.current_time_ms = 3500;

    TEST_ASSERT_FALSE(dht22_can_read(&s_test_handle));

    s_mock_state.current_time_ms = 5000;
    TEST_ASSERT_TRUE(dht22_can_read(&s_test_handle));
}

void test_can_read_null_handle(void) {
    TEST_ASSERT_FALSE(dht22_can_read(NULL));
}

void test_cached_last_reading(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    float temp, hum;
    dht22_read(&s_test_handle, &temp, &hum);

    /* Get cached reading */
    float cached_temp, cached_hum;
    dht22_err_t err = dht22_get_last_reading(&s_test_handle, &cached_temp, &cached_hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, temp, cached_temp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, hum, cached_hum);
}

void test_cached_reading_before_first_read(void) {
    dht22_init(&s_test_handle, 4);

    float temp, hum;
    dht22_err_t err = dht22_get_last_reading(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_NOT_READY, err);
}

void test_negative_temperature(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    /* Set negative temperature: -10.5C (0x8000 | 105 = 0x8069) */
    priv_mock_set_raw_data(0x02, 0x00, 0x80, 0x69);

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -10.5f, temp);
}

void test_extreme_negative_temperature(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    /* Set -40C (minimum valid) */
    priv_mock_set_raw_data(0x02, 0x00, 0x81, 0x90);  /* 0x8000 | 400 */

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -40.0f, temp);
}

/* === Error Recovery Tests === */

void test_timeout_condition(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    s_mock_state.timeout_count = 1;

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_TIMEOUT, err);
}

void test_recovery_after_timeout(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    /* First read times out */
    s_mock_state.timeout_count = 1;
    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_TIMEOUT, err);

    /* Wait and retry succeeds */
    s_mock_state.current_time_ms += DHT22_MIN_INTERVAL_MS;
    err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
}

void test_failed_read_no_response(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    s_mock_state.read_success = false;

    float temp, hum;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_ERR_TIMEOUT, err);
}

void test_multiple_consecutive_failures(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    s_mock_state.timeout_count = 3;

    float temp, hum;
    for (int i = 0; i < 3; i++) {
        s_mock_state.current_time_ms += DHT22_MIN_INTERVAL_MS;
        dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
        TEST_ASSERT_EQUAL(DHT22_ERR_TIMEOUT, err);
    }

    /* Recovery on 4th attempt */
    s_mock_state.current_time_ms += DHT22_MIN_INTERVAL_MS;
    dht22_err_t err = dht22_read(&s_test_handle, &temp, &hum);
    TEST_ASSERT_EQUAL(DHT22_OK, err);
}

void test_read_null_params(void) {
    dht22_init(&s_test_handle, 4);
    s_test_handle.last_read_time = 0;

    float temp, hum;
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, dht22_read(&s_test_handle, NULL, &hum));
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, dht22_read(&s_test_handle, &temp, NULL));
    TEST_ASSERT_EQUAL(DHT22_ERR_INVALID_ARG, dht22_read(NULL, &temp, &hum));
}

/* Test runner */
int runUnityTests(void) {
    UNITY_BEGIN();

    /* Initialization Tests */
    RUN_TEST(test_init_null_handle);
    RUN_TEST(test_init_invalid_gpio);
    RUN_TEST(test_init_gpio_boundary);
    RUN_TEST(test_init_success);
    RUN_TEST(test_init_gpio_failure);

    /* Configuration Tests */
    RUN_TEST(test_config_temp_compensation_positive);
    RUN_TEST(test_config_temp_compensation_negative);
    RUN_TEST(test_config_null_handle);
    RUN_TEST(test_config_uninitialized_handle);

    /* Reading Tests */
    RUN_TEST(test_read_timing_violation);
    RUN_TEST(test_read_after_min_interval);
    RUN_TEST(test_read_checksum_failure);
    RUN_TEST(test_read_out_of_range_temperature_high);
    RUN_TEST(test_read_out_of_range_humidity);
    RUN_TEST(test_read_normal_values);
    RUN_TEST(test_read_with_compensation);

    /* Edge Cases */
    RUN_TEST(test_can_read_check);
    RUN_TEST(test_can_read_null_handle);
    RUN_TEST(test_cached_last_reading);
    RUN_TEST(test_cached_reading_before_first_read);
    RUN_TEST(test_negative_temperature);
    RUN_TEST(test_extreme_negative_temperature);

    /* Error Recovery Tests */
    RUN_TEST(test_timeout_condition);
    RUN_TEST(test_recovery_after_timeout);
    RUN_TEST(test_failed_read_no_response);
    RUN_TEST(test_multiple_consecutive_failures);
    RUN_TEST(test_read_null_params);

    return UNITY_END();
}

void app_main(void) {
    runUnityTests();
}
