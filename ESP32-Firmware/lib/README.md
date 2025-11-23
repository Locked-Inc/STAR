# STAR Firmware Libraries

This directory contains the modular libraries for the STAR ESP32 firmware. The architecture follows the **Dependency Inversion Principle (DIP)** to ensure loose coupling between components.

## Why This Architecture?

### The Problem Without DIP

Without dependency inversion, you get tightly coupled code:

```c
// star_bus_manager.c - TIGHTLY COUPLED (bad)
#include "star_error_handler.h"  // Direct dependency on concrete implementation
#include "star_pin_validator.h"  // Can't swap these out

void init_bus() {
    error_handler_record_error(...);  // Hardcoded to one implementation
    star_register_pin(...);           // Can't test without real hardware
}
```

**Problems:**
- Can't test `star_bus` without the real error handler and pin validator
- Can't swap implementations (e.g., mock for testing, different validator for different boards)
- Circular dependencies become impossible to resolve
- Changes in one module ripple through the entire codebase

### What DIP Gives You

**1. Testability**
```c
// In tests - inject mock interfaces
star_error_interface_t mock_error = {
    .record_error = mock_record_error,  // Your test spy
    .can_retry = always_return_false,
    .ctx = &test_data
};
star_bus_manager_init(&manager, "test", &mock_error, NULL);
// Now you can test bus manager behavior without real error handler
```

**2. Swappable Implementations**
```c
// Production - use real implementations
error_handler_get_interface(&error_iface, &production_handler);

// Testing - use mocks
mock_error_interface(&error_iface);

// Different board - use board-specific validator
custom_pin_validator_get_interface(&pin_iface);

// Same bus manager code works with all of them
star_bus_manager_init(&manager, "main", &error_iface, &pin_iface);
```

**3. Optional Dependencies**
```c
// Don't need error tracking? Pass NULL
star_bus_manager_init(&manager, "simple", NULL, NULL);

// Only need pin validation?
star_bus_manager_init(&manager, "validated", NULL, &pin_iface);
```

**4. No Circular Dependencies**
- `star_bus` depends on `star_core` (interfaces only)
- `star_error_handler` depends on `star_core`
- `star_pin_validator` depends on `star_core`
- Nobody depends on each other - clean dependency graph

**5. Compile-Time Independence**
- Change `star_error_handler` internals? Only that library recompiles
- Add features to `star_pin_validator`? Doesn't affect `star_bus`
- Each library can evolve independently

### When You Should Care

- **Writing unit tests**: Mock the interfaces instead of needing real hardware
- **Supporting multiple boards**: Different pin validators for different GPIO layouts
- **Debugging**: Inject logging wrappers around interfaces
- **Future-proofing**: Swap error handler strategies without touching bus code
- **Code reuse**: Use `star_bus` in another project with completely different error handling

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           APPLICATION LAYER                              │
│                              (main.c)                                    │
│  • Sensor initialization    • Business logic    • Task scheduling        │
└────────────────────────────────────┬─────────────────────────────────────┘
                                     │
       ┌─────────────────────────────┼─────────────────────────────┐
       │                             │                             │
       ▼                             ▼                             ▼
┌────────────────┐       ┌────────────────────┐       ┌────────────────┐
│ SENSOR DRIVERS │       │   COMMUNICATION    │       │ POWER/CONTROL  │
│                │       │                    │       │                │
│ • MPU6050      │       │ • A7670G (4G LTE)  │       │ • BQ7850 (BMS) │
│ • BNO055+BMP280│       │ • GPS6MV2 (GNSS)   │       │ • PCA9685 (PWM)│
│ • DHT22, BH1750│       │ • RPLiDAR, TiM561  │       │                │
│ • HC-SR04      │       │ • IMX219 (Camera)  │       │                │
│ • QMC5883L     │       │                    │       │                │
│ • MQ135        │       │                    │       │                │
└───────┬────────┘       └─────────┬──────────┘       └───────┬────────┘
        │                          │                          │
        └──────────────────────────┼──────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                            STAR_BUS LAYER                                │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │                     star_bus_manager                               │  │
│  │  • Thread-safe lifecycle    • Dynamic configuration                │  │
│  │  • Pin registration via interfaces                                 │  │
│  └──────────────────────────────┬─────────────────────────────────────┘  │
│       ┌──────────┬──────────────┼──────────────┬──────────┐              │
│       ▼          ▼              ▼              ▼          ▼              │
│    ┌─────┐   ┌─────┐       ┌────────┐     ┌──────┐   ┌────────┐          │
│    │ I2C │   │ SPI │       │  UART  │     │SMBus │   │OneWire │          │
│    └─────┘   └─────┘       └────────┘     └──────┘   └────────┘          │
└──────────────────────────────────┬───────────────────────────────────────┘
                                   │ Depends only on interfaces
                                   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                           STAR_CORE LAYER                                │
│                      (Abstract Interfaces - DIP)                         │
│                                                                          │
│   ┌───────────────────────┐           ┌───────────────────────┐          │
│   │ star_error_interface  │           │  star_pin_interface   │          │
│   │ • record_error()      │           │ • register_pin()      │          │
│   │ • can_retry()         │           │ • unregister_pin()    │          │
│   │ • get_retry_delay()   │           │ • validate_pins()     │          │
│   └──────────┬────────────┘           └──────────┬────────────┘          │
└──────────────┼───────────────────────────────────┼───────────────────────┘
               │ Implements                        │ Implements
               ▼                                   ▼
┌──────────────────────┐               ┌──────────────────────┐
│ star_error_handler   │               │ star_pin_validator   │
│ • Retry w/ backoff   │               │ • Conflict detection │
│ • Error logging      │               │ • Shared pin support │
│ • Thread-safe mutex  │               │ • User descriptions  │
└──────────────────────┘               └──────────────────────┘
```

## Libraries

### star_core
**Core interfaces module - the foundation for dependency inversion**

Contains abstract interface definitions that break circular dependencies between modules:

- `star_error_interface.h` - Abstract error handler interface
- `star_pin_interface.h` - Abstract pin validator interface

**Dependencies:** None

**Used by:** All other libraries

---

### star_error_handler
**Error recording and retry logic with exponential backoff**

Provides robust error handling with:
- Error recording with file/line/function tracking
- Configurable retry counts and delays
- Exponential backoff for transient errors
- State reset functionality
- Interface adapter for DIP compliance

**Key Functions:**
- `error_handler_init()` - Initialize handler with retry config
- `error_handler_record_error()` - Record an error occurrence
- `error_handler_can_retry()` - Check if retry is available
- `error_handler_reset_state()` - Reset error state
- `error_handler_get_interface()` - Get DIP interface adapter

**Dependencies:** star_core

---

### star_pin_validator
**GPIO pin conflict detection and validation**

Prevents pin conflicts at runtime:
- Register pins with descriptions
- Detect conflicts between exclusive pins
- Support for shared pins (like I2C buses)
- Global validation before hardware init

**Key Functions:**
- `star_register_pin()` - Register a pin usage
- `star_unregister_pin()` - Release a pin
- `star_validate_pins()` - Check for conflicts
- `star_free_pin_validator()` - Cleanup resources
- `pin_validator_get_interface()` - Get DIP interface adapter

**Dependencies:** star_core

---

### star_bus
**Unified hardware interface for I2C, SPI, UART protocols**

Comprehensive bus management system:
- Thread-safe bus manager with mutex protection
- Support for I2C master/slave, SPI controller/peripheral
- Dynamic bus configuration and lifecycle management
- Pin registration through injected interfaces
- SPI host tracking to prevent resource conflicts

**Key Components:**
- `star_bus_manager` - Central bus lifecycle management
- `star_bus_config` - Bus configuration creation/destruction
- `star_bus_i2c` - I2C protocol operations
- `star_bus_spi` - SPI protocol operations

**Key Functions:**
- `star_bus_manager_init()` - Initialize with DIP interfaces
- `star_bus_manager_add_bus()` - Add a bus configuration
- `star_bus_manager_find_bus()` - Find bus by name
- `star_bus_manager_remove_bus()` - Remove and cleanup bus
- `star_bus_config_create_i2c()` - Create I2C config
- `star_bus_config_create_spi_device()` - Create SPI config

**Dependencies:** star_core

---

### star_bms_bq7850
**BQ7850 Battery Management System driver**

Driver for Texas Instruments BQ7850x battery monitors.

**Dependencies:** star_bus

---

## Sensor and Hardware Libraries

### star_sensor_pca9685
**16-Channel 12-bit PWM Controller**

Driver for PCA9685 PWM/Servo driver with:
- 16 independent PWM outputs
- 12-bit resolution (4096 steps)
- Frequency range: 24-1526 Hz
- Phase-shifted outputs for smooth operation

**Key Functions:** `init`, `set_frequency`, `set_pwm`, `set_duty_cycle`

**Dependencies:** star_bus, star_error_handler

---

### star_sensor_bh1750
**Digital Ambient Light Sensor**

Driver for BH1750 light intensity sensor:
- High/low resolution modes
- Lux measurement (0.5-65535 lux)
- One-time and continuous modes
- Adjustable measurement time

**Key Functions:** `init`, `start_measurement`, `read_lux`, `set_measurement_time`

**Dependencies:** star_bus, star_error_handler

---

### star_sensor_qmc5883l
**3-Axis Digital Magnetometer**

Driver for QMC5883L compass sensor:
- 3-axis magnetic field measurement
- Configurable range (±2G, ±8G)
- Heading calculation with declination
- Temperature sensor
- Data ready interrupt support

**Key Functions:** `init`, `read_raw`, `read_mag`, `read_heading`

**Dependencies:** star_bus, star_error_handler

---

### star_sensor_mpu6050
**6-Axis IMU (Accelerometer + Gyroscope)**

Driver for MPU6050 motion tracking device:
- 3-axis accelerometer (±2g to ±16g)
- 3-axis gyroscope (±250 to ±2000°/s)
- 1024-byte FIFO buffer
- Digital low pass filter
- Temperature sensor

**Key Functions:** `init`, `read_raw`, `read_accel`, `read_gyro`, `fifo_read`

**Dependencies:** star_bus, star_error_handler

---

### star_sensor_hcsr04
**Ultrasonic Distance Sensor**

Driver for HC-SR04 ultrasonic ranging module:
- Distance measurement (2-400 cm)
- ISR-based echo timing
- Temperature compensation
- Non-blocking operation
- Timeout protection

**Key Functions:** `init`, `trigger`, `is_complete`, `get_result`, `read_distance`

**Dependencies:** star_error_handler

---

### star_sensor_dht22
**Temperature and Humidity Sensor**

Driver for DHT22 (AM2302) sensor:
- Temperature: -40 to 80°C (±0.5°C)
- Humidity: 0-100% RH (±2%)
- Single-wire protocol
- Checksum validation
- 2-second minimum sample period

**Key Functions:** `init`, `read`, `get_last_reading`, `can_read`

**Dependencies:** star_error_handler

---

### star_sensor_mq135
**Air Quality Gas Sensor**

Driver for MQ135 multi-gas sensor:
- CO2, NH3, Alcohol, Smoke detection
- ADC-based analog reading
- Calibration support (R0 determination)
- Temperature/humidity compensation
- PPM calculation for multiple gases

**Key Functions:** `init`, `calibrate`, `read`, `calculate_ppm`, `compensate`

**Dependencies:** star_error_handler

---

### star_sensor_gps6mv2
**GPS Module (NEO-6M)**

Driver for GY-GPS6MV2 GPS module:
- NMEA sentence parsing (GPGGA, GPRMC, GPGSA)
- Position, speed, course, altitude
- Satellite tracking
- Haversine distance calculation
- 9600 baud UART

**Key Functions:** `init`, `process`, `get_data`, `has_fix`, `calculate_distance`

**Dependencies:** star_error_handler

---

### star_sensor_rplidar_c1
**360° DTOF Laser Scanner**

Driver for RPLiDAR C1 laser range scanner:
- 360° scanning (12m range)
- 10000 samples/sec
- Binary UART protocol (256000 baud)
- Motor PWM control
- Express scan mode
- Dynamic scan buffer

**Key Functions:** `init`, `start_scan`, `process`, `get_scan_data`, `start_motor`

**Dependencies:** star_error_handler

---

### star_sensor_bno055_bmp280
**10-DOF IMU with Barometric Pressure**

Combined driver for BNO055 + BMP280:
- 9-axis sensor fusion (accelerometer, gyroscope, magnetometer)
- Quaternion and Euler angle output
- Barometric pressure and altitude
- Automatic calibration
- On-chip Cortex M0+ for fusion

**Key Functions:** `init`, `read`, `get_calibration`, `set_mode`

**Dependencies:** star_bus, star_error_handler

---

### star_module_a7670g
**4G LTE CAT-1 Communication Module**

Driver for A7670G cellular module:
- AT command interface
- Network registration (2G/3G/4G)
- TCP/IP stack
- SMS send/receive
- GPS/GNSS positioning
- HTTP/HTTPS client

**Key Functions:** `init`, `send_at_command`, `connect_network`, `send_sms`, `http_get`

**Dependencies:** star_error_handler

---

### star_sensor_sick_tim561
**2D LiDAR Scanner**

Driver for SICK TiM561 industrial LiDAR:
- 270° scanning angle
- 10m detection range
- 15 Hz scan frequency
- 2880 measurement points
- TCP/IP over Ethernet
- COLA ASCII protocol

**Key Functions:** `init`, `connect`, `start_scan`, `read_scan`, `send_command`

**Dependencies:** star_error_handler

---

### star_camera_imx219
**8MP Stereo Camera Module**

Driver for IMX219-83 stereo camera:
- Dual 8MP sensors (3280x2464)
- CSI-2 + I2C control
- Stereo capture support
- Multiple resolution modes
- Exposure and gain control
- Synchronized frame capture

**Key Functions:** `init`, `start_streaming`, `capture_stereo`, `set_exposure`, `set_gain`

**Dependencies:** star_bus, star_error_handler

---

## Usage Example

```c
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_pin_validator.h"

// Create concrete implementations
error_handler_t g_error_handler;
error_handler_init(&g_error_handler, 3, 100, 5000, NULL, NULL);

// Create interface adapters
star_error_interface_t error_iface;
star_pin_interface_t pin_iface;
error_handler_get_interface(&error_iface, &g_error_handler);
pin_validator_get_interface(&pin_iface);

// Initialize bus manager with interfaces (DIP)
star_bus_manager_t manager;
star_bus_manager_init(&manager, "main", &error_iface, &pin_iface);

// Create and add bus configurations
star_bus_config_t* i2c = star_bus_config_create_i2c(
    "sensors", I2C_NUM_0, 0x50, GPIO_NUM_21, GPIO_NUM_22, 100000);
star_bus_manager_add_bus(&manager, i2c);

// Validate all pin assignments
star_validate_pins();
```

## Building

The libraries are automatically discovered by PlatformIO's Library Dependency Finder (LDF) in `deep` mode. Dependencies are specified in each library's `library.json` file.

```bash
# Build main project
pio run

# Build and run tests
pio test -e esp32_wroom_test --without-uploading --without-testing
```

## Testing

Each library has comprehensive unit tests in the `test/` directory:

- `test_core` - Interface adapter tests
- `test_error_handler` - Error handler tests
- `test_pin_validator` - Pin validator tests
- `test_bus` - Bus manager and config tests
- `test_bms` - BMS driver tests
