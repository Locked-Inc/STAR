# DHT22 Quick Reference Guide
## Cheat Sheet for ESP32-IDF Development

---

## Protocol Timing (Microseconds)

```
Initialization:
  MCU pulls LOW:   1-10ms (minimum 1ms)
  MCU pulls HIGH:  20-40μs

Sensor Response:
  LOW pulse:       80μs
  HIGH pulse:      80μs

Data Bits:
  Bit LOW:         50μs (both 0 and 1)
  Bit 0 HIGH:      26-28μs
  Bit 1 HIGH:      70μs (approximately)

Sampling:          2000ms minimum between reads
Total read time:   ~5-6ms (communication only)
```

---

## 40-Bit Data Format

```
Byte Layout:
┌────────────┬────────────┬────────────┬────────────┬────────────┐
│ Byte 0     │ Byte 1     │ Byte 2     │ Byte 3     │ Byte 4     │
│ RH High    │ RH Low     │ Temp High  │ Temp Low   │ Checksum   │
└────────────┴────────────┴────────────┴────────────┴────────────┘

Data Interpretation:
  Humidity  = ((Byte0 << 8) | Byte1) / 10.0
  Temp      = (int16_t)((Byte2 << 8) | Byte3) / 10.0  [supports negative!]
  Checksum  = (Byte0 + Byte1 + Byte2 + Byte3) & 0xFF
```

---

## Coding Templates

### Initialization

```c
gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << GPIO_NUM_4),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
};
gpio_config(&io_conf);
```

### Critical Section (Interrupt-Safe)

```c
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
portENTER_CRITICAL(&mux);
{
    // Timing-sensitive code here
    // Interrupts disabled on this core
}
portEXIT_CRITICAL(&mux);
```

### Checksum Calculation

```c
uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
if (checksum != data[4]) {
    // Error: data corrupted
}
```

### Negative Temperature Handling

```c
// CORRECT: Uses two's complement automatically
int16_t temp_raw = (int16_t)((data[2] << 8) | data[3]);
float temperature = temp_raw / 10.0f;  // Works for negative too!

// WRONG: Treats as sign-magnitude (buggy!)
if (data[2] & 0x80) {
    temperature = -(data[2] & 0x7F) / 10.0f;  // INCORRECT
}
```

### Bit Timing with Cycle Counter

```c
#define CPU_FREQ_MHZ 240
#define CYCLES_PER_US (CPU_FREQ_MHZ)

uint32_t start = xthal_get_ccount();
while (gpio_get_level(pin) == 0) {
    if ((xthal_get_ccount() - start) > (DHT22_TIMEOUT_US * CYCLES_PER_US)) {
        return ESP_ERR_TIMEOUT;
    }
}
uint32_t elapsed_us = (xthal_get_ccount() - start) / CYCLES_PER_US;
```

### Bit Value Determination

```c
#define BIT_0_MIN_US 20
#define BIT_0_MAX_US 35
#define BIT_1_MIN_US 60
#define BIT_1_MAX_US 85

int bit_value;
if (pulse_us >= BIT_1_MIN_US && pulse_us <= BIT_1_MAX_US) {
    bit_value = 1;
} else if (pulse_us >= BIT_0_MIN_US && pulse_us <= BIT_0_MAX_US) {
    bit_value = 0;
} else {
    return ESP_ERR_INVALID_RESPONSE;  // Timing error
}
```

### Sampling Interval Check

```c
#define MIN_INTERVAL_MS 2000

static uint32_t last_read_ms = 0;

if ((esp_timer_get_time() / 1000) - last_read_ms < MIN_INTERVAL_MS) {
    return ESP_ERR_INVALID_STATE;  // Too fast!
}
```

### Data Validation

```c
// Basic checks
if (temperature < -40.0f || temperature > 80.0f) return ESP_ERR_INVALID_RESPONSE;
if (humidity < 0.0f || humidity > 100.0f) return ESP_ERR_INVALID_RESPONSE;

// Rate-of-change check (optional but effective)
if (fabs(temperature - last_temp) > 50.0f) return ESP_ERR_INVALID_RESPONSE;
if (fabs(humidity - last_humidity) > 50.0f) return ESP_ERR_INVALID_RESPONSE;
```

---

## Error Codes

```c
ESP_OK                          // Success
ESP_ERR_INVALID_ARG            // Bad function arguments
ESP_ERR_INVALID_SIZE           // Buffer too small
ESP_ERR_INVALID_STATE          // Sampling too fast
ESP_ERR_INVALID_RESPONSE       // Sensor not responding
ESP_ERR_INVALID_CRC            // Checksum failed
ESP_ERR_TIMEOUT                // Read timeout
```

---

## Common Mistakes & Fixes

### Mistake 1: Wrong Temperature Sign Handling

```c
// WRONG
temperature = data[2] & 0x7F;  // Strips sign bit!
if (data[2] & 0x80) temperature = -temperature;

// RIGHT
int16_t temp_raw = (int16_t)((data[2] << 8) | data[3]);
float temperature = temp_raw / 10.0f;
```

### Mistake 2: No Critical Section

```c
// WRONG - WiFi ISR could interrupt mid-read
while (gpio_get_level(pin) == 0) count++;

// RIGHT - Interrupts disabled during critical timing
portENTER_CRITICAL(&mux);
{
    while (gpio_get_level(pin) == 0) count++;
}
portEXIT_CRITICAL(&mux);
```

### Mistake 3: Sampling Too Fast

```c
// WRONG - Sensor hangs if polled too fast
for (int i = 0; i < 100; i++) dht22_read();

// RIGHT - Enforce 2-second minimum
vTaskDelay(2000 / portTICK_PERIOD_MS);
dht22_read();
```

### Mistake 4: Unchecked Checksum

```c
// WRONG - Uses corrupted data
read_bits(data);
temperature = ((data[2] << 8) | data[3]) / 10.0f;

// RIGHT - Validate first
read_bits(data);
if ((data[0] + data[1] + data[2] + data[3] & 0xFF) != data[4]) {
    return ESP_ERR_INVALID_CRC;
}
temperature = ((int16_t)((data[2] << 8) | data[3])) / 10.0f;
```

### Mistake 5: No Timeout Protection

```c
// WRONG - Infinite loop if sensor hangs
while (gpio_get_level(pin) == 0);

// RIGHT - Timeout stops infinite wait
uint32_t start = xthal_get_ccount();
while (gpio_get_level(pin) == 0) {
    if ((xthal_get_ccount() - start) > max_cycles) {
        return ESP_ERR_TIMEOUT;
    }
}
```

---

## Performance Targets

```
Read time:           5-10ms (including all error checking)
Task blocked:        ~10ms maximum
WiFi latency impact: 5-10ms acceptable
GPIO latency:        <5ms impact on other GPIO
Sampling rate:       0.5 Hz (2 second minimum interval)
```

---

## Testing Checklist

```
Unit Tests:
  [ ] Checksum validation (positive temp)
  [ ] Checksum validation (negative temp)
  [ ] Checksum overflow (sum > 255)
  [ ] Humidity parsing (0%, 50%, 100%)
  [ ] Temperature parsing (positive, negative, extremes)
  [ ] Bit assembly (40 bits to 5 bytes)
  [ ] Bit timing thresholds (0 vs 1 determination)

Integration Tests:
  [ ] Read with WiFi disabled
  [ ] Read with WiFi enabled
  [ ] Continuous reads (24 hours)
  [ ] Sampling interval enforcement
  [ ] Error recovery (power cycle)
  [ ] Multiple sensors on different pins

Stress Tests:
  [ ] Maximum interrupt load
  [ ] Rapid GPIO transitions
  [ ] Concurrent WiFi + read operations
  [ ] Temperature extremes (-40°C to +80°C)
  [ ] Humidity extremes (0% to 100%)
```

---

## Reference Values for Testing

```c
// Normal room conditions
data[5] = {0x03, 0xE8, 0x01, 0xA2, 0xFE};  // 62.5% RH, 41.8°C

// Cold conditions
data[5] = {0x01, 0xF4, 0xFF, 0x5E, 0x?};   // 50% RH, -16.2°C
// Checksum = (0x01 + 0xF4 + 0xFF + 0x5E) & 0xFF = 0x52

// Hot/humid conditions
data[5] = {0x06, 0x40, 0x50, 0x00, 0x?};   // 100% RH, 80°C
// Checksum = (0x06 + 0x40 + 0x50 + 0x00) & 0xFF = 0x96
```

---

## Power Consumption Notes

```
DHT22 typical: 1-2mA @ 3.3V
During transmission: 300-400μA spike
Idle (not communicating): <100μA

For battery applications:
- Use power pin on GPIO (pull LOW between reads)
- 500ms power-off period to reset if stuck
- Sample every 30-60 seconds for battery life
```

---

## Wiring Diagram

```
    VCC (3.3-6V)
     |
     +-- 1kΩ pull-up resistor
     |
    ESP32 GPIO --- DHT22 DATA
     |
    GND --- DHT22 GND
```

Pin Configuration (Example: GPIO4):
```c
#define DHT22_PIN GPIO_NUM_4
```

---

## Module Integration

### FreeRTOS Task Pattern

```c
void dht22_task(void *pvParameters) {
    dht22_t sensor;
    dht22_init(&sensor, GPIO_NUM_4);

    while (1) {
        if (dht22_read(&sensor) == ESP_OK) {
            float temp = sensor.temperature;
            float humidity = sensor.humidity;
            ESP_LOGI(TAG, "T=%.1f°C, RH=%.1f%%", temp, humidity);
        }
        vTaskDelay(3000 / portTICK_PERIOD_MS);  // 3s (safety margin on 2s min)
    }
}

xTaskCreate(dht22_task, "dht22", 4096, NULL, 5, NULL);
```

---

## Debugging Tips

```bash
# Enable verbose logging
ESP_LOGD(TAG, "Bit %d: %"PRIu32"μs -> %d",
         bit_index, pulse_us, bit_value);

# Monitor checksum failures
if (checksum != data[4]) {
    ESP_LOGW(TAG, "Checksum fail: got 0x%02X, expected 0x%02X",
             data[4], checksum);
}

# Track sampling violations
if (time_since_last_read < 2000) {
    ESP_LOGW(TAG, "Sampling too fast: %"PRIu32"ms", time_since_last_read);
}

# Measure interrupt jitter
uint32_t jitter_cycles = xthal_get_ccount() - expected_cycles;
ESP_LOGI(TAG, "Jitter: %"PRIu32" cycles (~%uμs)",
         jitter_cycles, jitter_cycles / 240);
```

---

## Further Reading

- **Full Technical Spec:** DHT22_TECHNICAL_SPECIFICATION.md
- **Security Details:** DHT22_SECURITY_AND_ADVANCED.md
- **Test Suite:** DHT22_TESTING_AND_VALIDATION.c
- **Official Datasheet:** Aosong Electronics AM2302
- **ESP-IDF Docs:** https://docs.espressif.com/projects/esp-idf/

---

## File Locations

All reference files created at:
```
C:\Users\sikar\CLionProjects\untitled\
├── DHT22_TECHNICAL_SPECIFICATION.md      (Complete technical details)
├── DHT22_SECURITY_AND_ADVANCED.md        (Security & advanced topics)
├── DHT22_TESTING_AND_VALIDATION.c        (Test suite implementation)
└── DHT22_QUICK_REFERENCE.md              (This file)
```
