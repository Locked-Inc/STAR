# ESP32 Firmware Cleanup Plan

Based on the analysis of the project documentation and confirmation, the following libraries and library components have been identified as unnecessary for the final project and are slated for removal.

## Unnecessary Libraries

1.  **`star_servo`**
    *   **Reason:** Not mentioned in any implementation plans. Considered a remnant of an earlier design.

2.  **`star_sensor_mpu6050`**
    *   **Reason:** Redundant. The `BNO055` is the primary IMU for the project. This library is for a legacy hardware component.

3.  **`star_sensor_hcsr04`**
    *   **Reason:** Hardware is not connected to the ESP32. The ultrasonic sensors are connected to the Raspberry Pi 5.

4.  **`star_sensor_bno055_bmp280`**
    *   **Reason:** Hardware is not connected to the ESP32. The IMU is connected to the Raspberry Pi 5.

5.  **`star_sensor_pca9685`**
    *   **Reason:** Used only for a software demonstration (`led_task` and `dht22_task`). It will not be needed in the final project.

## Unnecessary Components within `star_bus` Library

The `star_bus` library is essential, but can be slimmed down. Based on the confirmed hardware connections, the only required protocols are **SPI**, **SMBus (I2C)**, and **1-Wire**.

### Unnecessary Protocol Implementations

The following files implementing unused communication protocols can be removed. Note that removing these will require editing `star_bus_config.c` to remove their corresponding `#include` and `create` functions.

1.  **`star_bus_uart.c`**
    *   **Reason:** No essential hardware connected to the ESP32 uses UART.

2.  **`star_bus_dht22_proprietary.c`**
    *   **Reason:** The DHT22 sensor was for a software demo only.

3.  **`star_bus_gpio.c`**
    *   **Reason:** The primary consumer of this was likely the `HC-SR04` sensor, which is not connected to the ESP32.

### Unnecessary Peripheral-Mode Drivers

The following files for implementing peripheral (slave) mode can be removed, as the ESP32 acts as the controller (master) on these buses.

1.  **`star_bus_i2c_peripheral.c`**
2.  **`star_bus_smbus_peripheral.c`**

**Note:** `star_bus_spi_peripheral.c` **must be kept**, as it is required for the ESP32 to communicate with the Raspberry Pi, which acts as the SPI controller.

### Unnecessary Optional Utilities

The following files provide optional, higher-level utility functions. They are not direct dependencies of the essential protocol drivers and can be removed for a minimal firmware.

1.  **`star_bus_async.c`**
2.  **`star_bus_batch.c`**
3.  **`star_bus_debug.c`**
4.  **`star_bus_devices.c`**
5.  **`star_bus_stats.c`** (A `FIXME` comment in the code suggests this is for debugging and should be deleted).
6.  **`star_bus_helpers.c`**
