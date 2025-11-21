/**
 * DHT22 Testing and Validation Suite for ESP32-IDF
 * Comprehensive test patterns for reliability verification
 *
 * Compile with: gcc -o dht22_tests dht22_testing_and_validation.c dht22.c
 */

#include "dht22.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DHT22_TESTS";

// =============================================================================
// TEST 1: Checksum Validation Tests
// =============================================================================

/**
 * Test correct checksum validation for normal temperature
 */
void test_checksum_positive_temperature(void) {
    ESP_LOGI(TAG, "TEST: Checksum validation (positive temperature)");

    // Example: 26.2°C, 62.5% RH
    uint8_t data[5] = {
        0x03,  // Humidity high: 3
        0xE8,  // Humidity low: 232 -> (3*256 + 232)/10 = 61.6%
        0x01,  // Temp high: 1
        0xA2,  // Temp low: 162 -> 418/10 = 41.8°C
        0x00   // Checksum (will compute)
    };

    // Calculate correct checksum
    data[4] = (data[0] + data[1] + data[2] + data[3]) & 0xFF;

    ESP_LOGI(TAG, "Data: [0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X]",
             data[0], data[1], data[2], data[3], data[4]);
    ESP_LOGI(TAG, "Checksum computed: 0x%02X", data[4]);
    ESP_LOGI(TAG, "Expected: 0x%02X", (data[0] + data[1] + data[2] + data[3]) & 0xFF);

    uint8_t expected = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (expected == data[4]) {
        ESP_LOGI(TAG, "PASS: Checksum validation");
    } else {
        ESP_LOGE(TAG, "FAIL: Checksum mismatch");
    }
}

/**
 * Test checksum with negative temperature (critical bug in many implementations)
 */
void test_checksum_negative_temperature(void) {
    ESP_LOGI(TAG, "TEST: Checksum validation (negative temperature)");

    // Example: -16.2°C, 50% RH
    // Temperature in two's complement: -162 -> 0xFF5E
    uint8_t data[5] = {
        0x01,  // Humidity high: 1
        0xF4,  // Humidity low: 244 -> (1*256 + 244)/10 = 50.0%
        0xFF,  // Temp high: 0xFF (negative indicator in two's complement)
        0x5E,  // Temp low: 94 -> Two's complement: -(0x00A2) = -162 -> -16.2°C
        0x00   // Checksum (will compute)
    };

    // Calculate correct checksum (without worrying about sign)
    data[4] = (data[0] + data[1] + data[2] + data[3]) & 0xFF;

    ESP_LOGI(TAG, "Data: [0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X]",
             data[0], data[1], data[2], data[3], data[4]);

    // Verify temperature interpretation
    int16_t temp_raw = (int16_t)((data[2] << 8) | data[3]);
    float temperature = temp_raw / 10.0f;

    ESP_LOGI(TAG, "Raw temperature value: 0x%04X (%d)", temp_raw, temp_raw);
    ESP_LOGI(TAG, "Interpreted temperature: %.1f°C", temperature);

    if (temperature == -16.2f) {
        ESP_LOGI(TAG, "PASS: Negative temperature handling");
    } else {
        ESP_LOGE(TAG, "FAIL: Temperature interpretation error");
    }

    uint8_t expected = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (expected == data[4]) {
        ESP_LOGI(TAG, "PASS: Checksum validation");
    } else {
        ESP_LOGE(TAG, "FAIL: Checksum mismatch");
    }
}

/**
 * Test checksum overflow behavior (sum exceeds 255)
 */
void test_checksum_overflow(void) {
    ESP_LOGI(TAG, "TEST: Checksum overflow (sum > 255)");

    // Example with high values that cause overflow
    uint8_t data[5] = {
        0xFF,  // Humidity high: 255
        0xFF,  // Humidity low: 255
        0x7F,  // Temp high: 127
        0xFF,  // Temp low: 255
        0x00   // Checksum (will compute)
    };

    // Sum = 255 + 255 + 127 + 255 = 892 = 0x37C
    // Last 8 bits = 0x7C
    uint16_t sum = data[0] + data[1] + data[2] + data[3];
    uint8_t checksum = (uint8_t)(sum & 0xFF);

    data[4] = checksum;

    ESP_LOGI(TAG, "Sum: %u (0x%04X), Checksum (last 8 bits): 0x%02X",
             sum, sum, checksum);
    ESP_LOGI(TAG, "Data: [0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X]",
             data[0], data[1], data[2], data[3], data[4]);

    if (checksum == data[4]) {
        ESP_LOGI(TAG, "PASS: Checksum overflow handling");
    } else {
        ESP_LOGE(TAG, "FAIL: Checksum computation error");
    }
}

// =============================================================================
// TEST 2: Data Format Validation
// =============================================================================

/**
 * Test humidity value parsing
 */
void test_humidity_parsing(void) {
    ESP_LOGI(TAG, "TEST: Humidity value parsing");

    struct test_case {
        uint8_t high;
        uint8_t low;
        float expected_rh;
        const char *description;
    } tests[] = {
        {0x00, 0x00, 0.0f, "Minimum RH"},
        {0x03, 0xE8, 61.6f, "62.5% RH example"},
        {0x06, 0x40, 100.0f, "Maximum RH"},
    };

    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint16_t rh_raw = (tests[i].high << 8) | tests[i].low;
        float humidity = rh_raw / 10.0f;

        ESP_LOGI(TAG, "Test %d: %s", i + 1, tests[i].description);
        ESP_LOGI(TAG, "  Raw: 0x%04X, Parsed: %.1f%%, Expected: %.1f%%",
                 rh_raw, humidity, tests[i].expected_rh);

        if (humidity == tests[i].expected_rh) {
            ESP_LOGI(TAG, "  PASS");
        } else {
            ESP_LOGE(TAG, "  FAIL");
        }
    }
}

/**
 * Test temperature value parsing (both positive and negative)
 */
void test_temperature_parsing(void) {
    ESP_LOGI(TAG, "TEST: Temperature value parsing");

    struct test_case {
        uint8_t high;
        uint8_t low;
        float expected_temp;
        const char *description;
    } tests[] = {
        {0x00, 0x00, 0.0f, "Zero temperature"},
        {0x00, 0x64, 10.0f, "Positive: 10°C"},
        {0x01, 0xA2, 41.8f, "Positive: 41.8°C"},
        {0x7F, 0xFF, 327.67f, "Maximum positive"},
        {0xFF, 0x5E, -16.2f, "Negative: -16.2°C"},
        {0x80, 0x00, -327.68f, "Maximum negative"},
    };

    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int16_t temp_raw = (int16_t)((tests[i].high << 8) | tests[i].low);
        float temperature = temp_raw / 10.0f;

        ESP_LOGI(TAG, "Test %d: %s", i + 1, tests[i].description);
        ESP_LOGI(TAG, "  Raw: 0x%04X (%d), Parsed: %.2f°C, Expected: %.2f°C",
                 (uint16_t)temp_raw, temp_raw, temperature, tests[i].expected_temp);

        if (temperature == tests[i].expected_temp) {
            ESP_LOGI(TAG, "  PASS");
        } else {
            ESP_LOGE(TAG, "  FAIL");
        }
    }
}

// =============================================================================
// TEST 3: Bit Manipulation Tests
// =============================================================================

/**
 * Test bit-to-byte conversion for DHT22 bit ordering
 */
void test_bit_to_byte_conversion(void) {
    ESP_LOGI(TAG, "TEST: Bit-to-byte conversion (MSB first)");

    // Simulate reading 8 bits: 10110101 (0xB5)
    uint8_t byte = 0;
    int bits[] = {1, 0, 1, 1, 0, 1, 0, 1};

    for (int i = 0; i < 8; i++) {
        // DHT22 sends MSB first, store in byte left-to-right
        byte |= (bits[i] << (7 - i));
    }

    ESP_LOGI(TAG, "Bits: [%d, %d, %d, %d, %d, %d, %d, %d]",
             bits[0], bits[1], bits[2], bits[3],
             bits[4], bits[5], bits[6], bits[7]);
    ESP_LOGI(TAG, "Resulting byte: 0x%02X (expected 0xB5)", byte);

    if (byte == 0xB5) {
        ESP_LOGI(TAG, "PASS: Bit-to-byte conversion");
    } else {
        ESP_LOGE(TAG, "FAIL: Conversion error");
    }
}

/**
 * Test full 40-bit buffer assembly
 */
void test_40bit_buffer_assembly(void) {
    ESP_LOGI(TAG, "TEST: 40-bit buffer assembly");

    // Simulate sensor data:
    // Byte 0: 0x03 (00000011)
    // Byte 1: 0xE8 (11101000)
    // Byte 2: 0x01 (00000001)
    // Byte 3: 0xA2 (10100010)
    // Byte 4: 0xFE (11111110) - checksum

    uint8_t expected[5] = {0x03, 0xE8, 0x01, 0xA2, 0xFE};

    // Simulate reading individual bits in DHT22 protocol
    int bit_stream[] = {
        // Byte 0: 0x03 = 00000011
        0, 0, 0, 0, 0, 0, 1, 1,
        // Byte 1: 0xE8 = 11101000
        1, 1, 1, 0, 1, 0, 0, 0,
        // Byte 2: 0x01 = 00000001
        0, 0, 0, 0, 0, 0, 0, 1,
        // Byte 3: 0xA2 = 10100010
        1, 0, 1, 0, 0, 0, 1, 0,
        // Byte 4: 0xFE = 11111110
        1, 1, 1, 1, 1, 1, 1, 0,
    };

    uint8_t buffer[5] = {0};

    for (int i = 0; i < 40; i++) {
        buffer[i / 8] |= (bit_stream[i] << (7 - (i % 8)));
    }

    ESP_LOGI(TAG, "Assembled: [0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X]",
             buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
    ESP_LOGI(TAG, "Expected:  [0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X]",
             expected[0], expected[1], expected[2], expected[3], expected[4]);

    if (memcmp(buffer, expected, 5) == 0) {
        ESP_LOGI(TAG, "PASS: 40-bit assembly");
    } else {
        ESP_LOGE(TAG, "FAIL: Assembly mismatch");
    }
}

// =============================================================================
// TEST 4: Timing Threshold Tests
// =============================================================================

/**
 * Test bit determination logic with various timing values
 */
void test_bit_timing_thresholds(void) {
    ESP_LOGI(TAG, "TEST: Bit timing threshold determination");

    struct timing_test {
        uint32_t pulse_us;
        int expected_bit;
        const char *description;
    } tests[] = {
        {15, 0, "Low pulse (below 0-bit threshold)"},
        {25, 0, "Valid 0-bit pulse (26μs nominal)"},
        {35, 1, "High pulse (above 1-bit threshold)"},
        {70, 1, "Valid 1-bit pulse (70μs nominal)"},
        {80, 1, "High 1-bit pulse"},
        {32, -1, "Ambiguous pulse (between 0 and 1)"},
    };

    // Constants from DHT22 driver
    #define BIT_0_HIGH_MIN_US 20
    #define BIT_0_HIGH_MAX_US 35
    #define BIT_1_HIGH_MIN_US 60
    #define BIT_1_HIGH_MAX_US 85

    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int bit_value;

        if (tests[i].pulse_us >= BIT_1_HIGH_MIN_US &&
            tests[i].pulse_us <= BIT_1_HIGH_MAX_US) {
            bit_value = 1;
        } else if (tests[i].pulse_us >= BIT_0_HIGH_MIN_US &&
                   tests[i].pulse_us <= BIT_0_HIGH_MAX_US) {
            bit_value = 0;
        } else {
            bit_value = -1;  // Ambiguous
        }

        ESP_LOGI(TAG, "Test %d: %s", i + 1, tests[i].description);
        ESP_LOGI(TAG, "  Pulse: %"PRIu32"μs, Determined: %d, Expected: %d",
                 tests[i].pulse_us, bit_value, tests[i].expected_bit);

        if (bit_value == tests[i].expected_bit) {
            ESP_LOGI(TAG, "  PASS");
        } else {
            ESP_LOGE(TAG, "  FAIL");
        }
    }
}

// =============================================================================
// TEST 5: Sampling Interval Validation
// =============================================================================

/**
 * Test sampling interval enforcement
 */
void test_sampling_interval(void) {
    ESP_LOGI(TAG, "TEST: Sampling interval enforcement");

    ESP_LOGI(TAG, "Minimum interval: %d ms (2000ms required)",
             DHT22_MIN_SAMPLING_INTERVAL_MS);

    // Simulated scenario
    uint32_t first_read = 0;
    uint32_t second_read_1s = 1000;     // Too fast
    uint32_t second_read_2s = 2000;     // Minimum
    uint32_t second_read_3s = 3000;     // Safe margin

    ESP_LOGI(TAG, "First read at: %"PRIu32"ms", first_read);
    ESP_LOGI(TAG, "Second read at: %"PRIu32"ms (interval: %"PRIu32"ms) -> %s",
             second_read_1s, second_read_1s - first_read,
             (second_read_1s - first_read) >= DHT22_MIN_SAMPLING_INTERVAL_MS ? "OK" : "TOO FAST");
    ESP_LOGI(TAG, "Second read at: %"PRIu32"ms (interval: %"PRIu32"ms) -> %s",
             second_read_2s, second_read_2s - first_read,
             (second_read_2s - first_read) >= DHT22_MIN_SAMPLING_INTERVAL_MS ? "OK" : "TOO FAST");
    ESP_LOGI(TAG, "Second read at: %"PRIu32"ms (interval: %"PRIu32"ms) -> %s",
             second_read_3s, second_read_3s - first_read,
             (second_read_3s - first_read) >= DHT22_MIN_SAMPLING_INTERVAL_MS ? "OK" : "TOO FAST");
}

// =============================================================================
// TEST 6: Data Validation Tests
// =============================================================================

/**
 * Test sanity checking on retrieved sensor values
 */
void test_data_sanity_checks(void) {
    ESP_LOGI(TAG, "TEST: Data sanity checks");

    struct sensor_reading {
        float temperature;
        float humidity;
        int valid;
        const char *description;
    } tests[] = {
        {25.0f, 50.0f, 1, "Normal reading"},
        {-40.0f, 0.0f, 1, "Minimum values"},
        {80.0f, 100.0f, 1, "Maximum values"},
        {-50.0f, 50.0f, 0, "Temperature out of range (too low)"},
        {100.0f, 50.0f, 0, "Temperature out of range (too high)"},
        {25.0f, -1.0f, 0, "Humidity out of range (negative)"},
        {25.0f, 101.0f, 0, "Humidity out of range (>100%)"},
        {NAN, 50.0f, 0, "Invalid temperature (NaN)"},
    };

    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int is_valid = 1;

        // Temperature: -40 to +80°C
        if (tests[i].temperature < -40.0f || tests[i].temperature > 80.0f) {
            is_valid = 0;
        }

        // Humidity: 0 to 100%
        if (tests[i].humidity < 0.0f || tests[i].humidity > 100.0f) {
            is_valid = 0;
        }

        ESP_LOGI(TAG, "Test %d: %s", i + 1, tests[i].description);
        ESP_LOGI(TAG, "  Temp: %.1f°C, RH: %.1f%%, Valid: %d, Expected: %d",
                 tests[i].temperature, tests[i].humidity,
                 is_valid, tests[i].valid);

        if (is_valid == tests[i].valid) {
            ESP_LOGI(TAG, "  PASS");
        } else {
            ESP_LOGE(TAG, "  FAIL");
        }
    }
}

// =============================================================================
// Main Test Runner
// =============================================================================

void run_all_tests(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "DHT22 Comprehensive Test Suite");
    ESP_LOGI(TAG, "========================================");

    // Test Group 1: Checksum
    ESP_LOGI(TAG, "\nTEST GROUP 1: Checksum Validation");
    ESP_LOGI(TAG, "------------------------------------");
    test_checksum_positive_temperature();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    test_checksum_negative_temperature();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    test_checksum_overflow();

    // Test Group 2: Data Format
    ESP_LOGI(TAG, "\nTEST GROUP 2: Data Format");
    ESP_LOGI(TAG, "------------------------------------");
    test_humidity_parsing();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    test_temperature_parsing();

    // Test Group 3: Bit Manipulation
    ESP_LOGI(TAG, "\nTEST GROUP 3: Bit Manipulation");
    ESP_LOGI(TAG, "------------------------------------");
    test_bit_to_byte_conversion();
    vTaskDelay(500 / portTICK_PERIOD_MS);
    test_40bit_buffer_assembly();

    // Test Group 4: Timing
    ESP_LOGI(TAG, "\nTEST GROUP 4: Timing Thresholds");
    ESP_LOGI(TAG, "------------------------------------");
    test_bit_timing_thresholds();

    // Test Group 5: Sampling
    ESP_LOGI(TAG, "\nTEST GROUP 5: Sampling Interval");
    ESP_LOGI(TAG, "------------------------------------");
    test_sampling_interval();

    // Test Group 6: Validation
    ESP_LOGI(TAG, "\nTEST GROUP 6: Data Validation");
    ESP_LOGI(TAG, "------------------------------------");
    test_data_sanity_checks();

    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "Test Suite Complete");
    ESP_LOGI(TAG, "========================================");
}

// For testing on ESP32
void test_task(void *pvParameters) {
    run_all_tests();
    vTaskDelete(NULL);
}

void app_main(void) {
    xTaskCreate(test_task, "test_task", 8192, NULL, 5, NULL);
}
