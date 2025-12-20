/**
 * @file main.h
 * @brief STAR application configuration constants and definitions
 * @details
 * This header defines configuration parameters for the main application
 * entry point, including SPI bus and ARQ protocol settings used during
 * system initialization. Contains enum-based configuration constants for
 * reliable communication setup.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- Configuration Constants --- */

/* SPI configuration */
typedef enum {
  k_spi_queue_size = 3, /* Transaction queue depth */
} spi_config_t;

/* ARQ configuration */
typedef enum {
  k_arq_max_retries      = 3,  /* Maximum retry attempts */
  k_arq_retry_timeout_ms = 10, /* Timeout between retries (ms) */
} arq_config_t;

#ifdef __cplusplus
}
#endif

#endif /* MAIN_H */
