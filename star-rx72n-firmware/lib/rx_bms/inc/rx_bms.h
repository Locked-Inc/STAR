/* lib/rx_bms/inc/rx_bms.h */

/**
 * @file rx_bms.h
 * @brief Battery Management System (BMS) Driver Library
 *
 * Unified interface for battery fuel gauge ICs implementing Smart Battery System (SBS).
 *
 * Supported devices:
 * - Texas Instruments BQ78350-R1A (16-cell, automotive-grade)
 * - Texas Instruments BQ4050 (4-cell, industrial-grade)
 *
 * All drivers implement SBS 1.1 specification via SMBus protocol.
 *
 * @date 2026-01-09
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef STAR_RX_BMS_H
#define STAR_RX_BMS_H

/* Include all BMS device drivers */
#include "rx_bq78350.h"
#include "rx_bq4050.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BMS device types
 */
typedef enum {
  k_bms_device_bq78350 = 0, /**< TI BQ78350-R1A (16-cell) */
  k_bms_device_bq4050  = 1, /**< TI BQ4050 (4-cell) */
} rx_bms_device_type_t;

#ifdef __cplusplus
}
#endif

#endif /* STAR_RX_BMS_H */
