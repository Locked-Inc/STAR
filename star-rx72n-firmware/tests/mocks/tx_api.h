/**
 * @file tx_api.h
 * @brief Mock ThreadX API for unit testing
 *
 * Provides minimal stub definitions to allow compiling drivers
 * that reference ThreadX types without linking ThreadX.
 *
 * STAR Project - Texas A&M University
 * January 2026
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
