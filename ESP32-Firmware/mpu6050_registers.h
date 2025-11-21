/**
 * MPU6050 Register Definitions and Constants
 *
 * Complete register map for the InvenSense MPU-6050 6-axis IMU
 * Designed for ESP32-IDF development
 *
 * Register addresses: 0x00-0x75
 * I2C Addresses: 0x68 (AD0=0) or 0x69 (AD0=1)
 */

#ifndef MPU6050_REGISTERS_H
#define MPU6050_REGISTERS_H

#include <stdint.h>

// ============================================================================
// I2C SLAVE ADDRESSES
// ============================================================================

#define MPU6050_ADDR_AD0_LOW    0x68    // AD0 connected to GND
#define MPU6050_ADDR_AD0_HIGH   0x69    // AD0 connected to VDD
#define MPU6050_DEVICE_ID       0x68    // WHO_AM_I register value

// ============================================================================
// REGISTER MAP - ADDRESSES
// ============================================================================

// Self-Test and Factory Trim Values (0x00-0x05)
#define MPU6050_REG_XGOFFS_TC           0x00    // X Gyroscope Offset TC
#define MPU6050_REG_YGOFFS_TC           0x01    // Y Gyroscope Offset TC
#define MPU6050_REG_ZGOFFS_TC           0x02    // Z Gyroscope Offset TC
#define MPU6050_REG_X_FINE_GAIN         0x03    // X Fine Gain
#define MPU6050_REG_Y_FINE_GAIN         0x04    // Y Fine Gain
#define MPU6050_REG_Z_FINE_GAIN         0x05    // Z Fine Gain

// Accelerometer Offset Registers (0x06-0x0B)
#define MPU6050_REG_XA_OFFSET_H         0x06    // X Accel Offset High Byte
#define MPU6050_REG_XA_OFFSET_L_TC      0x07    // X Accel Offset Low Byte + TC
#define MPU6050_REG_YA_OFFSET_H         0x08    // Y Accel Offset High Byte
#define MPU6050_REG_YA_OFFSET_L_TC      0x09    // Y Accel Offset Low Byte + TC
#define MPU6050_REG_ZA_OFFSET_H         0x0A    // Z Accel Offset High Byte
#define MPU6050_REG_ZA_OFFSET_L_TC      0x0B    // Z Accel Offset Low Byte + TC

// Self-Test Factory Output Values (0x0D-0x10)
#define MPU6050_REG_SELF_TEST_X         0x0D    // Self Test X Output
#define MPU6050_REG_SELF_TEST_Y         0x0E    // Self Test Y Output
#define MPU6050_REG_SELF_TEST_Z         0x0F    // Self Test Z Output
#define MPU6050_REG_SELF_TEST_A         0x10    // Self Test A Output

// Gyroscope User-Controlled Offset Trim (0x13-0x18)
#define MPU6050_REG_XG_OFFS_USRH        0x13    // X Gyro User Offset High
#define MPU6050_REG_XG_OFFS_USRL        0x14    // X Gyro User Offset Low
#define MPU6050_REG_YG_OFFS_USRH        0x15    // Y Gyro User Offset High
#define MPU6050_REG_YG_OFFS_USRL        0x16    // Y Gyro User Offset Low
#define MPU6050_REG_ZG_OFFS_USRH        0x17    // Z Gyro User Offset High
#define MPU6050_REG_ZG_OFFS_USRL        0x18    // Z Gyro User Offset Low

// Configuration Registers (0x19-0x22)
#define MPU6050_REG_SMPLRT_DIV          0x19    // Sample Rate Divider
#define MPU6050_REG_CONFIG              0x1A    // Configuration (DLPF)
#define MPU6050_REG_GYRO_CONFIG         0x1B    // Gyroscope Configuration
#define MPU6050_REG_ACCEL_CONFIG        0x1C    // Accelerometer Configuration
#define MPU6050_REG_FF_THR              0x1D    // Free Fall Threshold
#define MPU6050_REG_FF_DUR              0x1E    // Free Fall Duration
#define MPU6050_REG_MOT_THR             0x1F    // Motion Threshold
#define MPU6050_REG_MOT_DUR             0x20    // Motion Duration
#define MPU6050_REG_ZRMOT_THR           0x21    // Zero Motion Threshold
#define MPU6050_REG_ZRMOT_DUR           0x22    // Zero Motion Duration

// FIFO and I2C Master (0x23-0x36)
#define MPU6050_REG_FIFO_EN             0x23    // FIFO Enable
#define MPU6050_REG_I2C_MST_CTRL        0x24    // I2C Master Control
#define MPU6050_REG_I2C_SLV0_ADDR       0x25    // I2C Slave 0 Address
#define MPU6050_REG_I2C_SLV0_REG        0x26    // I2C Slave 0 Register
#define MPU6050_REG_I2C_SLV0_CTRL       0x27    // I2C Slave 0 Control
#define MPU6050_REG_I2C_SLV1_ADDR       0x28    // I2C Slave 1 Address
#define MPU6050_REG_I2C_SLV1_REG        0x29    // I2C Slave 1 Register
#define MPU6050_REG_I2C_SLV1_CTRL       0x2A    // I2C Slave 1 Control
#define MPU6050_REG_I2C_SLV2_ADDR       0x2B    // I2C Slave 2 Address
#define MPU6050_REG_I2C_SLV2_REG        0x2C    // I2C Slave 2 Register
#define MPU6050_REG_I2C_SLV2_CTRL       0x2D    // I2C Slave 2 Control
#define MPU6050_REG_I2C_SLV3_ADDR       0x2E    // I2C Slave 3 Address
#define MPU6050_REG_I2C_SLV3_REG        0x2F    // I2C Slave 3 Register
#define MPU6050_REG_I2C_SLV3_CTRL       0x30    // I2C Slave 3 Control
#define MPU6050_REG_I2C_SLV4_ADDR       0x31    // I2C Slave 4 Address
#define MPU6050_REG_I2C_SLV4_REG        0x32    // I2C Slave 4 Register
#define MPU6050_REG_I2C_SLV4_DO         0x33    // I2C Slave 4 Data Out
#define MPU6050_REG_I2C_SLV4_CTRL       0x34    // I2C Slave 4 Control
#define MPU6050_REG_I2C_SLV4_DI         0x35    // I2C Slave 4 Data In
#define MPU6050_REG_I2C_MST_STATUS      0x36    // I2C Master Status

// Interrupt Registers (0x37-0x3A)
#define MPU6050_REG_INT_PIN_CFG         0x37    // Interrupt Pin Configuration
#define MPU6050_REG_INT_ENABLE          0x38    // Interrupt Enable
#define MPU6050_REG_INT_STATUS          0x3A    // Interrupt Status

// Sensor Output Data (0x3B-0x48)
#define MPU6050_REG_ACCEL_XOUT_H        0x3B    // Accelerometer X High
#define MPU6050_REG_ACCEL_XOUT_L        0x3C    // Accelerometer X Low
#define MPU6050_REG_ACCEL_YOUT_H        0x3D    // Accelerometer Y High
#define MPU6050_REG_ACCEL_YOUT_L        0x3E    // Accelerometer Y Low
#define MPU6050_REG_ACCEL_ZOUT_H        0x3F    // Accelerometer Z High
#define MPU6050_REG_ACCEL_ZOUT_L        0x40    // Accelerometer Z Low
#define MPU6050_REG_TEMP_OUT_H          0x41    // Temperature High
#define MPU6050_REG_TEMP_OUT_L          0x42    // Temperature Low
#define MPU6050_REG_GYRO_XOUT_H         0x43    // Gyroscope X High
#define MPU6050_REG_GYRO_XOUT_L         0x44    // Gyroscope X Low
#define MPU6050_REG_GYRO_YOUT_H         0x45    // Gyroscope Y High
#define MPU6050_REG_GYRO_YOUT_L         0x46    // Gyroscope Y Low
#define MPU6050_REG_GYRO_ZOUT_H         0x47    // Gyroscope Z High
#define MPU6050_REG_GYRO_ZOUT_L         0x48    // Gyroscope Z Low

// External Sensor Data (0x49-0x60)
#define MPU6050_REG_EXT_SENS_DATA_00    0x49    // External Sensor Data 00
#define MPU6050_REG_EXT_SENS_DATA_01    0x4A    // External Sensor Data 01
#define MPU6050_REG_EXT_SENS_DATA_02    0x4B    // External Sensor Data 02
#define MPU6050_REG_EXT_SENS_DATA_03    0x4C    // External Sensor Data 03
#define MPU6050_REG_EXT_SENS_DATA_04    0x4D    // External Sensor Data 04
#define MPU6050_REG_EXT_SENS_DATA_05    0x4E    // External Sensor Data 05
#define MPU6050_REG_EXT_SENS_DATA_06    0x4F    // External Sensor Data 06
#define MPU6050_REG_EXT_SENS_DATA_07    0x50    // External Sensor Data 07
#define MPU6050_REG_EXT_SENS_DATA_08    0x51    // External Sensor Data 08
#define MPU6050_REG_EXT_SENS_DATA_09    0x52    // External Sensor Data 09
#define MPU6050_REG_EXT_SENS_DATA_10    0x53    // External Sensor Data 10
#define MPU6050_REG_EXT_SENS_DATA_11    0x54    // External Sensor Data 11
#define MPU6050_REG_EXT_SENS_DATA_12    0x55    // External Sensor Data 12
#define MPU6050_REG_EXT_SENS_DATA_13    0x56    // External Sensor Data 13
#define MPU6050_REG_EXT_SENS_DATA_14    0x57    // External Sensor Data 14
#define MPU6050_REG_EXT_SENS_DATA_15    0x58    // External Sensor Data 15
#define MPU6050_REG_EXT_SENS_DATA_16    0x59    // External Sensor Data 16
#define MPU6050_REG_EXT_SENS_DATA_17    0x5A    // External Sensor Data 17
#define MPU6050_REG_EXT_SENS_DATA_18    0x5B    // External Sensor Data 18
#define MPU6050_REG_EXT_SENS_DATA_19    0x5C    // External Sensor Data 19
#define MPU6050_REG_EXT_SENS_DATA_20    0x5D    // External Sensor Data 20
#define MPU6050_REG_EXT_SENS_DATA_21    0x5E    // External Sensor Data 21
#define MPU6050_REG_EXT_SENS_DATA_22    0x5F    // External Sensor Data 22
#define MPU6050_REG_EXT_SENS_DATA_23    0x60    // External Sensor Data 23

// Control Registers (0x6A-0x75)
#define MPU6050_REG_USER_CTRL           0x6A    // User Control
#define MPU6050_REG_PWR_MGMT_1          0x6B    // Power Management 1
#define MPU6050_REG_PWR_MGMT_2          0x6C    // Power Management 2
#define MPU6050_REG_FIFO_COUNTH         0x72    // FIFO Count High
#define MPU6050_REG_FIFO_COUNTL         0x73    // FIFO Count Low
#define MPU6050_REG_FIFO_R_W            0x74    // FIFO Read/Write
#define MPU6050_REG_WHO_AM_I            0x75    // Device ID

// ============================================================================
// BIT DEFINITIONS FOR REGISTERS
// ============================================================================

// CONFIG Register (0x1A) - DLPF Configuration
#define MPU6050_CONFIG_DLPF_CFG_MASK        0x07
#define MPU6050_CONFIG_DLPF_CFG_260HZ       0x00    // 260Hz (accel), 0ms delay
#define MPU6050_CONFIG_DLPF_CFG_184HZ       0x01    // 184Hz (accel), 2.0ms delay
#define MPU6050_CONFIG_DLPF_CFG_94HZ        0x02    // 94Hz (accel), 3.0ms delay
#define MPU6050_CONFIG_DLPF_CFG_44HZ        0x03    // 44Hz (accel), 4.9ms delay
#define MPU6050_CONFIG_DLPF_CFG_21HZ        0x04    // 21Hz (accel), 8.5ms delay
#define MPU6050_CONFIG_DLPF_CFG_10HZ        0x05    // 10Hz (accel), 13.8ms delay
#define MPU6050_CONFIG_DLPF_CFG_5HZ         0x06    // 5Hz (accel), 19.0ms delay
#define MPU6050_CONFIG_DLPF_CFG_RESERVED    0x07    // Reserved

// GYRO_CONFIG Register (0x1B) - Full Scale Selection
#define MPU6050_GYRO_CONFIG_FS_SEL_MASK     0x18
#define MPU6050_GYRO_CONFIG_FS_SEL_250      0x00    // ±250 °/s (131 LSB/°/s)
#define MPU6050_GYRO_CONFIG_FS_SEL_500      0x08    // ±500 °/s (65.5 LSB/°/s)
#define MPU6050_GYRO_CONFIG_FS_SEL_1000     0x10    // ±1000 °/s (32.8 LSB/°/s)
#define MPU6050_GYRO_CONFIG_FS_SEL_2000     0x18    // ±2000 °/s (16.4 LSB/°/s)
#define MPU6050_GYRO_CONFIG_XSTF_BIT        7       // X Self-Test Enable
#define MPU6050_GYRO_CONFIG_YSTF_BIT        6       // Y Self-Test Enable
#define MPU6050_GYRO_CONFIG_ZSTF_BIT        5       // Z Self-Test Enable

// ACCEL_CONFIG Register (0x1C) - Full Scale Selection
#define MPU6050_ACCEL_CONFIG_FS_SEL_MASK    0x18
#define MPU6050_ACCEL_CONFIG_FS_SEL_2G      0x00    // ±2g (16384 LSB/g)
#define MPU6050_ACCEL_CONFIG_FS_SEL_4G      0x08    // ±4g (8192 LSB/g)
#define MPU6050_ACCEL_CONFIG_FS_SEL_8G      0x10    // ±8g (4096 LSB/g)
#define MPU6050_ACCEL_CONFIG_FS_SEL_16G     0x18    // ±16g (2048 LSB/g)
#define MPU6050_ACCEL_CONFIG_XSTF_BIT       7       // X Self-Test Enable
#define MPU6050_ACCEL_CONFIG_YSTF_BIT       6       // Y Self-Test Enable
#define MPU6050_ACCEL_CONFIG_ZSTF_BIT       5       // Z Self-Test Enable

// INT_ENABLE Register (0x38) - Interrupt Control
#define MPU6050_INT_ENABLE_FIFO_OFLOW_BIT   4       // FIFO Overflow
#define MPU6050_INT_ENABLE_MOT_BIT          3       // Motion Detect
#define MPU6050_INT_ENABLE_ZMOT_BIT         2       // Zero Motion Detect
#define MPU6050_INT_ENABLE_FF_BIT           1       // Free Fall
#define MPU6050_INT_ENABLE_DATA_RDY_BIT     0       // Data Ready

#define MPU6050_INT_ENABLE_FIFO_OFLOW       (1 << 4)
#define MPU6050_INT_ENABLE_MOT              (1 << 3)
#define MPU6050_INT_ENABLE_ZMOT             (1 << 2)
#define MPU6050_INT_ENABLE_FF               (1 << 1)
#define MPU6050_INT_ENABLE_DATA_RDY         (1 << 0)

// INT_STATUS Register (0x3A) - Interrupt Status (Read Only)
#define MPU6050_INT_STATUS_FIFO_OFLOW_BIT   4       // FIFO Overflow Flag
#define MPU6050_INT_STATUS_MOT_BIT          3       // Motion Detect Flag
#define MPU6050_INT_STATUS_ZMOT_BIT         2       // Zero Motion Flag
#define MPU6050_INT_STATUS_FF_BIT           1       // Free Fall Flag
#define MPU6050_INT_STATUS_DATA_RDY_BIT     0       // Data Ready Flag

#define MPU6050_INT_STATUS_FIFO_OFLOW       (1 << 4)
#define MPU6050_INT_STATUS_MOT              (1 << 3)
#define MPU6050_INT_STATUS_ZMOT             (1 << 2)
#define MPU6050_INT_STATUS_FF               (1 << 1)
#define MPU6050_INT_STATUS_DATA_RDY         (1 << 0)

// INT_PIN_CFG Register (0x37) - Interrupt Pin Configuration
#define MPU6050_INT_PIN_CFG_LEVEL_BIT       7       // 0=active high, 1=active low
#define MPU6050_INT_PIN_CFG_OPEN_BIT        6       // 0=push-pull, 1=open-drain
#define MPU6050_INT_PIN_CFG_LATCH_BIT       5       // 0=pulse, 1=latched
#define MPU6050_INT_PIN_CFG_RD_CLEAR_BIT    4       // 0=status cleared on read
#define MPU6050_INT_PIN_CFG_FSYNC_LEVEL_BIT 3       // FSYNC pin interrupt level
#define MPU6050_INT_PIN_CFG_FSYNC_INT_BIT   2       // Enable FSYNC as interrupt
#define MPU6050_INT_PIN_CFG_I2C_BYPASS_BIT  1       // Enable I2C bypass
#define MPU6050_INT_PIN_CFG_CLKOUT_BIT      0       // Enable output clock

// USER_CTRL Register (0x6A)
#define MPU6050_USER_CTRL_FIFO_RST_BIT      7       // FIFO Reset
#define MPU6050_USER_CTRL_I2C_MST_EN_BIT    5       // I2C Master Enable
#define MPU6050_USER_CTRL_FIFO_EN_BIT       6       // FIFO Enable
#define MPU6050_USER_CTRL_DMP_EN_BIT        3       // DMP Enable
#define MPU6050_USER_CTRL_FIFO_OFLOW_BIT    2       // FIFO Overflow Status
#define MPU6050_USER_CTRL_I2C_EN_BIT        1       // I2C Interface Enable

#define MPU6050_USER_CTRL_FIFO_RST          (1 << 7)
#define MPU6050_USER_CTRL_I2C_MST_EN        (1 << 5)
#define MPU6050_USER_CTRL_FIFO_EN           (1 << 6)
#define MPU6050_USER_CTRL_DMP_EN            (1 << 3)
#define MPU6050_USER_CTRL_FIFO_OFLOW        (1 << 2)
#define MPU6050_USER_CTRL_I2C_EN            (1 << 1)

// PWR_MGMT_1 Register (0x6B)
#define MPU6050_PWR_MGMT_1_RST_BIT          7       // Device Reset
#define MPU6050_PWR_MGMT_1_SLEEP_BIT        6       // Sleep Mode
#define MPU6050_PWR_MGMT_1_CYCLE_BIT        5       // Cycle Mode
#define MPU6050_PWR_MGMT_1_TEMP_DIS_BIT     3       // Disable Temperature
#define MPU6050_PWR_MGMT_1_CLKSEL_MASK      0x07    // Clock Source Select

#define MPU6050_PWR_MGMT_1_CLKSEL_INT       0x00    // Internal 8MHz oscillator
#define MPU6050_PWR_MGMT_1_CLKSEL_XGYRO    0x01    // PLL with X gyro reference
#define MPU6050_PWR_MGMT_1_CLKSEL_YGYRO    0x02    // PLL with Y gyro reference
#define MPU6050_PWR_MGMT_1_CLKSEL_ZGYRO    0x03    // PLL with Z gyro reference
#define MPU6050_PWR_MGMT_1_CLKSEL_EXT32K   0x04    // PLL with external 32.768kHz
#define MPU6050_PWR_MGMT_1_CLKSEL_EXT19M   0x05    // PLL with external 19.2MHz

// PWR_MGMT_2 Register (0x6C)
#define MPU6050_PWR_MGMT_2_LP_WAKE_CTRL_MASK    0xC0    // Low Power Wake Control
#define MPU6050_PWR_MGMT_2_STBY_XG_BIT          5       // Standby X Gyro
#define MPU6050_PWR_MGMT_2_STBY_YG_BIT          4       // Standby Y Gyro
#define MPU6050_PWR_MGMT_2_STBY_ZG_BIT          3       // Standby Z Gyro
#define MPU6050_PWR_MGMT_2_STBY_XA_BIT          2       // Standby X Accel
#define MPU6050_PWR_MGMT_2_STBY_YA_BIT          1       // Standby Y Accel
#define MPU6050_PWR_MGMT_2_STBY_ZA_BIT          0       // Standby Z Accel

// FIFO_EN Register (0x23)
#define MPU6050_FIFO_EN_TEMP_BIT            7       // Temperature in FIFO
#define MPU6050_FIFO_EN_GYRO_X_BIT          6       // Gyro X in FIFO
#define MPU6050_FIFO_EN_GYRO_Y_BIT          5       // Gyro Y in FIFO
#define MPU6050_FIFO_EN_GYRO_Z_BIT          4       // Gyro Z in FIFO
#define MPU6050_FIFO_EN_ACCEL_BIT           3       // Accel in FIFO
#define MPU6050_FIFO_EN_SLV2_BIT            2       // Slave 2 in FIFO
#define MPU6050_FIFO_EN_SLV1_BIT            1       // Slave 1 in FIFO
#define MPU6050_FIFO_EN_SLV0_BIT            0       // Slave 0 in FIFO

#define MPU6050_FIFO_EN_TEMP                (1 << 7)
#define MPU6050_FIFO_EN_GYRO_X              (1 << 6)
#define MPU6050_FIFO_EN_GYRO_Y              (1 << 5)
#define MPU6050_FIFO_EN_GYRO_Z              (1 << 4)
#define MPU6050_FIFO_EN_ACCEL               (1 << 3)

// ============================================================================
// SENSOR SENSITIVITY AND RANGE CONSTANTS
// ============================================================================

// Accelerometer Sensitivity (LSB/g)
#define MPU6050_ACCEL_LSB_2G        16384.0f    // ±2g range
#define MPU6050_ACCEL_LSB_4G        8192.0f     // ±4g range
#define MPU6050_ACCEL_LSB_8G        4096.0f     // ±8g range
#define MPU6050_ACCEL_LSB_16G       2048.0f     // ±16g range

// Accelerometer Sensitivity (LSB/mg)
#define MPU6050_ACCEL_LSB_2G_MG     16.384f     // ±2g range
#define MPU6050_ACCEL_LSB_4G_MG     8.192f      // ±4g range
#define MPU6050_ACCEL_LSB_8G_MG     4.096f      // ±8g range
#define MPU6050_ACCEL_LSB_16G_MG    2.048f      // ±16g range

// Gyroscope Sensitivity (LSB/°/s)
#define MPU6050_GYRO_LSB_250        131.0f      // ±250°/s range
#define MPU6050_GYRO_LSB_500        65.5f       // ±500°/s range
#define MPU6050_GYRO_LSB_1000       32.8f       // ±1000°/s range
#define MPU6050_GYRO_LSB_2000       16.4f       // ±2000°/s range

// Temperature Sensor
#define MPU6050_TEMP_LSB            340.0f      // LSB/°C
#define MPU6050_TEMP_OFFSET         36.53f      // Reference temperature (°C)

// Gravity constant
#define MPU6050_GRAVITY             9.81f       // m/s²

// ============================================================================
// FIFO CONFIGURATION CONSTANTS
// ============================================================================

#define MPU6050_FIFO_SIZE           1024        // Maximum FIFO capacity in bytes
#define MPU6050_FIFO_SAMPLE_SIZE    14          // Bytes per sample (6+2+6)
#define MPU6050_MAX_FIFO_SAMPLES    (MPU6050_FIFO_SIZE / MPU6050_FIFO_SAMPLE_SIZE)

// ============================================================================
// SAMPLE RATE CALCULATIONS
// ============================================================================

// Output Sample Rate = Internal Sample Rate / (1 + SMPLRT_DIV)
// Internal Rate: 8kHz (DLPF=0) or 1kHz (DLPF=1-6)

// For DLPF_CFG = 0 (8kHz internal)
#define MPU6050_SMPLRT_DIV_8000HZ   0           // 8000 Hz
#define MPU6050_SMPLRT_DIV_4000HZ   1           // 4000 Hz
#define MPU6050_SMPLRT_DIV_2000HZ   3           // 2000 Hz
#define MPU6050_SMPLRT_DIV_1000HZ   7           // 1000 Hz
#define MPU6050_SMPLRT_DIV_500HZ    15          // 500 Hz
#define MPU6050_SMPLRT_DIV_250HZ    31          // 250 Hz
#define MPU6050_SMPLRT_DIV_125HZ    63          // 125 Hz
#define MPU6050_SMPLRT_DIV_100HZ    79          // 100 Hz
#define MPU6050_SMPLRT_DIV_50HZ     159         // 50 Hz

// For DLPF_CFG = 1-6 (1kHz internal)
#define MPU6050_SMPLRT_DIV_1000HZ_DLPF  0       // 1000 Hz
#define MPU6050_SMPLRT_DIV_500HZ_DLPF   1       // 500 Hz
#define MPU6050_SMPLRT_DIV_250HZ_DLPF   3       // 250 Hz
#define MPU6050_SMPLRT_DIV_200HZ_DLPF   4       // 200 Hz
#define MPU6050_SMPLRT_DIV_100HZ_DLPF   9       // 100 Hz
#define MPU6050_SMPLRT_DIV_50HZ_DLPF    19      // 50 Hz
#define MPU6050_SMPLRT_DIV_25HZ_DLPF    39      // 25 Hz
#define MPU6050_SMPLRT_DIV_10HZ_DLPF    99      // 10 Hz
#define MPU6050_SMPLRT_DIV_5HZ_DLPF     199     // 5 Hz

// ============================================================================
// REGISTER LIMITS AND VALIDATION
// ============================================================================

#define MPU6050_REG_MIN             0x00
#define MPU6050_REG_MAX             0x75
#define MPU6050_I2C_TIMEOUT_MS      1000        // I2C transaction timeout
#define MPU6050_STARTUP_TIME_MS     100         // Startup delay after power-on

// ============================================================================
// UTILITY MACROS
// ============================================================================

// Assemble 16-bit signed value from MSB and LSB
#define MPU6050_ASSEMBLE_INT16(msb, lsb) \
    ((int16_t)(((uint16_t)(msb) << 8) | (uint16_t)(lsb)))

// Extract bit field from register value
#define MPU6050_GET_BITS(value, mask, shift) \
    (((value) & (mask)) >> (shift))

// Set bit field in register value
#define MPU6050_SET_BITS(value, mask, shift, bits) \
    (((value) & ~(mask)) | (((bits) << (shift)) & (mask)))

// Check if bit is set
#define MPU6050_BIT_IS_SET(value, bit) \
    (((value) >> (bit)) & 1)

// ============================================================================
// TEMPERATURE CONVERSION HELPERS
// ============================================================================

/**
 * Convert raw temperature value to Celsius
 * Temperature (°C) = (Raw / 340) + 36.53
 */
static inline float mpu6050_temp_celsius(int16_t raw_temp) {
    return (raw_temp / 340.0f) + 36.53f;
}

/**
 * Convert raw temperature value to Fahrenheit
 * Temperature (°F) = (Raw / 340) * 9/5 + 97.54
 */
static inline float mpu6050_temp_fahrenheit(int16_t raw_temp) {
    return mpu6050_temp_celsius(raw_temp) * 9.0f / 5.0f + 32.0f;
}

// ============================================================================
// SENSOR DATA CONVERSION HELPERS
// ============================================================================

/**
 * Convert raw accelerometer value to g (depends on range setting)
 * Range: ±2g -> divisor = 16384, ±4g -> 8192, ±8g -> 4096, ±16g -> 2048
 */
static inline float mpu6050_accel_g(int16_t raw_accel, float lsb_per_g) {
    return raw_accel / lsb_per_g;
}

/**
 * Convert raw accelerometer value to m/s²
 */
static inline float mpu6050_accel_ms2(int16_t raw_accel, float lsb_per_g) {
    return mpu6050_accel_g(raw_accel, lsb_per_g) * 9.81f;
}

/**
 * Convert raw gyroscope value to °/s (depends on range setting)
 * Range: ±250°/s -> divisor = 131, ±500°/s -> 65.5, ±1000°/s -> 32.8, ±2000°/s -> 16.4
 */
static inline float mpu6050_gyro_dps(int16_t raw_gyro, float lsb_per_dps) {
    return raw_gyro / lsb_per_dps;
}

/**
 * Convert raw gyroscope value to rad/s
 */
static inline float mpu6050_gyro_rads(int16_t raw_gyro, float lsb_per_dps) {
    return mpu6050_gyro_dps(raw_gyro, lsb_per_dps) * 3.14159265f / 180.0f;
}

#endif  // MPU6050_REGISTERS_H
