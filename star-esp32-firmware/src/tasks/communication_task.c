/* src/tasks/communication_task.c - Communication task implementation */

#include "communication_task.h"

#include "esp_log.h"

#include <string.h>

#include "star_bus_spi_peripheral.h"

#include "system_config.h"

extern const char* const s_TAG;

/* Protocol definitions */
static const uint8_t s_cmd_set_motor_speed    = 0x01;
static const uint8_t s_cmd_stop_motor         = 0x02;
static const uint8_t s_cmd_emergency_stop     = 0x03;
static const uint8_t s_cmd_request_telemetry  = 0x10;

static const uint8_t s_resp_status_ok         = 0x00;

/* Packet sizes */
static const size_t s_cmd_packet_size  = 5;  /* CMD + MOTOR_ID + SETPOINT_MSB + SETPOINT_LSB + CRC8 */
static const size_t s_resp_packet_size = 17; /* STATUS + 4*RPM(2) + 4*CURRENT(2) */

/* CRC polynomial */
static const uint8_t s_crc8_poly = 0x07;
static const uint8_t s_crc8_init = 0xFF;

/* Packet offsets */
static const size_t s_crc_offset          = 4;
static const size_t s_cmd_type_offset     = 0;
static const size_t s_motor_id_offset     = 1;
static const size_t s_setpoint_msb_offset = 2;
static const size_t s_setpoint_lsb_offset = 3;

static const size_t s_resp_status_offset  = 0;
static const size_t s_resp_rpm_offset     = 1;
static const size_t s_resp_current_offset = 9;

/* Data encoding */
static const size_t s_bytes_per_value = 2;
static const uint8_t s_bits_per_byte  = 8;
static const uint8_t s_byte_mask      = 0xFF;

/**
 * @brief Calculate CRC-8 checksum
 *
 * @param data Data buffer
 * @param len Length of data
 * @return uint8_t CRC-8 checksum
 */
static uint8_t internal_crc8(const uint8_t* data, size_t len)
{
    uint8_t crc = s_crc8_init;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ s_crc8_poly;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void communication_task(void* pvParameters)
{
    system_context_t* ctx = (system_context_t*)pvParameters;

    uint8_t cmd_buffer[s_cmd_packet_size];
    uint8_t resp_buffer[s_resp_packet_size];

    ESP_LOGI(s_TAG, "Communication task started");

    while (ctx->system_enabled) {
        /* Wait for command from RPi5 */
        esp_err_t ret = star_bus_spi_peripheral_receive(&ctx->bus_manager,
                                                          "rpi_spi",
                                                          cmd_buffer,
                                                          s_cmd_packet_size,
                                                          s_comm_period_ms);

        if (ret == ESP_OK) {
            /* Verify CRC */
            uint8_t calculated_crc = internal_crc8(cmd_buffer, s_cmd_packet_size - 1);
            uint8_t received_crc   = cmd_buffer[s_crc_offset];

            if (calculated_crc != received_crc) {
                ESP_LOGW(s_TAG,
                         "CRC mismatch: expected 0x%02X, got 0x%02X",
                         calculated_crc,
                         received_crc);
                continue;
            }

            /* Parse command */
            uint8_t cmd_type  = cmd_buffer[s_cmd_type_offset];
            uint8_t motor_id  = cmd_buffer[s_motor_id_offset];
            int16_t setpoint  = (cmd_buffer[s_setpoint_msb_offset] << s_bits_per_byte)
                              | cmd_buffer[s_setpoint_lsb_offset];

            /* Process command */
            switch (cmd_type) {
                case s_cmd_set_motor_speed:
                    if (motor_id < NUM_MOTORS) {
                        if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                            ctx->setpoint_rpm[motor_id] = (float)setpoint;
                            xSemaphoreGive(ctx->state_mutex);
                            ESP_LOGI(s_TAG,
                                     "Motor %d setpoint: %.1f RPM",
                                     motor_id + 1,
                                     (float)setpoint);
                        }
                    } else {
                        ESP_LOGW(s_TAG, "Invalid motor ID: %d", motor_id);
                    }
                    break;

                case s_cmd_stop_motor:
                    if (motor_id < NUM_MOTORS) {
                        star_motor_stop(&ctx->motors[motor_id], true);
                        if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                            ctx->setpoint_rpm[motor_id] = 0.0f;
                            xSemaphoreGive(ctx->state_mutex);
                        }
                        ESP_LOGI(s_TAG, "Motor %d stopped", motor_id + 1);
                    }
                    break;

                case s_cmd_emergency_stop:
                    ESP_LOGW(s_TAG, "EMERGENCY STOP received from RPi5!");
                    for (int i = 0; i < NUM_MOTORS; i++) {
                        star_motor_stop(&ctx->motors[i], true);
                    }
                    if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
                        for (int i = 0; i < NUM_MOTORS; i++) {
                            ctx->setpoint_rpm[i] = 0.0f;
                        }
                        xSemaphoreGive(ctx->state_mutex);
                    }
                    ctx->system_enabled = false;
                    break;

                case s_cmd_request_telemetry:
                    /* Telemetry will be sent below */
                    break;

                default:
                    ESP_LOGW(s_TAG, "Unknown command: 0x%02X", cmd_type);
                    break;
            }
        } else if (ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(s_TAG, "SPI receive error: %s", esp_err_to_name(ret));
        }

        /* Prepare telemetry response packet */
        resp_buffer[s_resp_status_offset] = s_resp_status_ok;

        if (xSemaphoreTake(ctx->state_mutex, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < NUM_MOTORS; i++) {
                int16_t rpm        = (int16_t)ctx->current_rpm[i];
                int16_t current_ma = (int16_t)ctx->motor_current_ma[i];

                /* RPM data */
                resp_buffer[s_resp_rpm_offset + i * s_bytes_per_value]
                    = (rpm >> s_bits_per_byte) & s_byte_mask;
                resp_buffer[s_resp_rpm_offset + i * s_bytes_per_value + 1] = rpm & s_byte_mask;

                /* Current data */
                resp_buffer[s_resp_current_offset + i * s_bytes_per_value]
                    = (current_ma >> s_bits_per_byte) & s_byte_mask;
                resp_buffer[s_resp_current_offset + i * s_bytes_per_value + 1]
                    = current_ma & s_byte_mask;
            }
            xSemaphoreGive(ctx->state_mutex);
        } else {
            ESP_LOGW(s_TAG, "Failed to take mutex for telemetry response");
        }

        /* Send telemetry to RPi5 */
        ret = star_bus_spi_peripheral_transmit(&ctx->bus_manager,
                                                "rpi_spi",
                                                resp_buffer,
                                                s_resp_packet_size,
                                                s_comm_period_ms);

        if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
            ESP_LOGE(s_TAG, "SPI transmit error: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(s_comm_period_ms));
    }

    ESP_LOGW(s_TAG, "Communication task exiting");
    vTaskDelete(NULL);
}
