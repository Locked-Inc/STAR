# STAR Bus Manager

Unified bus management system for ESP32 with support for I2C, SPI (master and peripheral modes), and SMBus protocols.

## Overview

The STAR Bus Manager provides a centralized, type-safe interface for managing multiple communication buses with automatic resource management, pin conflict detection, and error handling.

### Key Features

- **Unified API**: Single interface for I2C, SPI master, and SPI peripheral buses
- **Pin Validation**: Automatic GPIO conflict detection with shareable/exclusive pin tracking
- **Error Handling**: Built-in retry logic with exponential backoff
- **SMBus Support**: Complete SMBus 2.0 protocol implementation with PEC
- **Thread-Safe**: Mutex-protected operations for FreeRTOS environments
- **Resource Management**: Automatic cleanup and lifecycle management

## Architecture

```
star_bus_manager
    +-- star_bus_config      (Bus configuration)
    +-- star_bus_i2c         (I2C master operations)
    +-- star_bus_spi         (SPI master operations)
    +-- star_bus_spi_peripheral (SPI peripheral/slave mode)
    +-- star_bus_smbus       (SMBus protocol layer)
    +-- star_pin_validator   (GPIO conflict detection)
```

## Quick Start

### 1. Initialize Bus Manager

```c
#include "star_bus_manager.h"

star_bus_manager_t manager;
esp_err_t result = star_bus_manager_init(&manager, "MyDevice");
if (result != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize bus manager");
    return result;
}
```

### 2. Add an I2C Bus

```c
#include "star_bus_i2c.h"

star_bus_config_t i2c_config;
result = star_bus_i2c_create_config(
    &i2c_config,
    "sensor_bus",           // Bus name
    I2C_NUM_0,              // I2C port
    GPIO_NUM_21,            // SDA pin
    GPIO_NUM_22,            // SCL pin
    100000,                 // 100kHz
    0x50                    // Device address
);

if (result == ESP_OK) {
    result = star_bus_manager_add_bus(&manager, &i2c_config);
}
```

### 3. Perform I2C Operations

```c
uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};

// Write to device
result = star_bus_i2c_write(&manager, "sensor_bus", data, 4, 0x10, NULL);

// Read from device
uint8_t read_buf[4];
result = star_bus_i2c_read(&manager, "sensor_bus", read_buf, 4, 0x20, NULL);
```

### 4. Use SMBus Protocol

```c
#include "star_bus_smbus.h"

// SMBus read byte
uint8_t value;
result = star_smbus_read_byte(&manager, "sensor_bus", 0x50, 0x10, &value);

// SMBus write word
result = star_smbus_write_word(&manager, "sensor_bus", 0x50, 0x20, 0xABCD);

// SMBus block read
uint8_t block_data[32];
uint8_t block_len;
result = star_smbus_block_read(&manager, "sensor_bus", 0x50, 0x30,
                                block_data, 32, &block_len);
```

### 5. Cleanup

```c
star_bus_manager_deinit(&manager);
```

## Components

### Bus Manager (`star_bus_manager`)

Central coordinator for all bus operations.

**Key Functions:**
- `star_bus_manager_init()` - Initialize manager
- `star_bus_manager_add_bus()` - Add a bus configuration
- `star_bus_manager_remove_bus()` - Remove a bus
- `star_bus_manager_find_bus()` - Find bus by name
- `star_bus_manager_deinit()` - Cleanup all resources

### I2C Operations (`star_bus_i2c`)

I2C master mode operations.

**Key Functions:**
- `star_bus_i2c_create_config()` - Create I2C bus configuration
- `star_bus_i2c_write()` - Write data to I2C device
- `star_bus_i2c_read()` - Read data from I2C device
- `star_bus_i2c_write_command()` - Send single command byte
- `star_bus_i2c_read_raw()` - Read without sending command first

**Example:**
```c
star_bus_config_t config;
star_bus_i2c_create_config(&config, "i2c0", I2C_NUM_0,
                           GPIO_NUM_21, GPIO_NUM_22, 400000, 0x3C);
star_bus_manager_add_bus(&manager, &config);

uint8_t cmd = 0x00;
uint8_t display_data[128];
star_bus_i2c_write(&manager, "i2c0", display_data, 128, cmd, NULL);
```

### SPI Master Operations (`star_bus_spi`)

SPI master mode for interfacing with peripheral devices.

**Key Functions:**
- `star_bus_spi_create_config()` - Create SPI bus configuration
- `star_bus_spi_transmit()` - Send data
- `star_bus_spi_receive()` - Receive data
- `star_bus_spi_transceive()` - Simultaneous send/receive

**Example:**
```c
spi_device_interface_config_t dev_cfg = {
    .mode = 0,
    .clock_speed_hz = 1000000,
    .spics_io_num = GPIO_NUM_5,
    .queue_size = 3
};

star_bus_config_t spi_config;
star_bus_spi_create_config(&spi_config, "spi_flash", SPI2_HOST,
                           GPIO_NUM_18, GPIO_NUM_23, GPIO_NUM_19,
                           &dev_cfg);
star_bus_manager_add_bus(&manager, &spi_config);

uint8_t tx_data[] = {0x03, 0x00, 0x00, 0x00};  // Read command
uint8_t rx_data[256];
star_bus_spi_transceive(&manager, "spi_flash", tx_data, rx_data, 256, NULL);
```

### SPI Peripheral Mode (`star_bus_spi_peripheral`)

SPI peripheral/slave mode for ESP32 acting as a peripheral device.

**Key Functions:**
- `star_bus_spi_peripheral_create_config()` - Create peripheral configuration
- `star_bus_spi_peripheral_receive()` - Receive data from master
- `star_bus_spi_peripheral_transmit()` - Send data to master
- `star_bus_spi_peripheral_transceive()` - Simultaneous send/receive

**Modes:**
- Mode 0: CPOL=0, CPHA=0 (sample on rising, shift on falling)
- Mode 1: CPOL=0, CPHA=1 (sample on falling, shift on rising)
- Mode 2: CPOL=1, CPHA=0 (sample on falling, shift on rising)
- Mode 3: CPOL=1, CPHA=1 (sample on rising, shift on falling)

**Example:**
```c
star_bus_config_t periph_config;
star_bus_spi_peripheral_create_config(
    &periph_config,
    "spi_slave",
    SPI2_HOST,
    GPIO_NUM_12,  // MISO
    GPIO_NUM_13,  // MOSI
    GPIO_NUM_14,  // SCLK
    GPIO_NUM_15,  // CS
    0,            // Mode 0
    7             // Queue size
);

star_bus_manager_add_bus(&manager, &periph_config);

// Prepare data to send
uint8_t tx_buffer[32] = {0x01, 0x02, 0x03, ...};
uint8_t rx_buffer[32];

// Wait for master transaction
star_bus_spi_peripheral_transceive(&manager, "spi_slave",
                                   tx_buffer, rx_buffer, 32, NULL);
```

### SMBus Protocol (`star_bus_smbus`)

Complete SMBus 2.0 protocol implementation over I2C.

**Protocol Commands:**
- `star_smbus_quick_command()` - Quick command (R/W bit only)
- `star_smbus_send_byte()` - Send single byte
- `star_smbus_receive_byte()` - Receive single byte
- `star_smbus_write_byte()` - Write byte to register
- `star_smbus_read_byte()` - Read byte from register
- `star_smbus_write_word()` - Write 16-bit word (little-endian)
- `star_smbus_read_word()` - Read 16-bit word (little-endian)
- `star_smbus_process_call()` - Write word, read word response
- `star_smbus_block_write()` - Write up to 32 bytes
- `star_smbus_block_read()` - Read up to 32 bytes
- `star_smbus_block_process_call()` - Write block, read block response

**PEC Support:**
- `star_smbus_calculate_pec()` - Calculate CRC-8 for Packet Error Checking

**Example - Smart Battery:**
```c
// Read battery voltage (SMBus word read)
uint16_t voltage_mv;
star_smbus_read_word(&manager, "battery_bus", 0x0B, 0x09, &voltage_mv);
ESP_LOGI(TAG, "Battery voltage: %d mV", voltage_mv);

// Read battery manufacturer (SMBus block read)
uint8_t mfg_data[32];
uint8_t mfg_len;
star_smbus_block_read(&manager, "battery_bus", 0x0B, 0x20,
                      mfg_data, 32, &mfg_len);
mfg_data[mfg_len] = '\0';  // Null terminate
ESP_LOGI(TAG, "Manufacturer: %s", (char*)mfg_data);

// Calculate PEC for verification
uint8_t pec = star_smbus_calculate_pec(mfg_data, mfg_len, 0);
```

## Pin Validation

The bus manager automatically validates GPIO pins to prevent conflicts.

### Pin Shareability Rules

**Shareable Pins** (can be used by multiple buses):
- I2C SDA/SCL pins
- SPI MISO/MOSI/SCLK pins (bus signals)

**Non-Shareable Pins** (exclusive to one bus):
- SPI CS (Chip Select) pins
- Any pin explicitly marked as non-shareable

### Example - Pin Conflict Detection

```c
// First bus uses GPIO 21, 22 as shareable I2C pins
star_bus_i2c_create_config(&config1, "i2c0", I2C_NUM_0,
                          GPIO_NUM_21, GPIO_NUM_22, 100000, 0x50);
star_bus_manager_add_bus(&manager, &config1);  // Success

// Second bus can share the same I2C pins
star_bus_i2c_create_config(&config2, "i2c1", I2C_NUM_1,
                          GPIO_NUM_21, GPIO_NUM_22, 100000, 0x51);
star_bus_manager_add_bus(&manager, &config2);  // Success - pins are shareable

// But CS pins cannot be shared
star_bus_spi_create_config(&spi_config1, "spi0", SPI2_HOST,
                          GPIO_NUM_18, GPIO_NUM_23, GPIO_NUM_19, &dev_cfg1);
star_bus_manager_add_bus(&manager, &spi_config1);  // Success

dev_cfg2.spics_io_num = GPIO_NUM_5;  // Different CS
star_bus_spi_create_config(&spi_config2, "spi1", SPI2_HOST,
                          GPIO_NUM_18, GPIO_NUM_23, GPIO_NUM_19, &dev_cfg2);
star_bus_manager_add_bus(&manager, &spi_config2);  // Success - shared bus pins OK

dev_cfg3.spics_io_num = GPIO_NUM_5;  // Same CS as spi0
star_bus_spi_create_config(&spi_config3, "spi2", SPI2_HOST,
                          GPIO_NUM_18, GPIO_NUM_23, GPIO_NUM_19, &dev_cfg3);
star_bus_manager_add_bus(&manager, &spi_config3);  // FAILS - CS conflict
```

## Error Handling

The bus manager includes automatic retry logic with exponential backoff.

### Default Behavior

- **Max retries**: 3
- **Base delay**: 100ms
- **Max delay**: 5000ms
- **Backoff**: Exponential (delay doubles each retry)

### Custom Error Handler

```c
esp_err_t custom_reset(void* context) {
    ESP_LOGW(TAG, "Attempting bus reset...");
    // Perform custom recovery
    return ESP_OK;
}

star_bus_manager_set_reset_function(&manager, custom_reset, NULL);
```

### Error Recovery

```c
esp_err_t result = star_bus_i2c_write(&manager, "sensor_bus", data, 4, 0x10, NULL);
if (result != ESP_OK) {
    if (error_handler_can_retry(&manager.error_handler)) {
        ESP_LOGW(TAG, "Retrying operation...");
        // Automatic retry will occur on next operation
    } else {
        ESP_LOGE(TAG, "Max retries exceeded");
        error_handler_reset_state(&manager.error_handler);
    }
}
```

## Testing

The component includes comprehensive unit tests (225 tests total).

### Run All Tests

```bash
./run_tests.sh target    # Run on ESP32 hardware
```

### Test Coverage

- **Pin Validator**: 35 tests
- **Error Handler**: 30 tests
- **Bus Manager**: 50 tests
- **SPI Peripheral**: 22 tests
- **SMBus Protocol**: 32 tests
- **Protocol Parsing**: 36 tests
- **Integration**: 20 tests

See [TESTING.md](../../TESTING.md) for detailed testing documentation.

## API Reference

### Error Codes

All functions return `esp_err_t`:

- `ESP_OK` - Success
- `ESP_ERR_INVALID_ARG` - Invalid parameter
- `ESP_ERR_INVALID_STATE` - Invalid state for operation
- `ESP_ERR_NOT_FOUND` - Bus not found
- `ESP_ERR_NO_MEM` - Out of memory
- `ESP_ERR_TIMEOUT` - Operation timeout
- `ESP_FAIL` - Generic failure

### Bus Types

```c
typedef enum {
    STAR_BUS_TYPE_NONE = 0,
    STAR_BUS_TYPE_I2C,
    STAR_BUS_TYPE_SPI,
    STAR_BUS_TYPE_SPI_PERIPHERAL,
    STAR_BUS_TYPE_COUNT
} star_bus_type_t;
```

### Configuration Structures

```c
typedef struct {
    char name[STAR_BUS_MAX_NAME_LENGTH];
    star_bus_type_t type;
    bool initialized;
    union {
        star_bus_i2c_config_t i2c;
        star_bus_spi_config_t spi;
        star_bus_spi_peripheral_config_t spi_peripheral;
    } config;
} star_bus_config_t;
```

## Hardware Requirements

### ESP32 GPIO Constraints

- **Valid GPIO Range**: 0-39 (GPIO_NUM_MAX = 40)
- **Input-only pins**: GPIO 34-39 (cannot be used as outputs)
- **Strapping pins**: GPIO 0, 2, 5, 12, 15 (use with caution)

### I2C Specifications

- **Clock Speed**: 100kHz (standard), 400kHz (fast), 1MHz (fast plus)
- **Pull-ups**: External 4.7kOhm recommended for 100kHz
- **Max capacitance**: 400pF per bus

### SPI Specifications

- **Max Speed**: 80MHz (hardware limitation)
- **Recommended**: <=20MHz for reliable operation
- **Modes**: 0, 1, 2, 3 supported
- **Queue Size**: 1-7 transactions

### SMBus Specifications

- **Timeout**: 25-35ms (configurable, default 30ms)
- **Clock Low**: Min 4.7us
- **Block Size**: Max 32 bytes
- **PEC**: CRC-8 (polynomial 0x07)

## Examples

Complete usage examples are available in:

- [examples/i2c_sensor_example.c](examples/i2c_sensor_example.c) - I2C sensor interface
- [examples/spi_flash_example.c](examples/spi_flash_example.c) - SPI flash memory
- [examples/spi_peripheral_example.c](examples/spi_peripheral_example.c) - SPI slave mode
- [examples/smbus_battery_example.c](examples/smbus_battery_example.c) - Smart Battery System
- [examples/multi_bus_example.c](examples/multi_bus_example.c) - Multiple buses

## License

Part of the STAR project. See repository LICENSE for details.

## Contributing

See [styleguide.md](../../styleguide.md) for coding standards and contribution guidelines.
