# DHT22 (AM2302) Sensor - Comprehensive Technical Specification
## For C Driver Development on ESP32-IDF

---

## Table of Contents
1. [Overview](#overview)
2. [Single-Wire Protocol Timing](#single-wire-protocol-timing)
3. [40-Bit Data Format](#40-bit-data-format)
4. [Timing Requirements](#timing-requirements)
5. [Checksum Calculation](#checksum-calculation)
6. [Security Considerations](#security-considerations)
7. [Memory Safety](#memory-safety)
8. [Reference C Driver Implementation](#reference-c-driver-implementation)

---

## Overview

The DHT22 (also known as AM2302) is a digital temperature and humidity sensor that uses a single-wire protocol for communication. It combines excellent reliability with simple integration, making it ideal for IoT and embedded applications.

**Key Specifications:**
- Temperature Range: -40°C to +80°C (±0.5°C accuracy)
- Humidity Range: 0-100% RH (±2-5% accuracy)
- Operating Voltage: 3.3V to 6.0V DC
- Current Consumption: 1-2mA
- 40-bit output (16-bit RH + 16-bit Temp + 8-bit Checksum)
- Communication: Proprietary single-wire digital protocol

---

## Single-Wire Protocol Timing

The DHT22 uses a bidirectional single-wire protocol where the microcontroller and sensor share control of a single GPIO line. Precise timing is critical for proper operation.

### Protocol States

```
MCU Initiates:
-----------
1. Pull line LOW for 1-10ms (typically 1ms minimum)
2. Release line HIGH (pull-up to VCC via resistor)
3. Wait 20-40μs for sensor to respond

Sensor Responds:
-----------
1. Sensor pulls LOW for 80μs
2. Sensor pulls HIGH for 80μs
3. Transmits 40 bits of data

Data Transmission:
-----------
- Each bit starts with LOW for 50μs
- Bit "0": LOW 50μs + HIGH 26-28μs
- Bit "1": LOW 50μs + HIGH 70μs (approximate)
- Total bit transmission: ~120μs per bit
- Total data transmission: ~5ms for 40 bits
```

### Timing Diagram

```
MCU pulls LOW:
            |<------ 1-10ms ------>|
            ___________________________
Data line: |                       |___

Response signal LOW:
                   |<-80μs->|
           _________|        |________
Data line:         |        |

Response signal HIGH:
                         |<-80μs->|
           _______________|        |___
Data line:               |        |

Data bit transmission (0):
                                 |<-50μs->|<-26-28μs->|
           _____________________|        |___        |____
Data line:                      |        |   |      |
                                v        v   v      v
                           LOW start    HIGH 0    LOW start

Data bit transmission (1):
                                 |<-50μs->|<-~70μs->|
           _____________________|        |__       |____
Data line:                      |        |  |     |
                                v        v  v     v
                           LOW start    HIGH 1   LOW start
```

### Timing Tolerances (Microseconds)

| Parameter | Min | Typical | Max | Unit |
|-----------|-----|---------|-----|------|
| Initialization LOW | 1000 | 1000 | 10000 | μs |
| Initialization HIGH | 20 | 30 | 40 | μs |
| Response LOW | 75 | 80 | 85 | μs |
| Response HIGH | 75 | 80 | 85 | μs |
| Bit LOW | 48 | 50 | 55 | μs |
| Bit 0 HIGH | 22 | 26 | 30 | μs |
| Bit 1 HIGH | 68 | 70 | 75 | μs |
| Sampling Period | 2000 | 2000 | N/A | ms |

---

## 40-Bit Data Format

The DHT22 transmits exactly 40 bits in the following structure:

```
Byte Layout:
┌────────────────────────────────────────────────────────────┐
│ Byte 1 (Bits 0-7)   │ Byte 2 (Bits 8-15) │ Byte 3         │
│ RH Integer (bits)   │ RH Decimal (bits)  │ Temp Integer   │
├────────────────────────────────────────────────────────────┤
│ Byte 4 (Bits 24-31) │ Byte 5 (Bits 32-39)                 │
│ Temp Decimal        │ Checksum                              │
└────────────────────────────────────────────────────────────┘

Bit Order: Most Significant Bit (MSB) first within each byte
Data Range: Big-endian (network byte order)
```

### Data Interpretation

**Humidity Data (Bytes 1-2):**
```
RH = ((Byte1 << 8) | Byte2) / 10.0
Range: 0-100.0%
Example: 0x0319 = 793 / 10 = 79.3% RH
```

**Temperature Data (Bytes 3-4):**
```
Temperature encoding (two's complement for negative values):
- Bit 15 of Byte 3: Sign bit (0 = positive, 1 = negative)
- Bits 14-0 (Byte3[0:7] + Byte4[0:7]): Magnitude

For positive temperatures:
  Temp = ((Byte3 << 8) | Byte4) / 10.0

For negative temperatures (when Bit 15 = 1):
  Temp = -1 * ((Byte3 & 0x7F) << 8 | Byte4) / 10.0

Or universal approach (two's complement):
  int16_t temp_raw = (int16_t)((Byte3 << 8) | Byte4);
  Temp = temp_raw / 10.0;  // Automatically handles negative
```

Example Values:
- 0x01A2 (26.2°C): 418 / 10 = 26.2°C
- 0xFF5E (-16.2°C): two's complement of 0x00A2 = -162 / 10 = -16.2°C

**Checksum (Byte 5):**
```
Checksum = (Byte1 + Byte2 + Byte3 + Byte4) & 0xFF
Validate: Checksum == computed value (last 8 bits only)
```

---

## Timing Requirements

### Sampling Period (Critical)

**Minimum 2-Second Interval Between Readings**

The DHT22 operates at a 0.5 Hz sampling rate internally. This is NOT a software limitation but a sensor hardware constraint.

```c
// CORRECT: Space readings at least 2 seconds apart
vTaskDelay(2000 / portTICK_PERIOD_MS);
dht22_read();

// INCORRECT: Will cause sensor to hang or return corrupted data
for (int i = 0; i < 100; i++) {
    dht22_read();  // TOO FAST!
}
```

### Communication Timing Budget

```
Total transaction time: ~5-6ms
- Initialization: 1-10ms (MCU controlled)
- Sensor response: 160μs
- Data transmission: 5ms (40 bits × ~120μs/bit)
- Total worst-case: ~15-16ms
```

### CPU Cycle Timing on ESP32

**At 240 MHz CPU frequency (default):**
- 1 cycle = 4.17 nanoseconds
- 1 microsecond = 240 CPU cycles
- xthal_get_ccount() overhead: ~50 cycles (~200ns)

**Timing measurement accuracy:**
```c
// CCOUNT wraps every 17.9 seconds (32-bit register at 240 MHz)
uint32_t start = xthal_get_ccount();
// ... do work ...
uint32_t elapsed_cycles = xthal_get_ccount() - start;
uint32_t elapsed_us = elapsed_cycles / 240;
```

### Recommended Implementation: Timer-Based (Not Polling)

**Preferred approach:** Use hardware timer interrupts instead of polling:

```c
// Configure 1 MHz timer (240 MHz / 240 prescaler = 1 MHz = 1μs per tick)
timer_config_t timer_cfg = {
    .divider = 80,           // Prescaler: 240 MHz / 80 = 3 MHz
    .counter_dir = TIMER_COUNT_UP,
    .counter_en = TIMER_PAUSE,
    .alarm_en = TIMER_ALARM_EN,
    .auto_reload = true,
    .intr_type = TIMER_INTR_LEVEL,
};

// Set alarm for 10-second intervals (10,000,000 μs)
timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 10000000);
```

---

## Checksum Calculation

### Validation Algorithm

The checksum is the last 8 bits of the sum of the first 4 data bytes. Overflow beyond 255 is intentionally ignored.

```c
uint8_t calculate_checksum(uint8_t byte1, uint8_t byte2,
                           uint8_t byte3, uint8_t byte4) {
    // Sum the four data bytes
    uint16_t sum = byte1 + byte2 + byte3 + byte4;

    // Take only the lowest 8 bits (overflow is ignored)
    return (uint8_t)(sum & 0xFF);
}

// Validation
uint8_t expected_checksum = calculate_checksum(data[0], data[1],
                                               data[2], data[3]);
if (expected_checksum != data[4]) {
    // Checksum mismatch - data corrupted or timing error
    return DHT22_ERROR_CHECKSUM;
}
```

### Critical Issue: Negative Temperature Checksum

**IMPORTANT BUG IN MANY IMPLEMENTATIONS:**

Many libraries fail with negative temperatures because they don't properly handle two's complement representation.

```c
// INCORRECT: Assumes sign-magnitude encoding
int16_t temp = (int16_t)((data[2] << 8) | data[3]);
if (data[2] & 0x80) {  // If sign bit set
    temp = -(temp & 0x7FFF);  // Strip sign bit (WRONG)
}

// CORRECT: Use two's complement directly
int16_t temp = (int16_t)((data[2] << 8) | data[3]);
// int16_t automatically interprets two's complement correctly!
float temperature = temp / 10.0;  // Works for negative too
```

**Why this matters:**
- At -4.9°C: Sensor sends 0xFF5E (binary: 1111111101011110)
- Two's complement interpretation: -162 / 10 = -16.2°C ✓
- Sign-magnitude interpretation: -(5E) = -94 / 10 = -9.4°C ✗

---

## Security Considerations

### 1. Timing Attacks (Theoretical)

While not a practical threat for DHT22 (not a security-critical component), the timing-sensitive protocol creates theoretical timing side channels:

**Vulnerability:** Checksum validation timing could theoretically leak information about data values through measurement duration.

**Mitigation:**
```c
// Use constant-time comparison
static int constant_time_compare(uint8_t a, uint8_t b) {
    volatile uint8_t result = a ^ b;  // XOR: 0 if equal, non-zero if different
    return result == 0;
}

// Apply in validation
int is_valid = constant_time_compare(expected_checksum, received_checksum);
```

However, for embedded IoT sensors, this is a low-priority concern compared to functional correctness.

### 2. Checksum Validation (Primary Security Concern)

The 8-bit checksum detects single-byte errors and most multi-byte corruptions but is not cryptographically secure.

**Possible Attacks:**
- Electromagnetic interference (EMI) causing bit flips
- Timing violations reading during transmission
- GPIO race conditions with other interrupts

**Mitigation:**
```c
// Implement retry logic with exponential backoff
#define DHT22_MAX_RETRIES 3
#define DHT22_RETRY_DELAY_MS 2000

esp_err_t dht22_read_reliable(dht22_t *sensor, float *temp, float *humidity) {
    for (int attempt = 0; attempt < DHT22_MAX_RETRIES; attempt++) {
        esp_err_t ret = dht22_read(sensor);

        if (ret == ESP_OK) {
            *temp = sensor->temperature;
            *humidity = sensor->humidity;
            return ESP_OK;
        }

        if (attempt < DHT22_MAX_RETRIES - 1) {
            vTaskDelay((DHT22_RETRY_DELAY_MS * (1 << attempt)) / portTICK_PERIOD_MS);
        }
    }

    return ESP_ERR_INVALID_CRC;  // All retries exhausted
}
```

### 3. GPIO Race Conditions (Critical for ESP32)

**TOCTOU (Time-of-Check to Time-of-Use) Vulnerability:**

The DHT22 reading process involves a sequence of GPIO operations where the state can change between checking and using it.

```c
// VULNERABLE CODE:
uint32_t pulse_width = 0;
while (gpio_get_level(DHT22_PIN) == 0) {  // Check: Pin is LOW
    pulse_width++;  // Use: counting while LOW
    if (pulse_width > TIMEOUT) break;
}
// Between "Check" and "Use", another ISR could modify pin state!
```

**Mitigation - Disable Interrupts During Critical Section:**

```c
#include "freertos/portmacro.h"

esp_err_t dht22_read_safe(dht22_t *sensor) {
    // Save interrupt state before disabling
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    portENTER_CRITICAL(&mux);
    {
        // CRITICAL SECTION: Interrupts disabled
        // Initialize sensor (pull LOW for 1-10ms)
        gpio_set_direction(sensor->pin, GPIO_MODE_OUTPUT);
        gpio_set_level(sensor->pin, 0);

        // Timing-critical delay using cycle counter
        uint32_t start = xthal_get_ccount();
        while ((xthal_get_ccount() - start) < (1000 * 240)) {
            // Busy-wait 1ms with interrupts disabled
        }

        // Release to HIGH (pullup)
        gpio_set_direction(sensor->pin, GPIO_MODE_INPUT);

        // Wait for response pulse...
        // [Read 40 bits with interrupts still disabled]
    }
    portEXIT_CRITICAL(&mux);

    return ESP_OK;
}
```

### 4. GPIO Interrupt Delays

On ESP32, various system tasks can delay GPIO interrupts by 50-100 microseconds:

```
Interrupt Delay Sources:
- WiFi transmission: 50-100μs delay
- LWIP spinlock contention: variable
- FreeRTOS critical sections: variable
- Other ISRs with higher priority: variable
```

**Mitigation:**

```c
// Increase timing margins when calculating bit values
#define BIT_0_HIGH_MIN_US 20  // Normal: 22μs, with margin: 20μs
#define BIT_0_HIGH_MAX_US 32  // Normal: 30μs, with margin: 32μs
#define BIT_1_HIGH_MIN_US 60  // Normal: 68μs, with margin: 60μs
#define BIT_1_HIGH_MAX_US 80  // Normal: 75μs, with margin: 80μs

int is_bit_one(uint32_t high_pulse_us) {
    if (high_pulse_us >= BIT_1_HIGH_MIN_US) {
        return 1;
    } else if (high_pulse_us <= BIT_0_HIGH_MAX_US) {
        return 0;
    } else {
        return -1;  // Ambiguous - timing error
    }
}
```

---

## Memory Safety

### 1. Bit Buffer Management

**Vulnerability:** Reading 40 bits into a buffer without bounds checking.

```c
// VULNERABLE: No size checking
void read_bits(uint8_t *buffer) {
    int bit_count = 0;
    while (bit_count < 40) {  // What if this never reaches 40?
        buffer[bit_count / 8] |= (get_bit() << (7 - (bit_count % 8)));
        bit_count++;
    }
}

// SAFE: With bounds and timeout
esp_err_t read_bits_safe(uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < 5) {
        return ESP_ERR_INVALID_SIZE;  // Need at least 5 bytes
    }

    int bit_count = 0;
    uint32_t start_time = esp_timer_get_time();

    while (bit_count < 40) {
        // Timeout protection (5ms per bit, 40 bits = 200ms buffer)
        if ((esp_timer_get_time() - start_time) > 250000) {  // 250ms
            return ESP_ERR_TIMEOUT;
        }

        int bit = get_bit();
        if (bit < 0) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        buffer[bit_count / 8] |= (bit << (7 - (bit_count % 8)));
        bit_count++;
    }

    return ESP_OK;
}
```

### 2. Timeout Handling (Prevents Infinite Loops)

**Vulnerability:** Sensor hangs or fails to respond, causing infinite loop.

```c
// VULNERABLE: No timeout
uint32_t wait_for_transition(int expected_level, uint32_t max_wait) {
    uint32_t count = 0;
    while (gpio_get_level(DHT22_PIN) == expected_level) {
        count++;
        if (count > max_wait) {  // Timeout check buried deep
            return 0;
        }
    }
    return count;
}

// SAFE: Multiple timeout mechanisms
typedef struct {
    uint32_t max_cycles;         // CPU cycle limit
    uint32_t max_iterations;     // Iteration limit as fallback
    esp_timer_handle_t watchdog; // Hardware watchdog
} dht22_timeout_config_t;

uint32_t wait_for_transition_safe(dht22_timeout_config_t *timeout) {
    uint32_t start = xthal_get_ccount();
    uint32_t iterations = 0;

    while (gpio_get_level(DHT22_PIN) == EXPECTED_LEVEL) {
        iterations++;

        // Check iteration limit (catches stupid bugs)
        if (iterations > timeout->max_iterations) {
            return -1;  // Error: too many iterations
        }

        // Check CPU cycle limit (accurate timing)
        uint32_t elapsed = xthal_get_ccount() - start;
        if (elapsed > timeout->max_cycles) {
            return -1;  // Error: timeout exceeded
        }
    }

    return iterations;
}
```

### 3. Critical Sections

**Vulnerability:** Data structure inconsistency from concurrent access.

```c
// DHT22 sensor structure
typedef struct {
    int gpio_pin;
    float temperature;
    float humidity;
    uint8_t checksum_valid;
    portMUX_TYPE data_lock;
} dht22_t;

// VULNERABLE: No synchronization
void set_temperature(dht22_t *sensor, float temp) {
    sensor->temperature = temp;
}

float get_temperature(dht22_t *sensor) {
    return sensor->temperature;
}

// SAFE: With critical section
void set_temperature_safe(dht22_t *sensor, float temp) {
    portENTER_CRITICAL(&sensor->data_lock);
    {
        sensor->temperature = temp;
        sensor->checksum_valid = 1;
    }
    portEXIT_CRITICAL(&sensor->data_lock);
}

float get_temperature_safe(dht22_t *sensor, uint8_t *valid) {
    float temp;
    portENTER_CRITICAL(&sensor->data_lock);
    {
        temp = sensor->temperature;
        *valid = sensor->checksum_valid;
    }
    portEXIT_CRITICAL(&sensor->data_lock);
    return temp;
}
```

### 4. Data Corruption Detection

```c
// Sanity check retrieved values
esp_err_t validate_sensor_data(float temperature, float humidity) {
    // Temperature: -40 to +80°C
    if (temperature < -40.0f || temperature > 80.0f) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Humidity: 0 to 100%
    if (humidity < 0.0f || humidity > 100.0f) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Additional sanity: rate-of-change limit
    // (optional: for detecting noise/corruption)

    return ESP_OK;
}
```

---

## Reference C Driver Implementation

### Basic ESP32-IDF DHT22 Driver

```c
// dht22.h
#ifndef DHT22_H
#define DHT22_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// Timing constants (microseconds)
#define DHT22_START_LOW_US 1000      // 1ms minimum
#define DHT22_START_HIGH_US 30       // ~30μs
#define DHT22_RESPONSE_LOW_US 80     // ~80μs
#define DHT22_RESPONSE_HIGH_US 80    // ~80μs
#define DHT22_BIT_LOW_US 50          // ~50μs
#define DHT22_BIT_0_HIGH_US 26       // 22-30μs for '0'
#define DHT22_BIT_1_HIGH_US 70       // 68-75μs for '1'

// Timing tolerances (margin for ESP32 interrupt jitter)
#define DHT22_BIT_0_HIGH_MIN_US 20   // Conservative margin
#define DHT22_BIT_0_HIGH_MAX_US 35
#define DHT22_BIT_1_HIGH_MIN_US 60   // Conservative margin
#define DHT22_BIT_1_HIGH_MAX_US 85

// Sampling constraints
#define DHT22_MIN_SAMPLING_INTERVAL_MS 2000  // 2 second minimum
#define DHT22_TIMEOUT_MS 250                  // Per-bit timeout

// Error codes (in addition to standard ESP_ERR_*)
#define DHT22_ERROR_CHECKSUM 0x0100
#define DHT22_ERROR_TIMEOUT 0x0101
#define DHT22_ERROR_NO_RESPONSE 0x0102

typedef struct {
    gpio_num_t pin;
    float temperature;
    float humidity;
    uint8_t checksum_valid;
    uint32_t last_read_time;
    portMUX_TYPE lock;
} dht22_t;

// Public API
esp_err_t dht22_init(dht22_t *sensor, gpio_num_t gpio_pin);
esp_err_t dht22_read(dht22_t *sensor);
esp_err_t dht22_get_temperature(dht22_t *sensor, float *temp);
esp_err_t dht22_get_humidity(dht22_t *sensor, float *humidity);
void dht22_cleanup(dht22_t *sensor);

#endif // DHT22_H
```

```c
// dht22.c
#include "dht22.h"
#include "xtensa/hal.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DHT22";

// CPU frequency dependent: 240 MHz = 240 cycles per microsecond
#define CPU_FREQ_MHZ 240
#define CYCLES_PER_US (CPU_FREQ_MHZ)

// Private function: Read pulse width in microseconds
static esp_err_t read_pulse_width(gpio_num_t pin, int expected_level,
                                   uint32_t max_us, uint32_t *width_us) {
    uint32_t start = xthal_get_ccount();
    uint32_t max_cycles = max_us * CYCLES_PER_US;

    // Wait for level change
    while (gpio_get_level(pin) == expected_level) {
        uint32_t elapsed = xthal_get_ccount() - start;
        if (elapsed > max_cycles) {
            return ESP_ERR_TIMEOUT;
        }
    }

    uint32_t end = xthal_get_ccount();
    uint32_t elapsed_cycles = end - start;
    *width_us = elapsed_cycles / CYCLES_PER_US;

    return ESP_OK;
}

// Private function: Read 40 bits from sensor
static esp_err_t dht22_read_bits(gpio_num_t pin, uint8_t *data) {
    // Clear data buffer
    memset(data, 0, 5);

    // Wait for response pulse (LOW then HIGH)
    uint32_t pulse_width;

    // Wait for initial LOW response
    ESP_RETURN_ON_ERROR(
        read_pulse_width(pin, 0, DHT22_RESPONSE_LOW_US * 2, &pulse_width),
        TAG
    );

    // Wait for HIGH response
    ESP_RETURN_ON_ERROR(
        read_pulse_width(pin, 1, DHT22_RESPONSE_HIGH_US * 2, &pulse_width),
        TAG
    );

    // Read 40 data bits
    for (int i = 0; i < 40; i++) {
        // Each bit starts with LOW pulse (~50μs)
        ESP_RETURN_ON_ERROR(
            read_pulse_width(pin, 0, DHT22_BIT_LOW_US * 2, &pulse_width),
            TAG
        );

        // Read HIGH pulse width to determine bit value
        ESP_RETURN_ON_ERROR(
            read_pulse_width(pin, 1, DHT22_BIT_1_HIGH_US * 2, &pulse_width),
            TAG
        );

        // Determine if bit is 0 or 1 based on pulse width
        int bit_value = 0;
        if (pulse_width >= DHT22_BIT_1_HIGH_MIN_US) {
            bit_value = 1;
        } else if (pulse_width <= DHT22_BIT_0_HIGH_MAX_US) {
            bit_value = 0;
        } else {
            // Ambiguous pulse width - timing error
            ESP_LOGW(TAG, "Ambiguous bit %d: %"PRIu32"μs", i, pulse_width);
            return ESP_ERR_INVALID_RESPONSE;
        }

        // Store bit in buffer (MSB first)
        data[i / 8] |= (bit_value << (7 - (i % 8)));
    }

    return ESP_OK;
}

// Private function: Calculate and validate checksum
static esp_err_t dht22_validate_checksum(const uint8_t *data) {
    uint8_t expected = (data[0] + data[1] + data[2] + data[3]) & 0xFF;

    if (expected != data[4]) {
        ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X",
                 expected, data[4]);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

// Public API: Initialize sensor
esp_err_t dht22_init(dht22_t *sensor, gpio_num_t gpio_pin) {
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }

    sensor->pin = gpio_pin;
    sensor->temperature = 0.0f;
    sensor->humidity = 0.0f;
    sensor->checksum_valid = 0;
    sensor->last_read_time = 0;
    sensor->lock = portMUX_INITIALIZER_UNLOCKED;

    // Configure GPIO as input with pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    return gpio_config(&io_conf);
}

// Public API: Read sensor data
esp_err_t dht22_read(dht22_t *sensor) {
    if (!sensor) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check minimum sampling interval
    uint32_t now = esp_timer_get_time() / 1000;  // Convert to ms
    if (now - sensor->last_read_time < DHT22_MIN_SAMPLING_INTERVAL_MS) {
        ESP_LOGW(TAG, "Sampling too fast: %"PRIu32"ms since last read",
                 now - sensor->last_read_time);
        return ESP_ERR_INVALID_STATE;
    }

    // Disable interrupts for timing-critical section
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);

    // Send start signal: LOW for 1-10ms
    gpio_set_direction(sensor->pin, GPIO_MODE_OUTPUT);
    gpio_set_level(sensor->pin, 0);

    // Busy-wait 1ms using cycle counter
    uint32_t start = xthal_get_ccount();
    while ((xthal_get_ccount() - start) < (DHT22_START_LOW_US * CYCLES_PER_US)) {
        // Busy wait
    }

    // Release to HIGH (pull-up takes over)
    gpio_set_direction(sensor->pin, GPIO_MODE_INPUT);

    // Busy-wait for 20-40μs while pulled high
    start = xthal_get_ccount();
    while ((xthal_get_ccount() - start) < (DHT22_START_HIGH_US * CYCLES_PER_US)) {
        // Busy wait
    }

    // Read 40 bits from sensor
    uint8_t data[5];
    esp_err_t ret = dht22_read_bits(sensor->pin, data);

    // Re-enable interrupts
    portEXIT_CRITICAL(&mux);

    // Check for communication errors
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read bits: 0x%02X", ret);
        return ret;
    }

    // Validate checksum
    ret = dht22_validate_checksum(data);
    if (ret != ESP_OK) {
        return ret;
    }

    // Parse humidity (bytes 0-1)
    uint16_t rh_raw = (data[0] << 8) | data[1];

    // Parse temperature (bytes 2-3) as signed 16-bit value
    int16_t temp_raw = (int16_t)((data[2] << 8) | data[3]);

    // Update sensor state (with lock)
    portENTER_CRITICAL(&sensor->lock);
    {
        sensor->humidity = rh_raw / 10.0f;
        sensor->temperature = temp_raw / 10.0f;
        sensor->checksum_valid = 1;
        sensor->last_read_time = esp_timer_get_time() / 1000;
    }
    portEXIT_CRITICAL(&sensor->lock);

    ESP_LOGI(TAG, "Read successful: Temp=%.1f°C, RH=%.1f%%",
             sensor->temperature, sensor->humidity);

    return ESP_OK;
}

// Public API: Get temperature
esp_err_t dht22_get_temperature(dht22_t *sensor, float *temp) {
    if (!sensor || !temp) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&sensor->lock);
    {
        *temp = sensor->temperature;
    }
    portEXIT_CRITICAL(&sensor->lock);

    return ESP_OK;
}

// Public API: Get humidity
esp_err_t dht22_get_humidity(dht22_t *sensor, float *humidity) {
    if (!sensor || !humidity) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&sensor->lock);
    {
        *humidity = sensor->humidity;
    }
    portEXIT_CRITICAL(&sensor->lock);

    return ESP_OK;
}

// Public API: Cleanup
void dht22_cleanup(dht22_t *sensor) {
    if (sensor) {
        gpio_reset_pin(sensor->pin);
        memset(sensor, 0, sizeof(dht22_t));
    }
}
```

### Example Usage in FreeRTOS Task

```c
#include "dht22.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "DHT22_TASK";
static dht22_t dht_sensor;

void dht22_task(void *pvParameters) {
    // Initialize sensor on GPIO 4
    esp_err_t ret = dht22_init(&dht_sensor, GPIO_NUM_4);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DHT22: 0x%02X", ret);
        vTaskDelete(NULL);
    }

    // Read sensor periodically
    while (1) {
        ret = dht22_read(&dht_sensor);

        if (ret == ESP_OK) {
            float temp, humidity;
            dht22_get_temperature(&dht_sensor, &temp);
            dht22_get_humidity(&dht_sensor, &humidity);
            ESP_LOGI(TAG, "Temperature: %.1f°C, Humidity: %.1f%%", temp, humidity);
        } else {
            ESP_LOGE(TAG, "DHT22 read failed: 0x%02X", ret);
        }

        // Wait 3 seconds before next read (safety margin on 2 second minimum)
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    xTaskCreate(dht22_task, "dht22_task", 4096, NULL, 5, NULL);
}
```

---

## Summary

The DHT22 sensor requires careful attention to:

1. **Timing**: Precise microsecond-level timing for protocol compliance
2. **Sampling**: Strict 2-second minimum interval between readings
3. **Data Format**: Proper two's-complement handling for negative temperatures
4. **Checksum**: Validation using last 8 bits of sum with overflow ignored
5. **Security**: Critical sections to prevent TOCTOU race conditions
6. **Memory Safety**: Timeout protection and bounds checking on buffers
7. **Error Handling**: Retry logic, validation, and graceful degradation

The reference implementation provided above incorporates all these considerations and serves as a starting point for production DHT22 drivers on ESP32-IDF.

---

**References:**
- Official DHT22/AM2302 Datasheet (Aosong Electronics)
- ESP-IDF Interrupt Allocation Documentation
- esp-idf-lib DHT Driver Implementation
- Random Nerd Tutorials DHT22 Troubleshooting
- TechTutorialsX ESP32 Timing and Interrupts
