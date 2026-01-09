# BMS Evaluation Firmware - Implementation Guide

This document provides a step-by-step guide to implementing the BMS command handler in the RX72N firmware.

## Architecture Overview

```
Desktop Tool (Wails)
    ↓ USB CDC (115200 baud, 8N1)
    ↓ Frame Protocol (0x55AA sync, CRC-32)
    ↓ Protocol Buffers (BmsCommandRequest/Response)
RX72N Firmware
    ↓ USB Task → receives frames, decodes protobuf
    ↓ BMS Task → executes commands
    ↓ rx_bus_manager + rx_bq78350
    ↓ SMBus (I2C, 100kHz)
BQ78350-R1A
    ↓
Battery Pack (4S-16S Li-ion)
```

## Required Libraries

- `rx_usb` - USB CDC hardware layer
- `rx_usb_comm` - Frame encoding/decoding with CRC-32
- `rx_bus_manager` - Bus management
- `rx_bq78350` - BQ78350 driver
- `nanopb` - Protocol Buffer encoding/decoding

## Implementation Steps

### Step 1: Initialize Hardware and Libraries

In `main.c` or initialization module:

```c
#include "rx_usb.h"
#include "rx_usb_comm.h"
#include "rx_bus_manager.h"
#include "rx_bq78350.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "star/v1/bms.pb.h"

static rx_bus_manager_t s_bus_manager;
static rx_usb_comm_handle_t s_usb_comm_handle;

void hardware_init(void) {
    // Initialize USB CDC
    rx_usb_config_t usb_config = {
        .vendor_id = 0x0483,
        .product_id = 0x5740,
        .manufacturer = "STAR Project",
        .product = "BMS Evaluation Tool",
    };
    rx_usb_init(&usb_config);

    // Initialize USB communication layer
    rx_usb_comm_init(&s_usb_comm_handle, &usb_config);

    // Wait for USB enumeration
    while (!rx_usb_is_configured()) {
        tx_thread_sleep(TX_TIMER_TICKS_PER_SECOND / 10);
    }

    // Initialize bus manager
    rx_bus_manager_init(&s_bus_manager);

    // Configure SMBus for BQ78350
    rx_bus_config_t bms_bus_config = {
        .name = "bms_smbus",
        .type = k_rx_bus_type_smbus,
        .proto = {
            .smbus = {
                .i2c_config = {
                    .channel = 0,  // RIIC0
                    .device_addr = 0x0B,  // BQ78350 default address
                    .frequency_hz = 100000,  // 100 kHz
                    .enable_pec = true,  // CRC-8 error checking
                },
            },
        },
    };
    rx_bus_manager_add_bus(&s_bus_manager, &bms_bus_config);

    // Initialize BQ78350 driver
    rx_bq78350_config_t bq78350_config = {
        .num_cells = 16,  // Adjust based on your battery pack
    };
    rx_bq78350_init(&s_bus_manager, "bms_smbus", &bq78350_config);
}
```

### Step 2: Implement BMS Command Handler

```c
static rx_err_t handle_read_telemetry(
    const star_v1_ReadTelemetryCommand* cmd,
    star_v1_BmsTelemetryData* telemetry
) {
    (void)cmd;  // No parameters in command

    rx_bq78350_status_t status;
    rx_err_t err = rx_bq78350_read_status(&s_bus_manager, "bms_smbus", &status, 16);
    if (err != k_rx_ok) {
        return err;
    }

    // Fill in protobuf telemetry structure
    telemetry->voltage_mv = status.voltage_mv;
    telemetry->current_ma = status.current_ma;
    telemetry->relative_soc_percent = status.relative_soc;
    telemetry->absolute_soc_percent = status.absolute_soc;
    telemetry->temperature_celsius = status.temperature_celsius;
    telemetry->remaining_capacity_mah = status.remaining_capacity_mah;
    telemetry->full_capacity_mah = status.full_capacity_mah;
    telemetry->design_capacity_mah = status.design_capacity_mah;
    telemetry->cycle_count = status.cycle_count;
    telemetry->time_to_empty_min = status.time_to_empty_min;
    telemetry->time_to_full_min = status.time_to_full_min;
    telemetry->is_charging = status.is_charging;
    telemetry->is_fully_charged = status.is_fully_charged;
    telemetry->is_fully_discharged = status.is_fully_discharged;
    telemetry->is_low_capacity = status.is_low_capacity;

    return k_rx_ok;
}

static rx_err_t handle_read_register(
    const star_v1_ReadRegisterCommand* cmd,
    star_v1_BmsRegisterData* reg_data
) {
    uint16_t value;
    rx_err_t err;

    if (cmd->num_bytes == 1) {
        uint8_t byte_val;
        err = rx_bq78350_read_byte(&s_bus_manager, "bms_smbus", cmd->address, &byte_val);
        value = byte_val;
    } else if (cmd->num_bytes == 2) {
        err = rx_bq78350_read_word(&s_bus_manager, "bms_smbus", cmd->address, &value);
    } else {
        return k_rx_err_invalid_arg;
    }

    if (err != k_rx_ok) {
        return err;
    }

    reg_data->address = cmd->address;
    reg_data->value = value;
    reg_data->num_bytes = cmd->num_bytes;

    return k_rx_ok;
}

static rx_err_t handle_write_register(
    const star_v1_WriteRegisterCommand* cmd,
    star_v1_BmsAckData* ack
) {
    rx_err_t err;

    if (cmd->num_bytes == 1) {
        err = rx_bq78350_write_byte(&s_bus_manager, "bms_smbus", cmd->address, (uint8_t)cmd->value);
    } else if (cmd->num_bytes == 2) {
        err = rx_bq78350_write_word(&s_bus_manager, "bms_smbus", cmd->address, (uint16_t)cmd->value);
    } else {
        return k_rx_err_invalid_arg;
    }

    ack->success = (err == k_rx_ok);
    if (err != k_rx_ok) {
        strncpy(ack->message, "Write failed", sizeof(ack->message) - 1);
    }

    return err;
}

static rx_err_t handle_read_cell_voltages(
    const star_v1_ReadCellVoltagesCommand* cmd,
    star_v1_BmsCellVoltagesData* cells
) {
    uint16_t cell_voltages[16];
    uint8_t num_cells = cmd->num_cells > 0 ? cmd->num_cells : 16;

    rx_err_t err = rx_bq78350_read_cell_voltages(
        &s_bus_manager, "bms_smbus", cell_voltages, num_cells
    );
    if (err != k_rx_ok) {
        return err;
    }

    // Copy to protobuf repeated field
    cells->cell_mv_count = num_cells;
    for (uint8_t i = 0; i < num_cells; i++) {
        cells->cell_mv[i] = cell_voltages[i];
    }

    // Calculate pack voltage, min, max, delta
    cells->pack_mv = 0;
    cells->min_cell_mv = 0xFFFF;
    cells->max_cell_mv = 0;

    for (uint8_t i = 0; i < num_cells; i++) {
        cells->pack_mv += cell_voltages[i];
        if (cell_voltages[i] < cells->min_cell_mv) {
            cells->min_cell_mv = cell_voltages[i];
        }
        if (cell_voltages[i] > cells->max_cell_mv) {
            cells->max_cell_mv = cell_voltages[i];
        }
    }

    cells->delta_mv = cells->max_cell_mv - cells->min_cell_mv;

    return k_rx_ok;
}

static rx_err_t handle_read_device_info(
    const star_v1_ReadDeviceInfoCommand* cmd,
    star_v1_BmsDeviceInfoData* info
) {
    (void)cmd;

    // Read manufacturer name
    rx_err_t err = rx_bq78350_read_manufacturer_name(
        &s_bus_manager, "bms_smbus", info->manufacturer, sizeof(info->manufacturer)
    );
    if (err != k_rx_ok) return err;

    // Read device name
    err = rx_bq78350_read_device_name(
        &s_bus_manager, "bms_smbus", info->device_name, sizeof(info->device_name)
    );
    if (err != k_rx_ok) return err;

    // Read chemistry
    err = rx_bq78350_read_chemistry(
        &s_bus_manager, "bms_smbus", info->chemistry, sizeof(info->chemistry)
    );
    if (err != k_rx_ok) return err;

    // Read serial number, versions, capacity, etc.
    // ...

    return k_rx_ok;
}
```

### Step 3: Implement BMS Task

```c
static void bms_task_entry(ULONG input) {
    (void)input;

    rx_usb_comm_frame_t rx_frame;
    uint8_t tx_payload[512];

    while (1) {
        // Receive frame from USB
        rx_err_t err = rx_usb_comm_receive(
            &s_usb_comm_handle, &rx_frame, 100  // 100ms timeout
        );

        if (err == k_rx_err_timeout) {
            continue;  // No data, keep waiting
        }

        if (err != k_rx_ok) {
            continue;  // Frame error, discard
        }

        // Decode BmsCommandRequest
        star_v1_BmsCommandRequest request = star_v1_BmsCommandRequest_init_zero;
        pb_istream_t istream = pb_istream_from_buffer(rx_frame.payload, rx_frame.length);

        if (!pb_decode(&istream, star_v1_BmsCommandRequest_fields, &request)) {
            // Protobuf decode error, send error response
            continue;
        }

        // Process command
        star_v1_BmsCommandResponse response = star_v1_BmsCommandResponse_init_zero;
        response.header.status = star_v1_Status_STATUS_OK;

        if (request.which_command == star_v1_BmsCommandRequest_read_telemetry_tag) {
            err = handle_read_telemetry(&request.command.read_telemetry, &response.response.telemetry_data);
            response.which_response = star_v1_BmsCommandResponse_telemetry_data_tag;
        }
        else if (request.which_command == star_v1_BmsCommandRequest_read_register_tag) {
            err = handle_read_register(&request.command.read_register, &response.response.register_data);
            response.which_response = star_v1_BmsCommandResponse_register_data_tag;
        }
        else if (request.which_command == star_v1_BmsCommandRequest_write_register_tag) {
            err = handle_write_register(&request.command.write_register, &response.response.ack_data);
            response.which_response = star_v1_BmsCommandResponse_ack_data_tag;
        }
        else if (request.which_command == star_v1_BmsCommandRequest_read_cell_voltages_tag) {
            err = handle_read_cell_voltages(&request.command.read_cell_voltages, &response.response.cell_voltages_data);
            response.which_response = star_v1_BmsCommandResponse_cell_voltages_data_tag;
        }
        else if (request.which_command == star_v1_BmsCommandRequest_read_device_info_tag) {
            err = handle_read_device_info(&request.command.read_device_info, &response.response.device_info_data);
            response.which_response = star_v1_BmsCommandResponse_device_info_data_tag;
        }
        else {
            response.header.status = star_v1_Status_STATUS_INVALID_REQUEST;
        }

        // Set error status if command failed
        if (err != k_rx_ok) {
            response.header.status = star_v1_Status_STATUS_INTERNAL_ERROR;
            strncpy(response.header.error_message, "Command execution failed", sizeof(response.header.error_message) - 1);
        }

        // Encode response
        pb_ostream_t ostream = pb_ostream_from_buffer(tx_payload, sizeof(tx_payload));
        if (!pb_encode(&ostream, star_v1_BmsCommandResponse_fields, &response)) {
            continue;  // Encode error
        }

        // Send response frame
        rx_usb_comm_send(
            &s_usb_comm_handle,
            k_frame_type_response,
            0,  // flags
            tx_payload,
            ostream.bytes_written
        );
    }
}
```

## Testing Procedure

1. **Build and flash firmware**:
   ```bash
   cd star-rx72n-firmware
   ./build.sh
   ./flash.sh
   ```

2. **Connect hardware**:
   - RX72N USB to PC
   - BQ78350 SMBus to RX72N (P12/SCL, P13/SDA, GND)
   - Battery pack to BQ78350

3. **Launch desktop tool**:
   ```bash
   cd star-bms-tool
   wails dev
   ```

4. **Test sequence**:
   - Select COM port and click Connect
   - Verify telemetry updates every second
   - Switch to Cell Voltages tab, click Refresh
   - Test register read (address 0x09 for voltage)
   - Switch to Device Info tab

## Troubleshooting

### USB Communication Issues
- Check USB cable quality
- Verify USB CDC driver loaded (should be automatic)
- Monitor with serial terminal at 115200 baud to see raw frames

### BMS Communication Issues
- Verify SMBus connections with multimeter or oscilloscope
- Check pull-up resistors on SCL/SDA (4.7kΩ typical)
- Try lower SMBus frequency (50 kHz) if seeing CRC errors
- Use logic analyzer to debug SMBus timing

### Protocol Issues
- Add debug logging to see decoded protobuf messages
- Verify CRC-32 calculations match (use test vectors)
- Check frame sync byte alignment (0x55AA)

## Next Steps

1. Implement remaining commands (block read/write, manufacturer access)
2. Add error recovery and retry logic
3. Implement battery protection monitoring
4. Add data logging to SD card
5. Create automated test suite

## References

- BQ78350-R1A Data Sheet
- Smart Battery System (SBS) 1.1 Specification
- nanopb documentation: https://jpa.kapsi.fi/nanopb/
- rx_usb_comm frame protocol: `lib/rx_usb/inc/rx_usb_comm.h`
- rx_bq78350 API: `lib/rx_bms/inc/rx_bq78350.h`
