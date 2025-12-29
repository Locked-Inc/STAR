/**
 * @file rx_spi_comm.c
 * @brief High-Level SPI Communication Layer for RX72N
 *
 * Implements reliable SPI communication by integrating the frame layer
 * and SPI HAL. Handles frame encoding/decoding, CRC validation, and
 * sequence number management.
 *
 * @note RX72N operates as SPI peripheral, RPi5 as SPI controller.
 *
 * STAR Project - Texas A&M University
 * December 2025
 */

#include "rx_spi_comm.h"
#include "hardware.h"

#include <string.h>

/* =============================================================================
 * Module State
 * =============================================================================
 */

static const char *s_tag = "SPI_COMM";

/* =============================================================================
 * Internal Helpers
 * =============================================================================
 */

/**
 * @brief Perform raw SPI transfer using staging buffers
 */
static rx_err_t internal_spi_transfer(rx_spi_comm_handle_t *handle,
                                      const uint8_t *tx_data, size_t tx_len,
                                      uint8_t *rx_data, size_t rx_len)
{
    /* Prepare TX buffer (pad with zeros if RX is larger) */
    size_t transfer_len = (tx_len > rx_len) ? tx_len : rx_len;

    if (transfer_len > k_spi_comm_tx_buffer_size) {
        return RX_ERR_INVALID_SIZE;
    }

    memset(handle->tx_buffer, 0, transfer_len);
    if (tx_data != NULL && tx_len > 0) {
        memcpy(handle->tx_buffer, tx_data, tx_len);
    }

    /* Perform SPI transfer */
    rx_err_t err = rspi_peripheral_transfer(handle->channel, handle->tx_buffer,
                                            handle->rx_buffer,
                                            (uint16_t)transfer_len);

    if (err != RX_OK) {
        return err;
    }

    /* Copy received data if requested */
    if (rx_data != NULL && rx_len > 0) {
        memcpy(rx_data, handle->rx_buffer, rx_len);
    }

    return RX_OK;
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

rx_err_t rx_spi_comm_init(rx_spi_comm_handle_t *handle,
                          const rx_spi_comm_config_t *config)
{
    if (handle == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    /* Clear handle */
    memset(handle, 0, sizeof(rx_spi_comm_handle_t));

    /* Apply configuration */
    if (config != NULL) {
        handle->channel     = config->channel;
        handle->fec_enabled = config->fec_enabled;
    } else {
        handle->channel     = k_spi_comm_default_channel;
        handle->fec_enabled = 0;
    }

    /* Initialize frame encoder */
    rx_err_t err = rx_frame_encoder_init(&handle->encoder);
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Failed to init frame encoder");
        return err;
    }

    /* Initialize frame decoder */
    err = rx_frame_decoder_init(&handle->decoder);
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Failed to init frame decoder");
        rx_frame_encoder_deinit(&handle->encoder);
        return err;
    }

    /* Initialize sequence counters */
    handle->tx_sequence = 0;
    handle->rx_sequence = 0;

    handle->initialized = 1;

    RX_LOG_DEBUG(s_tag, "SPI comm initialized");
    return RX_OK;
}

rx_err_t rx_spi_comm_deinit(rx_spi_comm_handle_t *handle)
{
    if (handle == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (handle->initialized) {
        rx_frame_encoder_deinit(&handle->encoder);
        rx_frame_decoder_deinit(&handle->decoder);
        handle->initialized = 0;
    }

    RX_LOG_DEBUG(s_tag, "SPI comm deinitialized");
    return RX_OK;
}

/* =============================================================================
 * Send API
 * =============================================================================
 */

rx_err_t rx_spi_comm_send(rx_spi_comm_handle_t *handle, rx_frame_type_t type,
                          uint8_t flags, const uint8_t *payload,
                          size_t payload_len)
{
    if (handle == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (!handle->initialized) {
        RX_LOG_ERROR(s_tag, "Handle not initialized");
        return RX_ERR_INVALID_STATE;
    }

    if (payload == NULL && payload_len > 0) {
        return RX_ERR_INVALID_ARG;
    }

    if (payload_len > k_frame_max_payload) {
        RX_LOG_ERROR(s_tag, "Payload too large");
        return RX_ERR_INVALID_SIZE;
    }

    /* Build frame */
    rx_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    frame.header.sequence = handle->tx_sequence;
    frame.header.length   = (uint16_t)payload_len;
    frame.header.type     = (uint8_t)type;
    frame.header.flags    = flags;

    if (payload != NULL && payload_len > 0) {
        memcpy(frame.payload, payload, payload_len);
    }

    /* Add FEC flag if enabled */
    if (handle->fec_enabled) {
        frame.header.flags |= k_frame_flag_fec_enabled;
    }

    /* Encode frame to wire format */
    uint8_t wire_buffer[k_frame_max_size];
    size_t wire_len = 0;

    rx_err_t err =
        rx_frame_encode(&handle->encoder, &frame, wire_buffer, &wire_len);
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Frame encode failed");
        return err;
    }

    /* Transfer via SPI */
    err = internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "SPI transfer failed");
        return err;
    }

    /* Increment TX sequence */
    handle->tx_sequence++;

    return RX_OK;
}

rx_err_t rx_spi_comm_send_ack(rx_spi_comm_handle_t *handle, uint16_t sequence)
{
    if (handle == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (!handle->initialized) {
        return RX_ERR_INVALID_STATE;
    }

    /* Create ACK frame */
    rx_frame_t ack_frame;
    rx_err_t err = rx_frame_create_ack(&ack_frame, sequence);
    if (err != RX_OK) {
        return err;
    }

    /* Encode and send */
    uint8_t wire_buffer[k_frame_min_size];
    size_t wire_len = 0;

    err = rx_frame_encode(&handle->encoder, &ack_frame, wire_buffer, &wire_len);
    if (err != RX_OK) {
        return err;
    }

    return internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
}

rx_err_t rx_spi_comm_send_nack(rx_spi_comm_handle_t *handle, uint16_t sequence,
                               uint8_t flags)
{
    if (handle == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (!handle->initialized) {
        return RX_ERR_INVALID_STATE;
    }

    /* Create NACK frame */
    rx_frame_t nack_frame;
    rx_err_t err = rx_frame_create_nack(&nack_frame, sequence, flags);
    if (err != RX_OK) {
        return err;
    }

    /* Encode and send */
    uint8_t wire_buffer[k_frame_min_size];
    size_t wire_len = 0;

    err =
        rx_frame_encode(&handle->encoder, &nack_frame, wire_buffer, &wire_len);
    if (err != RX_OK) {
        return err;
    }

    return internal_spi_transfer(handle, wire_buffer, wire_len, NULL, 0);
}

/* =============================================================================
 * Receive API
 * =============================================================================
 */

rx_err_t rx_spi_comm_receive(rx_spi_comm_handle_t *handle, rx_frame_t *frame,
                             uint32_t timeout_ms)
{
    if (handle == NULL || frame == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (!handle->initialized) {
        return RX_ERR_INVALID_STATE;
    }

    /* Check for available data */
    bool available = false;
    rx_err_t err = rspi_peripheral_read_available(handle->channel, &available);
    if (err != RX_OK) {
        return err;
    }

    if (!available) {
        if (timeout_ms == 0) {
            return RX_ERR_TIMEOUT;
        }

        /* Simple polling loop - in production, use ThreadX semaphore */
        uint32_t elapsed = 0;
        while (!available && elapsed < timeout_ms) {
            /* Simple delay - replace with tx_thread_sleep in ThreadX */
            for (volatile uint32_t i = 0; i < 10000; i++) {
                __asm__ volatile("nop");
            }
            elapsed++;

            err = rspi_peripheral_read_available(handle->channel, &available);
            if (err != RX_OK) {
                return err;
            }
        }

        if (!available) {
            return RX_ERR_TIMEOUT;
        }
    }

    /* Read frame header first to determine length */
    uint8_t header_buf[k_frame_sync_size + k_frame_header_size];
    size_t header_len = sizeof(header_buf);

    /* For SPI peripheral mode, we receive what the controller sends */
    err = internal_spi_transfer(handle, NULL, 0, header_buf, header_len);
    if (err != RX_OK) {
        return err;
    }

    /* Validate sync word */
    uint16_t sync = ((uint16_t)header_buf[0] << 8) | header_buf[1];
    if (sync != k_frame_sync_word) {
        RX_LOG_ERROR(s_tag, "Invalid sync word");
        return RX_ERR_PROTOCOL_ERROR;
    }

    /* Extract payload length (big-endian) */
    uint16_t payload_len = ((uint16_t)header_buf[4] << 8) | header_buf[5];
    if (payload_len > k_frame_max_payload) {
        RX_LOG_ERROR(s_tag, "Payload too large");
        return RX_ERR_INVALID_SIZE;
    }

    /* Calculate total frame size */
    size_t total_size =
        k_frame_sync_size + k_frame_header_size + payload_len + k_frame_crc_size;

    /* Read remaining data (payload + CRC) */
    size_t remaining = payload_len + k_frame_crc_size;
    if (remaining > 0) {
        err = internal_spi_transfer(handle, NULL, 0,
                                    handle->rx_buffer + header_len, remaining);
        if (err != RX_OK) {
            return err;
        }
    }

    /* Copy header to buffer for decoding */
    memcpy(handle->rx_buffer, header_buf, header_len);

    /* Decode frame */
    err = rx_frame_decode(&handle->decoder, handle->rx_buffer, total_size,
                          frame);
    if (err != RX_OK) {
        RX_LOG_ERROR(s_tag, "Frame decode failed");
        return err;
    }

    /* Update expected RX sequence */
    handle->rx_sequence = frame->header.sequence + 1;

    return RX_OK;
}

rx_err_t rx_spi_comm_data_available(rx_spi_comm_handle_t *handle,
                                    bool *available)
{
    if (handle == NULL || available == NULL) {
        return RX_ERR_INVALID_ARG;
    }

    if (!handle->initialized) {
        return RX_ERR_INVALID_STATE;
    }

    return rspi_peripheral_read_available(handle->channel, available);
}

/* =============================================================================
 * Utility Functions
 * =============================================================================
 */

void rx_spi_comm_reset_sequence(rx_spi_comm_handle_t *handle)
{
    if (handle != NULL) {
        handle->tx_sequence = 0;
        handle->rx_sequence = 0;
    }
}

uint16_t rx_spi_comm_get_tx_sequence(const rx_spi_comm_handle_t *handle)
{
    if (handle == NULL) {
        return 0;
    }
    return handle->tx_sequence;
}

uint16_t rx_spi_comm_get_rx_sequence(const rx_spi_comm_handle_t *handle)
{
    if (handle == NULL) {
        return 0;
    }
    return handle->rx_sequence;
}
