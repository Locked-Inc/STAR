# HC-SR04 Ultrasonic Sensor - Quick Reference Guide

## Physical Specifications

| Spec | Value |
|------|-------|
| Operating Voltage | 5V DC (±10%) |
| Current Consumption | <15 mA typical |
| Operating Frequency | 40 kHz |
| Measurement Range | 2 cm - 400 cm |
| Accuracy | ±3 mm |
| Measuring Angle | ±15° |
| Dimensions | 45.3mm × 20.4mm × 17.8mm |

## Timing Summary

### Trigger Signal
- **Width:** 10-20 μs (minimum 10 μs required)
- **Setup time before pulse:** 5 μs
- **Cleanup time after pulse:** 5 μs
- **Frequency:** Maximum 50 Hz (20 ms period)
- **Recommended cycle:** 60+ ms between measurements

### Echo Signal
- **Output:** TTL 5V logic level
- **Pulse Width Range:** 150 μs (2 cm) to 25 ms (400 cm)
- **Response Delay:** 200-300 μs after trigger
- **Output Impedance:** ~120 Ω
- **Rise/Fall Time:** <1 μs

## Distance Calculation Formulas

### Quick Formula (at 20°C)
```
Distance (cm) = Echo_Pulse_Width (μs) / 58
Distance (inches) = Echo_Pulse_Width (μs) / 148
```

### Temperature Compensated Formula
```
Speed_of_Sound (m/s) = 331.4 + 0.606 * Temperature (°C)
Distance (cm) = (Echo_Width (μs) × Speed (m/s)) / 2,000,000
```

## ESP32 Pinout Recommendations

```
HC-SR04           ESP32
-------           -----
VCC               5V (or 3.3V with 1.8kΩ pull-down)
GND               GND
TRIG              GPIO5
ECHO              GPIO18 (with voltage divider!)
```

### ECHO Pin Level Shifting (CRITICAL!)
HC-SR04 outputs 5V, ESP32 GPIO maximum 3.3V

**Voltage Divider (R1=3.3kΩ, R2=2.2kΩ):**
```
    5V
    |
    R1 (3.3kΩ)
    |
    +----> GPIO18 (to ESP32)
    |
    R2 (2.2kΩ)
    |
    GND

Output: 5V × 2.2/(3.3+2.2) = 2.0V (safe for ESP32)
```

## Basic Initialization Code

```c
#include "hc_sr04.h"

// Configure sensor
hc_sr04_config_t config = {
    .trigger_pin = GPIO_NUM_5,
    .echo_pin = GPIO_NUM_18,
    .echo_timeout_us = 30000,
    .use_interrupts = false,
    .measurement_cycle_ms = 60,
    .temperature.enabled = false,
};

// Initialize
hc_sr04_sensor_t sensor;
hc_sr04_init(&sensor, &config);

// Measure
hc_sr04_measurement_t measurement;
hc_sr04_status_t status = hc_sr04_measure(&sensor, &measurement);

if (status == HC_SR04_OK) {
    printf("Distance: %u cm\n", measurement.distance_cm);
} else {
    printf("Error: %s\n", hc_sr04_strerror(status));
}

// Clean up
hc_sr04_deinit(&sensor);
```

## Temperature Compensation Code

```c
// With DS18B20 temperature sensor
float temperature = read_temperature_from_ds18b20();

sensor.config.temperature.enabled = true;
sensor.config.temperature.ambient_temperature_c = (int32_t)temperature;

hc_sr04_measurement_t measurement;
hc_sr04_measure(&sensor, &measurement);

// Distance automatically compensated based on temperature
printf("Distance: %u cm (at %.1f°C)\n",
       measurement.distance_cm, temperature);
```

## Error Handling Examples

```c
hc_sr04_measurement_t measurement;
hc_sr04_status_t status = hc_sr04_measure(&sensor, &measurement);

switch (status) {
case HC_SR04_OK:
    // Valid measurement
    printf("Distance: %u cm\n", measurement.distance_cm);
    break;

case HC_SR04_ERR_TIMEOUT:
    // Object out of range (>400 cm) or sensor issue
    printf("No echo received - object too far or missing\n");
    break;

case HC_SR04_ERR_OUT_OF_RANGE_MIN:
    // Object too close (<2 cm)
    printf("Object too close\n");
    break;

case HC_SR04_ERR_INVALID_PULSE:
    // Electrical noise detected
    printf("Invalid pulse - likely noise\n");
    break;

case HC_SR04_ERR_IMPLAUSIBLE_CHANGE:
    // Distance changed >50 cm from previous
    printf("Suspicious distance change detected\n");
    break;

default:
    printf("Error: %s\n", hc_sr04_strerror(status));
}
```

## Filtering and Averaging

```c
#define MEASUREMENT_COUNT 10

uint32_t distances[MEASUREMENT_COUNT];

// Collect measurements
for (int i = 0; i < MEASUREMENT_COUNT; i++) {
    hc_sr04_measurement_t m;
    if (hc_sr04_measure(&sensor, &m) == HC_SR04_OK) {
        distances[i] = m.distance_cm;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Get median (noise robust)
uint32_t filtered_distance;
hc_sr04_filter_measurements(distances, MEASUREMENT_COUNT, &filtered_distance);

// Get statistics
hc_sr04_stats_t stats;
hc_sr04_calculate_stats(distances, MEASUREMENT_COUNT, &stats);

printf("Min: %u, Max: %u, Avg: %u, Median: %u cm\n",
       stats.min_distance_cm,
       stats.max_distance_cm,
       stats.avg_distance_cm,
       stats.median_distance_cm);
```

## Asynchronous (Non-blocking) Measurements

```c
// Configure for interrupt mode
hc_sr04_config_t config = {
    /* ... */
    .use_interrupts = true,
};

hc_sr04_init(&sensor, &config);

// Trigger measurement (returns immediately)
hc_sr04_trigger_async(&sensor);

// Do other work while measurement completes
perform_other_tasks();

// Check if ready and get result
if (hc_sr04_measurement_ready(&sensor)) {
    hc_sr04_measurement_t measurement;
    hc_sr04_get_last_measurement(&sensor, &measurement);
    printf("Distance: %u cm\n", measurement.distance_cm);
}
```

## Common Issues and Solutions

### Issue: Timeout (No Echo Received)
**Causes:**
- Object beyond 400 cm
- Sound-absorbing material (foam, fabric)
- Sensor wiring issue
- Trigger pulse not reaching sensor

**Solutions:**
- Verify object distance < 400 cm
- Use reflective targets
- Check wiring and connections
- Verify 5V power supply to sensor
- Test with voltmeter on trigger line

### Issue: Large Jitter (±4+ cm variation)
**Causes:**
- Electrical noise
- Environmental reflections
- Motor interference
- Poor sensor mounting

**Solutions:**
- Add 10-100 μF capacitor across VCC/GND
- Keep away from motors/WiFi
- Use averaging/median filtering
- Securely mount sensor
- Increase measurement delay (60+ ms)

### Issue: Consistently Wrong Distance
**Causes:**
- Temperature not compensated
- Angled measurement
- Incorrect formula applied
- Sensor calibration needed

**Solutions:**
- Enable temperature compensation
- Face sensor perpendicular to object
- Use formula: distance = echo_μs / 58
- Calibrate with known distance

### Issue: Rapid Inconsistent Readings
**Causes:**
- Measurement cycle too fast (<20 ms)
- Cross-talk from multiple sensors
- Environmental reflections
- Electrical interference

**Solutions:**
- Increase delay to 60+ ms between measurements
- Stagger multiple sensor triggers
- Add shielding
- Use median filtering

## GPIO Timing Considerations

### ESP32 Specific
- **Interrupt Latency (no WiFi):** 2-10 μs
- **Interrupt Latency (WiFi active):** 50-100 μs
- **GPIO Rise/Fall Time:** <1 μs
- **Timer Resolution:** 1 μs
- **delayMicroseconds() Accuracy:** ±5-10% without WiFi

### Critical Sections
```c
// Disable interrupts for microsecond-accurate timing
portDISABLE_INTERRUPTS();
{
    // Critical timing code (< 10 μs typically)
    uint32_t time_value = esp_timer_get_time();
}
portENABLE_INTERRUPTS();
```

## Memory Safety Notes

1. **Always initialize structures before use:**
   ```c
   hc_sr04_measurement_t measurement = {0};
   ```

2. **Check status codes before using results:**
   ```c
   if (hc_sr04_measure(&sensor, &measurement) != HC_SR04_OK) {
       return;  // Don't use measurement
   }
   ```

3. **Use atomic access for shared variables:**
   ```c
   // ISR and main code both access sensor->echo_width_us
   // Driver uses atomic_load/atomic_store
   ```

4. **Prevent buffer overflows:**
   ```c
   // Use hc_sr04_filter_measurements() safely
   if (count > 64) {
       return ERROR;  // Array size limit
   }
   ```

## Performance Targets

| Metric | Target |
|--------|--------|
| Measurement Latency | 5-8 ms per distance |
| Jitter (unfiltered) | ±2-4 cm indoors |
| Accuracy (compensated) | ±3 mm optimal, ±1 cm typical |
| Error Rate | < 5% |
| ISR Execution Time | < 1 μs |
| Memory Footprint | ~2 KB RAM, ~5 KB code |

## Troubleshooting Checklist

- [ ] VCC is 5V (or 3.3V with level shifter)
- [ ] GND properly connected
- [ ] TRIG GPIO configured as output
- [ ] ECHO GPIO configured as input with voltage divider
- [ ] Decoupling capacitor (10-100 μF) on VCC
- [ ] Sensor mounted securely
- [ ] Measurement delay >= 60 ms
- [ ] Object in range (2-400 cm)
- [ ] Object reflective enough (not foam/fabric)
- [ ] No electrical noise source nearby
- [ ] WiFi disabled during timing-critical measurements
- [ ] Temperature compensation enabled if accuracy critical

## Reference Documents

- **Technical Details:** HC-SR04_TECHNICAL_REFERENCE.md
- **Security & Testing:** HC-SR04_SECURITY_AND_TESTING.md
- **API Reference:** hc_sr04.h (header file)
- **Example Code:** hc_sr04_example.c

