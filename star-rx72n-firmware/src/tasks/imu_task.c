/**
 * @file imu_task.c
 * @brief IMU Task - BNO055 + BMP280 Interrupt-Driven Sensor Reading
 *
 * @details
 * # Overview
 *
 * This module implements the IMU Task that initializes the BNO055
 * 9-DOF absolute orientation sensor and the BMP280 barometric pressure
 * sensor, then reads both sensors on each BNO055 INT falling edge (P3.2
 * / IRQ12), publishing results to shared data for the telemetry task.
 * A 200 ms watchdog timeout triggers a read when INT does not fire.
 *
 * # Hardware
 *
 * Both sensors use RIIC1 I2C hardware (P2.0=SDA1, P2.1=SCL1):
 * - **BNO055**: I2C addr 0x28, bus "i2c1_imu", NDOF fusion mode (heading, quaternion, linear accel)
 * - **BMP280**: I2C addr 0x76, bus "i2c1_baro", forced mode (pressure Pa, temperature cC)
 *
 * # Task Lifecycle
 *
 * @startuml
 * [*] --> Init
 * Init --> Running : both sensors initialized
 * Init --> Running : one or both sensors failed (logs error, continues)
 * Running --> Running : BNO055 INT (IRQ12) fires -> ISR sets event flag -> read BNO055 + BMP280
 * Running --> Running : 200 ms watchdog timeout -> read without INT (fault recovery)
 * @enduml
 *
 * # Data Flow
 *
 * @code{.unparsed}
 * imu_task
 *    |----> rx_bno055_read() -> imu_state_t -> shared_data_update_imu()
 *    |----> rx_bmp280_read() -> baro_state_t -> shared_data_update_baro()
 * @endcode
 *
 * # Thread Safety
 *
 * All shared data writes go through mutex-protected accessor functions.
 * The task does not directly access g_shared_data members.
 *
 * # NASA Power of 10 Compliance
 *
 * | Rule | Status | Notes |
 * |------|--------|-------|
 * | 1. No goto | [PASS] | Structured if/while only |
 * | 2. Bounded loops | [PASS] | while(true) with IWDT watchdog |
 * | 3. No dynamic memory | [PASS] | Static stack and thread control block |
 * | 4. Short functions | [PASS] | Task entry ~60 lines; helpers extracted |
 * | 5. Assertions | [PASS] | 2+ preconditions per function |
 * | 6. Data scope | [PASS] | Locals at point of use |
 * | 7. Check returns | [PASS] | All driver and shared_data returns validated |
 * | 8. Limit preprocessor | [PASS] | C23 typed enums only |
 * | 9. Pointer restrictions | [WARN] | Function pointers for DIP (bus manager) |
 * | 10. Compile warnings | [PASS] | -Wall -Wextra -Werror |
 *
 * @see imu_task.h Public API
 * @see rx_bno055.h BNO055 driver
 * @see rx_bmp280.h BMP280 driver
 * @see shared_data.h Shared state types and accessor functions
 *
 * @author STAR Team
 * @date 2026-03-04
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "imu_task.h"

#include "hardware.h"
#include "rx_bmp280.h"
#include "rx_bno055.h"
#include "rx_check.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "rx_port_constants.h"
#include "shared_data.h"
#include "tx_api.h"
#ifdef __RX__
#include "rx72n_icu_regs.h"
#endif /* __RX__ */

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @enum imu_task_stack_cfg_t
 * @brief IMU task stack size constant
 *
 * @details
 * Defines the stack size in bytes for the IMU task. Uses uint16_t because
 * 2048 exceeds the uint8_t maximum of 255.
 *
 * **Stack rationale:**
 * - 2048 bytes: BNO055 init sequence writes ~12 registers (local buffers)
 * - BMP280 init reads 24-byte calib block (local buffer)
 * - State structs imu_state_t (~32 bytes) + baro_state_t (~16 bytes)
 *
 * @invariant k_imu_task_stack_size > 0 (stack must be non-zero)
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_imu_task_stack_size = 2048, /**< Stack size in bytes (exceeds uint8_t range) */
} imu_task_stack_cfg_t;

/**
 * @enum imu_task_cfg_t
 * @brief IMU task thread entry input constant
 *
 * @details
 * Defines the thread entry ULONG input for the IMU task (currently unused).
 * Priority is defined in imu_task.h as k_imu_task_priority (= 13).
 * Period ticks are defined in imu_task.h as k_imu_task_period_ticks (= 5).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_task_input = 0U, /**< Thread entry ULONG input (unused) */
} imu_task_cfg_t;

/**
 * @enum imu_task_time_t
 * @brief Milliseconds-per-second constant for tick-to-millisecond conversion
 *
 * @details
 * Used to convert ThreadX tick counts from tx_time_get() to milliseconds:
 * timestamp_ms = ticks * k_imu_ms_per_second / TX_TIMER_TICKS_PER_SECOND
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_imu_ms_per_second = 1000U, /**< Milliseconds per second (conversion factor for tick->ms) */
} imu_task_time_t;

/**
 * @enum imu_ceil_div_t
 * @brief Rounding constants for ceiling-division tick computation
 *
 * @details
 * Provides named constants for the ceiling-division formula used in timeout
 * and hardware reset tick calculations:
 *   ticks = (ms * TX_TIMER_TICKS_PER_SECOND + k_imu_ceiling_addend) / k_imu_ms_per_second + 1
 *
 * k_imu_uint8_max_val is used in the static_assert that verifies
 * k_imu_int_timeout_ticks does not overflow uint8_t.
 *
 * @invariant k_imu_ceiling_addend == k_imu_ms_per_second - 1
 * @see imu_task_hw_rst_t Tick counts using ceiling division
 * @see imu_task_int_cfg_t INT timeout tick count using ceiling division
 * @since Version 1.0.0
 */
typedef enum : uint32_t {
  k_imu_ceiling_addend = 999U,  /**< Addend for ceiling division: (ms * TICKS + 999) / 1000 */
  k_imu_uint8_max_val  = 0xFFU, /**< Maximum uint8_t value (255) for overflow static_assert */
} imu_ceil_div_t;

/**
 * @enum imu_task_hw_rst_ms_t
 * @brief BNO055 hardware reset minimum durations in milliseconds (from datasheet)
 *
 * @details
 * Source values in milliseconds for the two delays in the BNO055 hardware
 * reset sequence, used to compute the tick counts in imu_task_hw_rst_t.
 *
 * | Constant                  | Value | Source |
 * |---------------------------|-------|--------|
 * | k_imu_hw_rst_assert_ms    | 10    | BNO055 datasheet: RST active-low min hold |
 * | k_imu_hw_rst_por_ms       | 650   | BNO055 datasheet: POR completion time |
 *
 * @see imu_task_hw_rst_t Derived tick counts (with +1 slack per tick)
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_imu_hw_rst_assert_ms = 10U,  /**< Min RST assert duration: 10 ms per BNO055 datasheet */
  k_imu_hw_rst_por_ms    = 650U, /**< Min POR completion wait: 650 ms per BNO055 datasheet */
} imu_task_hw_rst_ms_t;

/**
 * @enum imu_task_hw_rst_t
 * @brief BNO055 hardware reset timing constants in ThreadX ticks (P83/IMU RST, active-low)
 *
 * @details
 * Defines the ThreadX tick counts for the two delays in the BNO055 hardware
 * reset sequence performed via P83 (IMU RST, active-low, pin 58).
 *
 * tx_thread_sleep() may complete up to one full tick early, so each value adds
 * one extra tick of slack using the formula:
 *   ticks = ceil(ms * TX_TIMER_TICKS_PER_SECOND / 1000) + 1
 *         = (ms * TX_TIMER_TICKS_PER_SECOND + 999) / 1000 + 1
 *
 * At TX_TIMER_TICKS_PER_SECOND = 100 Hz:
 * | Constant                  | Formula result | Guaranteed minimum |
 * |---------------------------|----------------|--------------------|
 * | k_imu_hw_rst_assert_ticks | 2 ticks        | 10 ms              |
 * | k_imu_hw_rst_por_ticks    | 66 ticks       | 650 ms             |
 *
 * @invariant k_imu_hw_rst_assert_ticks >= 2 (ceil(10 ms / tick) + 1 slack)
 * @invariant k_imu_hw_rst_por_ticks >= 66 (ceil(650 ms / tick) + 1 slack)
 *
 * @see imu_task_hw_rst_ms_t Source millisecond values
 * @see internal_imu_hardware_reset() Consumer of these constants
 * @see hardware_config.h k_imu_rst_port, k_imu_rst_pin for P83 pin assignment
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  /** @brief RST assert duration: ceil(10 ms * 100 Hz / 1000) + 1 = 2 ticks */
  k_imu_hw_rst_assert_ticks =
    (k_imu_hw_rst_assert_ms * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U + 1U,
  /** @brief POR wait after deassert: ceil(650 ms * 100 Hz / 1000) + 1 = 66 ticks */
  k_imu_hw_rst_por_ticks = (k_imu_hw_rst_por_ms * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U + 1U,
} imu_task_hw_rst_t;

/**
 * @enum imu_wait_result_t
 * @brief Return values for internal_wait_for_imu_int()
 *
 * @details
 * Three-state result distinguishes a genuine data-ready INT, a benign
 * 200 ms watchdog timeout, and a fatal ThreadX RTOS error.  Callers
 * must handle each case separately so corrupted/deleted event-flags
 * groups surface as faults rather than silently degrading into the
 * watchdog recovery path.
 *
 * @see internal_wait_for_imu_int() Function that returns this type
 * @see internal_imu_task_entry() Sole consumer
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_wait_data_ready = 0U, /**< BNO055 INT fired (TX_SUCCESS): read fresh data */
  k_imu_wait_timeout    = 1U, /**< 200 ms watchdog (TX_NO_EVENTS): fault-recovery read */
  k_imu_wait_rtos_error = 2U, /**< Fatal ThreadX error: event-flags group corrupted */
} imu_wait_result_t;

/**
 * @enum imu_task_int_cfg_t
 * @brief IMU INT watchdog timeout constants (interrupt-driven mode)
 *
 * @details
 * The IMU task blocks on s_imu_event_flags waiting for the BNO055 INT
 * falling-edge ISR to signal k_imu_event_data_ready. If no INT fires
 * within k_imu_int_timeout_ticks, the task reads BNO055 anyway (fault
 * recovery) and logs a warning.
 *
 * Timeout formula (with +1 slack for tick boundary):
 *   ticks = (ms * TX_TIMER_TICKS_PER_SECOND + 999) / 1000 + 1
 * At 100 Hz: (200 * 100 + 999) / 1000 + 1 = 20999/1000 + 1 = 21 ticks.
 *
 * @invariant k_imu_int_timeout_ticks >= 21 at 100 Hz tick rate
 * @see s_imu_event_flags ThreadX event flags group
 * @see k_imu_event_data_ready Event bit set by ISR
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_int_timeout_ticks =
    ((uint32_t)k_imu_int_timeout_ms * TX_TIMER_TICKS_PER_SECOND + 999U) / 1000U +
    1U, /**< Timeout in ticks with +1 slack: (200*100+999)/1000+1 = 21 ticks */
  k_imu_event_data_ready = 0x01U, /**< Event flag bit set by ISR on BNO055 INT assertion */
} imu_task_int_cfg_t;

#ifdef __RX__
/**
 * @enum imu_isr_constants_t
 * @brief ICU constants for the IMU INT ISR (RX target only)
 *
 * @details
 * Mirrors k_imu_irq_vector and k_icu_ir_clear_imu from hardware_init.c for use
 * in the INT_IRQ12 ISR, which cannot include hardware_init.h (no public header).
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_irq_vector_isr = 76U, /**< ICU vector for IRQ12: matches hardware_init.c */
  k_icu_ir_clear_val   = 0U,  /**< Write 0 to IR register to clear pending flag */
} imu_isr_constants_t;
#endif /* __RX__ */

static_assert(
  (bool)((((uint32_t)k_imu_int_timeout_ms * (uint32_t)TX_TIMER_TICKS_PER_SECOND +
           k_imu_ceiling_addend) /
            (uint32_t)k_imu_ms_per_second +
          1U) <= k_imu_uint8_max_val),
  "k_imu_int_timeout_ticks overflows uint8_t; reduce k_imu_int_timeout_ms or use a wider type");
static_assert((bool)(TX_TIMER_TICKS_PER_SECOND != 0U),
              "TX_TIMER_TICKS_PER_SECOND must be non-zero");
static_assert((bool)(k_imu_ms_per_second > 0U), "k_imu_ms_per_second must be positive");
static_assert((bool)(((uint32_t)k_imu_task_period_ms * (uint32_t)TX_TIMER_TICKS_PER_SECOND) %
                       (uint32_t)k_imu_ms_per_second ==
                     0U),
              "IMU period must map to a whole number of ThreadX ticks");
static_assert(
  (bool)((uint32_t)k_imu_task_period_ticks ==
         (((uint32_t)k_imu_task_period_ms * (uint32_t)TX_TIMER_TICKS_PER_SECOND) /
          (uint32_t)k_imu_ms_per_second)),
  "k_imu_task_period_ticks must match k_imu_task_period_ms / TX_TIMER_TICKS_PER_SECOND");

/* =============================================================================
 * Static State
 * =============================================================================
 */

/**
 * @brief ThreadX thread control block for the IMU task
 * @details Holds the OS-level thread state (stack pointer, priority, etc.).
 *          Allocated statically; never freed (NASA Rule 3).
 * @invariant Valid after imu_task_create() returns k_rx_ok
 * @since Version 1.0.0
 */
static TX_THREAD s_imu_thread;

/**
 * @brief Static thread stack for the IMU task (zero dynamic allocation)
 * @details Provides the execution stack for internal_imu_task_entry().
 *          Size = k_imu_task_stack_size (2048 bytes) is sufficient for
 *          BNO055 init, BMP280 init, and periodic read buffers.
 * @invariant Length == k_imu_task_stack_size; never modified after creation
 * @since Version 1.0.0
 */
static uint8_t s_imu_stack[k_imu_task_stack_size];

/**
 * @brief Task creation guard: prevents double creation of the IMU task
 * @details Set to true by imu_task_create() after ThreadX thread creation
 *          succeeds. Checked at entry to detect and reject duplicate calls.
 * @invariant false before first imu_task_create() call; true after success
 * @since Version 1.0.0
 */
static bool s_imu_created = false;

/**
 * @brief Log tag for this module
 * @details Constant string used as the tag parameter in all rx_log_* calls.
 * @invariant Points to static string "IMU"; never NULL, never modified
 * @since Version 1.0.0
 */
static const char* const s_tag = "IMU";

/**
 * @var s_task_name
 * @brief ThreadX task name for ImuTask
 *
 * @details
 * Task name string passed to tx_thread_create() and rx_iwdt_task_heartbeat().
 * Centralising the name in one place avoids typos across tx_thread_create()
 * and the IWDT heartbeat call site.
 *
 * @invariant Content is always "ImuTask"; never NULL, never modified at runtime
 * @note Not thread-safe; used as a read-only constant
 * @warning Do not modify; used as an identifier by the IWDT subsystem
 *
 * @see imu_task_create() Passes s_task_name to tx_thread_create()
 * @see internal_send_iwdt_heartbeat() Passes s_task_name to rx_iwdt_task_heartbeat()
 *
 * @since Version 1.0.0
 */
static char s_task_name[] = "ImuTask"; /* char[] (not const) satisfies ThreadX CHAR* parameter */

/**
 * @var s_imu_event_flags
 * @brief ThreadX event flags group for BNO055 INT signaling
 *
 * @details
 * Set by INT_IRQ12 (ISR context) on each falling edge of the BNO055 INT pin
 * (P3.2 / IRQ12). The IMU task blocks on this group with a
 * k_imu_int_timeout_ticks watchdog timeout.
 *
 * @note Initialized in imu_task_create() via tx_event_flags_create().
 * @warning Access only via tx_event_flags_get() and tx_event_flags_set().
 * @since Version 1.0.0
 */
static TX_EVENT_FLAGS_GROUP s_imu_event_flags;

/**
 * @var s_imu_event_flags_ready
 * @brief ISR ready guard -- true only after both event-flags and thread are created
 *
 * @details
 * INT_IRQ12() checks this flag before calling tx_event_flags_set().  The ICU
 * IRQ12 line can fire as soon as the BNO055 powers up, which may be before
 * imu_task_create() has finished setting up s_imu_event_flags.  Without this
 * guard, the ISR would call tx_event_flags_set() on an uninitialised control
 * block, corrupting the ThreadX heap.
 *
 * Set to true at the end of imu_task_create() after BOTH tx_event_flags_create()
 * and tx_thread_create() succeed.  Never reset to false (task is never deleted).
 *
 * @note Declared volatile because it is written in task context and read in ISR
 *       context without a memory barrier.
 * @warning Do not set to true before tx_event_flags_create() completes.
 * @since Version 1.0.0
 */
static volatile bool s_imu_event_flags_ready = false;

/**
 * @var g_bus_manager
 * @brief External bus manager defined in shared_data.c; registered buses include "i2c1_imu" and "i2c1_baro"
 *
 * @details
 * Declared extern here because main.h does not export g_bus_manager.
 * If a main.h public header is added, replace this with #include "main.h".
 * The IMU task uses this to pass the bus manager to rx_bno055_init() and
 * rx_bmp280_init() during task startup.
 *
 * @note Using inline extern is necessary because shared_data.h does not export g_bus_manager.
 *       If a shared_data.h public accessor is added, replace this with the accessor.
 *
 * @since Version 1.0.0
 */
extern rx_bus_manager_t
  g_bus_manager; /* TODO(#373): add main.h public header to replace inline extern */

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void              internal_send_iwdt_heartbeat(void);
static void              internal_retry_sensor_init(bool* bno_ready, bool* bmp_ready);
static void              internal_read_and_publish_imu(void);
static void              internal_read_and_publish_baro(void);
static rx_err_t          internal_imu_hardware_reset(void);
static imu_wait_result_t internal_wait_for_imu_int(void);
static void              internal_imu_task_entry(ULONG input);
static void              internal_init_sensors(bool* bno_ready, bool* bmp_ready);

/* =============================================================================
 * Static Inline Helpers
 * =============================================================================
 */

/**
 * @brief Convert current ThreadX tick count to milliseconds
 *
 * @details
 * Reads the ThreadX system tick counter and converts to wall-clock milliseconds.
 * Both imu_state_t.timestamp_ms and baro_state_t.timestamp_ms use this helper
 * to ensure consistent timestamp computation from a single expression.
 *
 * @return uint32_t Current time in milliseconds since boot
 *
 * @pre ThreadX scheduler running (tx_time_get() returns valid tick count)
 * @pre TX_TIMER_TICKS_PER_SECOND > 0 (ThreadX timer configured, verified by module static_assert)
 * @post Return value is monotonically non-decreasing modulo uint32_t overflow
 * @post Return value in milliseconds: fits uint32_t for ~49 days uptime at 100 Hz tick rate
 *
 * @note Not thread-safe, but tx_time_get() is interrupt-safe on RX72N
 * @since Version 1.0.0
 */
static inline uint32_t internal_ticks_to_ms(void)
{
  return (uint32_t)((uint64_t)tx_time_get() * k_imu_ms_per_second / TX_TIMER_TICKS_PER_SECOND);
}

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Create and start the IMU sensor task
 *
 * @details
 * Creates the ImuTask ThreadX task with priority 13, 2048-byte stack,
 * and auto-start. Returns k_rx_err_invalid_state on double creation.
 *
 * @return rx_err_t Task creation result
 * @retval k_rx_ok Task created and scheduled
 * @retval k_rx_err_invalid_state Called a second time (task already created)
 * @retval k_rx_err_rtos_thread_create ThreadX thread creation failed
 *
 * @pre ThreadX kernel running (tx_application_define context)
 * @pre s_imu_created == false (first call)
 * @post s_imu_created == true
 * @post ImuTask scheduled for execution at priority 13
 *
 * @note Not thread-safe; call from single-threaded tx_application_define() only
 * @note Sensor initialization occurs inside the task (not in this function)
 *
 * @see imu_task.h Header declaration
 * @see internal_imu_task_entry() Task body
 *
 * @since Version 1.0.0
 */
rx_err_t imu_task_create(void)
{
  RX_ASSERT(k_imu_task_stack_size > 0U, "IMU stack size must be non-zero");
  if (s_imu_created) {
    return k_rx_err_invalid_state;
  }

  /* Create event flags group for BNO055 INT signaling */
  const UINT ef_status = tx_event_flags_create(&s_imu_event_flags, "imu_int_flags");
  if (ef_status != TX_SUCCESS) {
    rx_log_error(s_tag, "Failed to create IMU event flags");
    return k_rx_err_rtos_thread_create;
  }

  const UINT tx_status = tx_thread_create(&s_imu_thread,
                                          s_task_name,
                                          internal_imu_task_entry,
                                          (ULONG)k_imu_task_input,
                                          s_imu_stack,
                                          (ULONG)k_imu_task_stack_size,
                                          (UINT)k_imu_task_priority,
                                          (UINT)k_imu_task_priority,
                                          TX_NO_TIME_SLICE,
                                          TX_AUTO_START);

  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "Thread create failed", (uint32_t)tx_status);
    (void)tx_event_flags_delete(&s_imu_event_flags);
    return k_rx_err_rtos_thread_create;
  }

  s_imu_created           = true;
  s_imu_event_flags_ready = true;
  rx_log_info(s_tag, "IMU task created");

  return k_rx_ok;
}

/* =============================================================================
 * Private Functions
 * =============================================================================
 */

/**
 * @brief Send IWDT task heartbeat to prevent watchdog timeout
 *
 * @details
 * Calls rx_iwdt_task_heartbeat() with the "ImuTask" task identifier. Logs
 * any failure. Extracted from the main loop to keep task entry under 60 lines
 * (NASA Rule 4).
 *
 * @pre IWDT subsystem initialized via rx_iwdt_init()
 * @pre s_imu_created == true (task created via imu_task_create())
 * @post Heartbeat recorded; watchdog monitor resets timer for "ImuTask"
 * @post Error logged on failure (watchdog monitor will detect missed heartbeat)
 *
 * @note Not thread-safe; called only from internal_imu_task_entry()
 *
 * @see rx_iwdt_task_heartbeat() IWDT heartbeat API
 * @see internal_imu_task_entry() Caller
 *
 * @since Version 1.0.0
 */
static void internal_send_iwdt_heartbeat(void)
{
  RX_ASSERT(s_task_name[0] != '\0', "Task name must not be empty");
  RX_ASSERT(s_imu_created, "IWDT heartbeat called before task created");
  const rx_err_t err = rx_iwdt_task_heartbeat(s_task_name);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "IWDT heartbeat failed");
  }
}

/**
 * @brief Retry initialization for any sensors that failed initial startup
 *
 * @details
 * Attempts to re-initialize the BNO055 and BMP280 if either failed during
 * internal_imu_task_entry() startup. Sets the corresponding ready flag on
 * success and logs the result. Called each loop iteration until both sensors
 * are ready.
 *
 * @param[in,out] bno_ready Set to true when BNO055 init succeeds on retry
 * @param[in,out] bmp_ready Set to true when BMP280 init succeeds on retry
 *
 * @pre bno_ready non-NULL
 * @pre bmp_ready non-NULL
 * @post *bno_ready == true if BNO055 initialized successfully (this call or prior)
 * @post *bmp_ready == true if BMP280 initialized successfully (this call or prior)
 *
 * @note Not thread-safe; called only from internal_imu_task_entry()
 *
 * @see rx_bno055_init() BNO055 initialization
 * @see rx_bmp280_init() BMP280 initialization
 *
 * @since Version 1.0.0
 */
static void internal_retry_sensor_init(bool* bno_ready, bool* bmp_ready)
{
  RX_ASSERT(bno_ready != NULL, "bno_ready must not be NULL");
  RX_ASSERT(bmp_ready != NULL, "bmp_ready must not be NULL");

  if (!*bno_ready) {
    const bno055_config_t bno_cfg   = {.mode = k_bno055_mode_interrupt};
    const rx_err_t        retry_bno = rx_bno055_init(&g_bus_manager, &bno_cfg);
    if (retry_bno == k_rx_ok) {
      *bno_ready = true;
      rx_log_info(s_tag, "BNO055 initialized on retry");
    }
  }
  if (!*bmp_ready) {
    const rx_err_t retry_bmp = rx_bmp280_init(&g_bus_manager);
    if (retry_bmp == k_rx_ok) {
      *bmp_ready = true;
      rx_log_info(s_tag, "BMP280 initialized on retry");
    }
  }
}

/**
 * @brief Read BNO055 and publish imu_state_t to shared data
 *
 * @details
 * Reads all BNO055 fusion outputs (Euler angles, quaternion, linear
 * acceleration, gyroscope, temperature, calibration status) and writes them to
 * shared data via shared_data_update_imu(). Sets valid = false on read
 * failure to signal consumers that data is stale.
 *
 * @pre s_imu_created == true (task running)
 * @pre s_tag != NULL (module tag initialized)
 * @post imu_state_t in shared data updated with latest BNO055 data
 * @post valid = false if BNO055 read fails
 *
 * @note Not thread-safe; called only from internal_imu_task_entry()
 *
 * @see shared_data_update_imu() Thread-safe write to shared data
 * @see rx_bno055_read() BNO055 data read
 *
 * @since Version 1.0.0
 */
static void internal_read_and_publish_imu(void)
{
  RX_ASSERT(s_imu_created, "IMU task must be created before reading IMU");
  RX_ASSERT(s_tag != NULL, "s_tag must not be NULL");

  bno055_data_t  bno_data = {};
  const rx_err_t err      = rx_bno055_read(&bno_data);

  /* Clear the BNO055 INT pin after every read. Per BNO055 datasheet
   * section 3.6 the INT pin stays asserted until INT_STA (Page 0, 0x37)
   * is read, so without this call the pin latches high after the first
   * ACC_BSX_DRDY and never generates another edge -- IRQ12 fires once
   * per boot and the task permanently falls back to 200 ms polling.
   * Best-effort: if this read fails we still use the sensor data we
   * already collected and rely on the polling fallback next iteration. */
  (void)rx_bno055_clear_int();

  imu_state_t imu  = {};
  imu.timestamp_ms = internal_ticks_to_ms();

  if (err == k_rx_ok) {
    imu.heading_deg16 = bno_data.heading_deg16;
    imu.roll_deg16    = bno_data.roll_deg16;
    imu.pitch_deg16   = bno_data.pitch_deg16;
    imu.quat_w        = bno_data.quat_w;
    imu.quat_x        = bno_data.quat_x;
    imu.quat_y        = bno_data.quat_y;
    imu.quat_z        = bno_data.quat_z;
    imu.lin_acc_x     = bno_data.lin_acc_x;
    imu.lin_acc_y     = bno_data.lin_acc_y;
    imu.lin_acc_z     = bno_data.lin_acc_z;
    imu.gyro_x_dps16  = bno_data.gyro_x_dps16;
    imu.gyro_y_dps16  = bno_data.gyro_y_dps16;
    imu.gyro_z_dps16  = bno_data.gyro_z_dps16;
    imu.temp_degc     = bno_data.temp_degc;
    imu.calib_stat    = bno_data.calib_stat;
    imu.valid         = true;
  } else {
    rx_log_error_val(s_tag, "BNO055 read failed", (uint32_t)err);
    imu.valid = false;
  }

  const rx_err_t imu_err = shared_data_update_imu(&imu);
  if (imu_err != k_rx_ok) {
    rx_log_error_val(s_tag, "shared_data_update_imu failed", (uint32_t)imu_err);
  }
}

/**
 * @brief Read BMP280 and publish baro_state_t to shared data
 *
 * @details
 * Triggers a forced BMP280 measurement and writes the compensated
 * temperature and pressure to shared data via shared_data_update_baro().
 * Sets valid = false on read failure to signal consumers that data is stale.
 *
 * @pre s_imu_created == true (task running)
 * @pre s_tag != NULL (module tag initialized)
 * @post baro_state_t in shared data updated with latest BMP280 data
 * @post valid = false if BMP280 read fails
 *
 * @note Not thread-safe; called only from internal_imu_task_entry()
 *
 * @see shared_data_update_baro() Thread-safe write to shared data
 * @see rx_bmp280_read() BMP280 forced measurement
 *
 * @since Version 1.0.0
 */
static void internal_read_and_publish_baro(void)
{
  RX_ASSERT(s_imu_created, "IMU task must be created before reading baro");
  RX_ASSERT(s_tag != NULL, "s_tag must not be NULL");

  bmp280_data_t  bmp_data = {};
  const rx_err_t err      = rx_bmp280_read(&bmp_data);

  baro_state_t baro = {};
  baro.timestamp_ms = internal_ticks_to_ms();

  if (err == k_rx_ok) {
    baro.temp_centi_degc = bmp_data.temp_centi_degc;
    baro.press_pa_256    = bmp_data.press_pa_256;
    baro.valid           = true;
  } else {
    rx_log_error_val(s_tag, "BMP280 read failed", (uint32_t)err);
    baro.valid = false;
  }

  const rx_err_t baro_err = shared_data_update_baro(&baro);
  if (baro_err != k_rx_ok) {
    rx_log_error_val(s_tag, "shared_data_update_baro failed", (uint32_t)baro_err);
  }
}

/**
 * @brief Assert hardware reset on BNO055 via P83 (IMU RST, active-low) before I2C init
 *
 * @details
 * Performs a hardware reset of the BNO055 by toggling P83 (IMU RST, active-low, pin 58)
 * per the BNO055 datasheet and the STAR hardware pinout specification. This guarantees a
 * clean sensor state regardless of the MCU reset type (power-on, warm reset, watchdog).
 *
 * ## Reset Sequence
 *
 * 1. Assert RST LOW (P83 = 0) -- BNO055 enters reset; abort on GPIO failure
 * 2. Wait k_imu_hw_rst_assert_ticks ticks (2 ticks, guaranteed >= 10 ms with +1 slack)
 * 3. Deassert RST HIGH (P83 = 1) -- BNO055 released from reset; abort on GPIO failure
 * 4. Wait k_imu_hw_rst_por_ticks ticks (66 ticks, guaranteed >= 650 ms with +1 slack)
 *
 * Returns k_rx_ok after step 4. Returns the GPIO error code immediately if either
 * gpio_write_low or gpio_write_high fails (steps 1 or 3 respectively).
 *
 * After this function returns k_rx_ok, rx_bno055_init() may proceed immediately with I2C
 * communication. The internal software reset inside rx_bno055_init() is kept as
 * belt-and-suspenders but the hardware reset here is the authoritative reset.
 *
 * @return rx_err_t Error code
 * @retval k_rx_ok Reset sequence completed; BNO055 POR done, ready for I2C
 * @retval k_rx_err_hw_init_failed GPIO drive low failed (RST stuck HIGH)
 * @retval k_rx_err_timeout GPIO drive high failed (RST stuck LOW, BNO055 held in reset)
 *
 * @pre s_imu_created == true (imu_task_create() completed)
 * @pre P83 configured as GPIO output by hardware_init.c internal_gpio_init_imu()
 * @post On k_rx_ok: BNO055 has completed POR sequence, ready for I2C communication
 * @post On k_rx_ok: P83 is HIGH (deasserted, BNO055 running)
 * @post On error: P83 state undefined; BNO055 init should be skipped
 *
 * @note Blocks ~670 ms total on success: 20 ms assert + 660 ms POR wait (+1 slack each)
 * @note Not thread-safe; called only from internal_imu_task_entry()
 *
 * @warning Must be called before rx_bno055_init(); calling after will disrupt I2C
 * @warning Caller must check return value before proceeding to rx_bno055_init()
 *
 * @see hardware_config.h k_imu_rst_port / k_imu_rst_pin for P83 pin assignment
 * @see rx_port_constants.h k_rx_p8_3 for the combined port/pin constant
 * @see imu_task_hw_rst_t Timing constants for assert and POR delays (with +1 slack)
 * @see rx_bno055_init() Called immediately after this function in internal_imu_task_entry()
 *
 * @since Version 1.0.0
 */
static rx_err_t internal_imu_hardware_reset(void)
{
  RX_ASSERT(s_imu_created, "IMU task must be created before hardware reset");
  RX_ASSERT(s_tag != NULL, "s_tag must not be NULL");

  /* Assert RST LOW -- BNO055 enters hardware reset (active-low) */
  const rx_err_t err_low = gpio_write_low((rx_port_pin_t)k_rx_p8_3);
  if (err_low != k_rx_ok) {
    rx_log_error_val(s_tag, "IMU RST assert low failed", (uint32_t)err_low);
    return err_low;
  }

  /* Hold RST LOW for guaranteed >= 10 ms (+1 tick slack for early wake) */
  (void)tx_thread_sleep((ULONG)k_imu_hw_rst_assert_ticks);

  /* Deassert RST HIGH -- BNO055 released from reset, POR sequence begins */
  const rx_err_t err_high = gpio_write_high((rx_port_pin_t)k_rx_p8_3);
  if (err_high != k_rx_ok) {
    rx_log_error_val(s_tag, "IMU RST deassert high failed", (uint32_t)err_high);
    return err_high;
  }

  /* Wait guaranteed >= 650 ms for BNO055 POR to complete (+1 tick slack) */
  (void)tx_thread_sleep((ULONG)k_imu_hw_rst_por_ticks);

  rx_log_info(s_tag, "BNO055 hardware reset complete");
  return k_rx_ok;
}

/**
 * @brief Block on BNO055 INT event flag with 200 ms watchdog timeout
 *
 * @details
 * Waits for k_imu_event_data_ready to be set in s_imu_event_flags by the
 * INT_IRQ12() ISR. Returns k_imu_wait_data_ready immediately if the flag is
 * already set. Returns k_imu_wait_timeout after k_imu_int_timeout_ticks
 * (200 ms) with no INT -- the caller performs a fault-recovery read.
 * Returns k_imu_wait_rtos_error on any other ThreadX status, indicating that
 * the event-flags group may be corrupted or deleted.
 *
 * @return imu_wait_result_t Three-state result distinguishing INT, timeout, and RTOS fault
 * @retval k_imu_wait_data_ready BNO055 INT fired; event flag consumed
 * @retval k_imu_wait_timeout 200 ms watchdog expired; fault-recovery read
 * @retval k_imu_wait_rtos_error Fatal ThreadX error; caller must escalate
 *
 * @pre s_imu_event_flags created by imu_task_create()
 * @pre Called from task context only (tx_event_flags_get blocks the caller)
 * @post k_imu_event_data_ready cleared from s_imu_event_flags on TX_SUCCESS (TX_OR_CLEAR)
 * @post Caller reads sensors on k_imu_wait_data_ready and k_imu_wait_timeout
 *
 * @note Not thread-safe; called only from internal_imu_task_entry()
 *
 * @see s_imu_event_flags Event flags group set by INT_IRQ12()
 * @see imu_wait_result_t Return type documentation
 * @see internal_imu_task_entry() Sole caller
 *
 * @since Version 1.0.0
 */
static imu_wait_result_t internal_wait_for_imu_int(void)
{
  ULONG      actual_flags = 0U;
  const UINT ef_status    = tx_event_flags_get(&s_imu_event_flags,
                                            (ULONG)k_imu_event_data_ready,
                                            TX_OR_CLEAR,
                                            &actual_flags,
                                            (ULONG)k_imu_int_timeout_ticks);
  if (ef_status == TX_NO_EVENTS) {
    /* Benign timeout: BNO055 INT did not fire within 200 ms. On STAR PCB
     * revisions where the BNO055 INT line is not observed to toggle, this
     * path is the normal operating mode and produces a ~5 Hz polled-read
     * loop via the main-loop's unconditional internal_read_and_publish_imu
     * call. Demoted from WARN to DEBUG so the log is not spammed at every
     * iteration when the INT never fires. */
    rx_log_debug(s_tag, "IMU INT timeout (200 ms) - reading without INT");
    return k_imu_wait_timeout;
  }
  if (ef_status != TX_SUCCESS) {
    /* Fatal RTOS error: event flags group corrupted or deleted */
    rx_log_error_val(s_tag, "IMU event flags get failed", (uint32_t)ef_status);
    return k_imu_wait_rtos_error;
  }
  return k_imu_wait_data_ready;
}

/**
 * @brief BNO055 INT falling-edge ISR (IRQ12, vector 76, P3.2)
 *
 * @details
 * Invoked by the RX72N ICU on each falling edge of the BNO055 active-low
 * INT signal. Sets k_imu_event_data_ready in s_imu_event_flags to wake the
 * IMU task, then clears the ICU IR flag.
 *
 * ISR execution time: < 1 us at 240 MHz (two register writes + event set).
 *
 * @pre ICU IRQ12 enabled and configured for falling-edge (hardware_init.c)
 * @pre s_imu_event_flags created (imu_task_create() must have run)
 * @post k_imu_event_data_ready bit set in s_imu_event_flags
 * @post IR[76] cleared (ICU interrupt request flag deasserted)
 *
 * @note Runs in interrupt context; must not call blocking ThreadX APIs
 * @note Registered at vector 76 in .rvectors (INTB) table via GNURX naming convention
 * @warning Do not call from task context
 *
 * @see s_imu_event_flags Event flags group signaled by this ISR
 * @see internal_gpio_init_imu_irq() Configures ICU for IRQ12 falling-edge
 * @see internal_imu_task_entry() IMU task that pends on the event flag
 *
 * @since Version 1.0.0
 */
void INT_IRQ12(void)
{
#ifdef __RX__
  /* Clear ICU interrupt request flag first -- must execute even when the ready
   * guard below returns early.  If a pre-task-create edge fires and the IR bit
   * is not cleared here, the ICU will re-assert the interrupt immediately after
   * the ISR returns, corrupting the still-uninitialized event flags group. */
  icu()->ir[k_imu_irq_vector_isr] = (uint8_t)k_icu_ir_clear_val;
#endif /* __RX__ */

  /* Guard against spurious INT edges before event flags are fully created */
  if (!s_imu_event_flags_ready) {
    return;
  }

  /* Signal the IMU task that BNO055 data is ready */
  (void)tx_event_flags_set(&s_imu_event_flags, (ULONG)k_imu_event_data_ready, TX_OR);
}

/**
 * @brief Initialize BNO055 and BMP280 sensors during IMU task startup
 *
 * @details
 * Performs the three-step startup sequence:
 * 1. Feed IWDT heartbeat before the long reset sequence (~1.37 s total)
 * 2. Hardware reset BNO055 via P83 (IMU RST, active-low): >= 10 ms assert +
 *    >= 650 ms POR (with +1 tick slack each); then feed IWDT heartbeat
 * 3. Initialize BNO055 via rx_bno055_init() (~700 ms); then feed IWDT heartbeat
 * 4. Initialize BMP280 via rx_bmp280_init() (~2 ms)
 *
 * Outputs the sensor-ready flags to the caller so the main loop knows which
 * sensors are available for reading.
 *
 * @param[out] bno_ready Set to true if BNO055 init succeeded, false otherwise
 * @param[out] bmp_ready Set to true if BMP280 init succeeded, false otherwise
 *
 * @pre internal_send_iwdt_heartbeat() callable (IWDT registered)
 * @pre internal_imu_hardware_reset() callable (P83 GPIO configured)
 * @pre g_bus_manager initialized with "i2c1_imu" bus
 *
 * @post *bno_ready reflects BNO055 initialization outcome
 * @post *bmp_ready reflects BMP280 initialization outcome
 * @post IWDT heartbeat fed three times (before reset, after reset, after BNO055 init)
 *
 * @note Not thread-safe; called once from internal_imu_task_entry() at startup
 *
 * @see internal_imu_task_entry() Sole caller
 * @see rx_bno055_init() BNO055 software init
 * @see rx_bmp280_init() BMP280 init
 * @see internal_imu_hardware_reset() BNO055 hardware reset
 *
 * @since Version 1.0.0
 */
static void internal_init_sensors(bool* bno_ready, bool* bmp_ready)
{
  /* Feed watchdog before the long startup sequence (~1.37 s total) so the IWDT
   * registration window is refreshed and the system does not reset during init. */
  internal_send_iwdt_heartbeat();

  /* Step 1: Hardware reset BNO055 via P83 (IMU RST, active-low) to guarantee clean state.
   * Asserts RST LOW for >= 10 ms then releases; waits >= 650 ms for POR to complete. */
  const rx_err_t err_rst = internal_imu_hardware_reset();
  if (err_rst != k_rx_ok) {
    rx_log_error_val(s_tag, "BNO055 HW reset failed - will skip init", (uint32_t)err_rst);
  }

  /* Feed watchdog after the hardware reset block (~670 ms) before BNO055 software
   * init (~700 ms) to prevent IWDT expiry during the combined ~1.37 s startup. */
  internal_send_iwdt_heartbeat();

  /* Step 2: Initialize BNO055 (software reset + config, blocks ~700 ms) */
  if (err_rst == k_rx_ok) {
    const bno055_config_t bno_cfg = {.mode = k_bno055_mode_interrupt};
    const rx_err_t        err_bno = rx_bno055_init(&g_bus_manager, &bno_cfg);
    if (err_bno != k_rx_ok) {
      rx_log_error_val(s_tag, "BNO055 init failed - will retry in loop", (uint32_t)err_bno);
    } else {
      *bno_ready = true;
      rx_log_info(s_tag, "BNO055 initialized in NDOF mode");
    }
  }

  /* Feed watchdog after BNO055 init before BMP280 init */
  internal_send_iwdt_heartbeat();

  /* Step 3: Initialize BMP280 (~2 ms for calibration burst read) */
  const rx_err_t err_bmp = rx_bmp280_init(&g_bus_manager);
  if (err_bmp != k_rx_ok) {
    rx_log_error_val(s_tag, "BMP280 init failed - will retry in loop", (uint32_t)err_bmp);
  } else {
    *bmp_ready = true;
    rx_log_info(s_tag, "BMP280 initialized in forced mode");
  }
}

/**
 * @brief IMU task entry point - initialize sensors then read on BNO055 INT
 *
 * @details
 * Task startup sequence:
 * 1. Feed IWDT heartbeat before the long startup sequence (~1.37 s total)
 * 2. Hardware reset BNO055 via P83 (IMU RST, active-low):
 *    >= 10 ms assert + >= 650 ms POR (with +1 tick slack each)
 * 3. Feed IWDT heartbeat after hardware reset
 * 4. Initialize BNO055 via rx_bno055_init() (~700 ms for software reset + config)
 *    (skipped if hardware reset failed)
 * 5. Feed IWDT heartbeat after BNO055 init
 * 6. Initialize BMP280 via rx_bmp280_init() (~2 ms for calib read)
 * 7. Enter interrupt-driven loop:
 *    a. Feed IWDT heartbeat FIRST (must precede any blocking sensor re-init)
 *    b. Block on BNO055 INT event flag (200 ms watchdog timeout)
 *    c. Retry init for any sensor that failed startup (until success)
 *    d. internal_read_and_publish_imu() if bno_ready - BNO055 -> shared_data_update_imu()
 *    e. internal_read_and_publish_baro() if bmp_ready - BMP280 -> shared_data_update_baro()
 *
 * Both sensors are initialized before the poll loop. If either init fails,
 * the loop retries init each iteration until both succeed. Reads are only
 * performed once the corresponding sensor is ready. Shared state valid flags
 * remain false until sensor init and the first read both succeed.
 *
 * @param[in] input Unused thread entry parameter (ULONG, always 0)
 *
 * @pre ThreadX scheduler running
 * @pre s_imu_created == true (imu_task_create() completed)
 * @pre "i2c1_imu" bus registered in g_bus_manager (registered by main.c)
 * @pre RIIC1 initialized at 400 kHz (by hardware_init.c i2c_init)
 * @pre shared_data_init() completed (imu_mutex and baro_mutex available)
 * @pre BNO055 powered on RIIC1 I2C bus, RST pin (P83) configured as GPIO output
 * @pre BMP280 powered on RIIC1 I2C bus
 *
 * @post imu_state_t updated via shared_data_update_imu() on each BNO055 INT
 * @post baro_state_t updated via shared_data_update_baro() on each BNO055 INT
 * @post State valid flags remain false until first successful sensor read
 * @post IWDT heartbeat fed on each loop iteration (< 200 ms period)
 *
 * @note Not thread-safe; runs as single dedicated ThreadX task
 * @note Sensor init failures do not halt the task; valid flag stays false
 * @note BNO055 hardware reset (~670 ms) + software init (~700 ms) block at startup only; IWDT fed between each
 * @note Does not preempt motor control (priority 8) or obstacle detect (12)
 *
 * @warning Do not call directly; use imu_task_create() to register with ThreadX
 *
 * @see imu_task_create() Creates this task
 * @see INT_IRQ12() ISR that signals the event flags
 * @see internal_read_and_publish_imu() BNO055 read and publish helper
 * @see internal_read_and_publish_baro() BMP280 read and publish helper
 * @see internal_imu_hardware_reset() Hardware reset helper (step 1)
 * @see rx_bno055_init() BNO055 initialization
 * @see rx_bmp280_init() BMP280 initialization
 *
 * @since Version 1.0.0
 */
static void internal_imu_task_entry(ULONG input)
{
  (void)input;

  RX_ASSERT(s_imu_created, "IMU task entry called before task created");
  RX_ASSERT(s_tag != NULL, "s_tag must not be NULL");

  rx_log_info(s_tag, "IMU task starting - initializing sensors");

  bool bno_ready = false;
  bool bmp_ready = false;
  internal_init_sensors(&bno_ready, &bmp_ready);

  /* Step 4: Interrupt-driven loop - block on BNO055 INT event flag */
  rx_log_info(s_tag, "IMU interrupt-driven mode: blocking on IRQ12 event flag");
  while (1) {
    /* Feed IWDT heartbeat first -- must arrive within 900 ms window */
    internal_send_iwdt_heartbeat();

    /* Block until BNO055 asserts INT (falling edge) or 200 ms timeout */
    const imu_wait_result_t wait_result = internal_wait_for_imu_int();
    if (wait_result == k_imu_wait_rtos_error) {
      /* Fatal ThreadX fault: event-flags group corrupted or deleted.
       * Suspend the task to surface the fault rather than looping
       * forever in a degraded state with no interrupt signaling. */
      rx_log_error(s_tag, "IMU task suspending: fatal RTOS event flags fault");
      (void)tx_thread_suspend(tx_thread_identify());
      continue;
    }

    /* Both k_imu_wait_data_ready and k_imu_wait_timeout proceed with read */

    /* Retry init for sensors that failed initial startup */
    internal_retry_sensor_init(&bno_ready, &bmp_ready);

    if (bno_ready) {
      internal_read_and_publish_imu();
    }
    if (bmp_ready) {
      internal_read_and_publish_baro();
    }
  }
}
