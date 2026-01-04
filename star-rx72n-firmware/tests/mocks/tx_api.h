/* tests/mocks/tx_api.h */

/**
 * @file tx_api.h
 * @brief Mock ThreadX API for unit testing
 *
 * @details
 * Provides minimal stub definitions to allow compiling drivers
 * that reference ThreadX types without linking ThreadX.
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_TX_API_H
#define MOCK_TX_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Stub - not used in unit tests */
typedef struct TX_MUTEX_STRUCT {
  int dummy;
} TX_MUTEX;

#ifdef __cplusplus
}
#endif

#endif /* MOCK_TX_API_H */
