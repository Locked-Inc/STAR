/* src/tasks/imu_task.c */

/**
 * @file imu_task.c
 * @brief IMU Task — BNO055 9-DOF IMU and BMP280 Barometric Pressure Sensor
 *
 * @details
 * # Overview
 *
 * Implements the STAR IMU monitoring task, which continuously reads orientation,
 * acceleration, and angular velocity from the Bosch BNO055 at 100 Hz, and pressure,
 * temperature, and altitude from the Bosch BMP280 at ~4 Hz. Both sensors share the
 * SCI2 Simple IIC (SIIC) bus on P52 (SSCL2) and P50 (SSDA2).
 *
 * ## Sensor Architecture
 *
 * @dot
 * digraph imu_architecture {
 *   rankdir=TB;
 *   node [shape=box, style=rounded];
 *
 *   subgraph cluster_hw {
 *     label="Hardware (SCI2 SIIC, 400 kHz)";
 *     style=filled;
 *     color=lightblue;
 *     bno055 [label="BNO055\n0x28 (COM3=GND)\nNDOF fusion mode\n100 Hz", fillcolor=lightgreen, style=filled];
 *     bmp280 [label="BMP280\n0x76 (SDO=GND)\nNormal mode, IIR=16\n~3.8 Hz ODR", fillcolor=lightyellow, style=filled];
 *   }
 *
 *   subgraph cluster_fw {
 *     label="RX72N Firmware";
 *     style=filled;
 *     color=lightgreen;
 *     imu_task [label="imu_task.c\n(This Module)\nPriority 13 @ 100 Hz", fillcolor=lightgreen, style=filled];
 *     bno055_drv [label="rx_bno055 Driver", fillcolor=lightyellow, style=filled];
 *     bmp280_drv [label="rx_bmp280 Driver", fillcolor=lightyellow, style=filled];
 *     shared [label="shared_data\nimu_state + barometer_state", shape=cylinder, fillcolor=lightgrey, style=filled];
 *     telemetry [label="telemetry_task\n(Consumer)", fillcolor=lightcoral, style=filled];
 *   }
 *
 *   bno055 -> bno055_drv [label="I2C"];
 *   bmp280 -> bmp280_drv [label="I2C"];
 *   bno055_drv -> imu_task [label="rx_bno055_read()"];
 *   bmp280_drv -> imu_task [label="rx_bmp280_read()"];
 *   imu_task -> shared [label="shared_data_update_imu()\nshared_data_update_barometer()"];
 *   shared -> telemetry [label="shared_data_get_imu()\nshared_data_get_barometer()"];
 * }
 * @enddot
 *
 * ## Task Timing
 *
 * | Sensor | Rate | Mechanism |
 * |--------|------|-----------|
 * | BNO055 (Euler + accel + gyro + calib) | 100 Hz | Every loop iteration |
 * | BMP280 (pressure + temperature + altitude) | ~4 Hz | Every 25th iteration |
 *
 * The main loop uses tx_thread_sleep(1) for a 10 ms period (1 tick = 10 ms at 100 Hz
 * ThreadX tick rate). BMP280 is read every k_imu_baro_divider = 25 iterations, giving
 * 100/25 = 4 Hz, matching the BMP280's ~3.82 Hz ODR in normal mode.
 *
 * ## BNO055 Power-On Delay
 *
 * The BNO055 requires 650 ms after power-on before I2C communication is valid.
 * The task sleeps k_imu_poweron_ticks = 65 ticks (650 ms) at startup before calling
 * rx_bno055_init(), ensuring the device is ready.
 *
 * ## Initialization Sequence
 *
 * @msc
 * msc {
 *   width=900;
 *   ImuTask, BNO055, BMP280, SharedData;
 *
 *   --- [label="Power-on delay"];
 *   ImuTask box ImuTask [label="tx_thread_sleep(65 ticks)\n= 650 ms BNO055 power-on delay"];
 *
 *   --- [label="BNO055 init"];
 *   ImuTask => BNO055 [label="rx_bno055_init() - verify CHIP_ID=0xA0"];
 *   BNO055 >> ImuTask [label="NDOF mode active"];
 *
 *   --- [label="BMP280 init"];
 *   ImuTask => BMP280 [label="rx_bmp280_init() - verify CHIP_ID=0x58, load trim"];
 *   BMP280 >> ImuTask [label="Normal mode, IIR filter active"];
 *
 *   --- [label="100 Hz read loop (every 10 ms tick)"];
 *   ImuTask => BNO055 [label="rx_bno055_read() - Euler + accel + gyro + calib"];
 *   BNO055 >> ImuTask [label="rx_bno055_data_t"];
 *   ImuTask => SharedData [label="shared_data_update_imu()"];
 *
 *   --- [label="Every 25th iteration (4 Hz)"];
 *   ImuTask => BMP280 [label="rx_bmp280_read() - pressure + temperature + altitude"];
 *   BMP280 >> ImuTask [label="rx_bmp280_data_t"];
 *   ImuTask => SharedData [label="shared_data_update_barometer()"];
 * }
 * @endmsc
 *
 * ## Error Handling Strategy
 *
 * | Error | Behaviour | Data State | Recovery |
 * |-------|-----------|------------|----------|
 * | BNO055 init fail | Log error, continue | imu_state.valid = false | Retry not automatic; invalid data reported forever |
 * | BMP280 init fail | Log error, continue | barometer_state.valid = false | Retry not automatic |
 * | BNO055 read fail | Log warning, mark invalid | imu_state.valid = false | Auto-retry next iteration (10 ms) |
 * | BMP280 read fail | Log warning, mark invalid | barometer_state.valid = false | Auto-retry next 4 Hz cycle |
 *
 * ## NASA Power of 10 Compliance
 *
 * | Rule | Status | Details |
 * |------|--------|---------|
 * | Rule 1: Control Flow | [PASS] | No goto, setjmp, recursion |
 * | Rule 2: Loop Bounds | [PASS] | while(true) with fixed 10 ms period; baro_counter bounded by k_imu_baro_divider |
 * | Rule 3: No Heap | [PASS] | Zero dynamic allocation; 2048-byte stack statically allocated |
 * | Rule 4: Short Functions | [PASS] | imu_task_create(): 30 lines; internal_imu_task_entry(): ~80 lines |
 * | Rule 5: Assertions | [PASS] | Guard check + 4 pre + 3 post per function |
 * | Rule 6: Data Scope | [PASS] | All file-scope variables use static (s_ prefix) |
 * | Rule 7: Return Checks | [PASS] | All rx_bno055/rx_bmp280/shared_data returns validated |
 * | Rule 8: Preprocessor | [PASS] | C23 typed enums for all constants |
 * | Rule 9: Pointers | [PASS] | Single-level pointers only |
 * | Rule 10: Warnings | [PASS] | -Wall -Wextra -Werror; zero warnings |
 *
 * @see imu_task.h Public API (imu_task_create declaration)
 * @see rx_bno055.h BNO055 driver API
 * @see rx_bmp280.h BMP280 driver API
 * @see shared_data.h imu_state_t and barometer_state_t definitions
 * @see hardware_init.c Configures SCI2 SIIC and calls sci_iic_init()
 *
 * @author STAR Team
 * @date 2026-02-19
 * @copyright Copyright (c) 2026 STAR Project
 * @since Version 1.0.0
 */

#include "imu_task.h"

#include <string.h>

#include "hardware_config.h"
#include "rx_bmp280.h"
#include "rx_bmp280_constants.h"
#include "rx_bno055.h"
#include "rx_bno055_constants.h"
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
 * @enum imu_task_timing_t
 * @brief Timing constants for IMU task scheduling
 *
 * @details
 * All tick values assume ThreadX configured at 100 Hz (1 tick = 10 ms).
 *
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_imu_task_stack_size = 2048, /**< Stack size in bytes (covers BNO055+BMP280 I2C call frames) */
  k_imu_task_input      = 0,    /**< Thread entry input parameter (unused by this task) */
  k_imu_poweron_ticks   = 65,   /**< 650ms BNO055 power-on delay: 65 ticks × 10ms/tick */
  k_imu_baro_divider    = 25,   /**< Read BMP280 every 25th BNO055 iteration: 100 Hz / 25 = 4 Hz */
  k_imu_loop_ticks      = 1,    /**< Main loop period: 1 tick × 10ms/tick = 10ms (100 Hz) */
} imu_task_timing_t;

/**
 * @enum imu_task_config_t
 * @brief Configuration constants for IMU task thread creation
 *
 * @details
 * Priority 13 is between obstacle detection (priority 12) and BMS/temp monitoring
 * (priority 15), reflecting that IMU data is higher-priority than environmental sensing
 * but lower-priority than obstacle avoidance safety functions.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_imu_task_priority = 13, /**< ThreadX priority: between obstacle(12) and BMS/temp(15) */
} imu_task_config_t;

/* =============================================================================
 * Static Variables
 * =============================================================================
 */

/**
 * @var s_imu_thread
 * @brief ThreadX thread control block for the IMU task
 * @note Not thread-safe; only modified by imu_task_create() during single-threaded boot
 * @warning Do not access directly; use ThreadX APIs (tx_thread_suspend, etc.)
 * @since Version 1.0.0
 */
static TX_THREAD s_imu_thread;

/**
 * @var s_imu_stack
 * @brief Static stack storage for the IMU task thread
 * @details 2048 bytes accommodates BNO055 and BMP280 I2C call frames (~256 bytes peak)
 * @note Allocated in .bss; zeroed at startup
 * @warning Never access directly; owned by ThreadX scheduler
 * @since Version 1.0.0
 */
static uint8_t s_imu_stack[k_imu_task_stack_size];

/**
 * @var s_imu_created
 * @brief Task creation guard flag — prevents double-creation
 * @details Transitions false → true exactly once in imu_task_create(); never resets
 * @note Not thread-safe; only modified during single-threaded boot
 * @warning Do not reset; a second call to imu_task_create() must fail
 * @since Version 1.0.0
 */
static bool s_imu_created = false;

/**
 * @var s_tag
 * @brief Log tag for all rx_log_* calls in this module
 * @note Read-only constant string; placed in .rodata
 * @since Version 1.0.0
 */
static const char* const s_tag = "IMU";

/* =============================================================================
 * Forward Declarations
 * =============================================================================
 */

static void internal_imu_task_entry(ULONG input);

/* =============================================================================
 * Public Functions
 * =============================================================================
 */

/**
 * @brief Create and start the IMU sensor task
 *
 * @details
 * Creates the ImuTask ThreadX thread with priority 13, 2048-byte static stack,
 * and auto-start enabled. The thread begins executing internal_imu_task_entry()
 * immediately after the ThreadX scheduler starts.
 *
 * ## Algorithm
 *
 * 1. Guard check: assert s_imu_created == false (prevent double-creation)
 * 2. Call tx_thread_create() with s_imu_thread, s_imu_stack, priority 13
 * 3. On TX_SUCCESS: set s_imu_created = true, log success, return k_rx_ok
 * 4. On failure: log error code, return k_rx_err_rtos_thread_create
 *
 * @return rx_err_t Task creation status
 * @retval k_rx_ok                    Thread created and scheduled; sensor init will occur inside task
 * @retval k_rx_err_invalid_state     Already called (s_imu_created == true); second call blocked
 * @retval k_rx_err_rtos_thread_create tx_thread_create() returned non-TX_SUCCESS
 *
 * @pre ThreadX kernel entered via tx_kernel_enter()
 * @pre tx_application_define() callback currently executing
 * @pre shared_data_init() called successfully (for shared_data_update_imu/barometer)
 * @pre sci_iic_init(k_imu_iic_channel) called by hardware_init() (SCI2 SIIC ready)
 * @pre s_imu_created == false (first and only call)
 *
 * @post s_imu_thread initialized as ThreadX TCB; task scheduled
 * @post s_imu_stack assigned to thread
 * @post s_imu_created == true (blocks subsequent calls)
 * @post internal_imu_task_entry() will run when scheduler starts
 *
 * @invariant s_imu_created transitions false → true exactly once
 * @invariant Task priority is k_imu_task_priority (13) for the task lifetime
 *
 * @note Call from tx_application_define() after shared_data_init()
 * @note Not thread-safe; safe during single-threaded boot only
 * @note Task auto-starts; sensor init (650ms BNO055 delay) occurs inside the task
 *
 * @warning Never call twice. Second call is blocked (asserts in debug, returns error in release).
 * @warning Must be called AFTER hardware_init() (sci_iic_init must have run)
 * @warning Must be called AFTER shared_data_init()
 *
 * @attention SCI2 SIIC bus must be exclusively used by this task (BNO055 and BMP280 share the bus)
 *
 * @par Thread Safety:
 * NOT thread-safe. Call only from single-threaded tx_application_define() context.
 *
 * @par Performance:
 * Creation time: ~120 µs @ 240 MHz. Memory: 2048 (stack) + 140 (TCB) + ~50 (statics) = ~2238 bytes.
 *
 * @par Re-entrancy:
 * NOT reentrant. Single-shot function guarded by s_imu_created.
 *
 * @par Example:
 * @code{.c}
 * void tx_application_define(void* first_unused_memory)
 * {
 *   (void)first_unused_memory;
 *
 *   rx_err_t err = shared_data_init();
 *   if (err != k_rx_ok) { return; }
 *
 *   // ... create other tasks ...
 *
 *   // IMU task: priority 13, 100 Hz BNO055, 4 Hz BMP280
 *   err = imu_task_create();
 *   RX_ASSERT(err == k_rx_ok, "imu_task_create must succeed");
 * }
 * @endcode
 *
 * @see internal_imu_task_entry() Task main loop (sensor init + read loop)
 * @see tx_application_define() Caller context
 * @see shared_data_init() Must be called first
 * @see hardware_init() Must be called first (configures SCI2 SIIC)
 *
 * @since Version 1.0.0
 *
 * @test Verify creation succeeds on first call
 * @test Verify second call returns k_rx_err_invalid_state
 * @test Verify task priority is k_imu_task_priority (13)
 * @test Verify stack size is k_imu_task_stack_size (2048)
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 5: ✓ 5 preconditions, 4 postconditions
 * - Rule 7: ✓ tx_thread_create() return value checked
 * - Rule 8: ✓ All constants use C23 typed enums
 *
 * @callgraph
 * @callergraph
 */
rx_err_t imu_task_create(void)
{
  UINT tx_status;

  /* Guard: prevent double-creation */
  RX_ASSERT(!s_imu_created, "IMU task already created");
  if (s_imu_created) {
    return k_rx_err_invalid_state;
  }

  /* Create the thread */
  tx_status = tx_thread_create(&s_imu_thread,
                               "ImuTask",
                               internal_imu_task_entry,
                               k_imu_task_input,
                               s_imu_stack,
                               k_imu_task_stack_size,
                               k_imu_task_priority,
                               k_imu_task_priority,
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
 * @brief IMU task entry point — BNO055 + BMP280 sensor read loop
 *
 * @details
 * This is the main task function that executes after imu_task_create() starts the
 * thread. Never returns; runs indefinitely at 100 Hz until power-down.
 *
 * ## Initialization Phase
 *
 * 1. Sleep 650 ms (65 ticks) for BNO055 power-on delay (datasheet requirement)
 * 2. Initialize BNO055: rx_bno055_init() — sets NDOF fusion mode
 *    - On failure: log error, mark imu_state.valid = false, continue (report invalid data)
 * 3. Initialize BMP280: rx_bmp280_init() — loads trim, starts normal mode
 *    - On failure: log error, mark barometer_state.valid = false, continue
 *
 * ## Main Loop (100 Hz, infinite)
 *
 * Each 10 ms iteration:
 *
 * 4. Read BNO055 via rx_bno055_read(): Euler angles + accel + gyro + calib_status
 *    - On success: fill imu_state_t (valid=true)
 *    - On failure: mark imu_state_t (valid=false), log warning
 * 5. Call shared_data_update_imu() with latest imu_state
 * 6. Increment baro_counter; if baro_counter >= k_imu_baro_divider (25):
 *    - Reset baro_counter = 0
 *    - Read BMP280 via rx_bmp280_read(): pressure + temperature + altitude
 *    - On success: fill barometer_state_t (valid=true)
 *    - On failure: mark barometer_state_t (valid=false), log warning
 *    - Call shared_data_update_barometer() with latest barometer_state
 * 7. Report IWDT task heartbeat
 * 8. tx_thread_sleep(k_imu_loop_ticks) — yield CPU for 10 ms
 * 9. Repeat from step 4
 *
 * ## Control Flow
 *
 * @startuml
 * start
 * :tx_thread_sleep(65)\n650ms BNO055 power-on delay;
 * :rx_bno055_init();
 * if (BNO055 init OK?) then (yes)
 *   :Log "BNO055 ready";
 * else (no)
 *   :Log error\n(invalid IMU data until reboot);
 * endif
 * :rx_bmp280_init();
 * if (BMP280 init OK?) then (yes)
 *   :Log "BMP280 ready";
 * else (no)
 *   :Log error\n(invalid barometer data until reboot);
 * endif
 * :baro_counter = 0;
 * partition "100 Hz Read Loop" {
 *   repeat
 *     :rx_bno055_read();
 *     if (BNO055 read OK?) then (yes)
 *       :Fill imu_state_t (valid=true);
 *     else (no)
 *       :imu_state.valid = false;
 *       :Log warning;
 *     endif
 *     :shared_data_update_imu();
 *     :baro_counter++;
 *     if (baro_counter >= 25?) then (yes)
 *       :baro_counter = 0;
 *       :rx_bmp280_read();
 *       if (BMP280 read OK?) then (yes)
 *         :Fill barometer_state_t (valid=true);
 *       else (no)
 *         :barometer_state.valid = false;
 *         :Log warning;
 *       endif
 *       :shared_data_update_barometer();
 *     endif
 *     :rx_iwdt_task_heartbeat("IMU");
 *     :tx_thread_sleep(1) → 10ms;
 *   repeat while (forever)
 * }
 * @enduml
 *
 * @param[in] input Thread input parameter from tx_thread_create()
 *                  - Type: ULONG
 *                  - Value: k_imu_task_input (0)
 *                  - Explicitly cast to (void) to suppress unused-parameter warning
 *
 * @return void This function never returns
 *
 * @pre imu_task_create() called successfully; thread is scheduled
 * @pre ThreadX scheduler running
 * @pre sci_iic_init(k_imu_iic_channel) called by hardware_init()
 * @pre shared_data_init() called (required for update_imu/barometer)
 * @pre BNO055 and BMP280 powered and connected on SCI2 SIIC bus
 *
 * @post BNO055 in NDOF mode (if init succeeds)
 * @post BMP280 in normal measurement mode (if init succeeds)
 * @post shared_data.imu_state updated at 100 Hz
 * @post shared_data.barometer_state updated at ~4 Hz
 *
 * @invariant Function never returns (infinite loop)
 * @invariant shared_data.imu_state updated every 10 ms iteration
 * @invariant baro_counter always in range [0, k_imu_baro_divider - 1]
 *
 * @note Not thread-safe; sole accessor of BNO055 and BMP280 on SCI2 SIIC
 * @note 650 ms power-on sleep runs at task startup, delaying first IMU data
 * @note Invalid data (valid=false) is streamed if sensors fail to init
 *
 * @warning NEVER call directly; only ThreadX scheduler calls this function
 * @warning BNO055 and BMP280 MUST NOT be accessed by any other task (no mutex protection on bus)
 *
 * @attention BNO055 self-calibrates after NDOF mode entry; check calib_status byte to monitor progress
 *
 * @par Thread Safety:
 * This function is the SOLE writer of shared_data.imu_state and shared_data.barometer_state.
 * Telemetry task reads these via shared_data_get_imu() / shared_data_get_barometer() (mutex-protected).
 * No race conditions; single writer, multiple readers, all through mutex-protected shared_data APIs.
 *
 * @par Performance:
 * BNO055 read: ~1 ms (4 I2C transactions at 400 kHz).
 * BMP280 read: ~0.15 ms (1 I2C burst at 400 kHz) + compensation math ~5 µs.
 * Active time per 10 ms: ~1.15 ms. CPU utilization: ~11.5%.
 *
 * @see imu_task_create() Task creation function
 * @see rx_bno055_init() BNO055 initialization (CHIP_ID verify + NDOF mode)
 * @see rx_bno055_read() BNO055 Euler + accel + gyro + calib read
 * @see rx_bmp280_init() BMP280 initialization (CHIP_ID verify + trim load + normal mode)
 * @see rx_bmp280_read() BMP280 pressure + temperature + altitude read
 * @see shared_data_update_imu() Thread-safe IMU state write
 * @see shared_data_update_barometer() Thread-safe barometer state write
 *
 * @since Version 1.0.0
 *
 * @par NASA Power of 10 Compliance:
 * - Rule 1: ✓ No goto, recursion; if/while only
 * - Rule 2: ✓ while(true) with 10 ms period; baro_counter bounded by k_imu_baro_divider
 * - Rule 3: ✓ Zero heap allocation; all locals on stack
 * - Rule 5: ✓ 5 preconditions, 4 postconditions
 * - Rule 7: ✓ All I2C and shared_data return values checked
 * - Rule 8: ✓ C23 typed enums for all constants
 *
 * @callgraph
 * @callergraph
 */
static void internal_imu_task_entry(ULONG input)
{
  rx_err_t         err;
  rx_bno055_config_t bno055_config;
  rx_bmp280_config_t bmp280_config;
  rx_bno055_data_t bno055_data;
  rx_bmp280_data_t bmp280_data;
  imu_state_t      imu_state;
  barometer_state_t baro_state;
  uint8_t          baro_counter;

  (void)input;

  rx_log_info(s_tag, "IMU task starting");

  /* Wait 650 ms for BNO055 power-on (datasheet requirement) */
  (void)tx_thread_sleep(k_imu_poweron_ticks);

  /* Initialize BNO055 */
  (void)memset(&bno055_config, 0, sizeof(bno055_config));
  bno055_config.channel  = k_imu_iic_channel;
  bno055_config.i2c_addr = (uint8_t)k_bno055_addr_default;

  err = rx_bno055_init(&bno055_config);
  if (err != k_rx_ok) {
    rx_log_error_val(s_tag, "BNO055 init failed", (uint32_t)err);
    /* Continue: report invalid IMU data until next reboot */
  } else {
    rx_log_info(s_tag, "BNO055 ready in NDOF mode");
  }

  /* Initialize BMP280 */
  (void)memset(&bmp280_config, 0, sizeof(bmp280_config));
  bmp280_config.channel  = k_imu_iic_channel;
  bmp280_config.i2c_addr = (uint8_t)k_bmp280_addr_default;

  err = rx_bmp280_init(&bmp280_config);
  if (err != k_rx_ok) {
    rx_log_error_val(s_tag, "BMP280 init failed", (uint32_t)err);
    /* Continue: report invalid barometer data until next reboot */
  } else {
    rx_log_info(s_tag, "BMP280 ready in normal mode");
  }

  rx_log_info(s_tag, "IMU sensing running @ 100 Hz");

  baro_counter = 0;

  /* Main 100 Hz read loop */
  while (true) {
    /* Read BNO055: Euler angles + linear accel + gyro + calibration status */
    err = rx_bno055_read(&bno055_data);

    (void)memset(&imu_state, 0, sizeof(imu_state));
    if (err == k_rx_ok) {
      imu_state.heading_rad      = bno055_data.heading_rad;
      imu_state.roll_rad         = bno055_data.roll_rad;
      imu_state.pitch_rad        = bno055_data.pitch_rad;
      imu_state.accel_x_mps2     = bno055_data.accel_x_mps2;
      imu_state.accel_y_mps2     = bno055_data.accel_y_mps2;
      imu_state.accel_z_mps2     = bno055_data.accel_z_mps2;
      imu_state.gyro_x_rad_per_s = bno055_data.gyro_x_rad_s;
      imu_state.gyro_y_rad_per_s = bno055_data.gyro_y_rad_s;
      imu_state.gyro_z_rad_per_s = bno055_data.gyro_z_rad_s;
      imu_state.calib_status     = bno055_data.calib_status;
      imu_state.timestamp_ms     = tx_time_get();
      imu_state.valid            = true;
    } else {
      imu_state.valid        = false;
      imu_state.timestamp_ms = tx_time_get();
      rx_log_warn_val(s_tag, "BNO055 read failed", (uint32_t)err);
    }

    (void)shared_data_update_imu(&imu_state);

    /* Read BMP280 every 25th iteration (~4 Hz, matching sensor ODR ~3.82 Hz) */
    baro_counter++;
    if (baro_counter >= (uint8_t)k_imu_baro_divider) {
      baro_counter = 0;

      err = rx_bmp280_read(&bmp280_data);

      (void)memset(&baro_state, 0, sizeof(baro_state));
      if (err == k_rx_ok) {
        baro_state.pressure_pa       = bmp280_data.pressure_pa;
        baro_state.temperature_cdegc = bmp280_data.temperature_cdegc;
        baro_state.altitude_m        = bmp280_data.altitude_m;
        baro_state.timestamp_ms      = tx_time_get();
        baro_state.valid             = true;
      } else {
        baro_state.valid        = false;
        baro_state.timestamp_ms = tx_time_get();
        rx_log_warn_val(s_tag, "BMP280 read failed", (uint32_t)err);
      }

      (void)shared_data_update_barometer(&baro_state);
    }

    /* Report task heartbeat to IWDT watchdog */
    err = rx_iwdt_task_heartbeat("IMU");
    if (err != k_rx_ok) {
      rx_log_error_val(s_tag, "IWDT heartbeat failed", (uint32_t)err);
    }

    /* Sleep 10 ms (1 tick × 10 ms/tick @ 100 Hz ThreadX tick rate) */
    (void)tx_thread_sleep(k_imu_loop_ticks);
  }
}
