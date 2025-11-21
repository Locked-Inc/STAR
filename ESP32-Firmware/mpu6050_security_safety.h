/**
 * MPU6050 Security and Memory Safety Guidelines
 *
 * Critical practices for production-grade MPU6050 drivers on ESP32-IDF
 * Addresses: FIFO overflow, data validation, I2C reliability, memory safety
 */

#ifndef MPU6050_SECURITY_SAFETY_H
#define MPU6050_SECURITY_SAFETY_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ============================================================================
// FIFO OVERFLOW PROTECTION
// ============================================================================

/**
 * CRITICAL ISSUE: FIFO Overflow Protection
 *
 * PROBLEM:
 * - FIFO capacity: 1024 bytes
 * - Data packet: 14 bytes per sample
 * - Max samples: 1024 / 14 = 73 samples
 * - If FIFO count exceeds buffer capacity, reading beyond FIFO causes corruption
 * - Integer overflow possible if FIFO count stored as uint8_t
 *
 * MITIGATION:
 * 1. Always read FIFO count as uint16_t (not uint8_t)
 * 2. Validate FIFO count < 1024 before reading
 * 3. Calculate number of samples = FIFO_count / 14
 * 4. Verify number of samples <= allocated buffer size
 * 5. Check FIFO overflow interrupt and handle gracefully
 */

#define MPU6050_FIFO_MAX_BYTES              1024
#define MPU6050_FIFO_SAMPLE_BYTES           14
#define MPU6050_FIFO_MAX_SAFE_SAMPLES       70  // Leave margin for safety

/**
 * FIFO validation structure
 * Use this to safely manage FIFO reads
 */
typedef struct {
    uint16_t byte_count;
    uint16_t sample_count;
    bool overflow_detected;
    uint64_t overflow_count;  // Cumulative overflow events
} mpu6050_fifo_status_t;

/**
 * Safe FIFO count validation
 *
 * CRITICAL: Must use uint16_t to prevent integer overflow
 *
 * @param fifo_h FIFO count high byte
 * @param fifo_l FIFO count low byte
 * @param status Output status structure
 * @return true if count is valid, false if invalid/overflow
 */
static inline bool mpu6050_validate_fifo_count(uint8_t fifo_h, uint8_t fifo_l,
                                                mpu6050_fifo_status_t *status) {
    // CRITICAL: Use uint16_t to handle full 12-bit FIFO count
    uint16_t byte_count = ((uint16_t)fifo_h << 8) | (uint16_t)fifo_l;

    // Validation checks
    if (byte_count > MPU6050_FIFO_MAX_BYTES) {
        status->overflow_detected = true;
        status->overflow_count++;
        return false;
    }

    // Check alignment to sample size
    if (byte_count % MPU6050_FIFO_SAMPLE_BYTES != 0) {
        return false;
    }

    status->byte_count = byte_count;
    status->sample_count = byte_count / MPU6050_FIFO_SAMPLE_BYTES;
    status->overflow_detected = false;

    return true;
}

/**
 * Get safe read count for FIFO
 * Prevents reading beyond allocated buffer
 *
 * @param fifo_status FIFO status (from validation)
 * @param max_buffer_samples Maximum samples application can accept
 * @return Safe number of samples to read, or 0 if error
 */
static inline uint16_t mpu6050_get_safe_read_count(
    const mpu6050_fifo_status_t *fifo_status,
    uint16_t max_buffer_samples) {

    if (fifo_status == NULL) {
        return 0;
    }

    // Return minimum of FIFO samples and buffer capacity
    return (fifo_status->sample_count < max_buffer_samples) ?
        fifo_status->sample_count : max_buffer_samples;
}

// ============================================================================
// SIGNED vs UNSIGNED DATA HANDLING
// ============================================================================

/**
 * CRITICAL ISSUE: Signed/Unsigned Data Conversion
 *
 * PROBLEM:
 * - MPU6050 returns signed 16-bit values (range: -32768 to +32767)
 * - Incorrect assembly creates unsigned interpretation (0 to 65535)
 * - Results in inverted signs and wrong measurement ranges
 * - Example: -100 becomes 65436
 *
 * INCORRECT CODE:
 *   uint16_t value = (buf[0] << 8) | buf[1];  // Interprets as unsigned
 *   float result = value / 16384.0;            // Results in 0-4g, not -2 to +2g
 *
 * CORRECT CODE:
 *   int16_t value = (int16_t)((buf[0] << 8) | buf[1]);  // Cast to signed
 *   float result = value / 16384.0;                      // Results in -2 to +2g
 */

/**
 * Safe 16-bit signed assembly from MSB/LSB pair
 * Explicitly handles sign extension
 *
 * @param msb Most significant byte
 * @param lsb Least significant byte
 * @return Signed 16-bit value
 */
static inline int16_t mpu6050_assemble_signed(uint8_t msb, uint8_t lsb) {
    // Combine as unsigned first
    uint16_t combined = ((uint16_t)msb << 8) | (uint16_t)lsb;

    // Reinterpret as signed 16-bit (2's complement)
    return (int16_t)combined;
}

/**
 * Safe data parsing with explicit type casting
 * Prevents accidental unsigned interpretation
 */
typedef struct {
    int16_t accel[3];   // Explicitly signed
    int16_t temp;
    int16_t gyro[3];
} mpu6050_raw_data_t;

/**
 * Parse 14-byte FIFO/register buffer safely
 *
 * Ensures all values are interpreted as signed 16-bit
 * Prevents sign inversion errors
 *
 * @param buffer 14-byte buffer from I2C read
 * @param data Output structure
 * @return true if parsing successful
 */
static inline bool mpu6050_parse_raw_data(const uint8_t *buffer,
                                          mpu6050_raw_data_t *data) {
    if (buffer == NULL || data == NULL) {
        return false;
    }

    // Explicitly cast each 16-bit pair to int16_t
    // This ensures 2's complement interpretation
    data->accel[0] = mpu6050_assemble_signed(buffer[0], buffer[1]);
    data->accel[1] = mpu6050_assemble_signed(buffer[2], buffer[3]);
    data->accel[2] = mpu6050_assemble_signed(buffer[4], buffer[5]);
    data->temp     = mpu6050_assemble_signed(buffer[6], buffer[7]);
    data->gyro[0]  = mpu6050_assemble_signed(buffer[8], buffer[9]);
    data->gyro[1]  = mpu6050_assemble_signed(buffer[10], buffer[11]);
    data->gyro[2]  = mpu6050_assemble_signed(buffer[12], buffer[13]);

    return true;
}

// ============================================================================
// INVALID RANGE DETECTION AND VALIDATION
// ============================================================================

/**
 * CRITICAL ISSUE: Invalid Data Detection
 *
 * SYMPTOMS of hardware failure or sensor malfunction:
 * - All values are 0x7FFF (32767) - indicates saturation or stuck bit
 * - All values are 0x0000 (0) - indicates communication failure
 * - Temperature outside physical range (-40°C to +85°C)
 * - Gyro values all exactly the same (stuck)
 * - Accel values missing gravity (sensor failure)
 *
 * MITIGATION:
 * 1. Validate each reading against physical limits
 * 2. Log and count invalid reads for diagnostics
 * 3. Implement fallback or error recovery strategy
 */

/**
 * Temperature range validation
 * Valid range: -40°C to +85°C
 * At ±2g: raw value at -40°C ~= -26000, at +85°C ~= 16500
 */
#define MPU6050_TEMP_RAW_MIN    -26000  // -40°C
#define MPU6050_TEMP_RAW_MAX    16500   // +85°C

/**
 * Sensor data validation structure
 */
typedef struct {
    bool valid;
    bool accel_range_error;
    bool gyro_range_error;
    bool temp_range_error;
    bool all_zero;
    bool all_max;
    uint32_t error_count;
} mpu6050_validation_result_t;

/**
 * Comprehensive sensor data validation
 * Detects hardware failures and communication errors
 *
 * @param data Raw sensor data to validate
 * @param result Output validation result
 * @return true if all data valid, false if errors detected
 */
static inline bool mpu6050_validate_data(const mpu6050_raw_data_t *data,
                                         mpu6050_validation_result_t *result) {
    if (data == NULL || result == NULL) {
        return false;
    }

    memset(result, 0, sizeof(*result));
    result->valid = true;

    // Check for all zeros (communication failure)
    if (data->accel[0] == 0 && data->accel[1] == 0 && data->accel[2] == 0 &&
        data->gyro[0] == 0 && data->gyro[1] == 0 && data->gyro[2] == 0 &&
        data->temp == 0) {
        result->all_zero = true;
        result->valid = false;
        result->error_count++;
        return false;
    }

    // Check for all max values (saturation/stuck bits)
    if (data->accel[0] == 32767 && data->accel[1] == 32767 &&
        data->accel[2] == 32767 && data->temp == 32767 &&
        data->gyro[0] == 32767 && data->gyro[1] == 32767 &&
        data->gyro[2] == 32767) {
        result->all_max = true;
        result->valid = false;
        result->error_count++;
        return false;
    }

    // Temperature should be in valid range
    if (data->temp < MPU6050_TEMP_RAW_MIN || data->temp > MPU6050_TEMP_RAW_MAX) {
        result->temp_range_error = true;
        result->valid = false;
        result->error_count++;
        // Note: Could still continue processing other data, or fail completely
        return false;
    }

    // Accel range check (±16g max with 16g range = ±32767)
    // Conservative: reject values > ±28000 (safety margin)
    if (data->accel[0] > 28000 || data->accel[0] < -28000 ||
        data->accel[1] > 28000 || data->accel[1] < -28000 ||
        data->accel[2] > 28000 || data->accel[2] < -28000) {
        result->accel_range_error = true;
        result->valid = false;
        result->error_count++;
    }

    // Gyro range check (±2000°/s max = ±32767)
    // Conservative: reject values > ±30000
    if (data->gyro[0] > 30000 || data->gyro[0] < -30000 ||
        data->gyro[1] > 30000 || data->gyro[1] < -30000 ||
        data->gyro[2] > 30000 || data->gyro[2] < -30000) {
        result->gyro_range_error = true;
        result->valid = false;
        result->error_count++;
    }

    return result->valid;
}

// ============================================================================
// I2C CLOCK STRETCHING AND TIMING SAFETY
// ============================================================================

/**
 * CRITICAL ISSUE: I2C Bus Hanging and Clock Stretching
 *
 * PROBLEM:
 * - MPU6050 may hold SCL low during internal processing (clock stretching)
 * - If master timeout too short, I2C bus can hang
 * - Master must wait for slave to release SCL
 * - Soft reset may not recover; physical power cycle required
 *
 * SYMPTOMS:
 * - I2C timeout on every read/write
 * - Master NACK received
 * - I2C bus freeze requiring power cycle
 *
 * MITIGATION:
 * 1. Set I2C timeout to at least 200ms (MPU6050 may need 5-10ms)
 * 2. Lower I2C clock speed to 100-400 kHz
 * 3. Implement I2C bus recovery on timeout
 * 4. Add stop bit forcing on I2C errors
 */

#define MPU6050_I2C_MIN_TIMEOUT_MS      200     // Minimum timeout for clock stretching
#define MPU6050_I2C_TYPICAL_TIMEOUT_MS  1000    // Typical safe timeout
#define MPU6050_I2C_CLOCK_SPEED_HZ      400000  // 400 kHz (safe, widely compatible)

/**
 * I2C reliability metrics
 */
typedef struct {
    uint32_t successful_reads;
    uint32_t failed_reads;
    uint32_t timeout_errors;
    uint32_t nack_errors;
    uint32_t bus_recovery_count;
} mpu6050_i2c_stats_t;

/**
 * Detect potential I2C bus hang
 * Should be called periodically to identify chronic issues
 *
 * @param stats I2C statistics
 * @return true if bus appears to be hanging frequently
 */
static inline bool mpu6050_i2c_bus_hanging(const mpu6050_i2c_stats_t *stats) {
    if (stats == NULL || stats->successful_reads == 0) {
        return false;
    }

    uint32_t total_errors = stats->timeout_errors + stats->nack_errors;
    float error_rate = (float)total_errors / (float)(stats->successful_reads + total_errors);

    // Flag if more than 5% of transactions failing
    return error_rate > 0.05f;
}

// ============================================================================
// INTERRUPT RACE CONDITION PREVENTION
// ============================================================================

/**
 * CRITICAL ISSUE: Interrupt Race Conditions
 *
 * PROBLEM:
 * - Data output registers are updated continuously
 * - Reading while registers are being updated can cause "tearing"
 * - Example: read accel[0] high byte, then sensor updates all bytes,
 *            then read accel[0] low byte - results in mixed old/new data
 *
 * SOLUTION:
 * 1. Read all 14 bytes atomically in single burst
 * 2. Use data ready interrupt to trigger reads
 * 3. Disable interrupts during critical reads (if applicable)
 * 4. Verify data consistency across multiple reads
 *
 * BEST PRACTICE: Always use burst read (I2C auto-increment) to read
 * all 14 bytes in single continuous I2C transaction
 */

/**
 * Data consistency check structure
 */
typedef struct {
    mpu6050_raw_data_t sample1;
    mpu6050_raw_data_t sample2;
    bool consistent;
    int16_t max_variance;
} mpu6050_consistency_check_t;

/**
 * Verify data consistency by reading twice
 * If values differ significantly, indicates interrupt race condition
 *
 * @param sample1 First read
 * @param sample2 Second read
 * @param max_allowed_variance Maximum acceptable difference per axis (in LSBs)
 * @return true if data is consistent
 */
static inline bool mpu6050_check_data_consistency(
    const mpu6050_raw_data_t *sample1,
    const mpu6050_raw_data_t *sample2,
    int16_t max_allowed_variance) {

    if (sample1 == NULL || sample2 == NULL) {
        return false;
    }

    // Check each axis for excessive variance
    for (int i = 0; i < 3; i++) {
        int16_t accel_delta = sample1->accel[i] - sample2->accel[i];
        int16_t gyro_delta = sample1->gyro[i] - sample2->gyro[i];

        if (accel_delta < -max_allowed_variance || accel_delta > max_allowed_variance ||
            gyro_delta < -max_allowed_variance || gyro_delta > max_allowed_variance) {
            return false;
        }
    }

    int16_t temp_delta = sample1->temp - sample2->temp;
    return (temp_delta >= -max_allowed_variance && temp_delta <= max_allowed_variance);
}

// ============================================================================
// REGISTER ACCESS BOUNDS CHECKING
// ============================================================================

/**
 * CRITICAL ISSUE: Register Address Validation
 *
 * PROBLEM:
 * - Invalid register addresses can cause I2C errors or read garbage
 * - Some register addresses are reserved/undefined
 * - Read-only registers should never be written
 * - Writing to reserved addresses may corrupt sensor state
 *
 * MITIGATION:
 * 1. Validate all register addresses before I2C access
 * 2. Maintain separate read/write access lists
 * 3. Check address range for burst reads
 */

#define MPU6050_REG_MIN         0x00
#define MPU6050_REG_MAX         0x75

/**
 * Register access type enumeration
 */
typedef enum {
    MPU6050_REG_RW,     // Read/Write
    MPU6050_REG_RO,     // Read-Only
    MPU6050_REG_WO,     // Write-Only
    MPU6050_REG_RESERVED
} mpu6050_reg_access_type_t;

/**
 * Validate single register address
 *
 * @param reg Register address
 * @param access_type Required access type (R, W, RW)
 * @return true if address valid for access type
 */
static inline bool mpu6050_validate_register_address(uint8_t reg,
                                                      const char *access_type) {
    // Basic bounds check
    if (reg < MPU6050_REG_MIN || reg > MPU6050_REG_MAX) {
        return false;
    }

    // Reserved address ranges (specific to MPU6050)
    if ((reg >= 0x11 && reg <= 0x12) ||  // Reserved
        (reg >= 0x31 && reg <= 0x48 && reg != 0x3A && reg != 0x3B &&
         reg != 0x3C && reg != 0x3D && reg != 0x3E && reg != 0x3F &&
         reg != 0x40 && reg != 0x41 && reg != 0x42 && reg != 0x43 &&
         reg != 0x44 && reg != 0x45 && reg != 0x46 && reg != 0x47 &&
         reg != 0x48)) {  // Some gaps in address space
        return false;
    }

    return true;
}

/**
 * Validate burst read address range
 *
 * @param start_reg Starting register address
 * @param count Number of bytes to read
 * @return true if entire range is valid
 */
static inline bool mpu6050_validate_burst_read(uint8_t start_reg, uint16_t count) {
    if (count == 0 || count > 128) {
        return false;
    }

    if (start_reg < MPU6050_REG_MIN) {
        return false;
    }

    if ((start_reg + count - 1) > MPU6050_REG_MAX) {
        return false;
    }

    return true;
}

// ============================================================================
// BUFFER OVERFLOW PREVENTION
// ============================================================================

/**
 * CRITICAL ISSUE: Stack Buffer Overflows
 *
 * PROBLEM:
 * - Large local arrays can overflow stack on embedded systems
 * - ESP32 stack size is limited (~8KB per task)
 * - FIFO read buffer of 1024 bytes on stack is dangerous
 *
 * WRONG:
 *   void read_fifo() {
 *       uint8_t buffer[1024];  // Dangerous stack allocation
 *   }
 *
 * CORRECT:
 *   static uint8_t fifo_buffer[1024];  // Static allocation
 *   void read_fifo() {
 *       use(fifo_buffer);
 *   }
 *
 * BEST:
 *   typedef struct {
 *       uint8_t fifo[1024];
 *       uint8_t sample[14];
 *   } mpu6050_buffers_t;
 *
 *   mpu6050_buffers_t* ctx;  // Allocate on heap once
 */

/**
 * Pre-allocated buffer context
 * Allocate once during initialization, reuse for reads
 */
typedef struct {
    uint8_t fifo_read_buffer[MPU6050_FIFO_MAX_BYTES];
    uint8_t single_sample_buffer[14];
    uint8_t register_cache[128];
    mpu6050_fifo_status_t fifo_status;
    mpu6050_i2c_stats_t i2c_stats;
    mpu6050_validation_result_t validation_result;
} mpu6050_driver_buffers_t;

/**
 * Initialize driver buffers (should be called once at startup)
 *
 * @param buffers Pre-allocated buffer structure
 * @return true if initialization successful
 */
static inline bool mpu6050_init_buffers(mpu6050_driver_buffers_t *buffers) {
    if (buffers == NULL) {
        return false;
    }

    memset(buffers, 0, sizeof(*buffers));
    return true;
}

// ============================================================================
// DIAGNOSTIC AND MONITORING
// ============================================================================

/**
 * Complete driver health status
 */
typedef struct {
    bool i2c_bus_healthy;
    bool fifo_stable;
    bool data_consistent;
    bool sensor_responding;
    uint32_t consecutive_good_reads;
    uint32_t consecutive_bad_reads;
    uint64_t total_read_count;
    mpu6050_i2c_stats_t i2c_stats;
    mpu6050_fifo_status_t last_fifo_status;
} mpu6050_health_status_t;

/**
 * Determine overall driver health
 *
 * @param health Health status structure
 * @return Overall health percentage (0-100)
 */
static inline uint8_t mpu6050_get_health_percentage(
    const mpu6050_health_status_t *health) {
    if (health == NULL || health->total_read_count == 0) {
        return 50;
    }

    uint8_t score = 100;

    // Deduct for failed reads
    uint32_t total_errors = health->i2c_stats.failed_reads +
                           health->i2c_stats.timeout_errors;
    if (total_errors > 0) {
        float error_rate = (float)total_errors / (float)health->total_read_count;
        score -= (uint8_t)(error_rate * 50);
    }

    // Deduct if FIFO has overflowed
    if (health->last_fifo_status.overflow_detected) {
        score -= 20;
    }

    // Deduct if I2C bus unhealthy
    if (!health->i2c_bus_healthy) {
        score -= 30;
    }

    return score > 0 ? score : 0;
}

#endif  // MPU6050_SECURITY_SAFETY_H
