/**
 * @file temp_sensor_task.h
 * @brief Temperature Sensor Task - DS18B20 Digital Thermometer Monitoring
 *
 * @details
 * Declares the temperature sensor task responsible for reading DS18B20
 * 1-Wire digital temperature sensors for thermal monitoring.
 *
 * **Responsibilities:**
 * - Read temperature from DS18B20 sensors
 * - Monitor critical temperatures (motor, battery, ambient)
 * - Trigger warnings on high temperature conditions
 * - Update shared temperature data
 * - Detect sensor faults (CRC errors, disconnects)
 *
 * **Sensor Configuration:**
 * - Sensors: DS18B20 digital thermometers
 * - Interface: 1-Wire protocol
 * - Resolution: 12-bit (0.0625degC precision)
 * - Range: -55degC to +125degC
 * - Update rate: 10 Hz (100 ms period)
 *
 * @see temp_sensor_task.c Implementation
 * @see rx_ds18b20.h DS18B20 driver
 * @see shared_data.h Temperature data
 *
 * @copyright Copyright (c) 2026 STAR Project
 */

#pragma once

#include "rx_err.h"

/**
 * @brief Create and start the temperature sensor task
 *
 * @details
 * Creates the TempSensorTask ThreadX task for temperature monitoring.
 * This task periodically reads DS18B20 sensors and detects thermal issues.
 *
 * **Task Configuration:**
 * - Priority: 11 (medium-low priority - thermal monitoring not time-critical)
 * - Stack: 2048 bytes
 * - Period: 100 ms (10 Hz temperature monitoring)
 * - Auto-start: Enabled
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Task created successfully
 * @retval k_rx_err_* ThreadX task creation failed
 *
 * @pre ThreadX kernel running
 * @pre DS18B20 sensors initialized
 * @pre 1-Wire bus configured
 * @pre Shared data initialized
 *
 * @post TempSensorTask created and running at 10 Hz
 * @post Temperature data updated in shared memory
 * @post Warnings triggered on high temperatures
 *
 * @note Call this from AppMainTask after sensor initialization
 * @note Thermal monitoring prevents motor/battery overheating
 *
 * @see temp_sensor_task.c Implementation details
 * @see app_main_task.c Task creation coordinator
 *
 * @since STAR v1.0.0
 */
rx_err_t temp_sensor_task_create(void);
