# HC-SR04 Security, Testing, and Best Practices Guide

## Table of Contents

1. Security Vulnerabilities and Mitigations
2. TOCTOU Prevention Strategies
3. GPIO Race Condition Prevention
4. Memory Safety Issues
5. Testing Strategies
6. Integration Testing
7. Performance Validation

---

## 1. SECURITY VULNERABILITIES AND MITIGATIONS

### 1.1 TOCTOU (Time-of-Check to Time-of-Use) in Echo Measurement

**Vulnerability:**
```c
// VULNERABLE: Read, check, then use - value can change between operations
hc_sr04_measurement_t measurement;
status = hc_sr04_measure(&sensor, &measurement);

if (status == HC_SR04_OK) {  // Time-of-Check
    // OS preemption could occur here
    // ISR could modify measurement.distance_cm

    use_distance(measurement.distance_cm);  // Time-of-Use
}
```

**Root Cause:**
- Between the status check and using the measurement data, an interrupt handler could modify the sensor's state
- If ISR updates the measurement structure, consumer sees inconsistent data

**Mitigation 1: Atomic Snapshot (Implemented in Driver)**
```c
// SAFE: Atomic capture during measurement
{
    portDISABLE_INTERRUPTS();
    uint32_t snapshot = echo_width;  // Atomic read
    portENABLE_INTERRUPTS();

    distance = snapshot / 58;  // Calculate from snapshot
}
```

**Mitigation 2: Structured Return with Validation**
```c
// SAFE: Measurement structure validated before use
hc_sr04_measurement_t measurement;
hc_sr04_status_t status = hc_sr04_measure(&sensor, &measurement);

// Status indicates successful, consistent measurement
if (status == HC_SR04_OK &&
    hc_sr04_measurement_valid(&measurement, last_distance)) {
    // Safe to use - internal consistency guaranteed
    use_distance(measurement.distance_cm);
}
```

**Mitigation 3: Read-Only Return Values**
```c
// SAFE: Return const structure - prevents modification
hc_sr04_status_t hc_sr04_measure(hc_sr04_sensor_t *sensor,
                                 hc_sr04_measurement_t *const measurement);
```

### 1.2 GPIO Race Conditions

**Vulnerability 1: Concurrent ISR and Main Code Access**
```c
// VULNERABLE: ISR and main code both access echo_us
volatile uint32_t echo_us;  // No atomic protection

void IRAM_ATTR echo_isr(void* arg) {
    echo_us = current_time;  // Write from ISR
}

void loop() {
    distance = echo_us / 58;  // Read from main - RACE CONDITION!
}
```

**Problem:**
- On 32-bit systems, 32-bit reads/writes are atomic, but 64-bit are not
- ISR could write while main code reads, causing torn reads
- Main code could buffer stale values

**Mitigation 1: Atomic Operations (Implemented)**
```c
#include <freertos/atomic.h>

// Use atomic types
volatile atomic_uint_least32_t echo_us;

// Safe ISR write
void IRAM_ATTR echo_isr(void* arg) {
    atomic_store(&echo_us, current_time);
}

// Safe main read
uint32_t snapshot = atomic_load(&echo_us);
```

**Mitigation 2: Disable Interrupts for Critical Section**
```c
// Alternative: Disable interrupts during access
portDISABLE_INTERRUPTS();
uint32_t snapshot = echo_us;
portENABLE_INTERRUPTS();

// Cost: 50-100 μs maximum interrupt delay
// Acceptable for ESP32 with FreeRTOS
```

**Mitigation 3: Semaphore Synchronization**
```c
SemaphoreHandle_t measurement_mutex;

// In ISR (ISR-safe version)
xSemaphoreGiveFromISR(measurement_mutex, &xHigherPriorityTaskWoken);

// In main
xSemaphoreTake(measurement_mutex, portMAX_DELAY);
uint32_t snapshot = echo_us;
xSemaphoreGive(measurement_mutex);
```

**Vulnerability 2: Pin State Change During Multiple Reads**
```c
// VULNERABLE: GPIO state can change between reads
if (gpio_get_level(pin) == 1 && gpio_get_level(pin) == 1) {
    // UNSAFE - pin could have gone low between the two reads
}
```

**Mitigation:** Use interrupt-driven edge detection instead of polling multiple reads

### 1.3 Interrupt Handler Safety Issues

**Vulnerability 1: Deadlock Through ISR Taking Mutex**
```c
// VULNERABLE: ISR can deadlock
SemaphoreHandle_t mutex;

void IRAM_ATTR echo_isr(void* arg) {
    xSemaphoreTake(mutex, 0);  // DEADLOCK if main holds it!
    // ...
}
```

**Mitigation:** Never take non-ISR-safe synchronization primitives in ISR
```c
// SAFE: Use ISR-safe versions only
void IRAM_ATTR echo_isr(void* arg) {
    // Use xQueueSendFromISR, xSemaphoreGiveFromISR only
    // Use atomic_store, atomic_load
}
```

**Vulnerability 2: Stack Overflow in ISR**
```c
// VULNERABLE: Large local arrays in ISR
void IRAM_ATTR bad_isr(void* arg) {
    uint8_t large_buffer[4096];  // Stack overflow risk!
}
```

**Mitigation:** Use static or global buffers
```c
// SAFE: Static allocation in ISR
static uint8_t isr_buffer[256];  // Pre-allocated

void IRAM_ATTR good_isr(void* arg) {
    // Only minimal local variables
    uint32_t time_value = esp_timer_get_time();
    atomic_store(&g_timestamp, time_value);
}
```

**Vulnerability 3: Long ISR Execution Time**
```c
// VULNERABLE: ISR blocks other interrupts
void IRAM_ATTR slow_isr(void* arg) {
    for (int i = 0; i < 1000; i++) {
        complex_calculation();  // ISR delay: milliseconds!
    }
}
```

**Mitigation:** Defer heavy processing to task context
```c
// SAFE: ISR minimal, heavy work in task
void IRAM_ATTR fast_isr(void* arg) {
    atomic_store(&measurement_ready, true);
    // Queue notification if needed
}

void process_task(void) {
    // Heavy lifting here
    while (1) {
        if (atomic_load(&measurement_ready)) {
            expensive_processing();
        }
    }
}
```

### 1.4 Input Injection and Spoofing

**Vulnerability: Externally Injected Echo Pulses**
```
Attacker could force echo pulses on GPIO pin to spoof distance measurements.
This could cause distance control systems to behave unsafely.
```

**Mitigation 1: Measurement Plausibility Checking**
```c
// Reject measurements with implausible changes
uint32_t last_distance = 100;  // Previous valid measurement

// New measurement shows 500 cm
if (abs(new_distance - last_distance) > 50) {
    // Likely spoofed or sensor error
    return MEASUREMENT_ERROR;
}
```

**Mitigation 2: Multiple Sensor Fusion**
```c
// Use multiple HC-SR04 sensors
// Reject measurements where all sensors disagree
hc_sr04_measurement_t sensor1, sensor2, sensor3;

// Take median of three sensors
uint32_t distances[3] = {sensor1.distance_cm,
                         sensor2.distance_cm,
                         sensor3.distance_cm};
uint32_t consensus = get_median(distances);

// Reject outliers
for (int i = 0; i < 3; i++) {
    if (abs(distances[i] - consensus) > 10) {
        // Likely spoofed sensor
        mark_sensor_unreliable(i);
    }
}
```

**Mitigation 3: Signal Strength Analysis**
```c
// Echo pulse amplitude indicates signal strength
// Weak pulses more likely to be noise/spoofed

// For basic HC-SR04 (no amplitude info), require:
// 1. Clean rising edge
// 2. Clean falling edge
// 3. Pulse width in expected range
// 4. Timing consistent with previous measurements
```

---

## 2. MEMORY SAFETY ISSUES

### 2.1 Timer Overflow Protection

**Vulnerability: Unsigned Arithmetic Comparison**
```c
// VULNERABLE: Timer overflow causes wrong behavior
uint32_t start_time = micros();

while (gpio_get_level(pin) == 1) {
    uint32_t elapsed = micros() - start_time;

    if (elapsed > 30000) {  // WRONG if overflow!
        // If timer wraps around:
        // elapsed becomes very large or negative
        break;
    }
}
```

**Mitigation 1: Signed Arithmetic for Overflow Safety**
```c
// SAFE: Signed comparison handles overflow
int32_t elapsed = (int32_t)(current_time - start_time);

if (elapsed > 30000 || elapsed < 0) {
    // Safe even with wrap-around
    timeout_occurred();
}
```

**Mitigation 2: Deadline-Based Checking**
```c
// SAFE: Calculate timeout deadline upfront
uint32_t start = get_time_us();
uint32_t deadline = start + TIMEOUT_US;

while (1) {
    uint32_t current = get_time_us();

    if (current >= deadline) {  // Single direction comparison
        timeout_occurred();
        break;
    }
}
```

**Mitigation 3: 64-bit Timer Values**
```c
// SAFE: 64-bit timer won't overflow for years
uint64_t start_us = esp_timer_get_time();  // Microseconds since boot

// Won't overflow for ~584,542 years
uint64_t elapsed = esp_timer_get_time() - start_us;

if (elapsed > 30000) {
    timeout_occurred();
}
```

### 2.2 Integer Overflow in Distance Calculation

**Vulnerability: Overflow in Intermediate Calculation**
```c
// VULNERABLE: Intermediate overflow
uint16_t echo_us = 20000;  // 16-bit
uint16_t distance = (echo_us * 343) / 2000000;

// Problem: echo_us * 343 = 6,860,000 exceeds 16-bit max (65,535)
// Result: Overflow, incorrect calculation
```

**Mitigation 1: Use Larger Integer Types**
```c
// SAFE: Use 32-bit for intermediate
uint32_t echo_us = 20000;
uint32_t distance = (echo_us * 343) / 2000000;
// echo_us * 343 = 6,860,000 (fits in 32-bit)
```

**Mitigation 2: Pre-Calculate Constants**
```c
// SAFE: Use pre-calculated divisor
uint32_t echo_us = 20000;
uint32_t distance = echo_us / 58;  // Pre-calculated: 343 / 2000000 ≈ 1/58
// No multiplication needed
```

**Mitigation 3: Floating Point for High Precision**
```c
// SAFE: Floating point handles large intermediate values
uint32_t echo_us = 20000;
float distance_m = (float)echo_us * 343.0 / 2000000.0;  // In meters
uint32_t distance_cm = (uint32_t)(distance_m * 100.0);
```

### 2.3 Buffer Overflow in Results Storage

**Vulnerability: Unbounded Array Access**
```c
// VULNERABLE: No bounds checking
#define MAX_MEASUREMENTS 10
uint32_t distances[MAX_MEASUREMENTS];
uint32_t index = 0;

void store_measurement(uint32_t distance) {
    distances[index++];  // Buffer overflow if index >= 10
}
```

**Mitigation 1: Explicit Bounds Check**
```c
// SAFE: Validate before storing
void store_measurement(uint32_t distance) {
    if (index < MAX_MEASUREMENTS) {
        distances[index++] = distance;
    } else {
        // Handle overflow: circular buffer, error logging, etc.
        handle_buffer_overflow();
    }
}
```

**Mitigation 2: Circular Buffer**
```c
// SAFE: Wrap around using modulo
void store_measurement(uint32_t distance) {
    distances[index % MAX_MEASUREMENTS] = distance;
    index++;
}
```

**Mitigation 3: Dynamic Allocation with Validation**
```c
// SAFE: Dynamic size with bounds checking
typedef struct {
    uint32_t *data;
    size_t capacity;
    size_t count;
} measurement_buffer_t;

bool store_measurement(measurement_buffer_t *buf, uint32_t distance) {
    if (buf->count >= buf->capacity) {
        // Reallocate or reject
        return false;
    }
    buf->data[buf->count++] = distance;
    return true;
}
```

### 2.4 Struct Padding and Alignment Issues

**Vulnerability: Uninitialized Padding Bytes**
```c
// POTENTIALLY UNSAFE: Padding bytes uninitialized
typedef struct {
    uint32_t distance_cm;   // 4 bytes
    // 4 bytes padding (uninitialized!)
    uint64_t timestamp_us;  // 8 bytes
} measurement_t;
```

**Mitigation: Explicit Padding Initialization**
```c
// SAFE: Explicitly initialize entire structure
hc_sr04_measurement_t measurement;
memset(&measurement, 0, sizeof(measurement));  // Clear all bytes
// Fill in actual values
measurement.distance_cm = calculated_distance;
```

---

## 3. TESTING STRATEGIES

### 3.1 Unit Testing

**Test: Basic Distance Calculation**
```c
void test_distance_calculation(void) {
    uint32_t distance;

    // Test: 10 cm = 580 μs
    assert(hc_sr04_echo_to_distance(580, 20, &distance) == HC_SR04_OK);
    assert(distance == 10);

    // Test: 100 cm = 5800 μs
    assert(hc_sr04_echo_to_distance(5800, 20, &distance) == HC_SR04_OK);
    assert(distance == 100);

    // Test: Minimum range (2 cm = 116 μs)
    assert(hc_sr04_echo_to_distance(116, 20, &distance) == HC_SR04_OK);
    assert(distance == 2);

    // Test: Out of range (< 116 μs)
    assert(hc_sr04_echo_to_distance(115, 20, &distance) != HC_SR04_OK);
}
```

**Test: Temperature Compensation**
```c
void test_temperature_compensation(void) {
    uint32_t distance;

    // At 20°C (reference)
    hc_sr04_echo_to_distance(5800, 20, &distance);
    uint32_t dist_20c = distance;

    // At 0°C (colder, slower sound)
    hc_sr04_echo_to_distance(5800, 0, &distance);
    // Should be larger (same echo time, slower sound = farther)
    assert(distance > dist_20c);

    // At 40°C (warmer, faster sound)
    hc_sr04_echo_to_distance(5800, 40, &distance);
    // Should be smaller (same echo time, faster sound = closer)
    assert(distance < dist_20c);
}
```

**Test: Bounds Checking**
```c
void test_measurement_validation(void) {
    hc_sr04_measurement_t m = {
        .distance_cm = 50,
        .echo_width_us = 2900,
        .temperature_c = 20
    };

    // Valid measurement
    assert(hc_sr04_measurement_valid(&m, -1) == true);

    // Below minimum
    m.distance_cm = 1;
    assert(hc_sr04_measurement_valid(&m, -1) == false);

    // Above maximum
    m.distance_cm = 500;
    assert(hc_sr04_measurement_valid(&m, -1) == false);

    // Implausible change (500 cm change)
    m.distance_cm = 100;
    assert(hc_sr04_measurement_valid(&m, 50) == false);
}
```

### 3.2 Integration Testing

**Test: Complete Measurement Cycle**
```c
void test_measurement_cycle(void) {
    hc_sr04_config_t config = {
        .trigger_pin = GPIO_NUM_5,
        .echo_pin = GPIO_NUM_18,
        .echo_timeout_us = 30000,
        .use_interrupts = false,
        .temperature.enabled = false,
    };

    hc_sr04_sensor_t sensor;
    assert(hc_sr04_init(&sensor, &config) == HC_SR04_OK);

    // Perform 10 measurements
    for (int i = 0; i < 10; i++) {
        hc_sr04_measurement_t measurement;
        hc_sr04_status_t status = hc_sr04_measure(&sensor, &measurement);

        if (status == HC_SR04_OK) {
            // Verify result structure is valid
            assert(measurement.distance_cm >= HC_SR04_MIN_RANGE_CM);
            assert(measurement.distance_cm <= HC_SR04_MAX_RANGE_CM);
            assert(measurement.echo_width_us > 0);
            assert(measurement.timestamp_us > 0);
        }
    }

    assert(hc_sr04_deinit(&sensor) == HC_SR04_OK);
}
```

**Test: Error Handling and Recovery**
```c
void test_error_recovery(void) {
    hc_sr04_config_t config = { /* ... */ };
    hc_sr04_sensor_t sensor;

    hc_sr04_init(&sensor, &config);

    uint32_t timeout_errors = 0;
    uint32_t valid_measurements = 0;

    for (int i = 0; i < 100; i++) {
        hc_sr04_measurement_t measurement;
        hc_sr04_status_t status = hc_sr04_measure(&sensor, &measurement);

        if (status == HC_SR04_ERR_TIMEOUT) {
            timeout_errors++;
        } else if (status == HC_SR04_OK) {
            valid_measurements++;
        }
    }

    // Expect mostly valid measurements (not all errors)
    assert(valid_measurements > 50);

    // Some timeouts acceptable (< 20% error rate)
    assert(timeout_errors < 20);
}
```

### 3.3 Performance Testing

**Test: Measurement Latency**
```c
void test_measurement_latency(void) {
    hc_sr04_config_t config = { /* ... */ };
    hc_sr04_sensor_t sensor;
    hc_sr04_init(&sensor, &config);

    uint32_t min_us = UINT32_MAX;
    uint32_t max_us = 0;
    uint64_t sum_us = 0;

    for (int i = 0; i < 1000; i++) {
        uint32_t start = esp_timer_get_time();
        hc_sr04_measurement_t measurement;
        hc_sr04_measure(&sensor, &measurement);
        uint32_t elapsed = esp_timer_get_time() - start;

        min_us = MIN(min_us, elapsed);
        max_us = MAX(max_us, elapsed);
        sum_us += elapsed;
    }

    uint32_t avg_us = sum_us / 1000;

    printf("Measurement latency: min=%u, avg=%u, max=%u μs\n",
          min_us, avg_us, max_us);

    // At 100 cm, echo is ~5800 μs
    // Total should be ~6000-7000 μs
    assert(avg_us >= 5800);
    assert(avg_us <= 30000);
}
```

**Test: Jitter and Noise**
```c
void test_measurement_jitter(void) {
    // Place sensor at fixed 100 cm distance
    // Collect N measurements
    // Calculate standard deviation

    uint32_t distances[100];
    for (int i = 0; i < 100; i++) {
        hc_sr04_measurement_t m;
        hc_sr04_measure(&sensor, &m);
        distances[i] = m.distance_cm;
    }

    hc_sr04_stats_t stats;
    hc_sr04_calculate_stats(distances, 100, &stats);

    uint32_t range = stats.max_distance_cm - stats.min_distance_cm;

    // Expect ±3-4 cm jitter indoors without filtering
    assert(range <= 8);
    printf("Jitter range: %u cm (expected ~4-8 cm)\n", range);
}
```

### 3.4 Stress Testing

**Test: High Frequency Measurements**
```c
void test_high_frequency_measurements(void) {
    hc_sr04_config_t config = { /* ... */ };
    hc_sr04_sensor_t sensor;
    hc_sr04_init(&sensor, &config);

    // Attempt 100 measurements as quickly as possible
    uint32_t success = 0;
    uint32_t failures = 0;

    for (int i = 0; i < 100; i++) {
        hc_sr04_measurement_t measurement;
        if (hc_sr04_measure(&sensor, &measurement) == HC_SR04_OK) {
            success++;
        } else {
            failures++;
        }
        // Note: No delay between measurements (violates 60ms recommendation)
    }

    printf("High frequency test: %u successes, %u failures\n", success, failures);

    // Even at high frequency, most should work (but will see errors)
    assert(success > 50);
}
```

**Test: Extended Operation**
```c
void test_extended_operation(void) {
    hc_sr04_config_t config = { /* ... */ };
    hc_sr04_sensor_t sensor;
    hc_sr04_init(&sensor, &config);

    uint32_t measurement_count = 0;
    uint32_t error_count = 0;
    uint32_t start_ms = xTaskGetTickCount();

    // Run for 1 hour (or test duration)
    while ((xTaskGetTickCount() - start_ms) < (3600000 / portTICK_PERIOD_MS)) {
        hc_sr04_measurement_t measurement;
        if (hc_sr04_measure(&sensor, &measurement) == HC_SR04_OK) {
            measurement_count++;
        } else {
            error_count++;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    printf("Extended operation: %u measurements, %u errors\n",
          measurement_count, error_count);

    // Should maintain low error rate over time
    float error_rate = (float)error_count / measurement_count;
    assert(error_rate < 0.05);  // Less than 5% error
}
```

---

## 4. SECURITY TESTING

### 4.1 Injection Testing

**Test: Echo Pulse Injection**
```
Procedure:
1. Connect function generator to Echo GPIO pin
2. Generate fake echo pulses (100-25000 μs range)
3. Observe if driver accepts spoofed distances
4. Verify bounds checking rejects invalid pulses

Expected: Driver validates all echo pulses against:
- Minimum width (116 μs)
- Maximum width (23200 μs)
- Plausible changes from previous measurement
```

**Test: Trigger Signal Noise**
```
Procedure:
1. Add 10 kHz noise to trigger pin
2. Measure if sensor still triggers correctly
3. Check for false measurements

Expected: Robust edge detection prevents noise interference
```

### 4.2 Timing Attack Resistance

**Test: TOCTOU Vulnerability Check**
```c
void test_toctou_safety(void) {
    volatile bool measurement_modified = false;
    hc_sr04_measurement_t measurement;

    hc_sr04_status_t status = hc_sr04_measure(&sensor, &measurement);

    if (status == HC_SR04_OK) {
        // ISR could potentially modify measurement here
        // (in vulnerable implementations)

        uint32_t distance_snapshot = measurement.distance_cm;

        // Use the snapshot
        process_distance(distance_snapshot);

        // Verify measurement wasn't modified
        assert(measurement.distance_cm == distance_snapshot);
    }
}
```

### 4.3 Race Condition Testing

**Test: Concurrent Access**
```c
void concurrent_measurement_task(void *arg) {
    for (int i = 0; i < 100; i++) {
        hc_sr04_measurement_t m;
        hc_sr04_get_last_measurement(&sensor, &m);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void test_concurrent_access(void) {
    // Create multiple tasks reading measurements
    for (int i = 0; i < 3; i++) {
        xTaskCreate(concurrent_measurement_task,
                   "reader",
                   2048, NULL, 1, NULL);
    }

    // One task performing measurements
    xTaskCreate(measurement_task,
               "measurement",
               2048, NULL, 2, NULL);

    // Run for 10 seconds
    vTaskDelay(10000 / portTICK_PERIOD_MS);

    // Verify no crashes or corruption occurred
    assert(sensor.measurement_count > 10);
}
```

---

## 5. PRODUCTION DEPLOYMENT CHECKLIST

- [ ] All unit tests passing
- [ ] Integration tests passing on target hardware (ESP32)
- [ ] Error rates < 5% under normal conditions
- [ ] Temperature compensation validated if enabled
- [ ] GPIO pins properly level-shifted for 3.3V (if needed)
- [ ] Decoupling capacitors installed on sensor power
- [ ] No blocking operations during critical sections
- [ ] ISR execution time < 10 μs
- [ ] Stack usage validated under all conditions
- [ ] Memory leaks tested over 24-hour runtime
- [ ] Measurement range verified for application (2-400 cm)
- [ ] Timeout handling tested with out-of-range scenarios
- [ ] Filtering/averaging implemented if jitter is unacceptable
- [ ] Security validation: bounds checking, plausibility checks
- [ ] Documentation updated with GPIO pin assignments
- [ ] Calibration procedure documented if needed

---

## 6. DEBUGGING AND DIAGNOSTICS

### Enable Debug Logging
```c
#define HC_SR04_DEBUG_ENABLED 1  // In hc_sr04.h

// Then use:
hc_sr04_debug_print_config(&config);
hc_sr04_debug_print_measurement(&measurement);
```

### Log-Based Analysis
```c
// Capture measurements to log file for analysis
FILE *log_file = fopen("/spiffs/measurements.log", "a");

for (int i = 0; i < 100; i++) {
    hc_sr04_measurement_t m;
    hc_sr04_measure(&sensor, &m);

    fprintf(log_file,
           "%u,%u,%d,%u,%s\n",
           m.distance_cm,
           m.echo_width_us,
           m.temperature_c,
           m.timestamp_us,
           m.temperature_compensated ? "1" : "0");
}

fclose(log_file);
```

### Serial Console Output
```c
// Real-time monitoring via UART
for (int i = 0; i < 1000; i++) {
    hc_sr04_measurement_t m;
    hc_sr04_status_t status = hc_sr04_measure(&sensor, &m);

    printf("%u,%u,%d\n", m.distance_cm, m.echo_width_us, m.temperature_c);

    vTaskDelay(100 / portTICK_PERIOD_MS);
}
```

