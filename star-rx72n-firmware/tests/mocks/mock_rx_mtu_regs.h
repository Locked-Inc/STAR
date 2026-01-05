/* tests/mocks/mock_rx_mtu_regs.h */

/**
 * @file mock_rx_mtu_regs.h
 * @brief Mock MTU Register Accessor Override for Unit Testing
 *
 * @details
 * This header overrides the hardware register accessor functions with mock
 * implementations for unit testing. It must be included BEFORE rx72n_mtu_regs.h
 * and rx72n_system_regs.h in test builds.
 *
 * @date 2026-01-05
 * @copyright Copyright (c) 2026 STAR Project
 */

#ifndef MOCK_RX_MTU_REGS_H
#define MOCK_RX_MTU_REGS_H

#ifdef UNIT_TEST

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Mock Register Structures
 *
 * These match the hardware layout but are allocated in RAM for testing.
 * =============================================================================
 */

/**
 * @brief Mock MTU channel registers (matches rx_mtu_channel_regs_t layout)
 */
typedef struct {
  volatile uint8_t  tcr;
  volatile uint8_t  tmdr;
  volatile uint8_t  tiorh;
  volatile uint8_t  tiorl;
  volatile uint8_t  tier;
  volatile uint8_t  tsr;
  volatile uint16_t tcnt;
  volatile uint16_t tgra;
  volatile uint16_t tgrb;
  volatile uint16_t tgrc;
  volatile uint16_t tgrd;
} rx_mtu_channel_regs_t;

/**
 * @brief Mock MTU3/4 extended channel registers
 */
typedef struct {
  volatile uint8_t  tcr;
  volatile uint8_t  tmdr;
  volatile uint8_t  tiorh;
  volatile uint8_t  tiorl;
  volatile uint8_t  tier;
  volatile uint8_t  tsr;
  volatile uint16_t tcnt;
  volatile uint16_t tgra;
  volatile uint16_t tgrb;
  volatile uint16_t tgrc;
  volatile uint16_t tgrd;
  volatile uint16_t tgre;
  volatile uint16_t tgrf;
  volatile uint8_t  tier2;
  volatile uint8_t  tsr2;
  volatile uint8_t  tbtm;
} rx_mtu34_channel_regs_t;

/**
 * @brief Mock system registers
 */
typedef struct {
  volatile uint16_t syscr0;
  volatile uint16_t syscr1;
  uint8_t           reserved0[2];
  volatile uint16_t sbycr;
  uint8_t           reserved1[2];
  volatile uint16_t prcr;
  uint8_t           reserved1a[2];
  volatile uint32_t mstpcra;
  volatile uint32_t mstpcrb;
  volatile uint32_t mstpcrc;
  volatile uint32_t mstpcrd;
} rx_system_regs_t;

#ifndef RX_SYSTEM_REGS_T_DEFINED
#define RX_SYSTEM_REGS_T_DEFINED
#endif

/* =============================================================================
 * Mock Register Storage (defined in mock_rx_mtu_encoder.c)
 * =============================================================================
 */

/** @brief Maximum MTU channels */
typedef enum {
  k_mock_mtu_max_channels = 7,
} mock_mtu_constants_t;

extern rx_mtu_channel_regs_t g_mock_mtu_regs[k_mock_mtu_max_channels];
extern rx_system_regs_t      g_mock_system_regs;

/* =============================================================================
 * Mock Accessor Functions
 *
 * These replace the hardware accessor functions for testing.
 * =============================================================================
 */

static inline volatile rx_mtu_channel_regs_t* mtu0(void)
{
  return &g_mock_mtu_regs[0];
}

static inline volatile rx_mtu_channel_regs_t* mtu1(void)
{
  return &g_mock_mtu_regs[1];
}

static inline volatile rx_mtu_channel_regs_t* mtu2(void)
{
  return &g_mock_mtu_regs[2];
}

static inline volatile rx_mtu34_channel_regs_t* mtu3(void)
{
  return (volatile rx_mtu34_channel_regs_t*)&g_mock_mtu_regs[3];
}

static inline volatile rx_mtu34_channel_regs_t* mtu4(void)
{
  return (volatile rx_mtu34_channel_regs_t*)&g_mock_mtu_regs[4];
}

static inline volatile rx_mtu_channel_regs_t* mtu6(void)
{
  return &g_mock_mtu_regs[5];
}

static inline volatile rx_mtu_channel_regs_t* mtu7(void)
{
  return &g_mock_mtu_regs[6];
}

static inline volatile rx_system_regs_t* system_regs(void)
{
  return &g_mock_system_regs;
}

/* =============================================================================
 * Prevent inclusion of real hardware headers
 *
 * The PRCR constants are defined in rx_gpio_constants.h which is still included.
 * =============================================================================
 */

#define STAR_RX72N_MTU_REGS_H
#define STAR_RX72N_SYSTEM_REGS_H

#ifdef __cplusplus
}
#endif

#endif /* UNIT_TEST */

#endif /* MOCK_RX_MTU_REGS_H */
