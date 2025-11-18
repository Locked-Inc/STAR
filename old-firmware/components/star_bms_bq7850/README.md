# BQ7850 Battery Management System Driver

This component provides a complete driver for the Texas Instruments BQ7850 multi-cell battery monitor and protector. It integrates seamlessly with the STAR Bus Manager and provides comprehensive battery monitoring and protection features.

## Features

- **Multi-cell monitoring**: Support for up to 16 series-connected cells
- **Cell voltage monitoring**: Individual cell and pack voltage readings
- **Temperature monitoring**: Support for up to 3 temperature sensors
- **Current measurement**: Instantaneous and average current monitoring
- **State of charge tracking**: Coulomb counting with SOC estimation
- **Cell balancing**: Programmable cell balancing control
- **Protection features**: Overvoltage, undervoltage, overcurrent, and temperature protection
- **SMBus communication**: Standard SMBus 2.0 protocol support
- **FET control**: Charge and discharge FET management

## Hardware Connection

The BQ7850 communicates via SMBus (I2C compatible):

```
ESP32          BQ7850
-----          ------
GPIO21 (SDA) -> SDA
GPIO22 (SCL) -> SCL
GND          -> GND
3.3V         -> VCC (if needed)
```

Default SMBus address: `0x08`

## Quick Start

### 1. Include Headers

```c
#include "star_bms_bq7850.h"
#include "star_bus_manager.h"
```

### 2. Initialize Bus Manager and I2C Bus

```c
star_bus_manager_t manager;
star_bus_manager_init(&manager, "BMS");

star_bus_config_t i2c_config;
star_bus_i2c_create_config(&i2c_config, "bms_i2c", I2C_NUM_0,
                           GPIO_NUM_21, GPIO_NUM_22, 100000,
                           BQ7850_DEFAULT_ADDR);

star_bus_manager_add_bus(&manager, &i2c_config);
```

### 3. Initialize BQ7850

```c
bq7850_config_t config = {
    .num_cells       = 4,                  // 4S battery
    .num_temp        = 1,                  // 1 temp sensor
    .smbus_addr      = BQ7850_DEFAULT_ADDR,
    .design_capacity = 2500,               // 2500 mAh
    .design_voltage  = 14800,              // 14.8V
};

esp_err_t ret = star_bms_bq7850_init(&manager, "bms_i2c", &config);
```

### 4. Read Battery State

```c
bq7850_battery_state_t state;
star_bms_bq7850_read_battery_state(&manager, "bms_i2c", &state);

// Access battery data
printf("Pack voltage: %d mV\n", state.cells.pack_mv);
printf("SOC: %d%%\n", state.soc.relative_soc);
printf("Current: %d mA\n", state.current.current_ma);
```

## API Reference

### Initialization

```c
esp_err_t star_bms_bq7850_init(star_bus_manager_t* manager,
                               const char* bus_name,
                               const bq7850_config_t* config);
```

Initialize the BQ7850 device and verify communication.

### Cell Voltage Monitoring

```c
// Read all cells
esp_err_t star_bms_bq7850_read_cells(star_bus_manager_t* manager,
                                     const char* bus_name,
                                     bq7850_cell_data_t* cell_data);

// Read single cell
esp_err_t star_bms_bq7850_read_cell_voltage(star_bus_manager_t* manager,
                                            const char* bus_name,
                                            uint8_t cell_index,
                                            uint16_t* voltage_mv);

// Read pack voltage
esp_err_t star_bms_bq7850_read_pack_voltage(star_bus_manager_t* manager,
                                            const char* bus_name,
                                            uint16_t* voltage_mv);
```

### Temperature Monitoring

```c
// Read all temperature sensors
esp_err_t star_bms_bq7850_read_temperatures(star_bus_manager_t* manager,
                                            const char* bus_name,
                                            bq7850_temp_data_t* temp_data);

// Read pack temperature
esp_err_t star_bms_bq7850_read_temperature(star_bus_manager_t* manager,
                                           const char* bus_name,
                                           int16_t* temp_c);
```

### Current and Power

```c
esp_err_t star_bms_bq7850_read_current(star_bus_manager_t* manager,
                                       const char* bus_name,
                                       bq7850_current_data_t* current_data);
```

### State of Charge

```c
esp_err_t star_bms_bq7850_read_soc(star_bus_manager_t* manager,
                                   const char* bus_name,
                                   bq7850_soc_data_t* soc_data);
```

### Status and Faults

```c
// Read all status flags
esp_err_t star_bms_bq7850_read_status(star_bus_manager_t* manager,
                                      const char* bus_name,
                                      bq7850_status_t* status);

// Read complete battery state (all parameters)
esp_err_t star_bms_bq7850_read_battery_state(star_bus_manager_t* manager,
                                             const char* bus_name,
                                             bq7850_battery_state_t* state);
```

### Cell Balancing

```c
// Enable balancing for specific cells
esp_err_t star_bms_bq7850_enable_cell_balancing(star_bus_manager_t* manager,
                                                const char* bus_name,
                                                uint16_t cell_mask);

// Disable all balancing
esp_err_t star_bms_bq7850_disable_cell_balancing(star_bus_manager_t* manager,
                                                 const char* bus_name);

// Get balancing status
esp_err_t star_bms_bq7850_get_balancing_status(star_bus_manager_t* manager,
                                               const char* bus_name,
                                               uint16_t* active_mask);
```

### Protection

```c
// Read protection thresholds
esp_err_t star_bms_bq7850_read_protection(star_bus_manager_t* manager,
                                          const char* bus_name,
                                          bq7850_protection_t* protection);

// Write protection thresholds (requires unsealed device)
esp_err_t star_bms_bq7850_write_protection(star_bus_manager_t* manager,
                                           const char* bus_name,
                                           const bq7850_protection_t* protection);
```

### FET Control

```c
esp_err_t star_bms_bq7850_control_fets(star_bus_manager_t* manager,
                                       const char* bus_name,
                                       bool charge_fet,
                                       bool discharge_fet);
```

## Data Structures

### Battery State

```c
typedef struct {
    bq7850_cell_data_t    cells;    // Cell voltage data
    bq7850_temp_data_t    temps;    // Temperature data
    bq7850_current_data_t current;  // Current and power
    bq7850_soc_data_t     soc;      // State of charge
    bq7850_status_t       status;   // Status flags
} bq7850_battery_state_t;
```

### Cell Data

```c
typedef struct {
    uint16_t cell_mv[BQ7850_MAX_CELLS];  // Individual cell voltages (mV)
    uint8_t  valid_cells;                 // Number of valid readings
    uint16_t pack_mv;                     // Total pack voltage (mV)
} bq7850_cell_data_t;
```

### Temperature Data

```c
typedef struct {
    int16_t temp_c[BQ7850_MAX_TEMP_SENSORS];  // Temps in 0.1degC
    uint8_t valid_sensors;                     // Number of valid sensors
    int16_t avg_temp_c;                        // Average temp (0.1degC)
} bq7850_temp_data_t;
```

## Safety Status Flags

The BQ7850 provides comprehensive protection monitoring:

- `BQ7850_SAFETY_STATUS_CUV` - Cell undervoltage
- `BQ7850_SAFETY_STATUS_COV` - Cell overvoltage
- `BQ7850_SAFETY_STATUS_OCC` - Overcurrent in charge
- `BQ7850_SAFETY_STATUS_OCD` - Overcurrent in discharge
- `BQ7850_SAFETY_STATUS_OTC` - Over-temperature charge
- `BQ7850_SAFETY_STATUS_OTD` - Over-temperature discharge
- `BQ7850_SAFETY_STATUS_UTC` - Under-temperature charge
- `BQ7850_SAFETY_STATUS_UTD` - Under-temperature discharge

## Example Usage

See `examples/bq7850_example.c` for a complete working example that demonstrates:

- BMS initialization
- Periodic battery state monitoring
- Fault detection and reporting
- Automatic cell balancing
- Temperature monitoring

## Configuration

### Typical 4S Li-ion/LiPo Configuration

```c
bq7850_config_t config = {
    .num_cells       = 4,      // 4S configuration
    .num_temp        = 1,      // 1 temperature sensor
    .smbus_addr      = 0x08,   // Default address
    .design_capacity = 2500,   // 2500 mAh
    .design_voltage  = 14800,  // 14.8V nominal (4 x 3.7V)
};
```

### Protection Thresholds (typical Li-ion)

```c
bq7850_protection_t protection = {
    .overvoltage_mv   = 4200,  // 4.2V per cell max
    .undervoltage_mv  = 3000,  // 3.0V per cell min
    .overcharge_ma    = 5000,  // 5A charge limit
    .overdischarge_ma = 10000, // 10A discharge limit
    .overtemp_c       = 600,   // 60degC max (in 0.1degC)
    .undertemp_c      = -100,  // -10degC min (in 0.1degC)
};
```

## Cell Balancing

Cell balancing is automatically managed by the BQ7850, but can be manually controlled:

```c
// Enable balancing for cells 1 and 3
uint16_t balance_mask = (1 << 0) | (1 << 2);  // Bits 0 and 2
star_bms_bq7850_enable_cell_balancing(&manager, "bms_i2c", balance_mask);

// Check balancing status
uint16_t active_mask;
star_bms_bq7850_get_balancing_status(&manager, "bms_i2c", &active_mask);

// Disable all balancing
star_bms_bq7850_disable_cell_balancing(&manager, "bms_i2c");
```

## Testing

The component includes comprehensive unit tests:

```bash
cd esp32-firmware
idf.py -C components/star_bms_bq7850/test build flash monitor
```

## Troubleshooting

### Communication Errors

If you get communication errors:

1. Check I2C connections (SDA, SCL, GND)
2. Verify pull-up resistors (typically 4.7kOhm on SDA and SCL)
3. Check SMBus address (default 0x08)
4. Verify I2C bus speed (100 kHz recommended)

### Invalid Readings

If readings are incorrect:

1. Ensure proper BQ7850 power supply
2. Check battery connections to BQ7850
3. Verify calibration data is loaded
4. Check for EMI interference

### Cell Balancing Not Working

1. Verify cells are above minimum voltage threshold
2. Check balancing is enabled
3. Ensure current is low (balancing typically disabled during charge/discharge)
4. Verify balancing threshold configuration

## Technical Details

### SMBus Communication

The BQ7850 uses SMBus 2.0 protocol:
- Standard read/write byte
- Read/write word (16-bit, little-endian)
- Block read for strings and extended data
- Manufacturer access commands for extended functionality

### Register Access

Most parameters are accessed via standard SMBus commands (0x00-0xFF).
Extended parameters use manufacturer access subcommands via register 0x00.

### Temperature Format

Temperatures are reported in 0.1K units. Use the helper function to convert:

```c
int16_t temp_raw = 2981;  // 298.1K from BQ7850
float temp_c = star_bms_bq7850_convert_temp_to_celsius(temp_raw);
// Returns 25.0degC
```

## Dependencies

- `star_bus` - STAR Bus Manager for I2C/SMBus communication
- `star_error_handler` - Error handling utilities
- ESP-IDF FreeRTOS components

## License

Part of the STAR project. See repository LICENSE for details.

## References

- BQ7850 Technical Reference Manual (Texas Instruments)
- SMBus Specification 2.0
- Smart Battery Data Specification
