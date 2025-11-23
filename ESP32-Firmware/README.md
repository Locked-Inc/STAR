# STAR Firmware

**S**ensor and Actuator Abstraction **R**untime for ESP32-IDF

A modular, production-ready embedded firmware framework implementing Dependency Inversion Principle (DIP) for loose coupling, testability, and maintainability.

## Features

- **DIP Architecture**: Abstract interfaces enable mock testing and swappable implementations
- **Unified Bus Abstraction**: Single API for I2C, SPI, UART, OneWire, and SMBus protocols
- **Thread-Safe Design**: Mutex-protected operations with configurable timeouts
- **Comprehensive Sensor Suite**: 15+ sensor drivers including IMU, GPS, LiDAR, temperature, and more
- **Production Error Handling**: Retry logic with exponential backoff
- **Pin Conflict Detection**: Runtime validation prevents GPIO conflicts

## Supported Hardware

| Category | Components |
|----------|------------|
| **Platforms** | ESP32-WROOM, ESP32-S3 |
| **IMU** | MPU6050, BNO055+BMP280 |
| **Distance** | HC-SR04, RPLiDAR C1, SICK TiM561 |
| **Environmental** | DHT22, BH1750, MQ135 |
| **Navigation** | GPS6MV2 (NEO-6M), QMC5883L |
| **Communication** | A7670G 4G LTE CAT-1 |
| **Vision** | IMX219-83 Stereo Camera |
| **Power** | PCA9685 PWM, BQ7850 BMS |

## Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) (VSCode extension or CLI)
- ESP-IDF 5.x (managed automatically by PlatformIO)

### Build

```bash
# Clone repository
git clone <repository-url>
cd star-firmware

# Build for ESP32-WROOM
pio run -e esp32_wroom

# Build for ESP32-S3
pio run -e esp32s3
```

### Run Tests

```bash
# Build tests (native simulation)
pio test -e esp32_wroom_test --without-uploading --without-testing

# Run on hardware
pio test -e esp32_wroom_test
```

### Upload

```bash
pio run -e esp32_wroom --target upload
```

## Architecture

```
                    ┌─────────────────────────────────────┐
                    │           Application               │
                    │            (main.c)                 │
                    └───────────────┬─────────────────────┘
                                    │
            ┌───────────────────────┼───────────────────────┐
            │                       │                       │
            ▼                       ▼                       ▼
┌───────────────────┐   ┌───────────────────┐   ┌───────────────────┐
│  Sensor Drivers   │   │   Communication   │   │   Power/Control   │
│  MPU6050, DHT22   │   │   A7670G, GPS     │   │   BQ7850, PCA9685 │
│  BH1750, etc.     │   │   LiDAR, etc.     │   │                   │
└─────────┬─────────┘   └─────────┬─────────┘   └─────────┬─────────┘
          │                       │                       │
          └───────────────────────┼───────────────────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    │        star_bus           │
                    │   (I2C/SPI/UART/SMBus)    │
                    └─────────────┬─────────────┘
                                  │
          ┌───────────────────────┼───────────────────────┐
          │                       │                       │
          ▼                       ▼                       ▼
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   star_core     │◄────│star_error_handler│     │star_pin_validator│
│  (Interfaces)   │     │ (Implementation) │     │ (Implementation) │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Project Structure

```
star-firmware/
├── lib/                    # Library components
│   ├── star_core/          # Abstract interfaces (DIP foundation)
│   ├── star_error_handler/ # Error handling with retry logic
│   ├── star_pin_validator/ # GPIO conflict detection
│   ├── star_bus/           # Unified bus abstraction
│   ├── star_sensor_*/      # Sensor drivers
│   ├── star_module_*/      # Communication modules
│   └── star_camera_*/      # Camera drivers
├── src/                    # Application entry point
├── test/                   # Unit tests
├── examples/               # Usage examples
├── partitions/             # ESP32 partition tables
└── platformio.ini          # Build configuration
```

## Usage Example

```c
#include "star_bus_manager.h"
#include "star_error_handler.h"
#include "star_sensor_mpu6050.h"

void app_main(void) {
    // Initialize error handler
    error_handler_t error_handler;
    error_handler_init(&error_handler, 3, 100, 5000, NULL, NULL);

    // Get DIP interface
    star_error_interface_t error_iface;
    error_handler_get_interface(&error_iface, &error_handler);

    // Initialize bus manager
    star_bus_manager_t bus_manager;
    star_bus_manager_init(&bus_manager, "main", &error_iface, NULL);

    // Create I2C bus for sensors
    star_bus_config_t* i2c_bus = star_bus_config_create_i2c(
        "imu", I2C_NUM_0, MPU6050_ADDR, GPIO_NUM_21, GPIO_NUM_22, 400000);
    star_bus_manager_add_bus(&bus_manager, i2c_bus);

    // Initialize MPU6050
    mpu6050_handle_t imu;
    mpu6050_config_t imu_cfg = {
        .bus_manager = &bus_manager,
        .bus_name = "imu"
    };
    star_sensor_mpu6050_init(&imu, &imu_cfg);

    // Read sensor data
    mpu6050_data_t data;
    star_sensor_mpu6050_read(&imu, &data);

    printf("Accel: X=%.2f Y=%.2f Z=%.2f\n",
           data.accel_x, data.accel_y, data.accel_z);
}
```

## Documentation

- [Library Documentation](lib/README.md) - Detailed API reference for all libraries
- [Examples](examples/) - 140+ working examples covering all features
- [Architecture Guide](docs/architecture/) - DIP patterns and design decisions

## Testing

The framework includes comprehensive unit tests using Unity test framework:

```bash
# Run all tests
pio test

# Run specific test suite
pio test --filter test_bus

# Run tests with verbose output
pio test -v
```

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style

- C99 with C++ extern guards
- 4-space indentation
- snake_case for functions and variables
- SCREAMING_SNAKE_CASE for macros and constants
- Doxygen-compatible documentation comments

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- ESP-IDF team for the excellent framework
- PlatformIO for seamless build tooling
- All sensor manufacturers for detailed datasheets
