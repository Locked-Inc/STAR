/**
 * @file rplidar_c1_driver.c
 * @brief RPLiDAR C1 DTOF Scanner Driver Implementation
 *
 * Core driver implementation with memory safety, error handling,
 * and secure packet parsing for ESP32-IDF.
 *
 * @author Generated for ESP32-IDF
 * @date 2025-11-20
 * @version 2.0
 */

#include "rplidar_c1_driver.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "RPLiDAR_C1";

/* ==================== Utility Functions ==================== */

/**
 * @brief Calculate XOR checksum
 */
uint8_t rplidar_calculate_checksum(const uint8_t* data, size_t length) {
    if (!data || length == 0) return 0;

    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief Convert PWM to percentage
 */
float rplidar_pwm_to_percent(uint16_t pwm) {
    if (pwm > RPLIDAR_PWM_MAX) pwm = RPLIDAR_PWM_MAX;
    return ((float)pwm / (float)RPLIDAR_PWM_MAX) * 100.0f;
}

/**
 * @brief Convert percentage to PWM
 */
uint16_t rplidar_percent_to_pwm(float percent) {
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    return (uint16_t)((percent / 100.0f) * (float)RPLIDAR_PWM_MAX);
}

/**
 * @brief Get error string
 */
const char* rplidar_error_string(rplidar_status_t error) {
    switch (error) {
        case RPLIDAR_OK:
            return "Success";
        case RPLIDAR_ERR_TIMEOUT:
            return "UART timeout";
        case RPLIDAR_ERR_CHECKSUM:
            return "Checksum mismatch";
        case RPLIDAR_ERR_INVALID_PKT:
            return "Invalid packet";
        case RPLIDAR_ERR_UART:
            return "UART error";
        case RPLIDAR_ERR_ALLOC:
            return "Memory allocation failed";
        case RPLIDAR_ERR_SIZE_EXCEEDED:
            return "Packet size exceeded";
        case RPLIDAR_ERR_INVALID_DESCRIPTOR:
            return "Invalid descriptor";
        case RPLIDAR_ERR_INCOMPLETE:
            return "Incomplete packet";
        case RPLIDAR_ERR_NOT_INITIALIZED:
            return "Driver not initialized";
        case RPLIDAR_ERR_ALREADY_RUNNING:
            return "Already running";
        default:
            return "Unknown error";
    }
}

/**
 * @brief Get health error code string
 */
const char* rplidar_error_code_string(uint16_t error_code) {
    if (error_code == 0x0000) return "No error";
    if (error_code & 0x0001) return "Hardware failure";
    if (error_code & 0x0002) return "JTAG malfunction";
    if (error_code & 0x0004) return "FPGA error";
    if (error_code & 0x0008) return "Power supply low";
    if (error_code & 0x0010) return "Motor circuit failure";
    if (error_code & 0x0020) return "Motor overspeed";
    if (error_code & 0x0040) return "Temperature sensor error";
    if (error_code & 0x0080) return "Temperature high";
    if (error_code & 0x0100) return "Optical signal loss";
    if (error_code & 0x0200) return "Light intensity low";
    return "Unknown error code";
}

/* ==================== Measurement Parsing ==================== */

/**
 * @brief Validate measurement data integrity
 */
bool rplidar_validate_measurement(const uint8_t* buf) {
    if (!buf) return false;

    // Check sync bits (S != S_inv)
    bool s = (buf[0] & 0x01);
    bool s_inv = (buf[0] & 0x02) >> 1;

    if (s == s_inv) {
        ESP_LOGV(TAG, "Sync bit mismatch: S=%d, S_inv=%d", s, s_inv);
        return false;
    }

    // Check C bit (must equal 1)
    uint8_t check_bit = (buf[1] & 0x80) >> 7;
    if (check_bit != 1) {
        ESP_LOGV(TAG, "Check bit not set: %d", check_bit);
        return false;
    }

    return true;
}

/**
 * @brief Parse measurement from raw 5-byte buffer
 */
bool rplidar_parse_measurement(const uint8_t* buf,
                               rplidar_measurement_t* measurement) {
    if (!buf || !measurement) return false;

    // Validate before parsing
    if (!rplidar_validate_measurement(buf)) {
        measurement->valid = false;
        return false;
    }

    // Extract and validate quality (6 bits)
    uint8_t quality = buf[0] >> 2;
    if (quality > 63) {
        ESP_LOGV(TAG, "Quality exceeds max: %u", quality);
        measurement->valid = false;
        return false;
    }

    // Extract angle (15 bits, MSB in byte 1, LSB in byte 2)
    uint16_t angle_raw = ((buf[1] & 0x7F) << 8) | buf[2];
    float angle_deg = angle_raw / 64.0f;

    // Validate angle range (0-360 degrees)
    if (angle_deg > 360.0f) {
        ESP_LOGV(TAG, "Angle out of range: %.2f", angle_deg);
        measurement->valid = false;
        return false;
    }

    // Extract distance (16 bits, little-endian)
    uint16_t distance_raw = (buf[4] << 8) | buf[3];

    // Validate physical range (0.05m - 12m)
    // Raw units are /4mm, so valid range is 200 to 48000
    if (distance_raw < 200 || distance_raw > 48000) {
        // Out of range typically means no return - mark as valid but far
        ESP_LOGV(TAG, "Distance out of range: %u raw", distance_raw);
    }

    // Populate measurement structure
    measurement->distance_mm = distance_raw / 4;
    measurement->angle_deg = angle_deg;
    measurement->quality = quality;
    measurement->is_new_scan = (buf[0] & 0x01);
    measurement->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
    measurement->valid = true;

    return true;
}

/* ==================== Packet Construction ==================== */

/**
 * @brief Build and send command packet (no payload)
 */
static rplidar_status_t send_simple_command(uart_port_t uart_num,
                                           uint8_t command) {
    uint8_t packet[4] = {
        RPLIDAR_SYNC_BYTE,
        command,
        0x00,  // No payload
        0x00   // Checksum placeholder
    };

    // Calculate checksum
    packet[3] = rplidar_calculate_checksum(packet, 3);

    // Send packet
    int bytes_sent = uart_write_bytes(uart_num, (const char*)packet, 4);
    if (bytes_sent != 4) {
        ESP_LOGE(TAG, "Failed to send command 0x%02X (sent %d bytes)",
                 command, bytes_sent);
        return RPLIDAR_ERR_UART;
    }

    ESP_LOGV(TAG, "Sent command 0x%02X", command);
    return RPLIDAR_OK;
}

/**
 * @brief Build and send command packet with payload
 */
static rplidar_status_t send_command_with_payload(uart_port_t uart_num,
                                                  uint8_t command,
                                                  const uint8_t* payload,
                                                  uint8_t payload_size) {
    if (!payload && payload_size > 0) {
        return RPLIDAR_ERR_INVALID_PKT;
    }

    // Maximum packet: 1 sync + 1 cmd + 1 size + 256 payload + 1 checksum
    uint8_t packet[260];
    if (payload_size > 255) {
        ESP_LOGE(TAG, "Payload too large: %u", payload_size);
        return RPLIDAR_ERR_SIZE_EXCEEDED;
    }

    packet[0] = RPLIDAR_SYNC_BYTE;
    packet[1] = command;
    packet[2] = payload_size;

    if (payload_size > 0) {
        memcpy(&packet[3], payload, payload_size);
    }

    // Calculate checksum over all bytes except checksum itself
    packet[3 + payload_size] = rplidar_calculate_checksum(packet, 3 + payload_size);

    // Send packet
    int bytes_sent = uart_write_bytes(uart_num, (const char*)packet,
                                     4 + payload_size);
    if (bytes_sent != (4 + payload_size)) {
        ESP_LOGE(TAG, "Failed to send command 0x%02X (sent %d bytes)",
                 command, bytes_sent);
        return RPLIDAR_ERR_UART;
    }

    ESP_LOGV(TAG, "Sent command 0x%02X with %u byte payload",
             command, payload_size);
    return RPLIDAR_OK;
}

/* ==================== UART Communication ==================== */

/**
 * @brief Read packet descriptor (7 bytes)
 */
static rplidar_status_t read_descriptor(uart_port_t uart_num,
                                       rplidar_descriptor_t* descriptor) {
    if (!descriptor) return RPLIDAR_ERR_INVALID_PKT;

    uint8_t buffer[7];
    int bytes_read = uart_read_bytes(uart_num, buffer, 7,
                                     RPLIDAR_UART_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (bytes_read != 7) {
        ESP_LOGD(TAG, "Descriptor read timeout (got %d bytes)", bytes_read);
        return RPLIDAR_ERR_TIMEOUT;
    }

    // Validate descriptor header
    if (buffer[0] != RPLIDAR_SYNC_BYTE || buffer[1] != RPLIDAR_DESC_BYTE) {
        ESP_LOGD(TAG, "Invalid descriptor header: 0x%02X 0x%02X",
                 buffer[0], buffer[1]);
        return RPLIDAR_ERR_INVALID_DESCRIPTOR;
    }

    // Validate checksum
    uint8_t expected_checksum = rplidar_calculate_checksum(buffer, 6);
    if (expected_checksum != buffer[6]) {
        ESP_LOGD(TAG, "Descriptor checksum mismatch: 0x%02X != 0x%02X",
                 buffer[6], expected_checksum);
        return RPLIDAR_ERR_CHECKSUM;
    }

    descriptor->sync_byte = buffer[0];
    descriptor->desc_byte = buffer[1];
    descriptor->data_length = buffer[2] | (buffer[3] << 8);
    descriptor->data_type = buffer[4];
    descriptor->single_response = buffer[5];
    descriptor->checksum = buffer[6];

    return RPLIDAR_OK;
}

/**
 * @brief Read response data with size validation
 */
static rplidar_status_t read_response_data(uart_port_t uart_num,
                                          uint16_t data_length,
                                          uint8_t* buffer,
                                          uint16_t buffer_size) {
    if (!buffer || data_length > buffer_size) {
        return RPLIDAR_ERR_SIZE_EXCEEDED;
    }

    int bytes_read = uart_read_bytes(uart_num, buffer, data_length,
                                     RPLIDAR_UART_TIMEOUT_MS / portTICK_PERIOD_MS);

    if (bytes_read != data_length) {
        ESP_LOGD(TAG, "Incomplete data read: %d / %u bytes",
                 bytes_read, data_length);
        return RPLIDAR_ERR_INCOMPLETE;
    }

    return RPLIDAR_OK;
}

/* ==================== Circular Frame Buffer ==================== */

/**
 * @brief Create circular frame buffer
 */
rplidar_circular_buffer_t* rplidar_create_frame_buffer(uint32_t capacity) {
    if (capacity == 0 || capacity > 1000) {
        ESP_LOGE(TAG, "Invalid buffer capacity: %u", capacity);
        return NULL;
    }

    rplidar_circular_buffer_t* buf = malloc(sizeof(rplidar_circular_buffer_t));
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer structure");
        return NULL;
    }

    buf->frames = malloc(capacity * sizeof(rplidar_frame_t));
    if (!buf->frames) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer (%u frames)",
                 capacity);
        free(buf);
        return NULL;
    }

    buf->capacity = capacity;
    buf->write_idx = 0;
    buf->read_idx = 0;
    buf->spinlock = portMUX_INITIALIZER_UNLOCKED;
    buf->total_frames = 0;
    buf->dropped_frames = 0;
    buf->overflow_count = 0;

    ESP_LOGI(TAG, "Created circular buffer for %u frames (%zu bytes total)",
             capacity, capacity * sizeof(rplidar_frame_t));

    return buf;
}

/**
 * @brief Write frame to circular buffer
 */
bool rplidar_write_frame(rplidar_circular_buffer_t* buffer,
                        const rplidar_frame_t* frame) {
    if (!buffer || !frame) return false;

    portENTER_CRITICAL(&buffer->spinlock);

    uint32_t next_write_idx = (buffer->write_idx + 1) % buffer->capacity;

    // Check for buffer full
    if (next_write_idx == buffer->read_idx) {
        buffer->overflow_count++;
        ESP_LOGW(TAG, "Frame buffer overflow at write_idx=%u",
                 buffer->write_idx);

        // Advance read pointer to make room
        buffer->read_idx = (buffer->read_idx + 1) % buffer->capacity;
        buffer->dropped_frames++;

        portEXIT_CRITICAL(&buffer->spinlock);
        return false;
    }

    // Copy frame data
    memcpy(&buffer->frames[buffer->write_idx], frame, sizeof(rplidar_frame_t));
    buffer->write_idx = next_write_idx;
    buffer->total_frames++;

    portEXIT_CRITICAL(&buffer->spinlock);
    return true;
}

/**
 * @brief Read frame from circular buffer
 */
bool rplidar_read_frame_buffer(rplidar_circular_buffer_t* buffer,
                              rplidar_frame_t* frame) {
    if (!buffer || !frame) return false;

    portENTER_CRITICAL(&buffer->spinlock);

    if (buffer->read_idx == buffer->write_idx) {
        // Buffer empty
        portEXIT_CRITICAL(&buffer->spinlock);
        return false;
    }

    // Copy frame
    memcpy(frame, &buffer->frames[buffer->read_idx], sizeof(rplidar_frame_t));
    buffer->read_idx = (buffer->read_idx + 1) % buffer->capacity;

    portEXIT_CRITICAL(&buffer->spinlock);
    return true;
}

/**
 * @brief Destroy circular buffer
 */
void rplidar_destroy_frame_buffer(rplidar_circular_buffer_t* buffer) {
    if (!buffer) return;
    free(buffer->frames);
    free(buffer);
}

/* ==================== Driver State Management ==================== */

/**
 * @brief Get current driver state
 */
rplidar_state_t rplidar_get_state(rplidar_driver_t* driver) {
    if (!driver) return RPLIDAR_STATE_ERROR;

    portENTER_CRITICAL(&driver->state_spinlock);
    rplidar_state_t state = driver->state;
    portEXIT_CRITICAL(&driver->state_spinlock);

    return state;
}

/**
 * @brief Check if scanning is active
 */
bool rplidar_is_scanning(rplidar_driver_t* driver) {
    rplidar_state_t state = rplidar_get_state(driver);
    return state == RPLIDAR_STATE_RUNNING ||
           state == RPLIDAR_STATE_STARTING;
}

/**
 * @brief Get buffer status
 */
void rplidar_get_buffer_status(rplidar_driver_t* driver,
                              uint32_t* available_frames,
                              float* capacity_percent) {
    if (!driver || !driver->frame_buffer) {
        if (available_frames) *available_frames = 0;
        if (capacity_percent) *capacity_percent = 0.0f;
        return;
    }

    portENTER_CRITICAL(&driver->frame_buffer->spinlock);

    uint32_t write_idx = driver->frame_buffer->write_idx;
    uint32_t read_idx = driver->frame_buffer->read_idx;
    uint32_t capacity = driver->frame_buffer->capacity;

    uint32_t avail;
    if (write_idx >= read_idx) {
        avail = write_idx - read_idx;
    } else {
        avail = capacity - (read_idx - write_idx);
    }

    if (available_frames) *available_frames = avail;
    if (capacity_percent) *capacity_percent = ((float)avail / capacity) * 100.0f;

    portEXIT_CRITICAL(&driver->frame_buffer->spinlock);
}

/**
 * @brief Get driver statistics
 */
void rplidar_get_stats(rplidar_driver_t* driver, rplidar_stats_t* stats) {
    if (!driver || !stats) return;

    memset(stats, 0, sizeof(rplidar_stats_t));

    stats->measurements_received = driver->measurements_received;
    stats->frames_completed = driver->frames_completed;

    if (driver->frame_buffer) {
        portENTER_CRITICAL(&driver->frame_buffer->spinlock);
        stats->frames_dropped = driver->frame_buffer->dropped_frames;
        portEXIT_CRITICAL(&driver->frame_buffer->spinlock);
    }

    uint64_t elapsed_us = esp_timer_get_time() - driver->start_time_us;
    if (elapsed_us > 0) {
        stats->avg_measurement_rate = (driver->measurements_received * 1000000.0f) /
                                     elapsed_us;
        stats->frame_rate = (driver->frames_completed * 1000000.0f) / elapsed_us;
    }
}

/* ==================== Core API Functions ==================== */

/**
 * @brief Initialize RPLiDAR driver
 */
rplidar_status_t rplidar_init(const rplidar_config_t* config,
                              rplidar_driver_t* driver) {
    if (!config || !driver) {
        return RPLIDAR_ERR_INVALID_PKT;
    }

    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_REF_TICK  // More stable than APB
    };

    if (uart_param_config(config->uart_num, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters");
        return RPLIDAR_ERR_UART;
    }

    if (uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins");
        return RPLIDAR_ERR_UART;
    }

    if (uart_driver_install(config->uart_num,
                           config->rx_buffer_size,  // RX buffer
                           2048,                     // TX buffer
                           20,                       // Queue size
                           &driver->uart_queue,
                           0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver");
        return RPLIDAR_ERR_UART;
    }

    // Initialize driver structure
    driver->uart_num = config->uart_num;
    driver->state = RPLIDAR_STATE_IDLE;
    driver->current_pwm = 0;
    driver->measurements_received = 0;
    driver->frames_completed = 0;
    driver->state_spinlock = portMUX_INITIALIZER_UNLOCKED;
    driver->start_time_us = esp_timer_get_time();

    // Setup callbacks
    driver->data_callback = config->data_callback;
    driver->frame_complete_callback = config->frame_complete_callback;
    driver->error_callback = config->error_callback;

    // Create circular buffer
    driver->frame_buffer = rplidar_create_frame_buffer(config->frame_buffer_size);
    if (!driver->frame_buffer) {
        ESP_LOGW(TAG, "Failed to create frame buffer (non-critical)");
    }

    ESP_LOGI(TAG, "RPLiDAR driver initialized successfully");
    return RPLIDAR_OK;
}

/**
 * @brief Start continuous scanning
 */
rplidar_status_t rplidar_start_scan(rplidar_driver_t* driver) {
    if (!driver) return RPLIDAR_ERR_NOT_INITIALIZED;

    rplidar_state_t current_state = rplidar_get_state(driver);
    if (current_state != RPLIDAR_STATE_IDLE) {
        ESP_LOGE(TAG, "Cannot start scan in state %d", current_state);
        return RPLIDAR_ERR_ALREADY_RUNNING;
    }

    // Send SCAN command
    rplidar_status_t status = send_simple_command(driver->uart_num,
                                                  RPLIDAR_CMD_SCAN);
    if (status != RPLIDAR_OK) {
        return status;
    }

    // Update state
    portENTER_CRITICAL(&driver->state_spinlock);
    driver->state = RPLIDAR_STATE_RUNNING;
    portEXIT_CRITICAL(&driver->state_spinlock);

    ESP_LOGI(TAG, "Standard scan started");
    return RPLIDAR_OK;
}

/**
 * @brief Stop scanning
 */
rplidar_status_t rplidar_stop_scan(rplidar_driver_t* driver) {
    if (!driver) return RPLIDAR_ERR_NOT_INITIALIZED;

    // Update state
    portENTER_CRITICAL(&driver->state_spinlock);
    driver->state = RPLIDAR_STATE_STOPPING;
    portEXIT_CRITICAL(&driver->state_spinlock);

    // Send STOP command
    rplidar_status_t status = send_simple_command(driver->uart_num,
                                                  RPLIDAR_CMD_STOP);

    // Wait for motor to stop
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Update state
    portENTER_CRITICAL(&driver->state_spinlock);
    driver->state = RPLIDAR_STATE_IDLE;
    portEXIT_CRITICAL(&driver->state_spinlock);

    ESP_LOGI(TAG, "Scan stopped");
    return status;
}

/**
 * @brief Set motor PWM speed
 */
rplidar_status_t rplidar_set_motor_pwm(rplidar_driver_t* driver, uint16_t pwm) {
    if (!driver) return RPLIDAR_ERR_NOT_INITIALIZED;

    // Clamp to valid range
    if (pwm > RPLIDAR_PWM_MAX) pwm = RPLIDAR_PWM_MAX;

    // Build payload (little-endian)
    uint8_t payload[2] = {
        (uint8_t)(pwm & 0xFF),
        (uint8_t)((pwm >> 8) & 0xFF)
    };

    rplidar_status_t status = send_command_with_payload(
        driver->uart_num,
        RPLIDAR_CMD_SET_PWM,
        payload,
        2
    );

    if (status == RPLIDAR_OK) {
        driver->current_pwm = pwm;
        ESP_LOGD(TAG, "Motor PWM set to %u (%.1f%%)",
                 pwm, rplidar_pwm_to_percent(pwm));
    }

    return status;
}

/**
 * @brief Soft-start motor
 */
rplidar_status_t rplidar_soft_start(rplidar_driver_t* driver,
                                   uint16_t target_pwm,
                                   uint16_t step_size,
                                   uint32_t step_delay_ms) {
    if (!driver) return RPLIDAR_ERR_NOT_INITIALIZED;

    if (target_pwm > RPLIDAR_PWM_MAX) target_pwm = RPLIDAR_PWM_MAX;
    if (step_size == 0) step_size = 50;
    if (step_delay_ms == 0) step_delay_ms = 100;

    uint16_t current_pwm = driver->current_pwm;

    if (current_pwm < target_pwm) {
        // Accelerate
        while (current_pwm < target_pwm) {
            current_pwm += step_size;
            if (current_pwm > target_pwm) current_pwm = target_pwm;

            rplidar_status_t status = rplidar_set_motor_pwm(driver, current_pwm);
            if (status != RPLIDAR_OK) {
                return status;
            }

            vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
        }
    }

    return RPLIDAR_OK;
}

/**
 * @brief Soft-stop motor
 */
rplidar_status_t rplidar_soft_stop(rplidar_driver_t* driver,
                                  uint16_t step_size,
                                  uint32_t step_delay_ms) {
    if (!driver) return RPLIDAR_ERR_NOT_INITIALIZED;

    if (step_size == 0) step_size = 50;
    if (step_delay_ms == 0) step_delay_ms = 100;

    uint16_t current_pwm = driver->current_pwm;

    // Decelerate
    while (current_pwm > 0) {
        if (current_pwm > step_size) {
            current_pwm -= step_size;
        } else {
            current_pwm = 0;
        }

        rplidar_status_t status = rplidar_set_motor_pwm(driver, current_pwm);
        if (status != RPLIDAR_OK) {
            return status;
        }

        vTaskDelay(step_delay_ms / portTICK_PERIOD_MS);
    }

    return RPLIDAR_OK;
}

/**
 * @brief Get device health status
 */
rplidar_status_t rplidar_get_health(rplidar_driver_t* driver,
                                   rplidar_health_t* health) {
    if (!driver || !health) return RPLIDAR_ERR_INVALID_PKT;

    // Send GET_HEALTH command
    rplidar_status_t status = send_simple_command(driver->uart_num,
                                                  RPLIDAR_CMD_GET_HEALTH);
    if (status != RPLIDAR_OK) {
        return status;
    }

    // Read response descriptor
    rplidar_descriptor_t descriptor;
    status = read_descriptor(driver->uart_num, &descriptor);
    if (status != RPLIDAR_OK) {
        return status;
    }

    // Validate health response size (must be 3 bytes)
    if (descriptor.data_length != 3) {
        ESP_LOGE(TAG, "Invalid health response size: %u", descriptor.data_length);
        return RPLIDAR_ERR_INVALID_PKT;
    }

    // Read health data
    uint8_t health_data[3];
    status = read_response_data(driver->uart_num, 3, health_data, 3);
    if (status != RPLIDAR_OK) {
        return status;
    }

    // Parse health data
    health->status = (rplidar_health_status_t)health_data[0];
    health->error_code = health_data[1] | (health_data[2] << 8);

    ESP_LOGD(TAG, "Device health: Status=%u, Error=0x%04X",
             health->status, health->error_code);

    return RPLIDAR_OK;
}

/**
 * @brief Get device information
 */
rplidar_status_t rplidar_get_info(rplidar_driver_t* driver,
                                 rplidar_device_info_t* info) {
    if (!driver || !info) return RPLIDAR_ERR_INVALID_PKT;

    // Send GET_INFO command
    rplidar_status_t status = send_simple_command(driver->uart_num,
                                                  RPLIDAR_CMD_GET_INFO);
    if (status != RPLIDAR_OK) {
        return status;
    }

    // Read response descriptor
    rplidar_descriptor_t descriptor;
    status = read_descriptor(driver->uart_num, &descriptor);
    if (status != RPLIDAR_OK) {
        return status;
    }

    // Validate info response size (must be 20 bytes)
    if (descriptor.data_length != 20) {
        ESP_LOGE(TAG, "Invalid info response size: %u", descriptor.data_length);
        return RPLIDAR_ERR_INVALID_PKT;
    }

    // Read info data
    uint8_t info_data[20];
    status = read_response_data(driver->uart_num, 20, info_data, 20);
    if (status != RPLIDAR_OK) {
        return status;
    }

    // Parse info data
    info->model = info_data[0];
    info->firmware_major = info_data[1];
    info->firmware_minor = info_data[2];
    info->hardware_version = info_data[3];
    memcpy(info->serial, &info_data[4], 16);

    ESP_LOGI(TAG, "Device info: Model=0x%02X, FW=%u.%u, HW=%u",
             info->model, info->firmware_major, info->firmware_minor,
             info->hardware_version);

    return RPLIDAR_OK;
}

/**
 * @brief Reset device
 */
rplidar_status_t rplidar_reset(rplidar_driver_t* driver) {
    if (!driver) return RPLIDAR_ERR_NOT_INITIALIZED;

    rplidar_status_t status = send_simple_command(driver->uart_num,
                                                  RPLIDAR_CMD_RESET);
    if (status == RPLIDAR_OK) {
        // Wait for device to restart
        vTaskDelay(300 / portTICK_PERIOD_MS);

        // Reset driver state
        portENTER_CRITICAL(&driver->state_spinlock);
        driver->state = RPLIDAR_STATE_IDLE;
        driver->current_pwm = 0;
        portEXIT_CRITICAL(&driver->state_spinlock);

        ESP_LOGI(TAG, "Device reset complete");
    }

    return status;
}

/**
 * @brief Clear UART buffers and resynchronize
 */
rplidar_status_t rplidar_clear_buffers(rplidar_driver_t* driver) {
    if (!driver) return RPLIDAR_ERR_NOT_INITIALIZED;

    uart_flush_input(driver->uart_num);
    ESP_LOGD(TAG, "UART buffers cleared");
    return RPLIDAR_OK;
}

/**
 * @brief Deinitialize driver
 */
void rplidar_deinit(rplidar_driver_t* driver) {
    if (!driver) return;

    // Stop scanning
    if (rplidar_is_scanning(driver)) {
        rplidar_stop_scan(driver);
    }

    // Destroy frame buffer
    if (driver->frame_buffer) {
        rplidar_destroy_frame_buffer(driver->frame_buffer);
        driver->frame_buffer = NULL;
    }

    // Uninstall UART driver
    uart_driver_delete(driver->uart_num);

    ESP_LOGI(TAG, "Driver deinitialized");
}

#endif // RPLIDAR_C1_DRIVER_C
