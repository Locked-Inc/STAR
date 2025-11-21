# DHT22 Security, Race Conditions, and Advanced Implementation
## ESP32-IDF Production Considerations

---

## Table of Contents
1. [GPIO Race Conditions and TOCTOU Vulnerabilities](#gpio-race-conditions-and-toctou-vulnerabilities)
2. [Critical Section Management](#critical-section-management)
3. [Interrupt Timing and Jitter](#interrupt-timing-and-jitter)
4. [Timing Attacks (Theoretical Analysis)](#timing-attacks-theoretical-analysis)
5. [Memory Safety and Buffer Management](#memory-safety-and-buffer-management)
6. [Corruption Detection and Recovery](#corruption-detection-and-recovery)
7. [Production-Grade Error Handling](#production-grade-error-handling)
8. [Multicore Safety (Dual-Core ESP32)](#multicore-safety-dual-core-esp32)

---

## GPIO Race Conditions and TOCTOU Vulnerabilities

### The Problem: Time-of-Check to Time-of-Use (TOCTOU)

The DHT22 reading process inherently contains TOCTOU vulnerabilities because the sensor state changes between GPIO operations.

```
Vulnerable Timeline:
┌─────────────────────────────────────────┐
│ MCU: Check GPIO state (READ)            │
│ Line is: HIGH                           │
└─────────────────────────────────────────┘
                    ↓ Time gap (vulnerable window)
                    ↓ Other ISR could modify GPIO state!
┌─────────────────────────────────────────┐
│ MCU: Use GPIO state (count cycles)      │
│ Line might now be: LOW (changed!)       │
└─────────────────────────────────────────┘
```

### Vulnerable Code Pattern

```c
// VULNERABLE: TOCTOU race condition
uint32_t read_pulse_width(gpio_num_t pin) {
    uint32_t count = 0;

    // Check: Is pin LOW?
    while (gpio_get_level(pin) == 0) {
        // Use: Count iterations
        count++;
        // RACE: Between check and use, another ISR could change pin state!
        if (count > TIMEOUT) break;
    }
    return count;
}
```

### Attack Scenario

```
Thread A (DHT22 Reader):
1. Checks: gpio_get_level(DHT22_PIN) == 0? Yes
2. [Interrupt occurs here!]

Attacker (ISR):
1. Modifies GPIO state somehow (e.g., WiFi ISR, timer ISR)
2. Disrupts timing measurement

Thread A resumes:
3. Counts iterations with corrupted timing data
4. Misinterprets bit value (0 vs 1)
5. Corrupts sensor data
```

### Safe Implementation: Disable Interrupts

```c
// SAFE: Critical section disables interrupts
uint32_t read_pulse_width_safe(gpio_num_t pin, int expected_level) {
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&mux);  // ← Interrupts disabled here
    {
        uint32_t start = xthal_get_ccount();
        uint32_t max_cycles = DHT22_TIMEOUT_US * CYCLES_PER_US;

        // Both check and use happen atomically
        while (gpio_get_level(pin) == expected_level) {
            uint32_t elapsed = xthal_get_ccount() - start;
            if (elapsed > max_cycles) {
                portEXIT_CRITICAL(&mux);
                return 0;  // Timeout
            }
        }

        uint32_t elapsed = xthal_get_ccount() - start;
        portEXIT_CRITICAL(&mux);  // ← Interrupts re-enabled here
        return elapsed / CYCLES_PER_US;  // Return microseconds
    }
}
```

### Cost Analysis

**What gets disabled:**
- All task switching
- All other interrupts
- System ticks (FreeRTOS scheduler pauses)

**Duration:** ~5-10 milliseconds total (1ms init + 160μs response + 5ms data)

**Impact Assessment:**
- WiFi/Bluetooth: Latency increases by ~10ms maximum
- Other GPIO: Blocked from responding
- FreeRTOS: Task switch delayed by ~10ms
- Real-time safety: Acceptable for non-critical sensors

**Acceptable for DHT22 because:**
- Sampling is only every 2+ seconds
- Critical section is brief (~10ms)
- Not safety-critical application
- Alternative (polling without interrupts) causes busy-waiting

---

## Critical Section Management

### FreeRTOS Critical Sections on ESP32

```c
#include "freertos/portmacro.h"

// Two types of critical sections on ESP32

// Type 1: Single-core critical (recommended for most use cases)
{
    portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&spinlock);
    {
        // Code here runs with interrupts disabled on this core
        // Other core can still run (multicore-safe)
    }
    portEXIT_CRITICAL(&spinlock);
}

// Type 2: Multicore critical (maximum protection, highest cost)
{
    portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL_ISR(&spinlock);  // Also prevents other core access
    {
        // Code here runs with both cores synchronized
        // Maximum latency cost
    }
    portEXIT_CRITICAL_ISR(&spinlock);
}
```

### DHT22 Critical Section Timing

```
Normal Case (interrupts enabled):
├─ Init LOW: 1ms (busy-wait)
├─ Init HIGH: 30μs (busy-wait)
├─ Wait response: 160μs (busy-wait)
├─ Read 40 bits: ~5000μs (busy-wait with cycle counting)
└─ Total blocked: ~6.2ms

Impact:
- FreeRTOS tick skipped? Maybe (depends on frequency)
- WiFi latency spike? 6-10ms (acceptable)
- Other GPIO blocked? 6.2ms maximum
- Context switches missed? ~1-2
```

### Recommended Pattern for DHT22

```c
// Use single-core critical section (less impact)
static portMUX_TYPE dht22_mux = portMUX_INITIALIZER_UNLOCKED;

void dht22_read_critical(void) {
    portENTER_CRITICAL(&dht22_mux);
    {
        // All timing-sensitive code here
        // Init signal, wait response, read bits
    }
    portEXIT_CRITICAL(&dht22_mux);
}
```

---

## Interrupt Timing and Jitter

### ESP32 Interrupt Latency Sources

| Source | Latency | Notes |
|--------|---------|-------|
| WiFi transmission | 50-100μs | Highest priority task |
| LWIP spinlock | variable | TCP/IP stack critical sections |
| FreeRTOS critical sections | 0-variable | RTOS kernel operations |
| Other ISRs | variable | Priority-dependent |
| Cache/memory access | 0-50ns | Negligible |

### Practical Jitter Measurement

```c
#include "xtensa/hal.h"

void measure_interrupt_jitter(void) {
    uint32_t baseline = xthal_get_ccount();

    // Measure pure cycle counter access cost
    for (int i = 0; i < 1000; i++) {
        volatile uint32_t now = xthal_get_ccount();
        // Typical: 50 cycles per access (~200ns)
    }

    uint32_t total = xthal_get_ccount() - baseline;
    ESP_LOGI(TAG, "Average access cost: %u cycles (~%uns)",
             total / 1000, total / (1000 * 240 / 1000));
}
```

### Mitigation: Conservative Timing Thresholds

Instead of exact timing, use margins:

```c
// Strict thresholds (vulnerable to jitter)
#define BIT_0_HIGH_US 26
#define BIT_1_HIGH_US 70

// Conservative thresholds (jitter-resistant)
#define BIT_0_HIGH_MIN_US 18   // +8μs margin below
#define BIT_0_HIGH_MAX_US 36   // +6μs margin above
#define BIT_1_HIGH_MIN_US 60   // +8μs margin below
#define BIT_1_HIGH_MAX_US 85   // +15μs margin above (wider for safety)

// Bit determination with hysteresis
int determine_bit(uint32_t high_pulse_us) {
    // Hysteresis: Once decided, be confident
    if (high_pulse_us >= BIT_1_HIGH_MIN_US) {
        return 1;
    } else if (high_pulse_us <= BIT_0_HIGH_MAX_US) {
        return 0;
    } else {
        // Dead zone for timing errors
        ESP_LOGW(TAG, "Ambiguous pulse: %"PRIu32"μs", high_pulse_us);
        return -1;  // Error
    }
}
```

### Signal Quality Margin Calculation

```
DHT22 typical pulse widths:
- Bit 0 HIGH: 22-30μs (8μs spread)
- Bit 1 HIGH: 68-75μs (7μs spread)
- Gap between ranges: 38-46μs (plenty!)

Safe thresholds:
- Bit 0 threshold: ≤35μs (18μs + 8μs + 9μs safety)
- Bit 1 threshold: ≥55μs (68μs - 13μs safety)
- Separation: 20μs margin (very safe)
```

---

## Timing Attacks (Theoretical Analysis)

### Can DHT22 Be Exploited via Timing Side-Channels?

**Short answer:** Theoretically yes, practically no.

### Attack Theory: Checksum Validation Timing

```c
// Vulnerable code (hypothetical):
int validate_checksum_fast(uint8_t received, uint8_t expected) {
    return received == expected;  // Returns immediately if match
}

// Could leak through timing analysis:
if (byte0_match && byte1_match && byte2_match && byte3_match) {
    if (byte4_match) {  // Timing here reveals whether checksum was checked
        // Attacker measures how long validation takes
        // Longer = more comparisons succeeded
    }
}
```

### Practical Exploit Difficulty

```
Attack requirements:
1. Physical access to ESP32 (measure power or timing)
2. Multiple measurements (noise is high)
3. Statistical analysis capability
4. Value of attack: Can't forge sensor data (no security risk)

Threat model: Who would care?
- IoT thermometer isn't security-critical
- Data is environmental, not cryptographic
- No cryptographic keys to leak
```

### Constant-Time Comparison (For Completeness)

If paranoid, use constant-time comparison:

```c
// Constant-time comparison
static int constant_time_compare(uint8_t a, uint8_t b) {
    // XOR: 0 if equal, non-zero if different
    // Always takes same time regardless of input
    volatile uint8_t result = a ^ b;
    return result == 0;
}

// Safe validation
int validate_checksum_constant_time(uint8_t received, uint8_t expected) {
    return constant_time_compare(received, expected);
}
```

### Conclusion on Timing Attacks

**Recommendation:** Not worth implementing for DHT22.
- Overhead: Additional instructions
- Benefit: Zero (no cryptographic risk)
- Apply only if: Building secure boot or cryptographic operations elsewhere

---

## Memory Safety and Buffer Management

### Vulnerability 1: Buffer Overflow in Bit Reading

```c
// VULNERABLE: No bounds checking
void read_sensor_data(uint8_t *data) {
    int bit_count = 0;
    while (bit_count < 40) {  // What if this never exits?
        int bit = read_bit();
        if (bit < 0) continue;  // What if all reads fail?

        // No bounds check - if bit_count gets corrupted, writes past buffer!
        data[bit_count / 8] |= (bit << (7 - (bit_count % 8)));
        bit_count++;
    }
}
```

### Safe Implementation

```c
// SAFE: With complete bounds checking
#define DHT22_BYTES 5
#define DHT22_BITS 40

esp_err_t read_sensor_data_safe(uint8_t *data, size_t data_size) {
    // Input validation
    if (!data || data_size < DHT22_BYTES) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clear buffer to known state
    memset(data, 0, data_size);

    int bit_count = 0;
    uint32_t start_time = esp_timer_get_time();
    const uint32_t TIMEOUT_US = 250000;  // 250ms

    while (bit_count < DHT22_BITS) {
        // Timeout protection
        uint32_t elapsed = esp_timer_get_time() - start_time;
        if (elapsed > TIMEOUT_US) {
            ESP_LOGE(TAG, "Timeout: only read %d bits", bit_count);
            return ESP_ERR_TIMEOUT;
        }

        // Read bit with error checking
        int bit = read_bit_with_timeout(TIMEOUT_US - elapsed);
        if (bit < 0) {
            ESP_LOGE(TAG, "Bit read error at bit %d", bit_count);
            return ESP_ERR_INVALID_RESPONSE;
        }

        // Bounds check (defensive programming)
        if (bit_count >= DHT22_BITS) {
            ESP_LOGE(TAG, "Bit count overflow at %d", bit_count);
            return ESP_ERR_INVALID_STATE;
        }

        // Safe indexing
        uint8_t byte_index = bit_count / 8;
        uint8_t bit_index = 7 - (bit_count % 8);

        data[byte_index] |= (bit << bit_index);
        bit_count++;
    }

    return ESP_OK;
}
```

### Vulnerability 2: Uninitialized Data

```c
// VULNERABLE: Uninitialized buffer read
void process_sensor_data(void) {
    uint8_t data[5];  // Not initialized!
    read_sensor_data(data);

    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (checksum != data[4]) {
        // What if read failed? data[] contains garbage!
        log_error("Checksum mismatch");
    }
}

// SAFE: Always initialize
uint8_t data[5] = {0};  // Now 0x00, 0x00, 0x00, 0x00, 0x00
esp_err_t ret = read_sensor_data_safe(data, sizeof(data));
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read: 0x%02X", ret);
    return;
}

uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
if (checksum != data[4]) {
    ESP_LOGE(TAG, "Checksum validation failed");
}
```

### Vulnerability 3: Stack Overflow in Recursive Retries

```c
// VULNERABLE: Unbounded recursion
void read_with_retry(int attempt) {
    if (dht22_read() != ESP_OK) {
        if (attempt > 0) {
            read_with_retry(attempt - 1);  // Recursion!
        }
    }
}

// Bad scenario: read_with_retry(1000) causes stack overflow!

// SAFE: Iterative approach
esp_err_t read_with_retry(int max_attempts) {
    for (int attempt = 0; attempt < max_attempts; attempt++) {
        esp_err_t ret = dht22_read();
        if (ret == ESP_OK) {
            return ESP_OK;
        }

        if (attempt < max_attempts - 1) {
            uint32_t delay_ms = (DHT22_MIN_SAMPLING_INTERVAL_MS *
                                 (1U << attempt));
            vTaskDelay(delay_ms / portTICK_PERIOD_MS);
        }
    }

    return ESP_ERR_INVALID_CRC;
}
```

---

## Corruption Detection and Recovery

### Data Validation Levels

```c
// Level 1: Checksum validation only
int level1_validate(uint8_t *data) {
    uint8_t expected = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    return expected == data[4];
}

// Level 2: Add range checks
int level2_validate(uint8_t *data) {
    // Checksum first
    if (!level1_validate(data)) return 0;

    // Parse values
    uint16_t rh_raw = (data[0] << 8) | data[1];
    int16_t temp_raw = (int16_t)((data[2] << 8) | data[3]);

    // Range checks
    if (rh_raw > 1000) return 0;  // Max 100.0%
    if (temp_raw < -4000 || temp_raw > 8000) return 0;  // -40 to +80°C

    return 1;
}

// Level 3: Add rate-of-change detection
struct sensor_state {
    float last_temperature;
    float last_humidity;
    uint32_t last_read_time;
};

int level3_validate(uint8_t *data, struct sensor_state *state) {
    // Basic validation first
    if (!level2_validate(data)) return 0;

    // Parse new values
    float temp = ((int16_t)((data[2] << 8) | data[3])) / 10.0f;
    float humidity = (((data[0] << 8) | data[1]) / 10.0f);

    // Rate-of-change check (impossible deltas)
    // Assuming DHT22 is in normal room, temperature can't jump 50°C in 2 seconds
    if (fabs(temp - state->last_temperature) > 50.0f) return 0;
    if (fabs(humidity - state->last_humidity) > 50.0f) return 0;

    // Update state for next check
    state->last_temperature = temp;
    state->last_humidity = humidity;
    state->last_read_time = esp_timer_get_time();

    return 1;
}
```

### Recovery Strategy: Exponential Backoff with Power Cycle

```c
#define DHT22_MAX_FAILURES 3
#define DHT22_POWER_CYCLE_MS 500

typedef struct {
    int failure_count;
    uint32_t last_failure_time;
    gpio_num_t power_pin;  // Optional: GPIO to power-cycle sensor
} dht22_recovery_t;

esp_err_t dht22_read_with_recovery(dht22_t *sensor,
                                    dht22_recovery_t *recovery) {
    esp_err_t ret = dht22_read(sensor);

    if (ret == ESP_OK) {
        // Success: reset failure counter
        recovery->failure_count = 0;
        return ESP_OK;
    }

    // Failure detected
    recovery->failure_count++;

    if (recovery->failure_count >= DHT22_MAX_FAILURES) {
        // Too many failures: power cycle the sensor
        ESP_LOGW(TAG, "Too many failures (%d), power cycling sensor",
                 recovery->failure_count);

        // Pull power pin LOW
        gpio_set_direction(recovery->power_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(recovery->power_pin, 0);

        // Wait 500ms for discharge
        vTaskDelay(DHT22_POWER_CYCLE_MS / portTICK_PERIOD_MS);

        // Restore power
        gpio_set_level(recovery->power_pin, 1);

        // Wait for sensor startup
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        // Reset counter and try again
        recovery->failure_count = 0;
    } else {
        // Regular backoff: wait before retry
        uint32_t backoff_ms = DHT22_MIN_SAMPLING_INTERVAL_MS *
                              (1U << recovery->failure_count);
        ESP_LOGW(TAG, "Read failed (attempt %d), retrying in %"PRIu32"ms",
                 recovery->failure_count, backoff_ms);

        vTaskDelay(backoff_ms / portTICK_PERIOD_MS);
    }

    return ret;
}
```

---

## Production-Grade Error Handling

### Complete Error State Machine

```c
typedef enum {
    DHT22_STATE_IDLE,
    DHT22_STATE_READING,
    DHT22_STATE_PROCESSING,
    DHT22_STATE_ERROR,
    DHT22_STATE_RECOVERY,
} dht22_state_t;

typedef struct {
    dht22_state_t state;
    esp_err_t last_error;
    uint32_t error_count;
    uint32_t success_count;
    uint64_t total_reads;
} dht22_stats_t;

esp_err_t dht22_read_with_stats(dht22_t *sensor, dht22_stats_t *stats) {
    stats->state = DHT22_STATE_READING;
    stats->total_reads++;

    esp_err_t ret = dht22_read(sensor);

    if (ret == ESP_OK) {
        stats->success_count++;
        stats->error_count = 0;  // Reset error streak
        stats->state = DHT22_STATE_IDLE;
        stats->last_error = ESP_OK;
    } else {
        stats->error_count++;
        stats->state = DHT22_STATE_ERROR;
        stats->last_error = ret;

        ESP_LOGW(TAG, "Error #%"PRIu32": %s (0x%02X)",
                 stats->error_count,
                 esp_err_to_name(ret),
                 ret);

        // Decide recovery action
        if (stats->error_count > 5) {
            stats->state = DHT22_STATE_RECOVERY;
            // Trigger recovery (power cycle, reset pin, etc.)
        }
    }

    return ret;
}

void dht22_print_stats(dht22_stats_t *stats) {
    uint32_t failure_rate_percent = 0;
    if (stats->total_reads > 0) {
        failure_rate_percent = (stats->error_count * 100) / stats->total_reads;
    }

    ESP_LOGI(TAG, "DHT22 Stats:");
    ESP_LOGI(TAG, "  Total reads: %"PRIu64, stats->total_reads);
    ESP_LOGI(TAG, "  Success: %"PRIu32", Failed: %"PRIu32,
             stats->success_count, stats->error_count);
    ESP_LOGI(TAG, "  Failure rate: %"PRIu32"%%", failure_rate_percent);
    ESP_LOGI(TAG, "  State: %d, Last error: 0x%02X",
             stats->state, stats->last_error);
}
```

---

## Multicore Safety (Dual-Core ESP32)

### Issue: Two Cores, One GPIO

```
Core 0:                      Core 1:
dht22_read()                 WiFi_ISR()
├─ Set pin LOW               ├─ Modify pin?
├─ Count cycles              ├─ Toggle GPIO?
└─ Read bits ← UNSAFE!       └─ Cause corruption
```

### Multicore-Safe Implementation

```c
#include "freertos/portmacro.h"

static portMUX_TYPE dht22_core_mux = portMUX_INITIALIZER_UNLOCKED;

esp_err_t dht22_read_multicore_safe(dht22_t *sensor) {
    // Prevent other core from accessing GPIO
    portENTER_CRITICAL_ISR(&dht22_core_mux);
    {
        // Now we have exclusive access from both cores
        // Critical section runs

        // Send start signal
        gpio_set_level(sensor->pin, 0);
        // ... delay and read ...
        gpio_set_direction(sensor->pin, GPIO_MODE_INPUT);

        // Read 40 bits
        uint8_t data[5] = {0};
        esp_err_t ret = dht22_read_bits(sensor->pin, data);

        portEXIT_CRITICAL_ISR(&dht22_core_mux);

        if (ret != ESP_OK) return ret;

        // Validation happens outside critical section
        return dht22_validate_checksum(data);
    }
}
```

### Cost Analysis

```
portENTER_CRITICAL() cost:    ~50-100 cycles (spinlock)
portENTER_CRITICAL_ISR() cost: ~200-300 cycles (with core sync)

For DHT22: Use regular CRITICAL for single-core safety,
only use _ISR variant if other cores aggressively touch GPIO
```

### Alternative: Pin Ownership Model

```c
// Owner core can access pin freely; others must lock
static uint8_t dht22_owner_core = 0;

#define dht22_verify_core() \
    do { \
        if (xPortGetCoreID() != dht22_owner_core) { \
            ESP_LOGE(TAG, "Wrong core! DHT22 owned by %d, current: %d", \
                     dht22_owner_core, xPortGetCoreID()); \
            return ESP_ERR_INVALID_STATE; \
        } \
    } while(0)

esp_err_t dht22_read_owner_only(dht22_t *sensor) {
    dht22_verify_core();  // Ensure correct core

    // No locking needed - we own this pin
    return dht22_read(sensor);
}
```

---

## Summary: Production Checklist

### Before Shipping DHT22 Driver

- [ ] **Timing Protection**
  - [ ] Critical sections disable interrupts during read
  - [ ] Use xthal_get_ccount() for microsecond timing
  - [ ] Conservative timing thresholds (20-35μs for 0, 60-85μs for 1)

- [ ] **Memory Safety**
  - [ ] Bounds checking on bit buffer reads
  - [ ] Timeout protection (250ms maximum)
  - [ ] Initialize all buffers before use
  - [ ] No recursion in retry logic

- [ ] **Error Handling**
  - [ ] Checksum validation on all reads
  - [ ] Range checking on temperature/humidity
  - [ ] Rate-of-change detection (optional)
  - [ ] Error statistics and logging

- [ ] **Sampling Compliance**
  - [ ] Enforce 2-second minimum interval
  - [ ] Track last_read_time
  - [ ] Reject too-fast read attempts

- [ ] **Race Condition Prevention**
  - [ ] Single-core: Use portENTER_CRITICAL()
  - [ ] Multi-core: Use portENTER_CRITICAL_ISR() or pin ownership
  - [ ] Test with WiFi enabled (highest interrupt contention)

- [ ] **Recovery Mechanisms**
  - [ ] Exponential backoff on consecutive failures
  - [ ] Optional power-cycle capability
  - [ ] Graceful degradation (return last valid reading)
  - [ ] Statistics collection for diagnostics

- [ ] **Testing**
  - [ ] Unit tests for checksum (positive, negative, overflow)
  - [ ] Integration tests with interrupts enabled
  - [ ] Stress tests (read every 2 seconds for 24 hours)
  - [ ] WiFi coexistence tests (Bluetooth enabled)

---

## References

- ESP-IDF Interrupt Allocation: https://docs.espressif.com/projects/esp-idf/
- FreeRTOS Critical Sections: https://www.freertos.org/
- DHT22 Datasheet: Aosong Electronics
- TOCTOU Vulnerabilities: CWE-367
