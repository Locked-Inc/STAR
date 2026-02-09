/* tests/mocks/mock_rx_system_regs.c */

/**
 * @file mock_rx_system_regs.c
 * @brief Mock Reset Status Registers Implementation
 *
 * @details
 * Implements mock reset status registers for unit testing main.c startup
 * validation. Provides controllable RSTSR0/1/2 values without hardware.
 *
 * @author STAR Team
 * @date 2026-01-29
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "mock_rx_system_regs.h"
#include "rx72n_system_regs.h"

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum rstsr0_flag_bits_t
 * @brief RSTSR0 flag bit positions
 */
typedef enum : uint8_t {
  k_rstsr0_porf_bit   = 0x01, /**< Power-On Reset Detect Flag */
  k_rstsr0_lvd0rf_bit = 0x02, /**< Voltage Monitor 0 Reset Detect Flag */
} rstsr0_flag_bits_t;

/**
 * @enum rstsr1_flag_bits_t
 * @brief RSTSR1 flag bit positions
 */
typedef enum : uint8_t {
  k_rstsr1_cwsf_bit = 0x01, /**< Cold/Warm Start Determination Flag */
} rstsr1_flag_bits_t;

/**
 * @enum rstsr2_flag_bits_t
 * @brief RSTSR2 flag bit positions
 */
typedef enum : uint8_t {
  k_rstsr2_iwdtrf_bit = 0x01, /**< Independent Watchdog Timer Reset Flag */
  k_rstsr2_wdtrf_bit  = 0x02, /**< Watchdog Timer Reset Flag */
  k_rstsr2_swrf_bit   = 0x04, /**< Software Reset Flag */
} rstsr2_flag_bits_t;

/* =============================================================================
 * Static Variables - Mock Register State
 * =============================================================================
 */

/** @brief Mock RSTSR0 register value */
static uint8_t s_rstsr0 = 0;

/** @brief Mock RSTSR1 register value */
static uint8_t s_rstsr1 = 0;

/** @brief Mock RSTSR2 register value */
static uint8_t s_rstsr2 = 0;

/** @brief Count of rstsr01() accessor calls */
static uint32_t s_rstsr01_read_count = 0;

/** @brief Count of rstsr2() accessor calls */
static uint32_t s_rstsr2_read_count = 0;

/** @brief Mock RSTSR01 register structure for accessor */
static rx_rstsr01_regs_t s_rstsr01_regs;

/** @brief Mock PRCR register (Protection Register) - global for inline accessor */
volatile uint16_t g_mock_prcr = 0;

/* =============================================================================
 * Mock Control Functions
 * =============================================================================
 */

void mock_system_regs_reset(void)
{
  /* Default to normal cold boot state: PORF=1, all others=0 */
  s_rstsr0 = k_rstsr0_porf_bit;
  s_rstsr1 = 0;
  s_rstsr2 = 0;

  /* Reset call counters */
  s_rstsr01_read_count = 0;
  s_rstsr2_read_count  = 0;

  /* Update the structure */
  s_rstsr01_regs.rstsr0 = s_rstsr0;
  s_rstsr01_regs.rstsr1 = s_rstsr1;
}

/* =============================================================================
 * RSTSR0 Flag Setters
 * =============================================================================
 */

void mock_system_regs_set_porf(bool set)
{
  if (set) {
    s_rstsr0 |= k_rstsr0_porf_bit;
  } else {
    s_rstsr0 &= (uint8_t)~k_rstsr0_porf_bit;
  }
  s_rstsr01_regs.rstsr0 = s_rstsr0;
}

void mock_system_regs_set_lvd0rf(bool set)
{
  if (set) {
    s_rstsr0 |= k_rstsr0_lvd0rf_bit;
  } else {
    s_rstsr0 &= (uint8_t)~k_rstsr0_lvd0rf_bit;
  }
  s_rstsr01_regs.rstsr0 = s_rstsr0;
}

/* =============================================================================
 * RSTSR1 Flag Setters
 * =============================================================================
 */

void mock_system_regs_set_cwsf(bool set)
{
  if (set) {
    s_rstsr1 |= k_rstsr1_cwsf_bit;
  } else {
    s_rstsr1 &= (uint8_t)~k_rstsr1_cwsf_bit;
  }
  s_rstsr01_regs.rstsr1 = s_rstsr1;
}

/* =============================================================================
 * RSTSR2 Flag Setters
 * =============================================================================
 */

void mock_system_regs_set_iwdtrf(bool set)
{
  if (set) {
    s_rstsr2 |= k_rstsr2_iwdtrf_bit;
  } else {
    s_rstsr2 &= (uint8_t)~k_rstsr2_iwdtrf_bit;
  }
}

void mock_system_regs_set_wdtrf(bool set)
{
  if (set) {
    s_rstsr2 |= k_rstsr2_wdtrf_bit;
  } else {
    s_rstsr2 &= (uint8_t)~k_rstsr2_wdtrf_bit;
  }
}

void mock_system_regs_set_swrf(bool set)
{
  if (set) {
    s_rstsr2 |= k_rstsr2_swrf_bit;
  } else {
    s_rstsr2 &= (uint8_t)~k_rstsr2_swrf_bit;
  }
}

/* =============================================================================
 * Scenario Helpers
 * =============================================================================
 */

void mock_system_regs_simulate_cold_boot(void)
{
  s_rstsr0 = k_rstsr0_porf_bit; /* PORF=1 (power-on) */
  s_rstsr1 = 0;                 /* CWSF=0 (cold start) */
  s_rstsr2 = 0;                 /* No watchdog/software resets */

  s_rstsr01_regs.rstsr0 = s_rstsr0;
  s_rstsr01_regs.rstsr1 = s_rstsr1;
}

void mock_system_regs_simulate_warm_boot(void)
{
  s_rstsr0 = 0;                 /* PORF=0 (no power-on) */
  s_rstsr1 = k_rstsr1_cwsf_bit; /* CWSF=1 (warm start) */
  s_rstsr2 = 0;                 /* No watchdog resets */

  s_rstsr01_regs.rstsr0 = s_rstsr0;
  s_rstsr01_regs.rstsr1 = s_rstsr1;
}

void mock_system_regs_simulate_iwdt_timeout(void)
{
  s_rstsr0 = 0;                  /* PORF=0 (not a cold boot) */
  s_rstsr1 = k_rstsr1_cwsf_bit;  /* CWSF=1 (warm start) */
  s_rstsr2 = k_rstsr2_iwdtrf_bit; /* IWDTRF=1 (CRITICAL!) */

  s_rstsr01_regs.rstsr0 = s_rstsr0;
  s_rstsr01_regs.rstsr1 = s_rstsr1;
}

void mock_system_regs_simulate_brownout(void)
{
  s_rstsr0 = k_rstsr0_lvd0rf_bit; /* LVD0RF=1 (brownout) */
  s_rstsr1 = k_rstsr1_cwsf_bit;   /* CWSF=1 (warm start) */
  s_rstsr2 = 0;                   /* No watchdog resets */

  s_rstsr01_regs.rstsr0 = s_rstsr0;
  s_rstsr01_regs.rstsr1 = s_rstsr1;
}

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

uint8_t mock_system_regs_get_rstsr0(void)
{
  return s_rstsr0;
}

uint8_t mock_system_regs_get_rstsr1(void)
{
  return s_rstsr1;
}

uint8_t mock_system_regs_get_rstsr2(void)
{
  return s_rstsr2;
}

uint32_t mock_system_regs_get_rstsr01_read_count(void)
{
  return s_rstsr01_read_count;
}

uint32_t mock_system_regs_get_rstsr2_read_count(void)
{
  return s_rstsr2_read_count;
}

/* =============================================================================
 * Mock Register Accessors - Called by main.c under test
 * =============================================================================
 */

/**
 * @brief Mock rstsr01() accessor - returns pointer to mock RSTSR0/1 registers
 * @return Pointer to mock RSTSR01 structure
 */
volatile rx_rstsr01_regs_t* rstsr01(void)
{
  s_rstsr01_read_count++;
  return (volatile rx_rstsr01_regs_t*)&s_rstsr01_regs;
}

/**
 * @brief Mock rstsr2() accessor - returns pointer to mock RSTSR2 register
 * @return Pointer to mock RSTSR2 value
 */
volatile uint8_t* rstsr2(void)
{
  s_rstsr2_read_count++;
  return (volatile uint8_t*)&s_rstsr2;
}
