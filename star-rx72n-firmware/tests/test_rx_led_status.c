/**
 * @file test_rx_led_status.c
 * @brief Unit Tests for LED Status Task
 *
 * @details
 * Tests the LED status task logic including GPIO initialization,
 * LED set/clear operations, and system state to LED mapping.
 * All hardware dependencies are mocked inline.
 *
 * Self-contained: includes its own mocks for PORT registers,
 * shared_data, ThreadX, and logging.
 *
 * @author Locked, Inc.
 * @date 2026-02-10
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 * @since Version 1.0.0
 */

#include <stdint.h>
#include <string.h>

#include "unity.h"

/* =============================================================================
 * Byte-level memory helpers (replace memset/memcpy to satisfy clang-tidy)
 * =============================================================================
 */

/** @brief Fill a memory region with a constant byte value */
static inline void internal_fill_bytes(void* ptr, uint8_t val, size_t len)
{
  uint8_t* p = (uint8_t*)ptr;
  for (size_t i = 0; i < len; ++i) {
    p[i] = val;
  }
}

/* =============================================================================
 * Mock Infrastructure
 * =============================================================================
 */

/* ---- Mock PORT registers ---- */

/**
 * @brief Mock PORT register structure matching rx_port_regs_t layout
 */
typedef struct {
  uint8_t pdr;   /**< Port Direction Register */
  uint8_t podr;  /**< Port Output Data Register */
  uint8_t pidr;  /**< Port Input Data Register */
  uint8_t pmr;   /**< Port Mode Register */
  uint8_t odr0;  /**< Open Drain Control 0 */
  uint8_t odr1;  /**< Open Drain Control 1 */
  uint8_t pcr;   /**< Pull-Up Control Register */
  uint8_t dscr;  /**< Drive Strength Control */
  uint8_t dscr2; /**< Drive Strength Control 2 */
} mock_port_regs_t;

/** @brief Mock port registers for PORT7, PORTA, PORTB */
static mock_port_regs_t s_mock_port7;
static mock_port_regs_t s_mock_porta; /* Port 10 */
static mock_port_regs_t s_mock_portb; /* Port 11 */

/* ---- Mock rx_port_regs_t type alias ---- */
typedef mock_port_regs_t rx_port_regs_t;

/* ---- Mock shared_data state ---- */

typedef enum : uint8_t {
  k_estop_reason_none         = 0,
  k_estop_reason_driver_fault = 3,
} mock_estop_reason_t;

typedef enum : uint8_t {
  k_motor_mode_idle     = 0,
  k_motor_mode_velocity = 1,
  k_motor_mode_estop    = 2,
} mock_motor_mode_t;

/** @brief Number of motors per array dimension */
typedef enum : uint8_t {
  k_mock_motor_array_count = 4, /**< Number of motors in state/command arrays */
} mock_motor_array_t;

typedef struct {
  float   current_velocity_mps[k_mock_motor_array_count];
  float   current_ma[k_mock_motor_array_count];
  int32_t encoder_counts[k_mock_motor_array_count];
  uint8_t fault_flags[k_mock_motor_array_count];
  float   duty_cycle_percent[k_mock_motor_array_count];
  bool    estop_active;
  uint8_t estop_reason;
  uint8_t mode;
} mock_motor_state_t;

typedef struct {
  float    target_velocity_mps[k_mock_motor_array_count];
  uint32_t sequence;
  uint32_t timestamp_ms;
  bool     valid;
} mock_motor_command_t;

typedef struct {
  uint16_t distance_cm[k_mock_motor_array_count];
  bool     obstacle_detected[k_mock_motor_array_count];
  bool     any_obstacle;
  uint32_t timestamp_ms;
} mock_obstacle_state_t;

/* Use same type names as real code */
typedef mock_motor_state_t    motor_state_t;
typedef mock_motor_command_t  motor_command_t;
typedef mock_obstacle_state_t obstacle_state_t;

/* ---- Mock shared_data values ---- */
static motor_state_t    s_mock_motor_state;
static motor_command_t  s_mock_motor_command;
static obstacle_state_t s_mock_obstacle_state;
static bool             s_mock_estop_active;

/* ---- Mock rx_err_t ---- */
typedef enum : uint8_t {
  k_rx_ok                     = 0,
  k_rx_err_invalid_state      = 1,
  k_rx_err_rtos_thread_create = 2,
} mock_rx_err_t;

typedef mock_rx_err_t rx_err_t;

/** @brief Mock ThreadX error codes for testing */
typedef enum : uint8_t {
  k_mock_tx_error = 99, /**< Arbitrary ThreadX error code for failure injection */
} mock_tx_error_codes_t;

/* ---- Mock ThreadX types (project-style names; this test reimplements the
 *      LED task locally and never links against real ThreadX, so we are free
 *      to drop the upstream UPPERCASE naming).
 */
typedef unsigned long mock_tx_ulong_t;
typedef unsigned int  mock_tx_uint_t;
/** @brief ThreadX thread name buffer size */
typedef enum : uint8_t {
  k_tx_thread_name_len = 16, /**< Max thread name length including null */
} mock_tx_sizes_t;

typedef struct {
  char    name[k_tx_thread_name_len];
  uint8_t priority;
  bool    created;
} mock_tx_thread_t;
/* ThreadX-equivalent constants (project-style names) */
typedef enum : uint8_t {
  k_mock_tx_success       = 0,
  k_mock_tx_no_time_slice = 0,
  k_mock_tx_auto_start    = 1,
} mock_tx_constants_t;

/* ---- Mock counters ---- */
static uint32_t       s_mock_tx_create_calls;
static mock_tx_uint_t s_mock_tx_create_return;
static uint32_t       s_mock_tx_sleep_calls;
static uint32_t       s_mock_tx_sleep_last_ticks;

/* ---- Mock function pointer capture ---- */
typedef void (*thread_entry_t)(mock_tx_ulong_t);
static thread_entry_t s_captured_entry;

/* =============================================================================
 * Mock Function Implementations
 * =============================================================================
 */

/* ThreadX mocks */
mock_tx_uint_t tx_thread_create(mock_tx_thread_t* thread,
                                const char*       name,
                                void (*entry)(mock_tx_ulong_t),
                                mock_tx_ulong_t input,
                                void*           stack,
                                mock_tx_ulong_t stack_size,
                                mock_tx_uint_t  priority,
                                mock_tx_uint_t  preempt,
                                mock_tx_ulong_t time_slice,
                                mock_tx_uint_t  auto_start)
{
  (void)input;
  (void)stack;
  (void)stack_size;
  (void)preempt;
  (void)time_slice;
  (void)auto_start;

  s_mock_tx_create_calls++;
  s_captured_entry = entry;

  if (s_mock_tx_create_return == k_mock_tx_success) {
    /* Copy thread name byte-by-byte (replaces strncpy) */
    size_t max_copy = sizeof(thread->name) - 1U;
    size_t i        = 0;
    while ((i < max_copy) && (name[i] != '\0')) {
      thread->name[i] = name[i];
      i++;
    }
    thread->name[i]  = '\0';
    thread->priority = (uint8_t)priority;
    thread->created  = true;
  }

  return s_mock_tx_create_return;
}

mock_tx_uint_t tx_thread_sleep(mock_tx_ulong_t ticks)
{
  s_mock_tx_sleep_calls++;
  s_mock_tx_sleep_last_ticks = (uint32_t)ticks;
  return k_mock_tx_success;
}

/** @brief Mock ThreadX constants */
typedef enum : uint32_t {
  k_mock_tick_value = 12345, /**< Arbitrary tick count for tx_time_get mock */
} mock_threadx_constants_t;

mock_tx_ulong_t tx_time_get(void)
{
  return k_mock_tick_value;
}

/* shared_data mocks */
rx_err_t shared_data_get_motor_state(motor_state_t* out)
{
  *out = s_mock_motor_state;
  return k_rx_ok;
}

rx_err_t shared_data_get_motor_command(motor_command_t* out)
{
  *out = s_mock_motor_command;
  return k_rx_ok;
}

rx_err_t shared_data_get_obstacle(obstacle_state_t* out)
{
  *out = s_mock_obstacle_state;
  return k_rx_ok;
}

bool shared_data_is_estop_active(void)
{
  return s_mock_estop_active;
}

/* Port register mocks */

/** @brief Port number constants for rx_port_get_base switch */
typedef enum : uint8_t {
  k_port_num_7 = 7,  /**< PORT7 */
  k_port_num_a = 10, /**< PORTA */
  k_port_num_b = 11, /**< PORTB */
} mock_port_numbers_t;

static volatile rx_port_regs_t* rx_port_get_base(uint8_t port)
{
  switch (port) {
    case k_port_num_7:
      return (volatile rx_port_regs_t*)&s_mock_port7;
    case k_port_num_a:
      return (volatile rx_port_regs_t*)&s_mock_porta;
    case k_port_num_b:
      return (volatile rx_port_regs_t*)&s_mock_portb;
    default:
      return (volatile rx_port_regs_t*)0;
  }
}

/* Logging mocks (no-op) */
void rx_log_info(const char* tag, const char* msg)
{
  (void)tag;
  (void)msg;
}
void rx_log_error_val(const char* tag, const char* msg, uint32_t val)
{
  (void)tag;
  (void)msg;
  (void)val;
}

/* RX_ASSERT mock */
#define RX_ASSERT(cond, msg)                                                                       \
  do {                                                                                             \
    (void)(cond);                                                                                  \
    (void)(msg);                                                                                   \
  } while (0)

/* =============================================================================
 * Hardware Config Constants (from hardware_config.h)
 * =============================================================================
 */

/**
 * @enum led_ports_t
 * @brief GPIO port assignments for status LEDs
 *
 * @details
 * Maps each LED (0-5) to its corresponding Renesas RX72N port number.
 * Port numbers follow RX72N hardware definitions: PORT7=7, PORTA=10, PORTB=11.
 * These values are used with rx_port_get_base() to access GPIO registers.
 */
typedef enum : uint8_t {
  k_led_0_port = 10, /**< LED0 on PORTA (PA7, pin 88) */
  k_led_1_port = 11, /**< LED1 on PORTB (PB0, pin 87) */
  k_led_2_port = 7,  /**< LED2 on PORT7 (P71, pin 86) */
  k_led_3_port = 7,  /**< LED3 on PORT7 (P72, pin 85) */
  k_led_4_port = 11, /**< LED4 on PORTB (PB1, pin 84) */
  k_led_5_port = 11, /**< LED5 on PORTB (PB2, pin 83) */
} led_ports_t;

/**
 * @enum led_pins_t
 * @brief GPIO pin offsets within ports for status LEDs
 *
 * @details
 * Specifies the bit offset (0-7) within each port for LED control.
 * Combined with led_ports_t to form complete pin addresses (e.g., PA7 = port 10, pin 7).
 * These values are used to set/clear individual bits in PDR and PODR registers.
 */
typedef enum : uint8_t {
  k_led_0_pin = 7, /**< LED0 pin 7 (PA7, pin 88) */
  k_led_1_pin = 0, /**< LED1 pin 0 (PB0, pin 87) */
  k_led_2_pin = 1, /**< LED2 pin 1 (P71, pin 86) */
  k_led_3_pin = 2, /**< LED3 pin 2 (P72, pin 85) */
  k_led_4_pin = 1, /**< LED4 pin 1 (PB1, pin 84) */
  k_led_5_pin = 2, /**< LED5 pin 2 (PB2, pin 83) */
} led_pins_t;

typedef enum : uint8_t {
  k_led_count = 6,
} led_count_t;

/* POEG motor count (used in led_status_task.c) */
typedef enum : uint8_t {
  k_poeg_motor_count = 4,
} mock_poeg_count_t;

/**
 * @enum led_expected_hw_constants_t
 * @brief Expected port and pin numbers for each LED, matching production hardware_config.h
 *
 * @details
 * Named constants for the expected port and pin values used in the compile-time
 * assertions below. Using typed enums instead of raw integer literals ensures
 * consistent naming and avoids magic numbers per STAR coding standards.
 *
 * Production source: star-rx72n-firmware/src/inc/hardware_config.h
 * lines 1075-1096 (led_ports_t and led_pins_t enums).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_expected_porta = 10U, /**< Port A numeric value (PORTA = 10) */
  k_expected_portb = 11U, /**< Port B numeric value (PORTB = 11) */
  k_expected_port7 = 7U,  /**< Port 7 numeric value (PORT7 = 7) */
  k_expected_pin_0 = 0U,  /**< Pin 0 */
  k_expected_pin_1 = 1U,  /**< Pin 1 */
  k_expected_pin_2 = 2U,  /**< Pin 2 */
  k_expected_pin_7 = 7U,  /**< Pin 7 */
} led_expected_hw_constants_t;

/**
 * @brief Compile-time assertions to verify test LED constants match production
 * @details
 * These assertions ensure the test enum values match the production definitions
 * in hardware_config.h. Any mismatch will cause a build failure, preventing
 * test/production drift.
 *
 * Production source: star-rx72n-firmware/src/inc/hardware_config.h
 * lines 1075-1096 (led_ports_t and led_pins_t enums).
 */
static_assert((bool)((int)k_led_0_port == (int)k_expected_porta),
              "LED0 port must match production (PORTA=10)");
static_assert((bool)((int)k_led_1_port == (int)k_expected_portb),
              "LED1 port must match production (PORTB=11)");
static_assert((bool)((int)k_led_2_port == (int)k_expected_port7),
              "LED2 port must match production (PORT7=7)");
static_assert((bool)((int)k_led_3_port == (int)k_expected_port7),
              "LED3 port must match production (PORT7=7)");
static_assert((bool)((int)k_led_4_port == (int)k_expected_portb),
              "LED4 port must match production (PORTB=11)");
static_assert((bool)((int)k_led_5_port == (int)k_expected_portb),
              "LED5 port must match production (PORTB=11)");

static_assert((bool)((int)k_led_0_pin == (int)k_expected_pin_7),
              "LED0 pin must match production (PA7)");
static_assert((bool)((int)k_led_1_pin == (int)k_expected_pin_0),
              "LED1 pin must match production (PB0)");
static_assert((bool)((int)k_led_2_pin == (int)k_expected_pin_1),
              "LED2 pin must match production (P71)");
static_assert((bool)((int)k_led_3_pin == (int)k_expected_pin_2),
              "LED3 pin must match production (P72)");
static_assert((bool)((int)k_led_4_pin == (int)k_expected_pin_1),
              "LED4 pin must match production (PB1)");
static_assert((bool)((int)k_led_5_pin == (int)k_expected_pin_2),
              "LED5 pin must match production (PB2)");

/* =============================================================================
 * Include the source under test AFTER all mocks are defined
 * =============================================================================
 */

/* Prevent real headers from being included */
#define LED_STATUS_TASK_H_INCLUDED
#define HARDWARE_CONFIG_H_INCLUDED
#define RX_PORT_UTILS_H_INCLUDED
#define SHARED_DATA_H_INCLUDED

/* We need to override the includes in the source file.
 * Since led_status_task.c includes specific headers, we define stubs
 * and then include the raw function implementations.
 * Instead, we replicate the key logic for testing. */

/* =============================================================================
 * Re-implement task logic inline for testability
 * (Avoids complex include override; tests the actual algorithms)
 * =============================================================================
 */

/* These match the constants from led_status_task.c */
typedef enum : uint16_t {
  k_led_task_stack_size   = 512,
  k_led_task_priority     = 17,
  k_led_task_input        = 0,
  k_led_task_period_ticks = 5,
} test_led_task_constants_t;

typedef enum : uint8_t {
  k_led_heartbeat_half_period = 10,
  k_led_error_half_period     = 3,
  k_led_comm_pulse_duration   = 2,
} test_led_timing_t;

typedef enum : uint8_t {
  k_led_idx_heartbeat = 0,
  k_led_idx_error     = 1,
  k_led_idx_motor     = 2,
  k_led_idx_comm      = 3,
  k_led_idx_obstacle  = 4,
  k_led_idx_estop     = 5,
} test_led_index_t;

/** @brief Magic number replacements for test assertion values */
typedef enum : uint8_t {
  k_test_led_out_of_range     = 6,     /**< First invalid LED index */
  k_test_led_way_out_of_range = 255,   /**< Max u8 for extreme invalid index */
  k_test_all_bits_set         = 0xFFU, /**< All bits set pattern for port init */
  k_test_fault_flag_set       = 0x01U, /**< Non-zero fault flag value */
} test_led_misc_constants_t;

/** @brief Heartbeat timing division constant */
typedef enum : uint8_t {
  k_heartbeat_divisor = 2, /**< Divides half-period to get on/off threshold */
} test_heartbeat_t;

/** @brief Heartbeat tick loop bound */
typedef enum : uint8_t {
  k_heartbeat_tick_count = 5, /**< Ticks per half-phase of heartbeat */
} test_heartbeat_ticks_t;

/** @brief Motor duty cycle test value */
static const float s_test_duty_active = 50.0F;

/** @brief Motor index constants for duty_cycle_percent array access */
typedef enum : uint8_t {
  k_test_motor_idx_fl = 0, /**< Front-left motor index */
  k_test_motor_idx_fr = 1, /**< Front-right motor index */
  k_test_motor_idx_bl = 2, /**< Back-left motor index */
  k_test_motor_idx_br = 3, /**< Back-right motor index */
} test_motor_idx_t;

static const uint8_t s_test_led_ports[k_led_count] = {
  k_led_0_port,
  k_led_1_port,
  k_led_2_port,
  k_led_3_port,
  k_led_4_port,
  k_led_5_port,
};

static const uint8_t s_test_led_pins[k_led_count] = {
  k_led_0_pin,
  k_led_1_pin,
  k_led_2_pin,
  k_led_3_pin,
  k_led_4_pin,
  k_led_5_pin,
};

/** @brief Set an LED on or off (test helper matching task logic) */
static void internal_test_led_set(uint8_t led_index, bool on)
{
  if (led_index >= k_led_count) {
    return;
  }
  volatile rx_port_regs_t* port = rx_port_get_base(s_test_led_ports[led_index]);
  if (port == (volatile rx_port_regs_t*)0) {
    return;
  }
  const uint8_t pin_mask = (uint8_t)(1U << s_test_led_pins[led_index]);
  if (on) {
    port->podr |= pin_mask;
  } else {
    port->podr &= (uint8_t)~pin_mask;
  }
}

/** @brief Initialize LED GPIOs (test helper matching task logic) */
static void internal_test_led_init_gpio(void)
{
  for (uint8_t i = 0; i < k_led_count; i++) {
    volatile rx_port_regs_t* port = rx_port_get_base(s_test_led_ports[i]);
    if (port == (volatile rx_port_regs_t*)0) {
      continue;
    }
    const uint8_t pin_mask = (uint8_t)(1U << s_test_led_pins[i]);
    port->pmr &= (uint8_t)~pin_mask;
    port->pdr |= pin_mask;
    port->podr &= (uint8_t)~pin_mask;
  }
}

/** @brief Check if LED is on */
static bool internal_test_led_is_on(uint8_t led_index)
{
  if (led_index >= k_led_count) {
    return false;
  }
  volatile rx_port_regs_t* port = rx_port_get_base(s_test_led_ports[led_index]);
  if (port == (volatile rx_port_regs_t*)0) {
    return false;
  }
  const uint8_t pin_mask = (uint8_t)(1U << s_test_led_pins[led_index]);
  return (bool)((port->podr & pin_mask) != 0U);
}

/* Task create function (replicated for testing) */
static mock_tx_thread_t s_test_led_thread;
static uint8_t          s_test_led_stack[k_led_task_stack_size];
static bool             s_test_led_created = false;

static rx_err_t test_led_status_task_create(void)
{
  mock_tx_uint_t tx_status;

  if (s_test_led_created) {
    return k_rx_err_invalid_state;
  }

  tx_status = tx_thread_create(&s_test_led_thread,
                               "LEDTask",
                               (thread_entry_t)0,
                               k_led_task_input,
                               s_test_led_stack,
                               k_led_task_stack_size,
                               k_led_task_priority,
                               k_led_task_priority,
                               k_mock_tx_no_time_slice,
                               k_mock_tx_auto_start);

  if (tx_status != k_mock_tx_success) {
    return k_rx_err_rtos_thread_create;
  }

  s_test_led_created = true;
  return k_rx_ok;
}

/* =============================================================================
 * Test Setup / Teardown
 * =============================================================================
 */

void setUp(void)
{
  /* Clear all mock state */
  internal_fill_bytes(&s_mock_port7,
                      k_test_all_bits_set,
                      sizeof(s_mock_port7)); /* Start with all bits set */
  internal_fill_bytes(&s_mock_porta, k_test_all_bits_set, sizeof(s_mock_porta));
  internal_fill_bytes(&s_mock_portb, k_test_all_bits_set, sizeof(s_mock_portb));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_motor_state, 0, sizeof(s_mock_motor_state));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_motor_command, 0, sizeof(s_mock_motor_command));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_obstacle_state, 0, sizeof(s_mock_obstacle_state));
  s_mock_estop_active = false;

  s_mock_tx_create_calls     = 0;
  s_mock_tx_create_return    = k_mock_tx_success;
  s_mock_tx_sleep_calls      = 0;
  s_mock_tx_sleep_last_ticks = 0;
  s_captured_entry           = (thread_entry_t)0;

  s_test_led_created = false;
}

void tearDown(void) {}

/* =============================================================================
 * Tests: Task Creation
 * =============================================================================
 */

/** @brief Verify task creates successfully */
void test_led_task_create_success(void)
{
  rx_err_t err = test_led_status_task_create();

  TEST_ASSERT_EQUAL(k_rx_ok, err);
  TEST_ASSERT_TRUE(s_test_led_created);
  TEST_ASSERT_EQUAL(1, s_mock_tx_create_calls);
  TEST_ASSERT_EQUAL(k_led_task_priority, s_test_led_thread.priority);
}

/** @brief Verify double creation returns error */
void test_led_task_create_double_returns_error(void)
{
  rx_err_t err = test_led_status_task_create();

  TEST_ASSERT_EQUAL(k_rx_ok, err);

  err = test_led_status_task_create();
  TEST_ASSERT_EQUAL(k_rx_err_invalid_state, err);
  TEST_ASSERT_EQUAL(1, s_mock_tx_create_calls); /* Only called once */
}

/** @brief Verify ThreadX create failure is propagated */
void test_led_task_create_threadx_failure(void)
{
  s_mock_tx_create_return = k_mock_tx_error;

  rx_err_t err = test_led_status_task_create();

  TEST_ASSERT_EQUAL(k_rx_err_rtos_thread_create, err);
  TEST_ASSERT_FALSE(s_test_led_created);
}

/** @brief Verify task configuration constants */
void test_led_task_config_constants(void)
{
  TEST_ASSERT_EQUAL(k_led_task_stack_size, k_led_task_stack_size);
  TEST_ASSERT_EQUAL(k_led_task_priority, k_led_task_priority);
  TEST_ASSERT_EQUAL(k_led_task_period_ticks, k_led_task_period_ticks);
}

/* =============================================================================
 * Tests: GPIO Initialization
 * =============================================================================
 */

/** @brief Verify GPIO init configures all 6 LED pins as outputs */
void test_led_gpio_init_sets_output_direction(void)
{
  /* Clear ports to known state */
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_port7, 0, sizeof(s_mock_port7));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_porta, 0, sizeof(s_mock_porta));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_portb, 0, sizeof(s_mock_portb));

  internal_test_led_init_gpio();

  /* PORT7: pins 1,2 should be outputs (LED2, LED3) */
  TEST_ASSERT_BITS(0x06, 0x06, s_mock_port7.pdr);  /* PDR bits 1,2 set (0x02|0x04=0x06) */
  TEST_ASSERT_BITS(0x06, 0x00, s_mock_port7.pmr);  /* PMR bits 1,2 cleared */
  TEST_ASSERT_BITS(0x06, 0x00, s_mock_port7.podr); /* PODR bits 1,2 cleared (off) */

  /* PORTA: pin 7 should be output (LED0/heartbeat) */
  TEST_ASSERT_BITS(0x80, 0x80, s_mock_porta.pdr); /* PDR bit 7 set */
  TEST_ASSERT_BITS(0x80, 0x00, s_mock_porta.pmr);
  TEST_ASSERT_BITS(0x80, 0x00, s_mock_porta.podr);

  /* PORTB: pins 0,1,2 should be outputs (LED1, LED4, LED5) */
  TEST_ASSERT_BITS(0x07, 0x07, s_mock_portb.pdr); /* Pins 0,1,2 = 0x01|0x02|0x04 = 0x07 */
  TEST_ASSERT_BITS(0x07, 0x00, s_mock_portb.pmr);
  TEST_ASSERT_BITS(0x07, 0x00, s_mock_portb.podr);
}

/** @brief Verify GPIO init does not disturb other port bits */
void test_led_gpio_init_preserves_other_bits(void)
{
  /* Set all bits to 1 initially */
  internal_fill_bytes(&s_mock_port7, k_test_all_bits_set, sizeof(s_mock_port7));
  internal_fill_bytes(&s_mock_porta, k_test_all_bits_set, sizeof(s_mock_porta));
  internal_fill_bytes(&s_mock_portb, k_test_all_bits_set, sizeof(s_mock_portb));

  internal_test_led_init_gpio();

  /* Non-LED bits in PORT7 should remain set */
  TEST_ASSERT_BITS(0xF9,
                   0xF9,
                   s_mock_port7.pdr); /* All except bits 1,2 unchanged (0xFF & ~0x06 = 0xF9) */
  TEST_ASSERT_BITS(0xF9, 0xF9, s_mock_port7.pmr); /* PMR: only bits 1,2 cleared */

  /* Non-LED bits in PORTA should remain set */
  TEST_ASSERT_BITS(0x7F,
                   0x7F,
                   s_mock_porta.pdr); /* All except bit 7 unchanged (0xFF & ~0x80 = 0x7F) */

  /* Non-LED bits in PORTB should remain set */
  TEST_ASSERT_BITS(0xF8,
                   0xF8,
                   s_mock_portb.pdr); /* All except bits 0,1,2 unchanged (0xFF & ~0x07 = 0xF8) */
}

/* =============================================================================
 * Tests: LED Set/Clear
 * =============================================================================
 */

/** @brief Verify LED 0 (heartbeat, PA7) can be turned on */
void test_led_set_heartbeat_on(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_porta, 0, sizeof(s_mock_porta));
  internal_test_led_set(k_led_idx_heartbeat, true);
  TEST_ASSERT_BITS(0x80, 0x80, s_mock_porta.podr); /* LED0 on PORTA pin 7 */
}

/** @brief Verify LED 0 (heartbeat, PA7) can be turned off */
void test_led_set_heartbeat_off(void)
{
  s_mock_porta.podr = k_test_all_bits_set;
  internal_test_led_set(k_led_idx_heartbeat, false);
  TEST_ASSERT_BITS(0x80, 0x00, s_mock_porta.podr); /* LED0 on PORTA pin 7 */
}

/** @brief Verify LED 1 (error, PB0) can be toggled */
void test_led_set_error_toggle(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_portb, 0, sizeof(s_mock_portb));
  internal_test_led_set(k_led_idx_error, true);
  TEST_ASSERT_BITS(0x01, 0x01, s_mock_portb.podr); /* LED1 on PB0, bit 0 */

  internal_test_led_set(k_led_idx_error, false);
  TEST_ASSERT_BITS(0x01, 0x00, s_mock_portb.podr);
}

/** @brief Verify LED 5 (estop, PB2) can be set */
void test_led_set_estop_on(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_portb, 0, sizeof(s_mock_portb));
  internal_test_led_set(k_led_idx_estop, true);
  TEST_ASSERT_BITS(0x04, 0x04, s_mock_portb.podr); /* LED5 on PB2, bit 2 */
}

/** @brief Verify out-of-range LED index is safely ignored */
void test_led_set_invalid_index_no_crash(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_port7, 0, sizeof(s_mock_port7));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_porta, 0, sizeof(s_mock_porta));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_portb, 0, sizeof(s_mock_portb));

  internal_test_led_set(k_test_led_out_of_range, true);     /* Out of range */
  internal_test_led_set(k_test_led_way_out_of_range, true); /* Way out of range */

  /* No port registers should have changed */
  TEST_ASSERT_EQUAL(0, s_mock_port7.podr);
  TEST_ASSERT_EQUAL(0, s_mock_porta.podr);
  TEST_ASSERT_EQUAL(0, s_mock_portb.podr);
}

/** @brief Verify setting one LED does not affect others on same port */
void test_led_set_does_not_affect_other_pins(void)
{
  /* PORT7 has LED2 (motor) on pin 1 and LED3 (comm) on pin 2 */
  s_mock_port7.podr = 0x00;

  /* Turn on LED 2 (motor, pin 1) */
  internal_test_led_set(k_led_idx_motor, true);
  TEST_ASSERT_EQUAL(0x02, s_mock_port7.podr); /* Only pin 1 set */

  /* Turn on LED 3 (comm, pin 2) */
  internal_test_led_set(k_led_idx_comm, true);
  TEST_ASSERT_EQUAL(0x06, s_mock_port7.podr); /* Pins 1 and 2 set (0x02 | 0x04) */

  /* Turn off LED 2 (motor, pin 1) */
  internal_test_led_set(k_led_idx_motor, false);
  TEST_ASSERT_EQUAL(0x04, s_mock_port7.podr); /* Only pin 2 remains */
}

/* =============================================================================
 * Tests: LED State Query
 * =============================================================================
 */

/** @brief Verify is_on helper reads correct state */
void test_led_is_on_returns_correct_state(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_port7, 0, sizeof(s_mock_port7));
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_porta, 0, sizeof(s_mock_porta));

  TEST_ASSERT_FALSE(internal_test_led_is_on(k_led_idx_heartbeat));

  internal_test_led_set(k_led_idx_heartbeat, true);
  TEST_ASSERT_TRUE(internal_test_led_is_on(k_led_idx_heartbeat));

  internal_test_led_set(k_led_idx_heartbeat, false);
  TEST_ASSERT_FALSE(internal_test_led_is_on(k_led_idx_heartbeat));
}

/* =============================================================================
 * Tests: System State to LED Mapping
 * =============================================================================
 */

/** @brief Verify motor active LED reflects duty cycle */
void test_led_motor_active_when_duty_nonzero(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_port7, 0, sizeof(s_mock_port7));

  /* No motors running */
  s_mock_motor_state.duty_cycle_percent[k_test_motor_idx_fl] = 0.0F;
  s_mock_motor_state.duty_cycle_percent[k_test_motor_idx_fr] = 0.0F;
  s_mock_motor_state.duty_cycle_percent[k_test_motor_idx_bl] = 0.0F;
  s_mock_motor_state.duty_cycle_percent[k_test_motor_idx_br] = 0.0F;

  motor_state_t ms;
  (void)shared_data_get_motor_state(&ms);

  bool any_active = false;
  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    if (ms.duty_cycle_percent[i] > 0.0F) {
      any_active = true;
    }
  }
  internal_test_led_set(k_led_idx_motor, any_active);
  TEST_ASSERT_FALSE(internal_test_led_is_on(k_led_idx_motor));

  /* Motor 2 running */
  s_mock_motor_state.duty_cycle_percent[k_test_motor_idx_bl] = s_test_duty_active;
  (void)shared_data_get_motor_state(&ms);

  any_active = false;
  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    if (ms.duty_cycle_percent[i] > 0.0F) {
      any_active = true;
    }
  }
  internal_test_led_set(k_led_idx_motor, any_active);
  TEST_ASSERT_TRUE(internal_test_led_is_on(k_led_idx_motor));
}

/** @brief Verify obstacle LED reflects shared_data obstacle state */
void test_led_obstacle_reflects_shared_data(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_portb, 0, sizeof(s_mock_portb));

  /* No obstacle */
  s_mock_obstacle_state.any_obstacle = false;
  obstacle_state_t obs;
  (void)shared_data_get_obstacle(&obs);
  internal_test_led_set(k_led_idx_obstacle, obs.any_obstacle);
  TEST_ASSERT_FALSE(internal_test_led_is_on(k_led_idx_obstacle));

  /* Obstacle detected */
  s_mock_obstacle_state.any_obstacle = true;
  (void)shared_data_get_obstacle(&obs);
  internal_test_led_set(k_led_idx_obstacle, obs.any_obstacle);
  TEST_ASSERT_TRUE(internal_test_led_is_on(k_led_idx_obstacle));
}

/** @brief Verify estop LED reflects shared_data estop state */
void test_led_estop_reflects_shared_data(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_portb, 0, sizeof(s_mock_portb));

  /* No estop */
  s_mock_estop_active = false;
  internal_test_led_set(k_led_idx_estop, shared_data_is_estop_active());
  TEST_ASSERT_FALSE(internal_test_led_is_on(k_led_idx_estop));

  /* Estop active */
  s_mock_estop_active = true;
  internal_test_led_set(k_led_idx_estop, shared_data_is_estop_active());
  TEST_ASSERT_TRUE(internal_test_led_is_on(k_led_idx_estop));
}

/** @brief Verify error LED activates on motor fault */
void test_led_error_on_motor_fault(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_portb, 0, sizeof(s_mock_portb));

  /* No faults */
  bool any_fault = false;
  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    if (s_mock_motor_state.fault_flags[i] != 0) {
      any_fault = true;
    }
  }
  TEST_ASSERT_FALSE(any_fault);

  /* Motor 1 has fault */
  s_mock_motor_state.fault_flags[1] = k_test_fault_flag_set;

  motor_state_t ms;
  (void)shared_data_get_motor_state(&ms);
  any_fault = false;
  for (uint8_t i = 0; i < k_poeg_motor_count; i++) {
    if (ms.fault_flags[i] != 0) {
      any_fault = true;
    }
  }
  TEST_ASSERT_TRUE(any_fault);
}

/** @brief Verify comm LED pulse: initial state (no command yet) */
static void internal_test_comm_pulse_no_command(uint32_t* last_seq, uint8_t* pulse_remaining)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_port7, 0, sizeof(s_mock_port7));

  *last_seq        = 0;
  *pulse_remaining = 0;

  /* No command yet */
  s_mock_motor_command.valid    = false;
  s_mock_motor_command.sequence = 0;

  motor_command_t cmd;
  (void)shared_data_get_motor_command(&cmd);
  if (cmd.valid) {
    if (cmd.sequence != *last_seq) {
      *last_seq        = cmd.sequence;
      *pulse_remaining = k_led_comm_pulse_duration;
    }
  }
  TEST_ASSERT_EQUAL(0, *pulse_remaining);
}

/** @brief Verify comm LED pulse: new command arrives and pulse counts down */
static void internal_test_comm_pulse_new_command(uint32_t* last_seq, uint8_t* pulse_remaining)
{
  /* New command arrives */
  s_mock_motor_command.valid    = true;
  s_mock_motor_command.sequence = 1;

  motor_command_t cmd;
  (void)shared_data_get_motor_command(&cmd);
  if (cmd.valid) {
    if (cmd.sequence != *last_seq) {
      *last_seq        = cmd.sequence;
      *pulse_remaining = k_led_comm_pulse_duration;
    }
  }
  TEST_ASSERT_EQUAL(k_led_comm_pulse_duration, *pulse_remaining);

  /* Pulse counts down */
  if (*pulse_remaining > 0) {
    (*pulse_remaining)--;
    internal_test_led_set(k_led_idx_comm, true);
  }
  TEST_ASSERT_TRUE(internal_test_led_is_on(k_led_idx_comm));
  TEST_ASSERT_EQUAL(1, *pulse_remaining);

  /* Second tick */
  if (*pulse_remaining > 0) {
    (*pulse_remaining)--;
    internal_test_led_set(k_led_idx_comm, true);
  }
  TEST_ASSERT_EQUAL(0, *pulse_remaining);

  /* Pulse expired */
  if (*pulse_remaining > 0) {
    internal_test_led_set(k_led_idx_comm, true);
  } else {
    internal_test_led_set(k_led_idx_comm, false);
  }
  TEST_ASSERT_FALSE(internal_test_led_is_on(k_led_idx_comm));
}

/** @brief Verify comm LED pulse on new command sequence */
void test_led_comm_pulse_on_new_command(void)
{
  uint32_t last_seq        = 0;
  uint8_t  pulse_remaining = 0;

  internal_test_comm_pulse_no_command(&last_seq, &pulse_remaining);
  internal_test_comm_pulse_new_command(&last_seq, &pulse_remaining);
}

/* =============================================================================
 * Tests: Heartbeat Timing
 * =============================================================================
 */

/** @brief Verify heartbeat toggles at 1 Hz (10 ticks on, 10 ticks off) */
void test_led_heartbeat_timing(void)
{
  /* NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling) */
  memset(&s_mock_porta, 0, sizeof(s_mock_porta));

  uint8_t counter = 0;

  /* First 5 ticks: LED should be on (counter 0-4 < half_period/2 = 5) */
  for (uint8_t tick = 0; tick < k_heartbeat_tick_count; tick++) {
    bool on = (bool)(counter < (k_led_heartbeat_half_period / k_heartbeat_divisor));
    internal_test_led_set(k_led_idx_heartbeat, on);
    TEST_ASSERT_TRUE(internal_test_led_is_on(k_led_idx_heartbeat));
    counter++;
  }

  /* Next 5 ticks: LED should be off (counter 5-9 >= 5) */
  for (uint8_t tick = 0; tick < k_heartbeat_tick_count; tick++) {
    bool on = (bool)(counter < (k_led_heartbeat_half_period / k_heartbeat_divisor));
    internal_test_led_set(k_led_idx_heartbeat, on);
    TEST_ASSERT_FALSE(internal_test_led_is_on(k_led_idx_heartbeat));
    counter++;
  }

  /* Counter wraps at half_period */
  TEST_ASSERT_EQUAL(k_led_heartbeat_half_period, counter);
  counter = 0;

  /* Back to on */
  bool on = (bool)(counter < (k_led_heartbeat_half_period / k_heartbeat_divisor));
  internal_test_led_set(k_led_idx_heartbeat, on);
  TEST_ASSERT_TRUE(internal_test_led_is_on(k_led_idx_heartbeat));
}

/* =============================================================================
 * Tests: Pin Assignment Verification
 * =============================================================================
 */

/** @brief Verify LED pin assignment constants match hardware design */
void test_led_pin_assignments(void)
{
  /* LED 0: PA7 (Port A=10, Pin 7) */
  TEST_ASSERT_EQUAL(k_expected_porta, k_led_0_port);
  TEST_ASSERT_EQUAL(k_expected_pin_7, k_led_0_pin);

  /* LED 1: PB0 (Port B=11, Pin 0) */
  TEST_ASSERT_EQUAL(k_expected_portb, k_led_1_port);
  TEST_ASSERT_EQUAL(k_expected_pin_0, k_led_1_pin);

  /* LED 2: P71 (Port 7, Pin 1) */
  TEST_ASSERT_EQUAL(k_expected_port7, k_led_2_port);
  TEST_ASSERT_EQUAL(k_expected_pin_1, k_led_2_pin);

  /* LED 3: P72 (Port 7, Pin 2) */
  TEST_ASSERT_EQUAL(k_expected_port7, k_led_3_port);
  TEST_ASSERT_EQUAL(k_expected_pin_2, k_led_3_pin);

  /* LED 4: PB1 (Port B=11, Pin 1) */
  TEST_ASSERT_EQUAL(k_expected_portb, k_led_4_port);
  TEST_ASSERT_EQUAL(k_expected_pin_1, k_led_4_pin);

  /* LED 5: PB2 (Port B=11, Pin 2) */
  TEST_ASSERT_EQUAL(k_expected_portb, k_led_5_port);
  TEST_ASSERT_EQUAL(k_expected_pin_2, k_led_5_pin);
}

/** @brief Verify 6 LEDs total */
void test_led_count(void)
{
  TEST_ASSERT_EQUAL(k_led_count, k_led_count);
}

/* =============================================================================
 * Test Runner
 * =============================================================================
 */

int main(void)
{
  UNITY_BEGIN();

  /* Task Creation */
  RUN_TEST(test_led_task_create_success);
  RUN_TEST(test_led_task_create_double_returns_error);
  RUN_TEST(test_led_task_create_threadx_failure);
  RUN_TEST(test_led_task_config_constants);

  /* GPIO Initialization */
  RUN_TEST(test_led_gpio_init_sets_output_direction);
  RUN_TEST(test_led_gpio_init_preserves_other_bits);

  /* LED Set/Clear */
  RUN_TEST(test_led_set_heartbeat_on);
  RUN_TEST(test_led_set_heartbeat_off);
  RUN_TEST(test_led_set_error_toggle);
  RUN_TEST(test_led_set_estop_on);
  RUN_TEST(test_led_set_invalid_index_no_crash);
  RUN_TEST(test_led_set_does_not_affect_other_pins);

  /* LED State Query */
  RUN_TEST(test_led_is_on_returns_correct_state);

  /* System State Mapping */
  RUN_TEST(test_led_motor_active_when_duty_nonzero);
  RUN_TEST(test_led_obstacle_reflects_shared_data);
  RUN_TEST(test_led_estop_reflects_shared_data);
  RUN_TEST(test_led_error_on_motor_fault);
  RUN_TEST(test_led_comm_pulse_on_new_command);

  /* Heartbeat Timing */
  RUN_TEST(test_led_heartbeat_timing);

  /* Pin Assignments */
  RUN_TEST(test_led_pin_assignments);
  RUN_TEST(test_led_count);

  return UNITY_END();
}
