/**
 * @file serial_bringup_task.c
 * @brief Temporary ASCII Line-Protocol Bring-Up Task for SLAM MVP
 *
 * @details
 * Owns SCI9 (Cypress CY7C65213 bridge -> /dev/ttyACM0 on Pi5) and implements
 * the ASCII line protocol described in serial_bringup_task.h. This file is
 * DELIBERATELY SIMPLE -- it exists only for the SLAM MVP demo on 2026-04-22
 * so we can skip the framed nanopb / CRC-32 / HARQ / session stack while
 * we tune the ROS2 side.
 *
 * # Wire protocol (locked)
 *
 * - RX (Pi5 -> MCU): "V <fl> <fr> <bl> <br>\n"
 *   Four space-separated ASCII floats in m/s. Lines longer than
 *   k_serial_max_line_len (80) bytes are silently dropped, the partial
 *   buffer is reset, and "# overrun" is logged.
 * - TX (MCU -> Pi5): "E <fl> <fr> <bl> <br> <ms>\n"
 *   Four int16 encoder tcnt values (MTU1, MTU2, TPU1, TPU2 in that fixed
 *   wire order) followed by the unsigned ms ThreadX tick. The Pi5 side
 *   knows about any harness quirks -- this firmware just reports raw
 *   tcnt in the fixed MTU1/MTU2/TPU1/TPU2 slot order.
 * - TX (MCU -> Pi5): "M <d_fl> <d_fr> <d_bl> <d_br>
 *                        <f_fl> <f_fr> <f_bl> <f_br> <ms>\n"
 *   Per-motor duty cycle in tenths of a percent (int16, divide by 10
 *   on host; signed so reverse duty is negative), then per-motor
 *   DRV8263 fault byte (uint8). Wire slot order [FL FR BL BR] matches
 *   the E and V lines. Source: shared_data_get_motor_state(). 20 Hz.
 * - TX (MCU -> Pi5): "I <qw> <qx> <qy> <qz> <roll> <pitch> <heading>
 *                       <gx> <gy> <gz> <ax> <ay> <az> <ms>\n"
 *   Raw int16 scaled values straight from imu_state_t. Host divides:
 *     quat  / k_imu_scale_quat  (= 16384) to get unit quaternion
 *     euler / k_imu_scale_euler (=    16) to get degrees
 *     gyro  / k_imu_scale_gyro  (=    16) to get deg/s
 *     accel / k_imu_scale_acc   (=   100) to get m/s^2
 *   Emitted at 20 Hz regardless of imu_state.valid; if the BNO055 has
 *   not been read successfully yet the fields are zero.
 *
 * # Control-path safety
 *
 * motor_control_task.c (the bring-up while-loop around line 1697) treats
 * `cmd.valid == false` as 0% duty for every motor. We exploit that by
 * pushing a zero, valid=false command on the watchdog edge -- we do NOT
 * need to touch rx_motor_* directly from this task.
 *
 * # Restoring framed comms
 *
 * To restore the framed nanopb path:
 *   1. In main.c internal_create_system_tasks(), uncomment the
 *      comm_task_create() + telemetry_task_create() lines.
 *   2. Comment out the serial_bringup_task_create() line.
 *   3. Rebuild.
 * No source in comm_task.c / telemetry_task.c / rx_uart_comm / rx_session
 * was modified, so this is a pure wiring flip in main.c.
 *
 * @see motor_control_task.c Consumer of motor_command_t.valid
 * @see uart.c SCI9 RXI9 ring buffer that uart_getc_channel() drains
 *
 * @copyright Copyright (c) 2026 Locked Inc.
 * SPDX-License-Identifier: MIT
 */

#include "serial_bringup_task.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware.h"          /* uart_getc_channel, k_uart_channel_9, k_uart_debug_channel */
#include "rx72n_mtu_regs.h"    /* mtu1(), mtu2() */
#include "rx72n_sci_regs.h"    /* sci9() for diag SCR/SSR readback */
#include "rx72n_tpu_regs.h"    /* tpu1(), tpu2() */
#include "rx_check.h"          /* RX_ASSERT */
#include "rx_err.h"            /* rx_err_t, k_rx_err_* */
#include "rx_iwdt.h"           /* rx_iwdt_task_heartbeat */
#include "rx_log.h"            /* rx_log_info / warn / error */
#include "shared_data.h"       /* motor_command_t, motor_state_t, imu_state_t, setters + getters */
#include "tx_api.h"            /* TX_THREAD, tx_thread_create, tx_time_get */

/* =============================================================================
 * Task configuration constants (C23 typed enums, NO magic numbers)
 * =============================================================================
 */

/**
 * @enum serial_task_constants_t
 * @brief ThreadX thread configuration and loop timing for the bring-up task
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  k_serial_stack_size = 4096, /**< Stack bytes: snprintf+strtof float locals need room */
  k_serial_task_input = 0,    /**< tx_thread_create input arg (unused) */
} serial_task_constants_t;

/**
 * @enum serial_small_constants_t
 * @brief Small 8-bit tunables (priority, ticks, buffer sizes)
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_serial_priority = 5,               /**< Match old comm_task priority 5 (highest app prio) */
  k_serial_max_line_len = 80,          /**< Drop lines longer than this (inc. terminator) */
  k_serial_watchdog_ticks = 20,        /**< 200 ms at 10 ms/tick = 20 ticks */
  k_serial_telemetry_period_ticks = 5, /**< 50 Hz emit inside a 100 Hz outer loop */
  k_serial_outer_loop_sleep_ticks = 1, /**< 1 tick = 10 ms -> 100 Hz outer */
  k_serial_boot_delay_ticks = 20,      /**< Match motor_control_task 200 ms start-up delay */
  k_serial_num_motors = 4,             /**< FL, FR, BL, BR */
} serial_small_constants_t;

/**
 * @enum serial_tx_buf_constants_t
 * @brief Telemetry snprintf buffer sizing
 * @since Version 1.0.0
 */
typedef enum : uint16_t {
  /* Worst-case line is "I ..." with 13 int16 fields + 1 uint32 ms.
   * Each int16 prints up to 7 chars ("-32768 "), uint32 ms up to 11,
   * plus 2-char prefix and CRLF: ~110 chars. Round to 160 for slack. */
  k_serial_tx_buf_size = 160,
} serial_tx_buf_constants_t;

/**
 * @enum serial_duty_scale_t
 * @brief Scale applied to duty_cycle_percent (float) before wire emission.
 * @details Host divides by this to recover percent with 1-decimal precision.
 * Kept as a typed enum to stay inside the NASA-10 / STAR no-magic-number rule.
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_serial_duty_scale = 10, /**< Wire value = (int)(duty_percent * 10) */
} serial_duty_scale_t;

/** @brief Multiply tx_time_get() ticks by this to get milliseconds (10ms tick). */
typedef enum : uint8_t {
  k_serial_ms_per_tick = 10,
} serial_time_constants_t;

/**
 * @enum serial_ascii_t
 * @brief ASCII control characters used by the parser
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_ascii_cr = 0x0D,        /**< Carriage return */
  k_ascii_lf = 0x0A,        /**< Line feed */
  k_ascii_space = 0x20,     /**< Space (lowest printable) */
  k_ascii_tilde = 0x7E,     /**< Tilde (highest printable) */
  k_ascii_hash = 0x23,      /**< '#' -- line treated as comment and ignored */
  k_ascii_cmd_velocity = 0x56, /**< 'V' -- velocity command prefix */
} serial_ascii_t;

/**
 * @enum serial_motor_slot_t
 * @brief Wire-order motor slots parsed from "V fl fr bl br"
 *
 * @details
 * These are the POSITIONS in the ASCII line, which are then mapped to the
 * motor_command_t.target_velocity_mps[] array slots using the existing
 * motor_index_t layout (k_motor_front_left=0, _front_right=1, _back_right=2,
 * _back_left=3). The Pi5 side sends the values as FL FR BL BR; we permute
 * into the FW's [FL FR BR BL] order when storing.
 *
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_wire_slot_fl = 0, /**< First float in "V ..." line is front-left */
  k_wire_slot_fr = 1, /**< Second float is front-right */
  k_wire_slot_bl = 2, /**< Third float is back-left */
  k_wire_slot_br = 3, /**< Fourth float is back-right */
} serial_motor_slot_t;

/**
 * @enum serial_motor_fw_idx_t
 * @brief Firmware motor_command_t array indices (mirrors motor_index_t)
 *
 * @details
 * Duplicated here to avoid pulling motor_control_task.h into this file, which
 * would cross-couple two otherwise independent tasks. The canonical values
 * live in motor_control_task.c `motor_index_t` (FL=0, FR=1, BR=2, BL=3) and
 * MUST stay in sync.
 *
 * @see motor_control_task.c motor_index_t Canonical layout
 * @since Version 1.0.0
 */
typedef enum : uint8_t {
  k_fw_idx_front_left = 0, /**< target_velocity_mps[0] */
  k_fw_idx_front_right = 1, /**< target_velocity_mps[1] */
  k_fw_idx_back_right = 2, /**< target_velocity_mps[2] */
  k_fw_idx_back_left = 3, /**< target_velocity_mps[3] */
} serial_motor_fw_idx_t;

/* =============================================================================
 * Static state
 * =============================================================================
 */

/** @brief ThreadX thread control block for SerialBU. */
static TX_THREAD s_serial_thread;

/** @brief Static thread stack (no dynamic allocation). */
static uint8_t s_serial_stack[k_serial_stack_size];

/** @brief Task creation guard flag. */
static bool s_serial_created = false;

/** @brief Log tag shared by every rx_log_* call in this module. */
static const char* const s_tag = "SBRINGUP";

/** @brief Accumulating RX line buffer (ASCII). Reset on \\n / \\r / overflow. */
static char s_line[k_serial_max_line_len];

/** @brief Current length of s_line (never reaches k_serial_max_line_len). */
static uint8_t s_line_len = 0;

/** @brief ThreadX tick of the last valid "V ..." command, for 200 ms watchdog. */
static ULONG s_last_cmd_tick = 0;

/** @brief True iff most-recent state was "have valid cmd" (watchdog edge detect). */
static bool s_last_valid = false;

/** @brief Monotonic sequence number assigned to each accepted command. */
static uint32_t s_seq = 0;

/** @brief DIAG: total bytes drained from RX since boot. */
static uint32_t s_total_rx_bytes = 0;

/** @brief DIAG: total V lines parsed since boot. */
static uint32_t s_total_v_parsed = 0;

/** @brief DIAG: total complete lines (any type) since boot. */
static uint32_t s_total_lines = 0;

/* =============================================================================
 * Forward declarations for static helpers
 * =============================================================================
 */

static void internal_task_entry(ULONG input);
static void internal_drain_rx(void);
static void internal_handle_line(const char* line, uint8_t len);
static void internal_emit_telemetry(void);
static void internal_emit_motor_telemetry(void);
static void internal_emit_imu_telemetry(void);
static void internal_check_watchdog(void);

/* =============================================================================
 * Public API
 * =============================================================================
 */

rx_err_t serial_bringup_task_create(void)
{
  RX_ASSERT(!s_serial_created, "serial_bringup_task already created");
  if (s_serial_created) {
    return k_rx_err_invalid_state;
  }

  const UINT tx_status = tx_thread_create(&s_serial_thread,
                                          "SerialBU",
                                          internal_task_entry,
                                          (ULONG)k_serial_task_input,
                                          s_serial_stack,
                                          (ULONG)k_serial_stack_size,
                                          (UINT)k_serial_priority,
                                          (UINT)k_serial_priority,
                                          TX_NO_TIME_SLICE,
                                          TX_AUTO_START);

  if (tx_status != TX_SUCCESS) {
    rx_log_error_val(s_tag, "Thread create failed", (uint32_t)tx_status);
    return k_rx_err_rtos_thread_create;
  }

  s_serial_created = true;
  rx_log_info(s_tag, "SerialBU task created");
  return k_rx_ok;
}

/* =============================================================================
 * Task entry + main loop
 * =============================================================================
 */

/**
 * @brief ThreadX entry function -- runs forever, drives the ASCII protocol
 *
 * @details
 * Sleeps briefly so other init tasks settle, then loops forever at
 * ~100 Hz: drain RX, evaluate watchdog, emit telemetry every 5th tick.
 *
 * @param[in] input Unused (ThreadX convention)
 *
 * @return void (ThreadX entry functions are void)
 *
 * @pre uart_debug_init() has initialized SCI9 before the kernel started.
 * @pre shared_data_init() has succeeded.
 * @post Drains RX / emits TX forever; never returns.
 *
 * @note Not thread-safe; single owner of SCI9 during MVP.
 *
 * @since Version 1.0.0
 */
static void internal_task_entry(ULONG input)
{
  (void)input;

  (void)tx_thread_sleep((ULONG)k_serial_boot_delay_ticks);
  s_last_cmd_tick = tx_time_get();
  uart_debug_puts("# serial_bringup_task ready\r\n");

  uint32_t tick = 0;
  while (true) {
    internal_drain_rx();
    internal_check_watchdog();
    if ((tick % (uint32_t)k_serial_telemetry_period_ticks) == 0U) {
      internal_emit_telemetry();
      internal_emit_motor_telemetry();
      internal_emit_imu_telemetry();
    }
    /* Feed the SerialBU IWDT slot every 10 ms. Registered from main.c
     * (replaces CommTask now that comm_task is dormant). Timeout is
     * 128 ms; 10 ms feed cadence leaves 12x margin. */
    (void)rx_iwdt_task_heartbeat("SerialBU");
    tick++;
    (void)tx_thread_sleep((ULONG)k_serial_outer_loop_sleep_ticks);
  }
}

/* =============================================================================
 * RX path
 * =============================================================================
 */

/**
 * @brief Pull every available RX byte from SCI9 and assemble ASCII lines
 *
 * @details
 * Drains the SCI9 RX ring (fed by the RXI9 ISR in libs/rx_hal/src/uart.c)
 * until empty. '\\n' or '\\r' terminates the current line; lines are
 * passed to internal_handle_line(). Non-printable bytes are dropped.
 * Overflow (line would exceed k_serial_max_line_len-1) resets the buffer
 * and emits "# overrun".
 *
 * @return void
 *
 * @pre SCI9 initialized via uart_debug_init()
 * @post s_line_len is always strictly less than k_serial_max_line_len
 *
 * @note Called at 100 Hz from internal_task_entry.
 *
 * @since Version 1.0.0
 */
static void internal_drain_rx(void)
{
  /* Clear sticky SCI9 RX error flags (FER/PER/ORER) so RDRF can latch.
   * Without this, a single framing error during boot/enum locks the
   * receiver -- the RXI9 ISR never fires and the ring stays empty.
   * Bit mask 0x38 = ORER|FER|PER per RX72N HUM 34.2.7. Read-modify-write
   * of SSR with the error bits cleared (TDRE/RDRF/TEND preserved). */
  {
    const uint8_t ssr = sci9()->ssr;
    if ((ssr & 0x38U) != 0U) {
      sci9()->ssr = (uint8_t)(ssr & ~0x38U);
    }
  }

  while (true) {
    char byte_c = 0;
    const rx_err_t err =
      uart_getc_channel((uart_channel_t)k_uart_debug_channel, &byte_c);
    if (err == k_rx_err_empty) {
      break;
    }
    if (err != k_rx_ok) {
      /* Transient UART error -- drop byte and keep draining. */
      continue;
    }

    s_total_rx_bytes++;
    const uint8_t byte_u = (uint8_t)byte_c;

    if ((byte_u == (uint8_t)k_ascii_lf) || (byte_u == (uint8_t)k_ascii_cr)) {
      if (s_line_len > 0U) {
        s_total_lines++;
        internal_handle_line(s_line, s_line_len);
      }
      s_line_len = 0U;
      continue;
    }

    if (byte_u < (uint8_t)k_ascii_space) {
      continue;
    }
    if (byte_u > (uint8_t)k_ascii_tilde) {
      continue;
    }

    if (s_line_len >= (uint8_t)(k_serial_max_line_len - 1U)) {
      uart_debug_puts("# overrun\r\n");
      rx_log_warn(s_tag, "line overrun, buffer reset");
      s_line_len = 0U;
      continue;
    }

    s_line[s_line_len] = byte_c;
    s_line_len++;
  }
}

/**
 * @brief Parse one ASCII line and push any resulting motor command
 *
 * @details
 * Only "V fl fr bl br" lines are accepted. '#' prefix is treated as a
 * comment and ignored silently. Parse errors are logged at warn level
 * and the line is dropped. On success, a motor_command_t is written to
 * shared_data with valid=true; motor_control_task will pick it up on
 * its next 10 ms iteration.
 *
 * @param[in] line Pointer to line buffer (NOT null-terminated; use len)
 * @param[in] len  Number of valid bytes in line (1 <= len < k_serial_max_line_len)
 *
 * @return void
 *
 * @pre line != NULL and len > 0 (callers guarantee this)
 * @post On "V ..." parse success: shared_data motor_command_t updated,
 *       s_last_cmd_tick bumped, s_seq incremented.
 * @post On parse failure: state unchanged, warn logged.
 *
 * @since Version 1.0.0
 */
static void internal_handle_line(const char* line, uint8_t len)
{
  if ((line == NULL) || (len == 0U)) {
    return;
  }
  /* Comment / non-command lines: ignore silently. */
  if ((uint8_t)line[0] != (uint8_t)k_ascii_cmd_velocity) {
    return;
  }

  /* NUL-terminate a scratch copy so strtof is safe. Local array is
   * 80 bytes -- well under task stack budget. */
  char scratch[k_serial_max_line_len];
  (void)memcpy(scratch, line, len);
  scratch[len] = '\0';

  /* Advance past the 'V'. */
  const char* cursor = &scratch[1];
  float parsed[k_serial_num_motors] = {0.0F, 0.0F, 0.0F, 0.0F};

  for (uint8_t i = 0; i < (uint8_t)k_serial_num_motors; i++) {
    char* endptr = NULL;
    /* strtof() in newlib-nano returns NaN on this build; strtod() works. */
    const double v = strtod(cursor, &endptr);
    if ((endptr == NULL) || (endptr == cursor)) {
      rx_log_warn(s_tag, "parse fail");
      return;
    }
    parsed[i] = (float)v;
    cursor = endptr;
  }

  motor_command_t cmd = {0};
  /* Map wire slots -> firmware command indices. Wire = FL FR BL BR,
   * firmware = FL FR BR BL (motor_index_t). */
  cmd.target_velocity_mps[k_fw_idx_front_left] = parsed[k_wire_slot_fl];
  cmd.target_velocity_mps[k_fw_idx_front_right] = parsed[k_wire_slot_fr];
  cmd.target_velocity_mps[k_fw_idx_back_left] = parsed[k_wire_slot_bl];
  cmd.target_velocity_mps[k_fw_idx_back_right] = parsed[k_wire_slot_br];
  s_seq++;
  cmd.sequence = s_seq;
  cmd.timestamp_ms = (uint32_t)tx_time_get();
  cmd.valid = true;

  const rx_err_t err = shared_data_set_motor_command(&cmd);
  if (err != k_rx_ok) {
    rx_log_error_val(s_tag, "set_motor_command failed", (uint32_t)err);
    return;
  }

  s_last_cmd_tick = tx_time_get();
  s_last_valid = true;
  s_total_v_parsed++;
}

/* =============================================================================
 * TX path
 * =============================================================================
 */

/**
 * @brief Build and send one "E ..." telemetry line over SCI9
 *
 * @details
 * Reads MTU1/MTU2/TPU1/TPU2 tcnt registers (16-bit hardware counters,
 * interpreted as int16 so wrap shows negative values) and the current
 * ThreadX tick, formats them as ASCII, and writes via uart_debug_puts().
 *
 * @return void
 *
 * @pre MTU1/MTU2/TPU1/TPU2 encoder hardware already initialized (done by
 *      motor_control_task's internal_init_encoders()). If not, we still
 *      emit -- the counts just stay 0.
 * @post One ASCII line written to SCI9 TX FIFO.
 *
 * @since Version 1.0.0
 */
static void internal_emit_telemetry(void)
{
  const int16_t enc_fl = (int16_t)mtu1()->tcnt;
  const int16_t enc_fr = (int16_t)mtu2()->tcnt;
  const int16_t enc_tpu1 = (int16_t)tpu1()->tcnt;
  const int16_t enc_tpu2 = (int16_t)tpu2()->tcnt;
  const uint32_t now_ms = (uint32_t)tx_time_get() * (uint32_t)k_serial_ms_per_tick;

  char buf[k_serial_tx_buf_size];
  const int written =
    snprintf(buf,
             (size_t)k_serial_tx_buf_size,
             "E %" PRId16 " %" PRId16 " %" PRId16 " %" PRId16 " %" PRIu32 "\n",
             enc_fl,
             enc_fr,
             enc_tpu1,
             enc_tpu2,
             now_ms);
  if (written <= 0) {
    return;
  }
  uart_debug_puts(buf);
}

/**
 * @brief Build and send one "M ..." motor-telemetry line over SCI9
 *
 * @details
 * Reads motor_state_t from shared_data and emits per-motor duty cycle
 * (tenths of a percent, signed int) and DRV8263 fault flags (uint8).
 * Motor index order on the wire is the firmware's motor_index_t order
 * (FL=0, FR=1, BR=2, BL=3) -- same as the E line.
 *
 * If shared_data_get_motor_state() fails (non-blocking try returned
 * busy before motor_control_task ran its first iteration), the line is
 * skipped silently so we don't flood the host with stale zeros.
 *
 * @return void
 * @pre motor_control_task has been created and populates motor_state_t
 * @post One ASCII "M ..." line written to SCI9 on success; nothing on busy
 * @since Version 1.0.0
 */
static void internal_emit_motor_telemetry(void)
{
  motor_state_t state;
  const rx_err_t err = shared_data_get_motor_state(&state);
  if (err != k_rx_ok) {
    return;
  }

  /* Wire slot order [FL FR BL BR] matches E and V -- NOT the firmware
   * motor_index_t order [FL FR BR BL]. We index shared_data by
   * k_fw_idx_* and fill the wire slots in the correct permutation. */
  const int16_t duty_fl =
    (int16_t)(state.duty_cycle_percent[k_fw_idx_front_left] * (float)k_serial_duty_scale);
  const int16_t duty_fr =
    (int16_t)(state.duty_cycle_percent[k_fw_idx_front_right] * (float)k_serial_duty_scale);
  const int16_t duty_bl =
    (int16_t)(state.duty_cycle_percent[k_fw_idx_back_left] * (float)k_serial_duty_scale);
  const int16_t duty_br =
    (int16_t)(state.duty_cycle_percent[k_fw_idx_back_right] * (float)k_serial_duty_scale);
  const uint8_t fault_fl = state.fault_flags[k_fw_idx_front_left];
  const uint8_t fault_fr = state.fault_flags[k_fw_idx_front_right];
  const uint8_t fault_bl = state.fault_flags[k_fw_idx_back_left];
  const uint8_t fault_br = state.fault_flags[k_fw_idx_back_right];
  const uint32_t now_ms = (uint32_t)tx_time_get() * (uint32_t)k_serial_ms_per_tick;

  char buf[k_serial_tx_buf_size];
  const int written =
    snprintf(buf,
             (size_t)k_serial_tx_buf_size,
             "M %" PRId16 " %" PRId16 " %" PRId16 " %" PRId16
             " %" PRIu8 " %" PRIu8 " %" PRIu8 " %" PRIu8
             " %" PRIu32 "\n",
             duty_fl,
             duty_fr,
             duty_bl,
             duty_br,
             fault_fl,
             fault_fr,
             fault_bl,
             fault_br,
             now_ms);
  if (written <= 0) {
    return;
  }
  uart_debug_puts(buf);
}

/**
 * @brief Build and send one "I ..." IMU-telemetry line over SCI9
 *
 * @details
 * Reads imu_state_t from shared_data and emits quaternion, Euler angles,
 * gyroscope, and linear-acceleration fields as raw int16 scaled values
 * (same scaling as the BNO055 register map). Host applies the divisors
 * in k_imu_scale_* to recover SI/degree units.
 *
 * Emission is unconditional on imu_state.valid; if imu_task has never
 * completed a read the fields will all be zero. This keeps the host-side
 * parser's cadence predictable at 20 Hz during BNO055 calibration drift.
 *
 * @return void
 * @pre imu_task has been created
 * @post One ASCII "I ..." line written to SCI9 on success
 * @since Version 1.0.0
 */
static void internal_emit_imu_telemetry(void)
{
  imu_state_t imu;
  const rx_err_t err = shared_data_get_imu(&imu);
  if (err != k_rx_ok) {
    return;
  }

  const uint32_t now_ms = (uint32_t)tx_time_get() * (uint32_t)k_serial_ms_per_tick;

  char buf[k_serial_tx_buf_size];
  const int written =
    snprintf(buf,
             (size_t)k_serial_tx_buf_size,
             "I %" PRId16 " %" PRId16 " %" PRId16 " %" PRId16
             " %" PRId16 " %" PRId16 " %" PRId16
             " %" PRId16 " %" PRId16 " %" PRId16
             " %" PRId16 " %" PRId16 " %" PRId16
             " %" PRIu32 "\n",
             imu.quat_w,
             imu.quat_x,
             imu.quat_y,
             imu.quat_z,
             imu.roll_deg16,
             imu.pitch_deg16,
             imu.heading_deg16,
             imu.gyro_x_dps16,
             imu.gyro_y_dps16,
             imu.gyro_z_dps16,
             imu.lin_acc_x,
             imu.lin_acc_y,
             imu.lin_acc_z,
             now_ms);
  if (written <= 0) {
    return;
  }
  uart_debug_puts(buf);
}

/* =============================================================================
 * Watchdog path
 * =============================================================================
 */

/**
 * @brief Push a zero, valid=false motor command if RX has been silent 200 ms
 *
 * @details
 * Edge-triggered: we only publish the safe command once per silent
 * period, when s_last_valid is still true. motor_control_task's
 * bring-up loop treats valid=false as 0% duty on all 4 motors.
 *
 * @return void
 *
 * @pre s_last_cmd_tick initialized in internal_task_entry()
 * @post On watchdog trip: s_last_valid = false and a zero motor_command_t
 *       is written to shared_data.
 * @post Otherwise: state unchanged.
 *
 * @since Version 1.0.0
 */
static void internal_check_watchdog(void)
{
  const ULONG now = tx_time_get();
  const ULONG elapsed = now - s_last_cmd_tick;
  if (elapsed < (ULONG)k_serial_watchdog_ticks) {
    return;
  }
  if (!s_last_valid) {
    return;
  }

  motor_command_t safe = {0};
  /* valid=false by the {0} initializer -- motor_control_task maps
   * valid=false -> 0% duty for every wheel. */
  const rx_err_t err = shared_data_set_motor_command(&safe);
  if (err != k_rx_ok) {
    rx_log_error_val(s_tag, "wd set_cmd fail", (uint32_t)err);
    return;
  }
  s_last_valid = false;
  rx_log_warn(s_tag, "rx silent 200 ms -> zero cmd");
}
