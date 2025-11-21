# HC-SR04 Ultrasonic Sensor: Comprehensive Technical Reference

## Executive Summary

The HC-SR04 is a widely-used ultrasonic distance sensor operating at 40 kHz. This document provides comprehensive technical specifications for C driver development on ESP32 and similar embedded systems, with emphasis on timing accuracy, edge case handling, security considerations, and memory safety.

---

## 1. TIMING DIAGRAM AND SIGNAL SPECIFICATIONS

### 1.1 Operational Timing Sequence

```
TRIGGER:     ┌─────────┐
             │ 10μs    │
       ───────────────────────────────────────────────→ time
             └─────────┘
                   ↑
                   └─ Minimum 10μs pulse required

ECHO:                        ┌──────────────────────────┐
       ────────────────────────                          └─────────
                   ↑                    ↑
                   └─ Pulse starts      └─ Pulse ends
                      (at receiver)
       |←───── Echo pulse width proportional to distance ────→|

Distance = (Echo_Width_μs / 58) cm
         = (Echo_Width_μs / 148) inches
         = (Echo_Width_μs × velocity_m_s) / 2,000,000
```

### 1.2 Trigger Pulse Requirements

| Parameter | Specification | Notes |
|-----------|---------------|-------|
| **Trigger Pulse Width** | 10 μs minimum (recommended: 10-20 μs) | TTL level (0V to 5V) |
| **Trigger Rising Edge** | 0V to 5V transition | Clean edge required |
| **Trigger Falling Edge** | 5V to 0V transition | Should complete before next trigger |
| **Trigger Frequency** | Max 50 Hz (20 ms cycle) | Higher rates cause cross-talk |
| **Recommended Cycle** | 60 ms or greater | Prevents overlapping measurements |
| **Trigger Setup Time** | 5-10 μs | Before pulse edge activation |

### 1.3 Echo Pulse Response Specifications

| Parameter | Specification | Notes |
|-----------|---------------|-------|
| **Echo Pulse Output** | TTL level, 0V to 5V | Open collector compatible |
| **Echo Pulse Width** | 150 μs to 25 ms | Proportional to distance |
| **Echo Rise Time** | <1 μs | Clean edge detection critical |
| **Echo Fall Time** | <1 μs | Clean edge detection critical |
| **Echo Delay (after trigger)** | 200-300 μs typical | Module processing time |
| **Maximum Wait Time** | ~30-38 ms | For 400 cm range at 20°C |
| **Output Impedance** | ~120 Ω | Low impedance, directly readable by GPIO |

**Echo Pulse Width Mapping:**
- 2 cm distance: ~116 μs
- 10 cm distance: ~580 μs
- 100 cm distance: ~5800 μs
- 200 cm distance: ~11600 μs
- 400 cm distance: ~23200 μs

---

## 2. MEASUREMENT SPECIFICATIONS

### 2.1 Operational Range

| Parameter | Specification |
|-----------|---------------|
| **Minimum Range** | 2 cm (blind zone) |
| **Maximum Range** | 400 cm (4 m) in ideal conditions |
| **Practical Maximum** | 200-250 cm (affected by object size, reflectivity) |
| **Measurement Angle** | ±15 degrees (cone) |
| **Frequency** | 40 kHz ±1% |

### 2.2 Accuracy and Resolution

| Parameter | Specification |
|-----------|---------------|
| **Accuracy** | ±3 mm (±0.3 cm) at optimal conditions |
| **Practical Accuracy** | ±1-2 cm typical |
| **Resolution** | 0.3 cm (approximately 1 bit per cm at typical ranges) |
| **Typical Jitter (Indoors)** | 2-4 cm without filtering |
| **Repeatability** | ±5 mm under controlled conditions |

### 2.3 Distance Calculation Formulas

**Standard Formula (at 20°C, sea level):**
```
Distance_cm = (Echo_Pulse_Width_μs / 58)
Distance_inch = (Echo_Pulse_Width_μs / 148)
Distance_m = (Echo_Pulse_Width_μs × 340) / 2,000,000
```

**Alternative Precise Formula:**
```
Distance_m = (Echo_Pulse_Width_μs × Speed_of_Sound_m_s) / 2,000,000

Where Speed_of_Sound varies with conditions:
- At 0°C: 331.4 m/s
- At 20°C: 343.2 m/s (standard reference)
- At 25°C: 346.3 m/s
```

---

## 3. TEMPERATURE COMPENSATION

### 3.1 Speed of Sound vs Temperature

The speed of sound is highly temperature-dependent, making temperature compensation critical for accuracy:

```c
// Speed of sound formula (m/s)
// ISO 9613-1 formula
speed_m_s = 331.4 + 0.606 * temperature_celsius

// More accurate formula with humidity
speed_m_s = 331.3 + 0.606 * temperature_celsius
            + 0.0124 * relative_humidity_percent
```

**Temperature Impact Example:**
- At 0°C: 331.4 m/s → 20 ms echo = 3.31 m
- At 20°C: 343.2 m/s → 20 ms echo = 3.43 m (11 cm difference!)
- At 40°C: 355.0 m/s → 20 ms echo = 3.55 m (24 cm difference!)

### 3.2 Temperature-Corrected Distance Calculation

```c
// Raw calculation at reference temperature (20°C)
float distance_raw_cm = (echo_us / 58.0);

// Temperature correction
float temp_correction_factor = (speed_20c / speed_actual);
float distance_corrected_cm = distance_raw_cm * temp_correction_factor;

// Where:
// speed_20c = 343.2 m/s (reference at 20°C = 0.034320 cm/μs)
// speed_actual = 331.4 + 0.606 * temperature_c
// speed_actual_cm_us = speed_actual / 10000
```

### 3.3 Implementation Considerations

- **Internal Temperature Sensor:** HC-SR04 has no internal temperature sensor; must add external (DS18B20, DHT22, HTU21, etc.)
- **Measurement Strategy:**
  - High precision: Temperature measurement before each distance measurement
  - Moderate precision: Sample temperature every 5-10 measurements
  - Basic: Assume room temperature (20°C) without correction
- **Latency Tradeoff:** Temperature measurement adds 750 ms - 2 seconds (depending on sensor)

---

## 4. ELECTRICAL SPECIFICATIONS

### 4.1 Power Requirements

| Parameter | Specification |
|-----------|---------------|
| **Operating Voltage** | 5V DC (±10%) |
| **Operating Current** | <15 mA typical, <20 mA peak |
| **Standby Current** | <2 mA |
| **Recommended Supply Capacitor** | 10-100 μF across VCC/GND |
| **Recommended Decoupling** | 0.1 μF ceramic near sensor |

### 4.2 GPIO Interface Specifications

| Parameter | Specification |
|-----------|---------------|
| **Trigger Input Voltage** | 5V TTL compatible |
| **Trigger Input Impedance** | >10 kΩ |
| **Echo Output Voltage** | 5V TTL (source impedance ~120 Ω) |
| **Echo Output Impedance** | ~120 Ω (low impedance) |
| **GPIO Level Shift** | For 3.3V microcontroller (ESP32), voltage divider or level shifter required on ECHO |
| **Maximum Input Current** | <1 mA per pin |

### 4.3 Level Shifting for 3.3V Systems (ESP32)

**Echo Pin Voltage Division (CRITICAL):**
```
    VCC (5V)
      |
      R1 (3.3kΩ)
      |
    ├─── Echo_GPIO (to ESP32)
      |
      R2 (2.2kΩ)
      |
      GND

Voltage divider ratio: 2.2/(3.3+2.2) = 0.4 = 40% voltage reduction
5V * 0.4 = 2.0V (safe for ESP32 <3.3V)
```

**Alternative: Use Dedicated Level Shifter IC**
- TXB0108 (8-channel)
- BSS138 MOSFETs (cost-effective)
- Diode clamp (pull-up to 3.3V with 1N4148 diode)

---

## 5. ESP32 GPIO TIMING REQUIREMENTS

### 5.1 Trigger Signal Generation

**Timing Constraints:**
```c
// Minimum pulse width: 10 μs
// Recommended pulse width: 10-20 μs
// Maximum pulse frequency: 50 Hz (20 ms period)
// Recommended minimum period: 60 ms

// Critical timing on ESP32:
// - Interrupt latency: ~2-10 μs (WiFi may increase to 50-100 μs)
// - GPIO toggle time: <1 μs
// - Timer resolution: 1 μs typical
// - delayMicroseconds() accuracy: ±5-10% without WiFi
```

**Best Practices for Trigger Generation:**
1. Use direct GPIO register writes for <100 ns latency
2. Disable WiFi during measurements if sub-microsecond accuracy needed
3. Account for OS task switching (FreeRTOS adds ~50-100 μs jitter)

### 5.2 Echo Pulse Measurement Methods

#### Method 1: Blocking pulseIn() - Simple but Not Ideal
```c
uint32_t echo_us = pulseIn(echo_pin, HIGH, 30000);  // 30 ms timeout
if (echo_us == 0) {
    // Timeout occurred - no echo received
}
```
- **Pros:** Simple, single function call
- **Cons:** Blocking (stops all other processing), ~2-10 μs jitter

#### Method 2: GPIO Interrupt + Timer - Recommended
```c
// ISR on rising edge: capture timer value
// ISR on falling edge: calculate pulse width
// Async measurement, non-blocking
```
- **Pros:** Non-blocking, 1-2 μs jitter possible
- **Cons:** More complex code, ISR management required

#### Method 3: Input Capture Mode (if available) - Most Accurate
- Hardware timer with input capture peripheral
- <500 ns jitter possible
- Not available on all GPIO pins (check datasheet)

### 5.3 Interrupt-Safe Timing Implementation

**Critical Timing Sections:**
```c
// For microsecond-accurate measurements:
portDISABLE_INTERRUPTS();  // Disable all interrupts
{
    // Read timer or GPIO value
    // Duration: <10 μs typically
}
portENABLE_INTERRUPTS();

// Alternative for IRAM_ATTR functions:
// Place ISR in IRAM using IRAM_ATTR to avoid flash access
void IRAM_ATTR echo_isr_handler(void* arg) {
    // Code executes from RAM, faster
}

// Use fast GPIO macros:
// GPIO_OUTPUT_SET(pin, 1);  // vs digitalWrite() - 10-100x faster
```

---

## 6. EDGE CASES AND ERROR HANDLING

### 6.1 No Echo Received (Timeout Condition)

**Causes:**
- Object out of range (>4 m)
- Sound-absorbing material (foam, fabric)
- Angled surface deflecting sound away
- Electrical noise or interference
- Sensor malfunction
- GPIO wiring issue

**Handling Strategy:**
```c
#define ECHO_TIMEOUT_US 30000  // 30 ms timeout = 5.1 m max range

uint32_t echo_width = measure_echo_pulse(echo_pin, ECHO_TIMEOUT_US);

if (echo_width == 0) {
    // Timeout - could not measure
    return SENSOR_ERROR_NO_ECHO;
} else if (echo_width < 116) {
    // Measurement too short - likely electrical noise
    return SENSOR_ERROR_OUT_OF_RANGE_MIN;
} else if (echo_width > 25000) {
    // Measurement too long - likely out of range or stuck
    return SENSOR_ERROR_OUT_OF_RANGE_MAX;
}
```

### 6.2 Short Pulses and Electrical Noise

**Problem:** Stray pulses, EMI can cause false edges

**Mitigation:**
```c
// Require minimum pulse width
#define MIN_PULSE_WIDTH_US 100

// Add input filtering
#define DEBOUNCE_COUNTS 3  // Require 3 consecutive high reads

// Check pulse width in ISR
if (pulse_width_us < MIN_PULSE_WIDTH_US) {
    // Reject as noise
    return;
}
```

### 6.3 Multiple Echoes (Multi-Path Reflections)

**Problem:** Sound bounces off multiple surfaces, creating multiple echo pulses

**Solution:**
```c
// Measure first echo pulse only (shortest distance)
// Disable interrupt after first pulse captured
void echo_isr(void* arg) {
    if (state == WAITING_FOR_ECHO) {
        timestamp_rising = esp_timer_get_time();
        state = MEASURING_ECHO;
        gpio_set_intr_type(echo_pin, GPIO_INTR_NEGEDGE);
    } else if (state == MEASURING_ECHO) {
        timestamp_falling = esp_timer_get_time();
        pulse_width = timestamp_falling - timestamp_rising;
        state = ECHO_COMPLETE;
        gpio_intr_disable(echo_pin);  // Stop measuring
    }
}
```

### 6.4 Trigger Signal Not Cleaned (Reflection on Trigger Line)

**Problem:** Trigger pulse bounces back, causing false triggers

**Mitigation:**
```c
// Keep trigger low for at least 5 μs before next measurement
#define TRIGGER_SETUP_DELAY_US 5
#define MIN_MEASUREMENT_CYCLE_MS 20  // 50 Hz max

// Ensure clean transitions
gpio_set_level(trigger_pin, 0);
ets_delay_us(TRIGGER_SETUP_DELAY_US);
gpio_set_level(trigger_pin, 1);
ets_delay_us(10);  // 10 μs pulse
gpio_set_level(trigger_pin, 0);
ets_delay_us(TRIGGER_SETUP_DELAY_US);
```

### 6.5 Cross-Talk Between Multiple Sensors

**Problem:** Multiple HC-SR04 sensors in close proximity interfere

**Mitigation:**
```c
// Stagger trigger timing
sensor1_trigger(); vTaskDelay(10 / portTICK_PERIOD_MS);
sensor2_trigger(); vTaskDelay(10 / portTICK_PERIOD_MS);
sensor3_trigger();

// Increase measurement cycle (60+ ms between triggers)
// Physically separate sensors (>50 cm apart)
// Use shielding around sensors if possible
```

---

## 7. SECURITY CONSIDERATIONS

### 7.1 TOCTOU (Time-of-Check to Time-of-Use) in Timing Measurement

While not a traditional cybersecurity vulnerability, TOCTOU-like patterns can cause timing inconsistencies:

**Vulnerable Pattern:**
```c
// VULNERABLE: Check timeout then use measurements
if (echo_timeout_us < MAX_TIMEOUT) {  // Time-of-check
    // ... do work ...
    distance = calculate_distance(echo_timeout_us);  // Time-of-use
    // Problem: echo_timeout_us could be modified by ISR between check and use
}
```

**Safe Pattern:**
```c
// SAFE: Capture value atomically, then use
portDISABLE_INTERRUPTS();
uint32_t echo_snapshot = echo_us;
portENABLE_INTERRUPTS();

if (echo_snapshot < MAX_TIMEOUT && echo_snapshot > MIN_TIMEOUT) {
    distance = calculate_distance(echo_snapshot);
}
```

### 7.2 GPIO Race Conditions

**Race Condition 1: Concurrent ISR + Main Code Access**
```c
// VULNERABLE: ISR and main both access echo_us
void IRAM_ATTR echo_isr(void* arg) {
    echo_us = current_time;  // Write
}

void loop() {
    distance = echo_us / 58;  // Read - could be half-written value!
}
```

**Safe Implementation:**
```c
// Use atomic access or mutex
volatile uint32_t echo_us = 0;
SemaphoreHandle_t echo_mutex = xSemaphoreCreateMutex();

void IRAM_ATTR echo_isr(void* arg) {
    // For ISR: use xSemaphoreGiveFromISR() or atomic compare-and-swap
    atomic_store(&echo_us, current_time);
}

void loop() {
    xSemaphoreTake(echo_mutex, portMAX_DELAY);
    uint32_t snapshot = echo_us;
    xSemaphoreGive(echo_mutex);
    distance = snapshot / 58;
}
```

**Race Condition 2: GPIO Pin State Change During Read**
```c
// VULNERABLE: Pin state could change between reads
if (gpio_get_level(echo_pin) == 1) {  // Read 1
    timestamp = micros();
    // Pin could go low here!
    if (gpio_get_level(echo_pin) == 1) {  // Read 2
        // Assumption violated - dangerous
    }
}
```

**Safe Implementation:**
```c
// Use interrupt-driven edge detection
void echo_isr(void* arg) {
    // ISR guarantees detection of state change
    // No need for multiple reads
}
```

### 7.3 Interrupt Handler Safety

**Issues:**
- Stack overflow if ISR not carefully sized
- Shared variable corruption if not atomic
- Deadlock if ISR tries to acquire mutex main code holds

**Safe ISR Pattern:**
```c
// SAFE: Minimal, atomic, no blocking calls
void IRAM_ATTR echo_isr(void* arg) {
    static uint32_t rising_time;
    uint32_t current_time = esp_timer_get_time();

    if (gpio_get_level(echo_pin) == 1) {
        rising_time = current_time;
    } else {
        uint32_t pulse_width = current_time - rising_time;
        // Atomic store or use xQueueSendFromISR if needed
        atomic_store(&g_echo_us, pulse_width);
    }
}
```

### 7.4 Input Validation and Bounds Checking

**Sensor Spoofing/Injection:**
```c
// Someone could externally inject pulses on echo line
// Validate measurement sanity

#define MIN_VALID_ECHO_US 116   // 2 cm minimum
#define MAX_VALID_ECHO_US 23200 // 400 cm maximum

uint32_t measure_distance_safe(uint32_t echo_us) {
    // Bounds check
    if (echo_us < MIN_VALID_ECHO_US || echo_us > MAX_VALID_ECHO_US) {
        return ERROR_OUT_OF_RANGE;
    }

    // Range check (allows consecutive measurement consistency checking)
    static uint32_t last_valid_distance = 0;
    uint32_t distance = echo_us / 58;

    // Reject if change >50cm between measurements
    if (last_valid_distance > 0 &&
        abs(distance - last_valid_distance) > 50) {
        return ERROR_IMPLAUSIBLE_CHANGE;
    }

    last_valid_distance = distance;
    return distance;
}
```

---

## 8. MEMORY SAFETY CONSIDERATIONS

### 8.1 Timer Overflow Protection

**Problem:** Timer values eventually wrap around
```c
// VULNERABLE: Overflow not handled
uint32_t start_time = micros();
while (gpio_get_level(echo_pin) == 1) {
    uint32_t current = micros();
    if (current - start_time > 30000) break;  // WRONG if overflow!
}
```

**Safe Implementation:**
```c
// Use signed arithmetic for overflow-safe comparison
int32_t elapsed = (int32_t)(current_time - start_time);
if (elapsed > TIMEOUT_US) {
    // Safe even with wrap-around
}

// Or track and compare absolute positions
if (current_time >= timeout_deadline) {
    // Safer for single-direction overflow
}
```

### 8.2 Integer Overflow in Distance Calculation

**Problem:** Echo width (0-25000 μs) * speed of sound can overflow

```c
// VULNERABLE: 16-bit overflow
uint16_t echo_us = 20000;
uint16_t distance_cm = (echo_us * 343) / 2000000;  // Overflow in intermediate!
```

**Safe Implementation:**
```c
// Use 32-bit integers for intermediate calculations
uint32_t echo_us = 20000;
uint32_t distance_cm = (echo_us * 343) / 2000000;  // Safe

// Or scale calculation to avoid overflow
uint32_t distance_cm = echo_us / 58;  // Pre-calculated constant
```

### 8.3 Stack Overflow in ISR

**Problem:** Deep call stacks in ISR context
```c
// VULNERABLE: Excessive stack use in ISR
void IRAM_ATTR bad_isr(void* arg) {
    uint8_t large_buffer[1024];  // Stack overflow risk!
    process_data(large_buffer);
    // ...
}
```

**Safe Implementation:**
```c
// Use static or global buffers
static uint8_t isr_buffer[256];  // Pre-allocated, reused

void IRAM_ATTR good_isr(void* arg) {
    // Minimal local variables
    uint32_t time_value = esp_timer_get_time();
    atomic_store(&g_timestamp, time_value);
}

// Process heavy data outside ISR
void process_measurements(void) {
    uint32_t ts = atomic_load(&g_timestamp);
    // Heavy processing here
}
```

### 8.4 Interrupt Re-entrancy and Atomicity

**Problem:** Reading/writing 32-bit values not atomic on 32-bit systems
```c
// VULNERABLE: 64-bit write in 32-bit ISR could be interrupted
volatile uint64_t timestamp_us = 0;

void IRAM_ATTR isr_handler(void* arg) {
    timestamp_us = esp_timer_get_time();  // 64-bit write in ISR!
    // Could be interrupted mid-write
}

uint64_t get_timestamp(void) {
    return timestamp_us;  // Could read half-updated value
}
```

**Safe Implementation:**
```c
// Use 32-bit values (atomic on 32-bit systems)
volatile uint32_t timestamp_ms = 0;

void IRAM_ATTR isr_handler(void* arg) {
    timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

// Or use atomic operations
atomic_t timestamp_us = ATOMIC_INIT(0);

void IRAM_ATTR isr_handler(void* arg) {
    atomic_set(&timestamp_us, esp_timer_get_time());
}

uint64_t get_timestamp(void) {
    return atomic_read(&timestamp_us);
}
```

### 8.5 Buffer Overflow in Result Storage

**Problem:** Storing measurement results without bounds checking
```c
// VULNERABLE: No buffer bounds
#define MAX_MEASUREMENTS 10
uint32_t distances[MAX_MEASUREMENTS];
uint32_t measurement_count = 0;

void store_measurement(uint32_t distance) {
    distances[measurement_count++];  // Buffer overflow if >10 measurements
}
```

**Safe Implementation:**
```c
#define MAX_MEASUREMENTS 10
uint32_t distances[MAX_MEASUREMENTS];
uint32_t measurement_count = 0;

void store_measurement(uint32_t distance) {
    if (measurement_count < MAX_MEASUREMENTS) {
        distances[measurement_count++] = distance;
    } else {
        // Handle overflow: circular buffer or error
        distances[measurement_count % MAX_MEASUREMENTS] = distance;
    }
}
```

---

## 9. PRACTICAL CONSIDERATIONS

### 9.1 Environmental Factors Affecting Measurement

| Factor | Impact | Mitigation |
|--------|--------|-----------|
| **Temperature** | ±0.2%/°C variation | Use temperature compensation |
| **Humidity** | 0.1-0.5% impact | Minor, usually ignored |
| **Wind** | ±2-5% at high speed | Use averaging/filtering |
| **Object Material** | ±10-30% (foam absorbs) | Use reflective targets if possible |
| **Object Angle** | ±20-50% if oblique | Face sensor directly at object |
| **Ambient Noise** | ±5% at high ambient | Use shielding, increase signal level |

### 9.2 Filtering and Averaging Strategies

```c
// Simple moving average (low-pass filter)
#define FILTER_SIZE 10
uint32_t distance_history[FILTER_SIZE];
uint32_t filter_index = 0;

uint32_t get_filtered_distance(uint32_t raw_distance) {
    distance_history[filter_index % FILTER_SIZE] = raw_distance;
    filter_index++;

    uint64_t sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += distance_history[i];
    }
    return sum / FILTER_SIZE;
}

// Median filter (more robust to outliers)
#define FILTER_SIZE 5
uint32_t get_median_distance(uint32_t raw_distance) {
    static uint32_t distances[FILTER_SIZE];
    static uint32_t index = 0;

    distances[index % FILTER_SIZE] = raw_distance;
    index++;

    // Sort and return median
    uint32_t sorted[FILTER_SIZE];
    memcpy(sorted, distances, sizeof(distances));
    // qsort on sorted array
    return sorted[FILTER_SIZE/2];
}
```

### 9.3 Deadzone and Blind Zone Management

```c
// HC-SR04 has 2 cm minimum dead zone
#define BLIND_ZONE_CM 2
#define MAX_RANGE_CM 400

uint32_t get_safe_distance(uint32_t distance_cm) {
    if (distance_cm < BLIND_ZONE_CM) {
        return 0;  // or ERROR_TOO_CLOSE
    }
    if (distance_cm > MAX_RANGE_CM) {
        return ERROR_OUT_OF_RANGE;
    }
    return distance_cm;
}
```

---

## 10. RECOMMENDED MEASUREMENT CYCLE

```
┌─────────────────────────────────────────┐
│ Measurement Cycle (60ms recommended)    │
├─────────────────────────────────────────┤
│ 1. Assert Trigger (0→1)   : 0 ms        │
│ 2. Hold Trigger High      : 10 μs       │
│ 3. Release Trigger (1→0)  : 10 μs       │
│ 4. Wait for Echo Response : 200-300 μs  │
│ 5. Echo High Period       : 150-25000 μs│
│ 6. Echo Goes Low          : Done        │
│ 7. Wait before next cycle : 45-50 ms    │
└─────────────────────────────────────────┘
Total: 60+ ms between measurements
```

---

## 11. QUICK REFERENCE: CRITICAL VALUES

| Parameter | Value | Units | Notes |
|-----------|-------|-------|-------|
| Trigger Width | 10-20 | μs | Minimum 10 μs |
| Trigger Cycle | 20+ | ms | 50 Hz max frequency |
| Measurement Cycle | 60+ | ms | Recommended minimum |
| Echo Timeout | 30 | ms | Corresponds to 5.1 m |
| Speed of Sound (20°C) | 343.2 | m/s | Varies with temperature |
| Distance Divisor (20°C) | 58 | (unitless) | For μs → cm conversion |
| Min Valid Range | 2 | cm | Blind zone |
| Max Typical Range | 400 | cm | Ideal conditions |
| Practical Accuracy | 3 | mm | ±3mm at optimal range |
| Typical Jitter | 2-4 | cm | Indoors, no filtering |
| Operating Voltage | 5 | V | DC, ±10% tolerance |
| Operating Current | 15 | mA | Average, <20 mA peak |
| GPIO Rise Time | <1 | μs | For clean edges |
| GPIO Fall Time | <1 | μs | For clean edges |
| Interrupt Latency (no WiFi) | 2-10 | μs | ESP32 typical |
| Interrupt Latency (WiFi on) | 50-100 | μs | WiFi interference |

---

## References

1. **Official Documentation:**
   - HC-SR04 User Manual (v1.0)
   - SparkFun HC-SR04 Datasheet

2. **Speed of Sound:**
   - ISO 9613-1 Standard
   - Physics formulas for temperature/humidity adjustment

3. **ESP32-Specific:**
   - Espressif ESP32 Technical Reference Manual
   - FreeRTOS interrupt handling documentation
   - ESP-IDF GPIO and Timer APIs

4. **Best Practices:**
   - Community implementations (GitHub: Hossam-Elbahrawy/Ultrasonic-sensor-driver)
   - STM32 timer input capture implementations
   - Raspberry Pi GPIO timing considerations

