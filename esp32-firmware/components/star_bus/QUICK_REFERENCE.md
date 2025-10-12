# STAR Bus Manager - Quick Reference

## Quick Start (30 seconds)

```c
#include "star_bus_manager.h"
#include "star_bus_i2c.h"

// 1. Initialize
star_bus_manager_t manager;
star_bus_manager_init(&manager, "MyApp");

// 2. Add I2C bus
star_bus_config_t config;
star_bus_i2c_create_config(&config, "sensor", I2C_NUM_0,
                          GPIO_NUM_21, GPIO_NUM_22, 100000, 0x76);
star_bus_manager_add_bus(&manager, &config);

// 3. Use it
uint8_t data[4];
star_bus_i2c_read(&manager, "sensor", data, 4, 0x10, NULL);

// 4. Cleanup
star_bus_manager_deinit(&manager);
```

---

## Common Operations

### I2C Operations

```c
// Write to register
uint8_t data[] = {0x01, 0x02, 0x03};
star_bus_i2c_write(&manager, "bus_name", data, 3, 0x10, NULL);
//                                                     ^^^^ register address

// Read from register
uint8_t buffer[10];
star_bus_i2c_read(&manager, "bus_name", buffer, 10, 0x20, NULL);

// Write command only
star_bus_i2c_write_command(&manager, "bus_name", 0x30);

// Read without command
star_bus_i2c_read_raw(&manager, "bus_name", buffer, 5, NULL);
```

### SPI Operations

```c
// Transmit only
uint8_t tx_data[] = {0x01, 0x02, 0x03};
star_bus_spi_transmit(&manager, "spi_bus", tx_data, 3, NULL);

// Receive only
uint8_t rx_data[10];
star_bus_spi_receive(&manager, "spi_bus", rx_data, 10, NULL);

// Transceive (simultaneous TX/RX)
uint8_t tx[5] = {0x9F, 0x00, 0x00, 0x00, 0x00};
uint8_t rx[5];
star_bus_spi_transceive(&manager, "spi_bus", tx, rx, 5, NULL);
```

### SMBus Operations

```c
// Read byte
uint8_t value;
star_smbus_read_byte(&manager, "bus", 0x50, 0x10, &value);
//                                    ^^^^  ^^^^
//                                    addr  cmd

// Write byte
star_smbus_write_byte(&manager, "bus", 0x50, 0x20, 0xAB);

// Read word (16-bit, little-endian)
uint16_t word;
star_smbus_read_word(&manager, "bus", 0x50, 0x30, &word);

// Write word
star_smbus_write_word(&manager, "bus", 0x50, 0x40, 0x1234);

// Block read (up to 32 bytes)
uint8_t block[32];
uint8_t length;
star_smbus_block_read(&manager, "bus", 0x50, 0x50, block, 32, &length);

// Block write
uint8_t data[10] = {...};
star_smbus_block_write(&manager, "bus", 0x50, 0x60, data, 10);
```

### SPI Peripheral Operations

```c
// Receive from master
uint8_t rx_buffer[32];
star_bus_spi_peripheral_receive(&manager, "slave", rx_buffer, 32, NULL);

// Transmit to master
uint8_t tx_buffer[32] = {...};
star_bus_spi_peripheral_transmit(&manager, "slave", tx_buffer, 32, NULL);

// Transceive (receive while sending)
uint8_t rx[32], tx[32] = {...};
star_bus_spi_peripheral_transceive(&manager, "slave", tx, rx, 32, NULL);
```

---

## Configuration Examples

### I2C Bus

```c
star_bus_config_t config;
star_bus_i2c_create_config(
    &config,
    "my_i2c",       // Bus name
    I2C_NUM_0,      // I2C port (0 or 1)
    GPIO_NUM_21,    // SDA pin
    GPIO_NUM_22,    // SCL pin
    100000,         // Clock: 100kHz (or 400000, 1000000)
    0x76            // Device address (7-bit)
);
```

### SPI Master

```c
spi_device_interface_config_t dev_cfg = {
    .mode = 0,                    // SPI mode (0-3)
    .clock_speed_hz = 1000000,    // 1 MHz
    .spics_io_num = GPIO_NUM_5,   // CS pin
    .queue_size = 3
};

star_bus_config_t config;
star_bus_spi_create_config(
    &config,
    "my_spi",       // Bus name
    SPI2_HOST,      // SPI host (SPI2_HOST or SPI3_HOST)
    GPIO_NUM_23,    // MOSI
    GPIO_NUM_25,    // MISO
    GPIO_NUM_19,    // SCLK
    &dev_cfg
);
```

### SPI Peripheral

```c
star_bus_config_t config;
star_bus_spi_peripheral_create_config(
    &config,
    "my_slave",     // Bus name
    SPI2_HOST,      // SPI host
    GPIO_NUM_12,    // MISO
    GPIO_NUM_13,    // MOSI
    GPIO_NUM_14,    // SCLK
    GPIO_NUM_15,    // CS
    0,              // Mode (0-3)
    7               // Queue size (1-7)
);
```

---

## Error Handling

### Check Return Values

```c
esp_err_t result = star_bus_i2c_read(...);
if (result != ESP_OK) {
    ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(result));
    // Handle error
}
```

### Common Error Codes

| Code | Meaning |
|------|---------|
| `ESP_OK` | Success |
| `ESP_ERR_INVALID_ARG` | Invalid parameter |
| `ESP_ERR_INVALID_STATE` | Invalid state |
| `ESP_ERR_NOT_FOUND` | Bus not found |
| `ESP_ERR_TIMEOUT` | Operation timeout |
| `ESP_FAIL` | Generic failure |

### Retry Logic

```c
// Automatic retry is built-in
if (error_handler_can_retry(&manager.error_handler)) {
    // Will retry automatically
} else {
    // Max retries exceeded
    error_handler_reset_state(&manager.error_handler);
}
```

### Custom Reset Function

```c
esp_err_t my_reset(void* context) {
    // Custom recovery logic
    return ESP_OK;
}

star_bus_manager_set_reset_function(&manager, my_reset, NULL);
```

---

## Pin Sharing Rules

### ✅ Shareable Pins
- I2C SDA/SCL
- SPI MISO/MOSI/SCLK

Multiple buses can share these pins.

### ❌ Non-Shareable Pins
- SPI CS (Chip Select)

Each SPI device needs unique CS pin.

### Example

```c
// Bus 1: SD card with CS on GPIO 5
star_bus_spi_create_config(..., GPIO_NUM_5, ...);  // OK

// Bus 2: Flash with CS on GPIO 26, shares MISO/MOSI/SCLK
star_bus_spi_create_config(..., GPIO_NUM_26, ...);  // OK - different CS

// Bus 3: Trying to reuse GPIO 5 as CS
star_bus_spi_create_config(..., GPIO_NUM_5, ...);  // FAILS - CS conflict
```

---

## GPIO Constraints

### ESP32 Valid Pins
- **Range**: GPIO 0-39
- **Input-only**: GPIO 34-39 (cannot be outputs)
- **Avoid**: GPIO 0, 2, 5, 12, 15 (strapping pins)

### Recommended Pins

**I2C**:
- Standard: GPIO 21 (SDA), GPIO 22 (SCL)

**SPI**:
- MOSI: GPIO 23
- MISO: GPIO 25
- SCLK: GPIO 19
- CS: GPIO 5 (or any available)

---

## SMBus Protocol Cheat Sheet

| Function | Use Case |
|----------|----------|
| `quick_command` | Device polling, on/off control |
| `send_byte` | Send single byte (no register) |
| `receive_byte` | Receive single byte (no register) |
| `write_byte` | Write to 8-bit register |
| `read_byte` | Read from 8-bit register |
| `write_word` | Write 16-bit value |
| `read_word` | Read 16-bit value |
| `process_call` | Write word, read word response |
| `block_write` | Write up to 32 bytes |
| `block_read` | Read up to 32 bytes |
| `block_process_call` | Write block, read block response |

### SMBus Timing
- Timeout: 30ms (default)
- Max block size: 32 bytes
- Word encoding: Little-endian

---

## Common Patterns

### Polling a Sensor

```c
while (1) {
    uint8_t temp[3];
    if (star_bus_i2c_read(&mgr, "sensor", temp, 3, 0xFA, NULL) == ESP_OK) {
        float temperature = (temp[0] << 12 | temp[1] << 4 | temp[2] >> 4) / 5120.0f;
        ESP_LOGI(TAG, "Temp: %.2f C", temperature);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

### Reading Smart Battery

```c
uint16_t voltage, current;
star_smbus_read_word(&mgr, "battery", 0x0B, 0x09, &voltage);
star_smbus_read_word(&mgr, "battery", 0x0B, 0x0A, &current);
ESP_LOGI(TAG, "Battery: %umV, %dmA", voltage, (int16_t)current);
```

### SPI Flash Read

```c
uint8_t tx[] = {0x03, 0x00, 0x00, 0x00};  // Read command + address
uint8_t rx[260];  // 4 dummy + 256 data
star_bus_spi_transceive(&mgr, "flash", tx, rx, 260, NULL);
// Data starts at rx[4]
```

---

## Testing

### Run All Tests

```bash
./run_tests.sh target    # Run on ESP32 hardware
```

### Build Only

```bash
cd test_app
idf.py build
```

### Flash and Monitor

```bash
idf.py -p /dev/ttyUSB1 flash monitor
```

---

## Troubleshooting

### Bus Not Found
```
Error: ESP_ERR_NOT_FOUND
Fix: Check bus name spelling
```

### Pin Conflict
```
Error: Pin already registered
Fix: Use different GPIO or share bus signals with different CS
```

### Timeout
```
Error: ESP_ERR_TIMEOUT
Fix:
- Check wiring
- Verify device address
- Add pull-ups (I2C: 4.7kΩ recommended)
```

### Invalid GPIO
```
Error: ESP_ERR_INVALID_ARG
Fix: Use GPIO 0-39 (avoid 34-39 for outputs)
```

---

## Performance Tips

1. **I2C Speed**: Use 400kHz for faster transfers (if device supports)
2. **SPI Queue**: Increase queue size for burst transfers
3. **Buffer Size**: Use appropriate buffer sizes (no dynamic allocation)
4. **Error Recovery**: Set custom reset function for application-specific recovery

---

## More Information

- **Full Documentation**: `components/star_bus/README.md`
- **Examples**: `components/star_bus/examples/`
- **Testing Guide**: `TESTING.md`
- **API Reference**: See header files in `components/star_bus/include/`

---

**Quick Links**:
- [I2C Example](examples/i2c_sensor_example.c)
- [SPI Example](examples/spi_flash_example.c)
- [SMBus Example](examples/smbus_battery_example.c)
- [Multi-Bus Example](examples/multi_bus_example.c)
