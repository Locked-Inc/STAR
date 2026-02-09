# HC-SR04 Ultrasonic Sensor Driver Implementation Plan

**Status:** 🔴 NOT STARTED - Sensors assigned but no driver
**Priority:** 🔴 HIGH - Required for obstacle detection and collision avoidance
**Estimated Effort:** 12-16 hours (implementation + testing + documentation)
**Dependencies:** IRQ driver, GPIO HAL, CMT/TMR for timing

---

## Executive Summary

The STAR robot requires **4 HC-SR04 ultrasonic sensors** for 360° obstacle detection and collision avoidance. Pin assignments have been finalized with **IRQ-capable pins** (P44-P47/IRQ12-15) for precise ECHO pulse timing. Currently, **no driver exists** - sensors are physically wired but non-functional.

This document provides a comprehensive plan to implement a production-ready HC-SR04 driver with multi-sensor support, collision detection zones, and robust error handling.

---

## Problem Statement

### Current Situation

| Sensor ID | Position | TRIG Pin | ECHO Pin | Status |
|-----------|----------|----------|----------|--------|
| Sonar 0 | Front Left | P55 (pin 39) | P44/IRQ12 (pin 90) | ❌ **No driver** |
| Sonar 1 | Front Right | P54 (pin 40) | P45/IRQ13 (pin 89) | ❌ **No driver** |
| Sonar 2 | Rear Left | P53 (pin 41) | P46/IRQ14 (pin 88) | ❌ **No driver** |
| Sonar 3 | Rear Right | P52 (pin 42) | P47/IRQ15 (pin 87) | ❌ **No driver** |

### Impact

Without ultrasonic sensors:
- ❌ No obstacle detection (robot blind)
- ❌ Cannot avoid collisions
- ❌ No cliff detection
- ❌ Cannot implement safe autonomous navigation
- ❌ No emergency stop on close objects
- ❌ Limited to manual control only

### Pin Assignment Design Rationale

**Previous issue:** Ultrasonic ECHO pins were incorrectly assigned to PC4, PC5, P50, P51 (NO IRQ capability).

**Solution:** Moved motor fault detection to polling, freed IRQ12-15 for ultrasonic sensors.

**Why IRQ is critical:**
- HC-SR04 ECHO pulse width = 150µs to 25ms (1cm to 400cm distance)
- 1µs timing error = 0.17mm distance error
- Polling at 100Hz (10ms) would miss short pulses entirely
- IRQ provides microsecond-accurate edge detection

---

## HC-SR04 Sensor Technical Specifications

### Electrical Characteristics

| Parameter | Value | Notes |
|-----------|-------|-------|
| Operating Voltage | 5V DC | PCB must provide 5V rail |
| Quiescent Current | <2 mA | Low power |
| Working Current | 15 mA | During ranging |
| Ultrasonic Frequency | 40 kHz | Standard ultrasonic |
| Max Range | 400 cm (4 meters) | Datasheet spec |
| Min Range | 2 cm | Objects closer saturate sensor |
| Measuring Angle | 15° cone | Narrow beam |
| Trigger Input | 10µs HIGH pulse | GPIO output |
| ECHO Output | 5V TTL | RX72N tolerates 5V on IRQ pins |

### Timing Characteristics

**Measurement cycle:**
1. **Trigger:** MCU outputs 10µs HIGH pulse on TRIG pin
2. **Burst:** Sensor transmits 8-cycle 40kHz ultrasonic burst
3. **ECHO pulse:** ECHO pin goes HIGH
4. **Echo received:** ECHO pin goes LOW
5. **Pulse width:** Time HIGH = round-trip time of ultrasound

**Distance calculation:**
```
Distance (cm) = Pulse Width (µs) / 58
Distance (inches) = Pulse Width (µs) / 148

Speed of sound = 343 m/s @ 20°C
Round trip = 2 × distance
Time per cm = 2 / 343 = 58.3 µs/cm
```

**Maximum pulse width:** 25 ms (out of range)

**Minimum pulse width:** ~116 µs (2cm minimum range)

**Recommended measurement interval:** 60ms between triggers (avoid crosstalk)

---

## Architecture Design

### Driver Layering

```
Application Layer (Obstacle Detection)
    ↓ (collision zones, filtering)
HC-SR04 High-Level API (lib/rx_hal/inc/rx_hc_sr04.h)
    ↓ (multi-sensor manager, distance conversion)
HC-SR04 Low-Level Driver (lib/rx_hal/src/rx_hc_sr04.c)
    ↓ (IRQ handlers, pulse timing)
RX72N HAL (GPIO, IRQ, CMT)
    ↓ (register access)
RX72N Hardware (ICU, GPIO ports)
```

### Multi-Sensor Management

**Challenge:** 4 sensors cannot trigger simultaneously (ultrasonic crosstalk).

**Solution:** Round-robin triggering with 60ms stagger:

```
Time (ms):   0    60   120   180   240   (cycle repeats)
Sonar 0:     TRIG      TRIG      TRIG
Sonar 1:          TRIG      TRIG      TRIG
Sonar 2:               TRIG      TRIG      TRIG
Sonar 3:                    TRIG      TRIG      TRIG

Full cycle: 240ms (4.16 Hz per sensor)
```

**Benefits:**
- No ultrasonic interference between sensors
- All sensors updated every 240ms (adequate for obstacle detection)
- Deterministic timing (easy to synchronize with control loop)

### State Machine

**Per-sensor state:**

```
    [IDLE]
      ↓ (trigger requested)
    [TRIGGERING] - 10µs TRIG pulse
      ↓ (TRIG complete)
    [WAITING_FOR_ECHO_RISE] - wait for ECHO HIGH
      ↓ (ECHO rises OR 25ms timeout)
    [MEASURING] - ECHO pulse active
      ↓ (ECHO falls OR 25ms timeout)
    [COMPLETE] - distance calculated
      ↓
    [IDLE]
```

**Timeout handling:**
- If ECHO never rises: sensor error (disconnected or faulty)
- If ECHO never falls: out of range (>400cm)

---

## Implementation Plan

### Phase 1: IRQ and GPIO HAL Verification

**Objective:** Verify IRQ12-15 and GPIO pins are correctly configured.

#### Prerequisites

**GPIO HAL** (already implemented):
- [ ] Verify `rx_gpio_init()` supports pins P52-P55 (TRIG outputs)
- [ ] Verify `rx_gpio_write()` can toggle TRIG pins
- [ ] Test 10µs pulse generation accuracy

**IRQ HAL** (needs implementation or verification):
- [ ] Check if `rx_icu.h` exists (Interrupt Controller Unit HAL)
- [ ] If missing, create IRQ HAL: `lib/rx_hal/inc/rx_icu.h`, `lib/rx_hal/src/rx_icu.c`
- [ ] Implement IRQ12-15 registration: `rx_icu_register_irq(irq_num, callback, ctx)`
- [ ] Implement edge detection configuration: rising, falling, both edges
- [ ] Verify IRQ latency <5µs (critical for timing accuracy)

#### Files to Check/Create

**1. GPIO HAL verification** (existing)
- `lib/rx_hal/inc/gpio.h` - Check P52-P55 pin definitions
- `lib/rx_hal/src/gpio.c` - Verify output control functions

**2. IRQ HAL** (may need to create)
- `lib/rx_hal/inc/rx_icu.h` - NEW if doesn't exist
- `lib/rx_hal/src/rx_icu.c` - NEW if doesn't exist
- Reference: RX72N Manual **Ch23** (Multi-Function Pin Controller), **Ch24** (Interrupt Controller)

#### Verification Checklist - Phase 1

- [ ] GPIO pins P52-P55 (TRIG) configured as outputs
- [ ] GPIO pins P44-P47 (ECHO) configured as IRQ inputs
- [ ] IRQ12-15 can register callbacks
- [ ] IRQ triggers on rising edge (ECHO start)
- [ ] IRQ triggers on falling edge (ECHO end)
- [ ] IRQ latency measured <5µs (oscilloscope verification)
- [ ] 10µs TRIG pulse measured with oscilloscope (verify accuracy)

---

### Phase 2: HC-SR04 Low-Level Driver

**Objective:** Implement single-sensor driver with IRQ-based timing.

#### Files to Create

**1. `lib/rx_hal/inc/rx_hc_sr04.h`** - HC-SR04 interface (~200 lines with docs)

```c
/**
 * @file rx_hc_sr04.h
 * @brief HC-SR04 ultrasonic sensor driver interface
 */

#ifndef RX_HAL_RX_HC_SR04_H
#define RX_HAL_RX_HC_SR04_H

#include "rx_err.h"
#include "gpio.h"
#include "rx_icu.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @enum hc_sr04_state_t
 * @brief HC-SR04 sensor state machine states
 */
typedef enum : uint8_t {
    k_hc_sr04_state_idle = 0,                /**< Idle, ready for trigger */
    k_hc_sr04_state_triggering = 1,          /**< Sending 10µs TRIG pulse */
    k_hc_sr04_state_waiting_echo_rise = 2,   /**< Waiting for ECHO HIGH */
    k_hc_sr04_state_measuring = 3,           /**< ECHO HIGH, timing in progress */
    k_hc_sr04_state_complete = 4,            /**< Measurement complete */
    k_hc_sr04_state_timeout = 5,             /**< Timeout (no echo or out of range) */
    k_hc_sr04_state_error = 6,               /**< Sensor error */
} hc_sr04_state_t;

/**
 * @enum hc_sr04_sensor_id_t
 * @brief Sensor identifier (0-3 for 4 sensors)
 */
typedef enum : uint8_t {
    k_hc_sr04_sensor_0 = 0,  /**< Front Left */
    k_hc_sr04_sensor_1 = 1,  /**< Front Right */
    k_hc_sr04_sensor_2 = 2,  /**< Rear Left */
    k_hc_sr04_sensor_3 = 3,  /**< Rear Right */
    k_hc_sr04_sensor_count = 4,
} hc_sr04_sensor_id_t;

/**
 * @struct hc_sr04_config_t
 * @brief HC-SR04 sensor configuration
 */
typedef struct {
    hc_sr04_sensor_id_t sensor_id;  /**< Sensor identifier (0-3) */
    rx_port_pin_t trig_pin;          /**< TRIG pin (GPIO output) */
    rx_port_pin_t echo_pin;          /**< ECHO pin (IRQ input) */
    rx_icu_irq_num_t irq_num;        /**< IRQ number (IRQ12-15) */
    uint16_t max_range_cm;           /**< Maximum range in cm (default 400) */
    uint16_t min_range_cm;           /**< Minimum range in cm (default 2) */
} hc_sr04_config_t;

/**
 * @struct hc_sr04_measurement_t
 * @brief HC-SR04 measurement result
 */
typedef struct {
    uint16_t distance_cm;        /**< Measured distance in cm */
    uint32_t pulse_width_us;     /**< Raw ECHO pulse width in µs */
    hc_sr04_state_t state;       /**< Sensor state at measurement */
    bool valid;                  /**< True if measurement is valid */
    uint32_t timestamp_ms;       /**< Timestamp of measurement (system time) */
} hc_sr04_measurement_t;

/**
 * @struct hc_sr04_handle_t
 * @brief HC-SR04 sensor handle (internal state)
 */
typedef struct {
    hc_sr04_config_t config;            /**< Sensor configuration */
    hc_sr04_state_t state;              /**< Current state */
    uint32_t echo_rise_time_us;         /**< Timestamp of ECHO rising edge (µs) */
    uint32_t echo_fall_time_us;         /**< Timestamp of ECHO falling edge (µs) */
    hc_sr04_measurement_t last_measurement;  /**< Last valid measurement */
    bool initialized;                   /**< Initialization flag */
} hc_sr04_handle_t;

/**
 * @brief Initialize HC-SR04 sensor
 *
 * @details
 * Configures GPIO pins for TRIG (output) and ECHO (IRQ input).
 * Registers IRQ handler for rising/falling edge detection.
 *
 * @param[in,out] handle Sensor handle (state initialized)
 * @param[in] config Sensor configuration (pins, IRQ, range limits)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sensor ready
 * @retval k_rx_err_null_ptr handle or config is NULL
 * @retval k_rx_err_invalid_arg Invalid sensor ID, pin, or IRQ
 * @retval k_rx_err_hardware GPIO or IRQ initialization failed
 *
 * @pre GPIO and IRQ HAL initialized
 * @pre Sensor pins not already in use
 * @post TRIG pin configured as output (LOW)
 * @post ECHO pin configured as IRQ input (both edges)
 * @post handle->initialized = true
 *
 * @note Not thread-safe, call once per sensor at startup
 * @warning Do not call multiple times for same handle
 *
 * @par Example:
 * @code
 * hc_sr04_handle_t sonar0 = {0};
 * hc_sr04_config_t config = {
 *     .sensor_id = k_hc_sr04_sensor_0,
 *     .trig_pin = k_rx_p5_5,
 *     .echo_pin = k_rx_p4_4,
 *     .irq_num = k_rx_irq12,
 *     .max_range_cm = 400,
 *     .min_range_cm = 2,
 * };
 * rx_err_t err = hc_sr04_init(&sonar0, &config);
 * if (err == k_rx_ok) {
 *     // Sensor ready
 * }
 * @endcode
 *
 * @see hc_sr04_trigger() Start a measurement
 * @since Version 1.0.0
 */
rx_err_t hc_sr04_init(hc_sr04_handle_t* handle, const hc_sr04_config_t* config);

/**
 * @brief Trigger ultrasonic measurement (non-blocking)
 *
 * @details
 * Sends 10µs HIGH pulse on TRIG pin to start measurement.
 * Measurement completes asynchronously via IRQ callbacks.
 * Use hc_sr04_get_distance() to retrieve result.
 *
 * Algorithm:
 * 1. Check state == IDLE (error if busy)
 * 2. Set TRIG pin HIGH
 * 3. Wait 10µs (blocking delay)
 * 4. Set TRIG pin LOW
 * 5. Set state = WAITING_FOR_ECHO_RISE
 * 6. Return (IRQ will complete measurement)
 *
 * @param[in,out] handle Sensor handle (state updated)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, measurement started
 * @retval k_rx_err_null_ptr handle is NULL
 * @retval k_rx_err_not_initialized Sensor not initialized
 * @retval k_rx_err_busy Previous measurement still in progress
 *
 * @pre Sensor initialized via hc_sr04_init()
 * @pre Sensor state == IDLE
 * @post TRIG pulse sent (10µs)
 * @post State = WAITING_FOR_ECHO_RISE
 *
 * @note Non-blocking (returns immediately after trigger)
 * @note Call hc_sr04_get_distance() after 25ms to retrieve result
 * @warning Do not trigger faster than 60ms (crosstalk)
 *
 * @par Performance:
 * Execution time: ~15µs (10µs pulse + overhead)
 *
 * @see hc_sr04_get_distance() Retrieve measurement result
 * @since Version 1.0.0
 */
rx_err_t hc_sr04_trigger(hc_sr04_handle_t* handle);

/**
 * @brief Get last measurement result
 *
 * @details
 * Returns last completed measurement. If measurement still in progress,
 * returns previous result and k_rx_err_busy.
 *
 * @param[in] handle Sensor handle
 * @param[out] measurement Measurement result (distance, pulse width, validity)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, valid measurement returned
 * @retval k_rx_err_null_ptr handle or measurement is NULL
 * @retval k_rx_err_not_initialized Sensor not initialized
 * @retval k_rx_err_busy Measurement in progress (check state)
 * @retval k_rx_err_timeout Sensor timeout (no echo received)
 *
 * @pre Sensor initialized
 * @post measurement filled with last result
 *
 * @note Returns previous measurement if new one not ready
 * @note Check measurement->valid flag
 *
 * @par Example:
 * @code
 * hc_sr04_measurement_t meas;
 * rx_err_t err = hc_sr04_get_distance(&sonar0, &meas);
 * if (err == k_rx_ok && meas.valid) {
 *     printf("Distance: %u cm\n", meas.distance_cm);
 * }
 * @endcode
 *
 * @since Version 1.0.0
 */
rx_err_t hc_sr04_get_distance(hc_sr04_handle_t* handle, hc_sr04_measurement_t* measurement);

/**
 * @brief Get sensor state
 *
 * @param[in] handle Sensor handle
 * @param[out] state Current state
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, state written
 * @retval k_rx_err_null_ptr handle or state is NULL
 *
 * @note Useful for debugging state machine
 *
 * @since Version 1.0.0
 */
rx_err_t hc_sr04_get_state(hc_sr04_handle_t* handle, hc_sr04_state_t* state);

/**
 * @brief Reset sensor (return to IDLE)
 *
 * @param[in,out] handle Sensor handle (state reset)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sensor reset
 * @retval k_rx_err_null_ptr handle is NULL
 *
 * @post State = IDLE
 * @post TRIG pin = LOW
 *
 * @note Use if sensor stuck in error state
 *
 * @since Version 1.0.0
 */
rx_err_t hc_sr04_reset(hc_sr04_handle_t* handle);

/**
 * @brief Deinitialize HC-SR04 sensor
 *
 * @param[in,out] handle Sensor handle (state cleared)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sensor stopped
 * @retval k_rx_err_null_ptr handle is NULL
 *
 * @post IRQ handler unregistered
 * @post GPIO pins released
 * @post handle->initialized = false
 *
 * @since Version 1.0.0
 */
rx_err_t hc_sr04_deinit(hc_sr04_handle_t* handle);

#endif  // RX_HAL_RX_HC_SR04_H
```

**2. `lib/rx_hal/src/rx_hc_sr04.c`** - HC-SR04 implementation (~400 lines)

**Key implementation details:**

**IRQ callback** (rising edge - ECHO start):
```c
/**
 * @brief IRQ callback for ECHO rising edge
 */
static void hc_sr04_echo_rise_callback(void* ctx)
{
    hc_sr04_handle_t* handle = (hc_sr04_handle_t*)ctx;

    if (handle->state == k_hc_sr04_state_waiting_echo_rise) {
        // Record ECHO start time
        handle->echo_rise_time_us = get_microseconds();  // CMT microsecond timer
        handle->state = k_hc_sr04_state_measuring;
    }
}
```

**IRQ callback** (falling edge - ECHO end):
```c
/**
 * @brief IRQ callback for ECHO falling edge
 */
static void hc_sr04_echo_fall_callback(void* ctx)
{
    hc_sr04_handle_t* handle = (hc_sr04_handle_t*)ctx;

    if (handle->state == k_hc_sr04_state_measuring) {
        // Record ECHO end time
        handle->echo_fall_time_us = get_microseconds();

        // Calculate pulse width
        uint32_t pulse_width_us = handle->echo_fall_time_us - handle->echo_rise_time_us;

        // Calculate distance (58µs per cm)
        uint16_t distance_cm = (uint16_t)(pulse_width_us / 58);

        // Validate range
        bool valid = (distance_cm >= handle->config.min_range_cm) &&
                     (distance_cm <= handle->config.max_range_cm);

        // Store measurement
        handle->last_measurement.distance_cm = distance_cm;
        handle->last_measurement.pulse_width_us = pulse_width_us;
        handle->last_measurement.valid = valid;
        handle->last_measurement.state = k_hc_sr04_state_complete;
        handle->last_measurement.timestamp_ms = get_milliseconds();

        handle->state = k_hc_sr04_state_complete;
    }
}
```

**10µs TRIG pulse generation:**
```c
rx_err_t hc_sr04_trigger(hc_sr04_handle_t* handle)
{
    RX_CHECK_NULL_PTR(handle);
    RX_CHECK_INITIALIZED(handle->initialized);

    // Check not busy
    if (handle->state != k_hc_sr04_state_idle &&
        handle->state != k_hc_sr04_state_complete &&
        handle->state != k_hc_sr04_state_timeout) {
        return k_rx_err_busy;
    }

    // Set TRIG HIGH
    rx_gpio_write(handle->config.trig_pin, true);

    // Wait 10µs (blocking)
    delay_microseconds(10);  // Use CMT or software delay

    // Set TRIG LOW
    rx_gpio_write(handle->config.trig_pin, false);

    // Update state
    handle->state = k_hc_sr04_state_waiting_echo_rise;

    return k_rx_ok;
}
```

**Timeout handling** (in periodic task):
```c
/**
 * @brief Check for sensor timeout (call periodically at 10Hz)
 */
rx_err_t hc_sr04_check_timeout(hc_sr04_handle_t* handle, uint32_t timeout_ms)
{
    RX_CHECK_NULL_PTR(handle);

    uint32_t current_time_ms = get_milliseconds();

    if (handle->state == k_hc_sr04_state_waiting_echo_rise ||
        handle->state == k_hc_sr04_state_measuring) {

        uint32_t elapsed_ms = current_time_ms - handle->last_measurement.timestamp_ms;

        if (elapsed_ms > timeout_ms) {  // Default 25ms
            handle->state = k_hc_sr04_state_timeout;
            handle->last_measurement.valid = false;
            return k_rx_err_timeout;
        }
    }

    return k_rx_ok;
}
```

#### Verification Checklist - Phase 2

- [ ] Single sensor init/deinit works
- [ ] TRIG pulse is exactly 10µs (oscilloscope verification)
- [ ] IRQ fires on ECHO rising edge
- [ ] IRQ fires on ECHO falling edge
- [ ] Pulse width calculation accurate (±1µs)
- [ ] Distance conversion correct (58µs/cm)
- [ ] Range validation works (2cm - 400cm)
- [ ] Timeout handling works (no echo → timeout state)
- [ ] State machine correct (IDLE → TRIGGERING → WAITING → MEASURING → COMPLETE)

---

### Phase 3: Multi-Sensor Manager

**Objective:** Manage 4 sensors with round-robin triggering.

#### Files to Create

**1. `lib/rx_hal/inc/rx_hc_sr04_manager.h`** - Multi-sensor interface

```c
/**
 * @file rx_hc_sr04_manager.h
 * @brief HC-SR04 multi-sensor manager
 */

/**
 * @struct hc_sr04_manager_t
 * @brief Multi-sensor manager state
 */
typedef struct {
    hc_sr04_handle_t sensors[k_hc_sr04_sensor_count];  /**< 4 sensor handles */
    uint8_t current_sensor;     /**< Currently active sensor (0-3) */
    uint32_t last_trigger_ms;   /**< Timestamp of last trigger */
    bool initialized;           /**< Manager initialized */
} hc_sr04_manager_t;

/**
 * @brief Initialize all 4 sensors
 *
 * @param[in,out] manager Manager handle
 * @param[in] configs Array of 4 sensor configs
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, all sensors ready
 * @retval k_rx_err_invalid_arg One or more sensor init failed
 *
 * @post All 4 sensors initialized
 * @post Manager ready for round-robin triggering
 */
rx_err_t hc_sr04_manager_init(hc_sr04_manager_t* manager, const hc_sr04_config_t configs[4]);

/**
 * @brief Trigger next sensor in round-robin (call at 16.67Hz = 60ms intervals)
 *
 * @details
 * Triggers sensors in sequence: 0 → 1 → 2 → 3 → 0 ...
 * 60ms between triggers prevents ultrasonic crosstalk.
 *
 * @param[in,out] manager Manager handle
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, sensor triggered
 * @retval k_rx_err_busy Too soon (< 60ms since last trigger)
 *
 * @note Call this from 16.67Hz task (every 60ms)
 * @note Full 4-sensor cycle = 240ms (4.16Hz update rate per sensor)
 */
rx_err_t hc_sr04_manager_trigger_next(hc_sr04_manager_t* manager);

/**
 * @brief Get all sensor distances
 *
 * @param[in] manager Manager handle
 * @param[out] measurements Array of 4 measurements
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, measurements filled
 */
rx_err_t hc_sr04_manager_get_all_distances(hc_sr04_manager_t* manager,
                                            hc_sr04_measurement_t measurements[4]);
```

**2. `lib/rx_hal/src/rx_hc_sr04_manager.c`** - Multi-sensor implementation (~200 lines)

#### Verification Checklist - Phase 3

- [ ] All 4 sensors initialize correctly
- [ ] Round-robin triggering sequences correctly (0→1→2→3→0)
- [ ] 60ms minimum interval enforced between triggers
- [ ] All 4 sensors return valid measurements
- [ ] No ultrasonic crosstalk (concurrent measurements don't interfere)

---

### Phase 4: Collision Detection Zones

**Objective:** Implement application-level obstacle detection with collision zones.

#### Files to Create

**1. `lib/rx_hal/inc/rx_collision_detection.h`** - Collision zone interface

```c
/**
 * @enum collision_zone_t
 * @brief Collision detection zones
 */
typedef enum : uint8_t {
    k_collision_zone_none = 0,      /**< No obstacles detected */
    k_collision_zone_front = 0x01,  /**< Obstacle in front (Sonar 0 or 1) */
    k_collision_zone_rear = 0x02,   /**< Obstacle in rear (Sonar 2 or 3) */
    k_collision_zone_left = 0x04,   /**< Obstacle on left (Sonar 0 or 2) */
    k_collision_zone_right = 0x08,  /**< Obstacle on right (Sonar 1 or 3) */
} collision_zone_t;

/**
 * @struct collision_thresholds_t
 * @brief Collision detection thresholds (in cm)
 */
typedef struct {
    uint16_t critical_distance_cm;  /**< Emergency stop distance (e.g., 10cm) */
    uint16_t warning_distance_cm;   /**< Slow down distance (e.g., 30cm) */
    uint16_t safe_distance_cm;      /**< Safe clearance (e.g., 50cm) */
} collision_thresholds_t;

/**
 * @brief Detect collision zones from 4 sensor measurements
 *
 * @param[in] measurements Array of 4 sensor measurements
 * @param[in] thresholds Distance thresholds
 * @param[out] zones Bitmask of detected zones (collision_zone_t)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, zones detected
 *
 * @post zones contains bitmask of active collision zones
 *
 * @par Example:
 * @code
 * collision_thresholds_t thresholds = {
 *     .critical_distance_cm = 10,
 *     .warning_distance_cm = 30,
 *     .safe_distance_cm = 50,
 * };
 *
 * uint8_t zones;
 * rx_collision_detect(measurements, &thresholds, &zones);
 *
 * if (zones & k_collision_zone_front) {
 *     // Emergency stop!
 * }
 * @endcode
 */
rx_err_t rx_collision_detect(const hc_sr04_measurement_t measurements[4],
                              const collision_thresholds_t* thresholds,
                              uint8_t* zones);

/**
 * @brief Calculate safe velocity based on obstacle distance
 *
 * @param[in] min_distance_cm Minimum distance from all 4 sensors
 * @param[in] thresholds Distance thresholds
 * @param[out] velocity_scale Velocity scaling factor (0.0 - 1.0)
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Success, scale calculated
 *
 * @details
 * Returns velocity scale based on closest obstacle:
 * - distance >= safe: scale = 1.0 (full speed)
 * - safe > distance >= warning: scale = 0.3 - 1.0 (linear ramp)
 * - warning > distance >= critical: scale = 0.1 - 0.3 (slow)
 * - distance < critical: scale = 0.0 (emergency stop)
 */
rx_err_t rx_collision_velocity_scale(uint16_t min_distance_cm,
                                      const collision_thresholds_t* thresholds,
                                      float* velocity_scale);
```

#### Verification Checklist - Phase 4

- [ ] Collision zones correctly identified from sensor data
- [ ] Bitmask logic correct (multiple zones can be active)
- [ ] Velocity scaling smooth and predictable
- [ ] Emergency stop triggers at critical distance
- [ ] Warning zone triggers at safe margins

---

### Phase 5: Testing

**Objective:** Comprehensive unit and integration tests.

#### Unit Tests

**1. `tests/test_rx_hc_sr04.c`** - Single sensor tests (~300 lines)

**Test cases:**
- Initialization (success, NULL pointers, invalid config)
- Trigger pulse (10µs duration, correct GPIO toggling)
- State machine transitions (IDLE→TRIGGERING→WAITING→MEASURING→COMPLETE)
- IRQ callbacks (rising edge, falling edge)
- Distance calculation (58µs/cm)
- Range validation (2-400cm)
- Timeout handling (no echo)
- Reset function

**2. `tests/test_rx_hc_sr04_manager.c`** - Multi-sensor tests (~200 lines)

**Test cases:**
- Manager initialization (all 4 sensors)
- Round-robin triggering (sequence 0→1→2→3→0)
- 60ms interval enforcement
- Get all distances
- Handle sensor timeouts gracefully

**3. `tests/test_rx_collision_detection.c`** - Collision zone tests (~150 lines)

**Test cases:**
- Zone detection (front, rear, left, right)
- Bitmask logic (multiple zones)
- Velocity scaling (critical, warning, safe)
- Edge cases (all sensors timeout, invalid measurements)

#### Integration Tests (Hardware)

**Test fixture:**
- 4 HC-SR04 sensors connected to RX72N
- Movable obstacles (cardboard boxes, walls)
- Tape measure for distance verification

**Test procedure:**
1. **Static distance test:** Place obstacle at known distances (10cm, 50cm, 100cm, 200cm), verify readings ±2cm
2. **Crosstalk test:** Trigger all 4 sensors simultaneously, verify no interference
3. **Round-robin test:** Verify 60ms stagger between triggers
4. **Collision zone test:** Approach obstacles from different directions, verify correct zones detected
5. **Velocity scaling test:** Verify robot slows down as obstacles approach

#### Verification Checklist - Phase 5

- [ ] All unit tests pass (95%+ coverage)
- [ ] Hardware static distance test ±2cm accuracy
- [ ] No crosstalk with round-robin triggering
- [ ] Collision zones correctly identified on hardware
- [ ] Velocity scaling integrates with motor control

---

### Phase 6: Documentation

**Objective:** Complete documentation per Doxygen standards.

#### Documentation Requirements

**Code documentation:**
- [ ] All functions have comprehensive Doxygen comments (15+ tags)
- [ ] All structs have inline member comments
- [ ] All enums have value descriptions
- [ ] State machine diagram (PlantUML or Graphviz)
- [ ] Usage examples in @code blocks

**LaTeX documentation:**

**1. Create `docs/sections/ultrasonic_sensors.tex`** (~400 lines)

**Content:**
- HC-SR04 technical specifications
- Pin assignments (4 sensors)
- IRQ-based timing explanation
- Round-robin triggering strategy
- Collision detection algorithm
- Performance analysis
- Troubleshooting guide

**2. Update `docs/sections/03_hardware_pinout.tex`**
- Add ultrasonic sensor pin table
- Reference ultrasonic_sensors.tex

**3. Update `README.md`**
- Add "Ultrasonic Obstacle Detection" to features
- Mention 4-sensor 360° coverage

#### Verification Checklist - Phase 6

- [ ] All code has comprehensive Doxygen documentation
- [ ] No Doxygen warnings (`doxygen 2>&1 | grep warning`)
- [ ] ultrasonic_sensors.tex compiled successfully
- [ ] Pinout documentation updated
- [ ] README.md updated

---

## Implementation Checklist

### Phase 1: IRQ/GPIO HAL ⏱️ 2-3 hours
- [ ] Verify GPIO HAL supports P52-P55 (TRIG)
- [ ] Create or verify IRQ HAL (rx_icu.h, rx_icu.c)
- [ ] Test 10µs TRIG pulse accuracy (oscilloscope)
- [ ] Test IRQ latency <5µs
- [ ] Code review

### Phase 2: HC-SR04 Driver ⏱️ 4-5 hours
- [ ] Create rx_hc_sr04.h interface
- [ ] Implement rx_hc_sr04.c driver
- [ ] Implement IRQ callbacks (rising/falling edge)
- [ ] Implement distance calculation (58µs/cm)
- [ ] Implement timeout handling
- [ ] Unit tests pass
- [ ] Code review

### Phase 3: Multi-Sensor Manager ⏱️ 2-3 hours
- [ ] Create rx_hc_sr04_manager.h interface
- [ ] Implement manager with round-robin triggering
- [ ] Test all 4 sensors on hardware
- [ ] Verify no crosstalk
- [ ] Code review

### Phase 4: Collision Detection ⏱️ 2-3 hours
- [ ] Create rx_collision_detection.h interface
- [ ] Implement collision zone detection
- [ ] Implement velocity scaling
- [ ] Unit tests pass
- [ ] Code review

### Phase 5: Testing ⏱️ 3-4 hours
- [ ] Write unit tests (sensor, manager, collision)
- [ ] Achieve 95%+ line coverage
- [ ] Hardware integration tests
- [ ] Distance accuracy verification (±2cm)
- [ ] Document test results

### Phase 6: Documentation ⏱️ 1-2 hours
- [ ] Add comprehensive Doxygen comments
- [ ] Create ultrasonic_sensors.tex
- [ ] Update pinout documentation
- [ ] Update README.md
- [ ] Generate Doxygen HTML

### Phase 7: Integration
- [ ] Run coderabbit review --plain
- [ ] Address feedback
- [ ] Commit and push
- [ ] Create pull request
- [ ] Merge to main

**Total Estimated Effort:** 12-16 hours

---

## Success Criteria

### Must Have (Blocking)
- ✅ All 4 sensors operational on hardware
- ✅ Distance accuracy ±2cm from 10cm to 300cm
- ✅ No ultrasonic crosstalk with round-robin triggering
- ✅ Collision zones correctly identified
- ✅ Velocity scaling integrates with motor control
- ✅ Unit tests pass with 95%+ coverage
- ✅ NASA Power of 10 compliant
- ✅ Comprehensive Doxygen documentation

### Nice to Have (Post-MVP)
- Sensor health monitoring (detect disconnected sensors)
- Temperature compensation (speed of sound varies with temp)
- Multi-path echo rejection (ignore spurious reflections)
- Kalman filtering for smoother distance readings

---

## Risk Mitigation

### Risk 1: IRQ latency too high (>5µs)
**Likelihood:** Low
**Impact:** Medium (distance accuracy degraded)
**Mitigation:** Verify IRQ priority high enough, minimize ISR execution time

### Risk 2: Ultrasonic crosstalk
**Likelihood:** Medium
**Impact:** High (invalid measurements)
**Mitigation:** Strict 60ms stagger between triggers, test on hardware

### Risk 3: TRIG pulse not accurate (≠10µs)
**Likelihood:** Medium
**Impact:** Medium (sensor may not trigger)
**Mitigation:** Use hardware timer for pulse generation instead of software delay

### Risk 4: 5V ECHO signal damages RX72N
**Likelihood:** Low
**Impact:** Critical (MCU damage)
**Mitigation:** Verify RX72N IRQ pins are 5V tolerant (check datasheet), add level shifters if needed

---

## References

### Datasheets
- **HC-SR04 Datasheet:** https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf
- **RX72N Manual Ch23:** Multi-Function Pin Controller (IRQ configuration)
- **RX72N Manual Ch24:** Interrupt Controller (ICU)

### Existing Code
- **GPIO HAL:** `/workspaces/STAR/star-rx72n-firmware/lib/rx_hal/src/gpio.c`
- **IRQ HAL:** Check if `/workspaces/STAR/star-rx72n-firmware/lib/rx_hal/inc/rx_icu.h` exists

### Project Documentation
- **Pin Assignment Plan:** `/home/star/.claude/plans/cheeky-snacking-quail.md` (See "Solution 1: Fix Ultrasonic Sensor IRQ Pins")
- **RX72N_ROADMAP.md:** `/workspaces/STAR/RX72N_ROADMAP.md`
- **CLAUDE.md:** `/workspaces/STAR/CLAUDE.md` (coding standards)

---

**Document Version:** 1.0
**Last Updated:** 2026-02-05
**Author:** STAR Development Team
**Status:** Ready for Implementation
