/* src/tasks/imu_task.c */

/**
 * @file imu_task.c
 * @brief IMU Task - BNO055 + BMP280 Sensor Polling at 20 Hz
 *
 * @details
 * # Overview
 *
 * This module implements the IMU Task that initializes and polls the
 * BNO055 9-DOF absolute orientation sensor and the BMP280 barometric
 * pressure sensor at 20 Hz (50 ms period), publishing results to
 * shared data for consumption by the telemetry task.
 *
 * # Hardware
 *
 * Both sensors share the RIIC1 I2C bus (bus name "i2c1"):
 * - **BNO055**: I2C addr 0x28, NDOF fusion mode (heading, quaternion, linear accel)
 * - **BMP280**: I2C addr 0x76, forced mode (pressure Pa, temperature cC)
 *
 * # Task Lifecycle
 *
 * @startuml
 * [*] --> Init
 * Init --> Running : both sensors initialized
 * Init --> Running : one or both sensors failed (logs error, continues)
 * Running --> Running : poll BNO055 + BMP280 every 50 ms
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
 * | 4. Short functions | [PASS] | Task entry ~50 lines; helpers extracted |
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
 * @copyright Copyright (c) 2026 STAR Project. MIT License.
 */

#include "imu_task.h"

#include "rx_bmp280.h"
#include "rx_bno055.h"
#include "rx_check.h"
#include "rx_iwdt.h"
#include "rx_log.h"
#include "shared_data.h"
#include "tx_api.h"

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
 * @brief IMU task priority, input, and period configuration constants
 *
 * @details
 * Defines thread priority, entry input, and period tick count for the IMU task.
 * The task runs at 20 Hz (50 ms period, 5 ticks at 100 Hz tick rate).
 *
 * **Priority rationale:**
 * - Priority 13 sits between obstacle detect (12) and temp sensor (15)
 * - IMU data needed by telemetry; not as time-critical as obstacle avoidance
 *
 * @invariant k_imu_task_period_ticks > 0 (loop must have finite period)
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_task_priority     = 13, /**< ThreadX priority (between obstacle=12 and temp=15) */
  k_imu_task_input        = 0,  /**< Thread entry ULONG input (unused) */
  k_imu_task_period_ticks = 5,  /**< 50 ms period: 5 ticks @ 10 ms/tick (100 Hz tick rate) */
} imu_task_cfg_t;

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
 * @var g_bus_manager
 * @brief External bus manager declared in main.c; registered buses include "i2c1" and "i2c1_baro"
 *
 * @details
 * Declared extern here because main.h does not export g_bus_manager.
 * If a main.h public header is added, replace this with #include "main.h".
 * The IMU task uses this to pass the bus manager to rx_bno055_init() and
 * rx_bmp280_init() during task startup.
 *
 * @note Using inline extern is necessary because main.h does not export g_bus_manager.
 *       If a main.h public header is added, replace this with #include "main.h".
 *
 * @since Version 1.0.0
 */
extern rx_bus_manager_t g_bus_manager;

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void internal_send_iwdt_heartbeat(void);
static void internal_read_and_publish_imu(void);
static void internal_read_and_publish_baro(void);
static void internal_imu_task_entry(ULONG input);

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
  RX_ASSERT(!s_imu_created, "IMU task already created");
  RX_ASSERT(k_imu_task_stack_size > 0U, "IMU stack size must be non-zero");
  if (s_imu_created) {
    return k_rx_err_invalid_state;
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
    return k_rx_err_rtos_thread_create;
  }

  s_imu_created = true;
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
  RX_ASSERT(s_tag != NULL, "s_tag must be non-NULL for IWDT heartbeat");
  RX_ASSERT(s_imu_created, "IWDT heartbeat called before task created");
  const rx_err_t err = rx_iwdt_task_heartbeat(s_task_name);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "IWDT heartbeat failed");
  }
}

/**
 * @brief Read BNO055 and publish imu_state_t to shared data
 *
 * @details
 * Reads all BNO055 fusion outputs (Euler angles, quaternion, linear
 * acceleration, temperature, calibration status) and writes them to
 * shared data via shared_data_update_imu(). Sets valid = false on read
 * failure to signal consumers that data is stale.
 *
 * @pre s_imu_created == true (task running)
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

  bno055_data_t bno_data = {0};
  const rx_err_t err     = rx_bno055_read(&bno_data);

  imu_state_t imu  = {0};
  imu.timestamp_ms = tx_time_get();

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

  bmp280_data_t  bmp_data = {0};
  const rx_err_t err      = rx_bmp280_read(&bmp_data);

  baro_state_t baro  = {0};
  baro.timestamp_ms  = tx_time_get();

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
 * @brief IMU task entry point - initialize sensors then poll at 20 Hz
 *
 * @details
 * Task startup sequence:
 * 1. Initialize BNO055 via rx_bno055_init() (~700 ms for POR)
 * 2. Initialize BMP280 via rx_bmp280_init() (~2 ms for calib read)
 * 3. Enter 50 ms periodic loop:
 *    a. internal_read_and_publish_imu() - BNO055 read -> shared_data_update_imu()
 *    b. internal_read_and_publish_baro() - BMP280 read -> shared_data_update_baro()
 *    c. Feed IWDT heartbeat
 *    d. Sleep 5 ticks (50 ms)
 *
 * Both sensors are initialized before the poll loop. If either init fails,
 * the error is logged but the task continues (graceful degradation: the
 * shared state valid flag will remain false until a read succeeds).
 *
 * @param[in] input Unused thread entry parameter (ULONG, always 0)
 *
 * @pre ThreadX scheduler running
 * @pre s_imu_created == true (imu_task_create() completed)
 * @pre "i2c1" bus registered in g_bus_manager (registered by main.c)
 * @pre RIIC1 initialized at 400 kHz (by hardware_init.c i2c_init)
 * @pre shared_data_init() completed (imu_mutex and baro_mutex available)
 * @pre BNO055 powered on RIIC1 I2C bus, RST pin driven HIGH (not in reset)
 * @pre BMP280 powered on RIIC1 I2C bus
 *
 * @post imu_state_t updated via shared_data_update_imu() at 20 Hz on success
 * @post baro_state_t updated via shared_data_update_baro() at 20 Hz on success
 * @post State valid flags remain false until first successful sensor read
 * @post IWDT heartbeat fed every 50 ms
 *
 * @note Not thread-safe; runs as single dedicated ThreadX task
 * @note Sensor init failures do not halt the task; valid flag stays false
 * @note BNO055 POR delay (~700 ms) blocks this task during startup only
 * @note Does not preempt motor control (priority 8) or obstacle detect (12)
 *
 * @warning Do not call directly; use imu_task_create() to register with ThreadX
 *
 * @see imu_task_create() Creates this task
 * @see internal_read_and_publish_imu() BNO055 read and publish helper
 * @see internal_read_and_publish_baro() BMP280 read and publish helper
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

  /* Step 1: Initialize BNO055 (blocks ~700 ms for POR sequence) */
  const rx_err_t err_bno = rx_bno055_init(&g_bus_manager);
  if (err_bno != k_rx_ok) {
    rx_log_error_val(s_tag, "BNO055 init failed - will retry reads", (uint32_t)err_bno);
  } else {
    rx_log_info(s_tag, "BNO055 initialized in NDOF mode");
  }

  /* Step 2: Initialize BMP280 (~2 ms for calibration burst read) */
  const rx_err_t err_bmp = rx_bmp280_init(&g_bus_manager);
  if (err_bmp != k_rx_ok) {
    rx_log_error_val(s_tag, "BMP280 init failed - will retry reads", (uint32_t)err_bmp);
  } else {
    rx_log_info(s_tag, "BMP280 initialized in forced mode");
  }

  rx_log_info(s_tag, "IMU polling at 20 Hz");

  /* Step 3: Periodic poll loop at 20 Hz (50 ms period = 5 ticks @ 100 Hz) */
  while (true) {
    internal_read_and_publish_imu();
    internal_read_and_publish_baro();

    /* Feed IWDT heartbeat (must execute within 150 ms timeout) */
    internal_send_iwdt_heartbeat();

    /* Sleep for remainder of 50 ms period */
    (void)tx_thread_sleep((ULONG)k_imu_task_period_ticks);
  }
}
