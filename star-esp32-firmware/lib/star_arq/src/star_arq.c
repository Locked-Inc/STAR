/**
 * @file star_arq.c
 * @brief Automatic Repeat Request (ARQ) protocol implementation with Stop-and-Wait
 * @details
 * Implements ARQ protocol layer with Stop-and-Wait strategy for reliable packet delivery
 * over SPI peripheral interface. Provides automatic retransmission, acknowledgment handling,
 * and configurable timeout/retry parameters for robust communication with RPi5 controller.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_arq.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "star_bus_spi_peripheral.h"

/* --- Constants --- */

static const char* s_tag = "STAR_ARQ";

/* --- Private Function Prototypes --- */

static esp_err_t internal_send_ack(star_arq_handle_t* handle, uint16_t seq);
static esp_err_t internal_send_nack(star_arq_handle_t* handle, uint16_t seq);
static esp_err_t internal_wait_for_ack(
    star_arq_handle_t* handle,
    uint16_t expected_seq,
    uint32_t timeout_ms,
    bool* is_ack);

/* --- Public Functions --- */

esp_err_t star_arq_init(star_arq_handle_t* handle, const star_arq_config_t* config) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config is NULL");
    ESP_RETURN_ON_FALSE(config->bus_manager, ESP_ERR_INVALID_ARG, s_tag, "Bus manager is NULL");
    ESP_RETURN_ON_FALSE(config->spi_bus_name, ESP_ERR_INVALID_ARG, s_tag, "SPI bus name is NULL");
    ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Already initialized");

    /* Zero out handle */
    memset(handle, 0, sizeof(star_arq_handle_t));

    /* Store configuration */
    handle->bus_manager = config->bus_manager;
    handle->spi_bus_name = config->spi_bus_name;
    handle->max_retries = (config->max_retries > 0) ? config->max_retries : k_star_arq_default_max_retries;
    handle->retry_timeout_ms = (config->retry_timeout_ms > 0) ? config->retry_timeout_ms : k_star_arq_default_timeout_ms;

    /* Initialize frame handles */
    esp_err_t ret = star_frame_init(&handle->tx_frame);
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to init TX frame");

    ret = star_frame_init(&handle->rx_frame);
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to init RX frame");

    /* Initialize sequence numbers */
    handle->tx_seq = 0;
    handle->rx_last_seq = 0;
    handle->rx_initialized = false;

    handle->initialized = true;

    ESP_LOGI(s_tag,
             "ARQ initialized: bus=%s, max_retries=%u, timeout=%lums",
             config->spi_bus_name,
             handle->max_retries,
             handle->retry_timeout_ms);

    return ESP_OK;
}

esp_err_t star_arq_deinit(star_arq_handle_t* handle) {
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
    ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

    /* Clear handle */
    memset(handle, 0, sizeof(star_arq_handle_t));

    ESP_LOGI(s_tag, "ARQ deinitialized");
    return ESP_OK;
}

esp_err_t star_arq_send(
    star_arq_handle_t* handle,
    const uint8_t* data,
    size_t len,
    uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle && data, ESP_ERR_INVALID_ARG, s_tag, "NULL pointer argument");
    ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");
    ESP_RETURN_ON_FALSE(len <= k_star_arq_max_data_size,
                        ESP_ERR_INVALID_ARG,
                        s_tag,
                        "Data too large: %zu > %d",
                        len,
                        k_star_arq_max_data_size);

    /* Build request frame with current sequence number */
    uint8_t* frame_data;
    size_t frame_len;

    esp_err_t ret = star_frame_build(
        &handle->tx_frame,
        handle->tx_seq,
        k_star_frame_type_request,
        data,
        len,
        &frame_data,
        &frame_len
    );
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to build request frame");

    /* Store in retry buffer */
    memcpy(handle->retry_buffer, frame_data, frame_len);
    handle->retry_buffer_len = frame_len;

    /* Stop-and-Wait: Send and retry until ACK received */
    for (handle->retry_count = 0; handle->retry_count <= handle->max_retries; handle->retry_count++) {
        /* Send frame */
        ret = star_bus_spi_peripheral_transmit(
            handle->bus_manager,
            handle->spi_bus_name,
            handle->retry_buffer,
            handle->retry_buffer_len,
            1000 /* 1 second SPI timeout */
        );

        if (ret != ESP_OK) {
            ESP_LOGW(s_tag, "SPI transmit failed (attempt %u/%u): %s",
                     handle->retry_count + 1,
                     handle->max_retries + 1,
                     esp_err_to_name(ret));
            continue;
        }

        ESP_LOGD(s_tag, "Sent request: seq=%u, len=%zu (attempt %u/%u)",
                 handle->tx_seq,
                 len,
                 handle->retry_count + 1,
                 handle->max_retries + 1);

        /* Wait for ACK/NACK */
        bool is_ack;
        ret = internal_wait_for_ack(handle, handle->tx_seq, handle->retry_timeout_ms, &is_ack);

        if (ret == ESP_OK && is_ack) {
            /* ACK received - success! */
            handle->tx_seq++;
            ESP_LOGD(s_tag, "ACK received, incremented seq to %u", handle->tx_seq);
            return ESP_OK;
        } else if (ret == ESP_OK && !is_ack) {
            /* NACK received - retry immediately */
            ESP_LOGW(s_tag, "NACK received for seq=%u, retrying...", handle->tx_seq);
            continue;
        } else if (ret == ESP_ERR_TIMEOUT) {
            /* Timeout - retry */
            ESP_LOGW(s_tag, "Timeout waiting for ACK (seq=%u), retrying...", handle->tx_seq);
            continue;
        } else {
            /* Other error - retry */
            ESP_LOGW(s_tag, "Error waiting for ACK: %s, retrying...", esp_err_to_name(ret));
            continue;
        }
    }

    /* Max retries exceeded */
    ESP_LOGE(s_tag, "Max retries exceeded for seq=%u", handle->tx_seq);
    return ESP_ERR_TIMEOUT;
}

esp_err_t star_arq_receive(
    star_arq_handle_t* handle,
    uint8_t* data,
    size_t max_len,
    size_t* out_len,
    uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(handle && data && out_len,
                        ESP_ERR_INVALID_ARG,
                        s_tag,
                        "NULL pointer argument");
    ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

    while (true) {
        /* Receive frame via SPI */
        uint8_t rx_buffer[k_star_frame_max_size];

        esp_err_t ret = star_bus_spi_peripheral_receive(
            handle->bus_manager,
            handle->spi_bus_name,
            rx_buffer,
            sizeof(rx_buffer),
            timeout_ms
        );
        ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to receive frame");

        /* Parse frame */
        uint16_t seq;
        star_frame_type_t type;
        uint8_t* payload;
        uint16_t payload_len;

        ret = star_frame_parse(
            &handle->rx_frame,
            rx_buffer,
            sizeof(rx_buffer),
            &seq,
            &type,
            &payload,
            &payload_len
        );

        if (ret != ESP_OK) {
            /* CRC error or malformed frame - send NACK */
            ESP_LOGW(s_tag, "Invalid frame received, sending NACK");
            internal_send_nack(handle, seq);
            continue;
        }

        /* Check frame type */
        if (type != k_star_frame_type_request && type != k_star_frame_type_response) {
            /* Not a data frame (probably ACK/NACK for our own transmissions) */
            ESP_LOGD(s_tag, "Received non-data frame (type=%d), ignoring", type);
            continue;
        }

        /* Initialize RX sequence tracking on first receive */
        if (!handle->rx_initialized) {
            handle->rx_last_seq = seq - 1;
            handle->rx_initialized = true;
        }

        /* Check sequence number */
        const uint16_t expected_seq = handle->rx_last_seq + 1;

        if (seq == expected_seq) {
            /* Expected sequence - new data */
            ESP_RETURN_ON_FALSE(payload_len <= max_len,
                                ESP_ERR_INVALID_SIZE,
                                s_tag,
                                "Received data too large: %u > %zu",
                                payload_len,
                                max_len);

            /* Copy payload */
            memcpy(data, payload, payload_len);
            *out_len = payload_len;

            /* Update RX sequence */
            handle->rx_last_seq = seq;

            /* Send ACK */
            internal_send_ack(handle, seq);

            ESP_LOGD(s_tag, "Received new data: seq=%u, len=%u", seq, payload_len);
            return ESP_OK;

        } else if (seq == handle->rx_last_seq) {
            /* Duplicate packet (our ACK was lost) */
            ESP_LOGD(s_tag, "Duplicate packet: seq=%u, re-sending ACK", seq);
            internal_send_ack(handle, seq);
            /* Continue waiting for next packet */
            continue;

        } else {
            /* Out-of-order packet */
            ESP_LOGW(s_tag, "Out-of-order packet: got seq=%u, expected=%u", seq, expected_seq);
            internal_send_nack(handle, seq);
            /* Continue waiting for correct packet */
            continue;
        }
    }

    /* Unreachable */
    return ESP_FAIL;
}

/* --- Private Functions --- */

/**
 * @brief Send ACK frame for a given sequence number
 *
 * Builds and transmits an ACK frame to acknowledge successful receipt.
 *
 * @param[in,out] handle  Pointer to ARQ handle
 * @param[in]     seq     Sequence number to acknowledge
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL on SPI transmission error
 */
static esp_err_t internal_send_ack(star_arq_handle_t* handle, uint16_t seq) {
    /* Build ACK frame (no payload) */
    uint8_t* frame_data;
    size_t frame_len;

    esp_err_t ret = star_frame_build(
        &handle->tx_frame,
        seq,
        k_star_frame_type_ack,
        NULL,
        0,
        &frame_data,
        &frame_len
    );
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to build ACK frame");

    /* Send via SPI */
    ret = star_bus_spi_peripheral_transmit(
        handle->bus_manager,
        handle->spi_bus_name,
        frame_data,
        frame_len,
        1000 /* 1 second timeout */
    );
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to transmit ACK");

    ESP_LOGD(s_tag, "Sent ACK: seq=%u", seq);
    return ESP_OK;
}

/**
 * @brief Send NACK frame for a given sequence number
 *
 * Builds and transmits a NACK frame to request retransmission.
 *
 * @param[in,out] handle  Pointer to ARQ handle
 * @param[in]     seq     Sequence number to NACK
 *
 * @return
 *     - ESP_OK on success
 *     - ESP_FAIL on SPI transmission error
 */
static esp_err_t internal_send_nack(star_arq_handle_t* handle, uint16_t seq) {
    /* Build NACK frame (no payload) */
    uint8_t* frame_data;
    size_t frame_len;

    esp_err_t ret = star_frame_build(
        &handle->tx_frame,
        seq,
        k_star_frame_type_nack,
        NULL,
        0,
        &frame_data,
        &frame_len
    );
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to build NACK frame");

    /* Send via SPI */
    ret = star_bus_spi_peripheral_transmit(
        handle->bus_manager,
        handle->spi_bus_name,
        frame_data,
        frame_len,
        1000 /* 1 second timeout */
    );
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to transmit NACK");

    ESP_LOGD(s_tag, "Sent NACK: seq=%u", seq);
    return ESP_OK;
}

/**
 * @brief Wait for ACK/NACK response with timeout
 *
 * Receives a frame and checks if it's an ACK or NACK for the expected sequence.
 *
 * @param[in,out] handle      Pointer to ARQ handle
 * @param[in]     expected_seq Expected sequence number
 * @param[in]     timeout_ms  Timeout in milliseconds
 * @param[out]    is_ack      Pointer to store ACK status (true=ACK, false=NACK)
 *
 * @return
 *     - ESP_OK if ACK or NACK received
 *     - ESP_ERR_TIMEOUT if timeout occurs
 *     - ESP_FAIL on other errors
 */
static esp_err_t internal_wait_for_ack(
    star_arq_handle_t* handle,
    uint16_t expected_seq,
    uint32_t timeout_ms,
    bool* is_ack)
{
    uint8_t rx_buffer[k_star_frame_max_size];
    size_t rx_len;

    /* Receive response frame */
    esp_err_t ret = star_bus_spi_peripheral_receive(
        handle->bus_manager,
        handle->spi_bus_name,
        rx_buffer,
        sizeof(rx_buffer),
        timeout_ms
    );

    if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGD(s_tag, "ACK timeout for seq=%u", expected_seq);
        return ESP_ERR_TIMEOUT;
    }
    ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to receive ACK/NACK");

    /* Parse frame */
    uint16_t seq;
    star_frame_type_t type;
    uint8_t* payload;
    uint16_t payload_len;

    ret = star_frame_parse(
        &handle->rx_frame,
        rx_buffer,
        sizeof(rx_buffer),
        &seq,
        &type,
        &payload,
        &payload_len
    );

    if (ret != ESP_OK) {
        ESP_LOGW(s_tag, "Invalid frame received (CRC error or malformed)");
        return ESP_FAIL;
    }

    /* Check if it's for our sequence */
    if (seq != expected_seq) {
        ESP_LOGW(s_tag, "Unexpected seq in response: got=%u, expected=%u", seq, expected_seq);
        return ESP_FAIL;
    }

    /* Check frame type */
    if (type == k_star_frame_type_ack) {
        *is_ack = true;
        ESP_LOGD(s_tag, "Received ACK: seq=%u", seq);
        return ESP_OK;
    } else if (type == k_star_frame_type_nack) {
        *is_ack = false;
        ESP_LOGD(s_tag, "Received NACK: seq=%u", seq);
        return ESP_OK;
    } else {
        ESP_LOGW(s_tag, "Unexpected frame type: %d", type);
        return ESP_FAIL;
    }
}
