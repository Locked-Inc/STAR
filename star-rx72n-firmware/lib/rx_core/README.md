# rx_core Library - Defensive Coding Modules

This library provides core defensive coding modules for safety-critical embedded systems following NASA Power of 10 rules.

## Table of Contents

- [Overview](#overview)
- [Watchdog System](#watchdog-system)
  - [Hardware IWDT](#hardware-iwdt)
  - [Task-Level Monitoring](#task-level-monitoring)
  - [Integration Strategy](#integration-strategy)
- [Register Guard](#register-guard)
  - [Purpose](#purpose)
  - [Integration Strategy](#integration-strategy-1)
  - [Protected Registers](#protected-registers)
- [Performance Impact](#performance-impact)
- [Error Handling Policy](#error-handling-policy)
- [Example Usage](#example-usage)

## Overview

The rx_core library provides two critical defensive coding modules:

1. **Independent Watchdog Timer (IWDT)** - Hardware watchdog with task-level deadlock detection
2. **Register Guard** - Protection against ESD/EMI-induced register corruption

Both modules are designed for zero-dynamic-allocation safety-critical systems.

## Watchdog System

### Hardware IWDT

The RX72N Independent Watchdog Timer (IWDT) provides hardware-level system recovery from software hangs.

**Key Features:**
- 120 kHz dedicated oscillator (independent of main clock)
- Configurable timeout periods (128ms to 16.4s)
- Full chip reset on timeout (not NMI)
- Continues counting during WAIT/STOP modes

**API:**
```c
#include "rx_iwdt.h"

// Initialize watchdog with 1000ms timeout
rx_err_t ret = rx_iwdt_init(1000);

// Feed watchdog (must be called regularly)
rx_iwdt_feed();

// Check if last reset was caused by watchdog
if (rx_iwdt_was_reset()) {
    // Log watchdog reset event
}
```

**Hardware Configuration:**
- Clock: 120 kHz IWDT-dedicated oscillator
- Timeout calculation: `(cycles * divisor) / 120000`
- Register base: `0x00088030`

### Task-Level Monitoring

In addition to hardware watchdog, the IWDT module provides task-level deadlock detection.

**Why Task Monitoring?**
- Hardware watchdog only detects total system hangs
- Individual task deadlocks can go undetected if other tasks keep feeding watchdog
- Task monitoring enables early detection of priority inversion and deadlocks

**Task Monitoring API:**
```c
#include "rx_iwdt.h"

// 1. Register task for monitoring (called once at task startup)
rx_err_t ret = rx_iwdt_register_task("Motor_Controller", 12);
if (ret != k_rx_ok) {
    rx_log_error("motor", "Failed to register with watchdog");
}

// 2. Send periodic heartbeat (called every task cycle)
void task_loop(void) {
    while (1) {
        // ... task work ...

        rx_iwdt_task_heartbeat("Motor_Controller");
        tx_thread_sleep(1);  // 4ms period at 250 Hz
    }
}

// 3. Check for task deadlocks (called by monitoring task)
rx_err_t ret = rx_iwdt_check_tasks();
if (ret != k_rx_ok) {
    // One or more tasks have timed out
    const char* failed = rx_iwdt_get_failed_task();
    if (failed != NULL) {
        rx_log_error("watchdog", failed);
    }
}
```

**Task Monitoring Limits:**
- Maximum 8 monitored tasks (`k_iwdt_max_tasks`)
- Task names: 15 characters + null terminator
- All state statically allocated (no dynamic memory)

### Integration Strategy

**Recommended Approach:**

1. **Hardware IWDT** - Initialize early in `main()` with conservative timeout
2. **Motor_Controller (250 Hz)** - Feeds hardware watchdog every 4ms
3. **All Tasks** - Register and send heartbeats
4. **System_Health (1 Hz)** - Checks for task deadlocks

**Task Timeout Guidelines:**

| Task | Period | Timeout | Ratio |
|------|--------|---------|-------|
| Motor_Controller | 4ms | 12ms | 3x |
| Comm_Manager | 10ms | 30ms | 3x |
| Env_Monitor | 20ms | 60ms | 3x |
| System_Health | 1000ms | 3000ms | 3x |

**Why 3x period?**
- Allows for some jitter and preemption
- Detects real deadlocks (3 missed heartbeats)
- Prevents false positives from CPU load spikes

**Example Integration:**

```c
// In main.c
int main(void) {
    system_init();

    // Initialize hardware watchdog (1000ms timeout)
    rx_err_t ret = rx_iwdt_init(1000);
    if (ret != k_rx_ok) {
        uart_puts("IWDT init failed\r\n");
        while (1);
    }

    // Check if last reset was caused by watchdog
    if (rx_iwdt_was_reset()) {
        uart_puts("WARNING: Watchdog reset detected\r\n");

        // Get failed task name if available
        const char* failed = rx_iwdt_get_failed_task();
        if (failed != NULL) {
            uart_puts("Failed task: ");
            uart_puts(failed);
            uart_puts("\r\n");
        }
    }

    uart_init();
    tx_kernel_enter();

    while (1);
    return 0;
}

// In motor_controller.c
static void motor_controller_entry(ULONG input) {
    // Register with watchdog (3x period = 3 * 4ms = 12ms)
    rx_err_t ret = rx_iwdt_register_task("Motor_Controller", 12);
    if (ret != k_rx_ok) {
        rx_log_error("motor", "Failed to register with watchdog");
    }

    // Initialize motor subsystem
    ret = init_motor_subsystem();
    if (ret != k_rx_ok) {
        rx_log_error("motor", "Motor init failed");
        while (1) { tx_thread_sleep(100); }
    }

    // Main control loop (250 Hz)
    while (1) {
        control_loop();

        // Feed hardware watchdog (CRITICAL - prevents reset)
        rx_iwdt_feed();

        // Record task heartbeat for deadlock detection
        rx_iwdt_task_heartbeat("Motor_Controller");

        tx_thread_sleep(1);  // 4ms at 250 Hz
    }
}

// In system_health.c
static void system_health_entry(ULONG input) {
    // Register with watchdog (3x period = 3 * 1000ms = 3000ms)
    rx_err_t ret = rx_iwdt_register_task("System_Health", 3000);
    if (ret != k_rx_ok) {
        rx_log_error("health", "Failed to register with watchdog");
    }

    // Main monitoring loop (1 Hz)
    while (1) {
        poll_battery();
        update_diagnostics();

        // Check for task deadlocks (runs at low priority)
        ret = rx_iwdt_check_tasks();
        if (ret != k_rx_ok) {
            rx_log_error("health", "Task deadlock detected");

            // Log failed task for diagnostics
            const char* failed = rx_iwdt_get_failed_task();
            if (failed != NULL) {
                diagnostics_log_fault("watchdog", failed, ret);
            }
        }

        // Refresh register guard (see below)
        rx_register_guard_refresh();

        // Record task heartbeat
        rx_iwdt_task_heartbeat("System_Health");

        tx_thread_sleep(100);  // 1 Hz
    }
}
```

## Register Guard

### Purpose

The register guard module protects critical hardware registers from corruption caused by:
- **ESD (Electrostatic Discharge)** - Sudden voltage spikes
- **EMI (Electromagnetic Interference)** - Radio frequency interference
- **Cosmic rays** - High-energy particle strikes (rare but possible)

These events can cause bit flips in memory-mapped peripheral registers, leading to:
- GPIO direction changes (outputs become inputs)
- Interrupt priority corruption (priority inversion)
- Module stop control errors (peripherals unexpectedly disabled)

**Why This Matters:**
- Safety-critical motor control requires predictable GPIO behavior
- Corrupted interrupt priorities can cause deadlocks
- Module stop corruption can disable critical peripherals

### Integration Strategy

**Recommended Approach: 1 Hz Polling in System_Health Thread**

**Rationale:**
1. **ESD/EMI events are rare** - Seconds to minutes between events
2. **1 Hz refresh is sufficient** - Catches corruption before it causes problems
3. **Low priority** - Doesn't disrupt Motor_Controller (highest priority)
4. **Minimal overhead** - < 0.1% CPU time

**API:**
```c
#include "rx_register_guard.h"

// 1. Initialize at startup (captures golden values)
rx_err_t ret = rx_register_guard_init();
if (ret != k_rx_ok) {
    uart_puts("Register guard init failed\r\n");
}

// 2. Refresh periodically (restores corrupted registers)
void system_health_loop(void) {
    while (1) {
        // ... battery/diagnostics ...

        // Restore corrupted registers
        rx_register_guard_refresh();

        // Track corrections in diagnostics
        uint32_t corrections = rx_register_guard_get_correction_count();
        if (corrections > 0) {
            diagnostics_increment_register_correction();
            rx_register_guard_reset_count();
        }

        tx_thread_sleep(100);  // 1 Hz
    }
}
```

### Protected Registers

**GPIO Port Direction Registers (PDR):**
- Ports 0-5, A-E, J (100-pin LFQFP package)
- Controls input/output direction for each pin
- Corruption causes outputs to become inputs (motor control failure)

**Interrupt Controller Priority Registers (IPR):**
- All 256 interrupt vectors
- Controls interrupt priority (0-15)
- Corruption causes priority inversion (task deadlocks)

**Module Stop Control Registers (MSTPCRA-D):**
- Controls peripheral clock gating
- Bit 0 = running, 1 = stopped
- Corruption can disable critical peripherals (UART, timers, etc.)

**Example Register Guard Operation:**

```c
// Startup: Capture golden values
rx_register_guard_init();  // Saves PORT0.PDR = 0xFF (all outputs)

// ... system runs normally ...

// ESD event occurs: PORT0.PDR corrupted to 0x00 (all inputs!)

// Next refresh cycle (1 second later):
rx_register_guard_refresh();
// Detects: PORT0.PDR (0x00) != golden (0xFF)
// Restores: PORT0.PDR = 0xFF
// Logs: "Register corruption detected and corrected"
// Increments: correction counter
```

**Integration Example:**

```c
// In tx_application_define() (called before threads start)
void tx_application_define(void *first_unused_memory) {
    // Initialize shared state
    shared_state_init();

    // Initialize hardware
    hardware_init();

    // Initialize register guard (capture golden values)
    rx_err_t ret = rx_register_guard_init();
    if (ret != k_rx_ok) {
        uart_puts("Register guard init failed\r\n");
    }

    // Create threads
    motor_controller_create();
    comm_manager_create();
    env_monitor_create();
    system_health_create();
}

// In system_health.c
static void system_health_entry(ULONG input) {
    rx_log_info("health", "System_Health thread started");

    // Main loop (1 Hz)
    while (1) {
        poll_battery();
        update_diagnostics();

        // Restore corrupted registers (every 1 second)
        rx_register_guard_refresh();

        // Track register corrections in diagnostics
        uint32_t corrections = rx_register_guard_get_correction_count();
        if (corrections > 0) {
            diagnostics_increment_register_correction();
            rx_register_guard_reset_count();

            rx_log_error("health", "Register corruption detected");
        }

        // Check for task deadlocks
        rx_err_t ret = rx_iwdt_check_tasks();
        if (ret != k_rx_ok) {
            rx_log_error("health", "Task deadlock detected");
            diagnostics_log_fault("watchdog", "Task deadlock", ret);
        }

        rx_iwdt_task_heartbeat("System_Health");
        tx_thread_sleep(100);  // 1 Hz
    }
}
```

## Performance Impact

### Watchdog System

**Hardware IWDT:**
- Feed operation: ~10 CPU cycles (inline assembly)
- Memory: 0 bytes (statically allocated in module)
- Overhead: < 0.001% CPU time (called once per 4ms)

**Task Monitoring:**
- Register: ~100 CPU cycles (string copy + initialization)
- Heartbeat: ~50 CPU cycles (timestamp update)
- Check: ~200 CPU cycles per task (8 tasks max = 1600 cycles)
- Memory: 208 bytes static (8 tasks × 26 bytes each)
- Overhead: < 0.01% CPU time (1 Hz checking)

**Total Watchdog Overhead:**
- CPU: < 0.02% (negligible)
- Memory: 208 bytes BSS (0.02% of 1MB RAM)

### Register Guard

**Initialization:**
- Capture golden values: ~500 CPU cycles (one-time at startup)

**Refresh:**
- Check + restore: ~2000 CPU cycles per refresh (worst case)
- Protected registers: 12 GPIO ports + 256 IPRs + 4 MSTPCR = 272 registers
- Overhead: < 0.1% CPU time (1 Hz refresh)

**Memory:**
- Golden values: 272 bytes static (0.03% of 1MB RAM)
- Correction counter: 4 bytes static

**Total Register Guard Overhead:**
- CPU: < 0.1% (1 Hz refresh at low priority)
- Memory: 276 bytes BSS (0.03% of 1MB RAM)

### Combined System Overhead

| Module | CPU | Memory | Impact |
|--------|-----|--------|--------|
| Watchdog | < 0.02% | 208 bytes | Negligible |
| Register Guard | < 0.1% | 276 bytes | Negligible |
| **Total** | **< 0.15%** | **484 bytes** | **Acceptable** |

**Conclusion:** Defensive coding overhead is minimal (< 0.15% CPU, < 0.05% RAM) and provides critical safety features.

## Error Handling Policy

### Watchdog Errors

**Hardware IWDT Timeout:**
- Action: Full chip reset (hardware-enforced)
- Recovery: Firmware reboots, checks `rx_iwdt_was_reset()`
- Logging: Log reset event to diagnostics with timestamp
- Prevention: Ensure Motor_Controller feeds watchdog every 4ms

**Task Deadlock Detection:**
- Action: Log error with task name and timeout value
- Recovery: Continue monitoring (don't trigger hardware reset immediately)
- Logging: `diagnostics_log_fault()` with task name
- Prevention: Ensure all tasks send heartbeats within timeout period

**Task Registration Errors:**
```c
rx_err_t ret = rx_iwdt_register_task("MyTask", 100);
switch (ret) {
    case k_rx_ok:
        // Success
        break;
    case k_rx_err_invalid_arg:
        // NULL task name
        rx_log_error("task", "Invalid task name");
        break;
    case k_rx_err_no_mem:
        // Too many tasks (max 8)
        rx_log_error("task", "Too many tasks registered");
        break;
    case k_rx_err_exists:
        // Task already registered
        rx_log_error("task", "Task already registered");
        break;
}
```

### Register Guard Errors

**Initialization Failure:**
```c
rx_err_t ret = rx_register_guard_init();
if (ret != k_rx_ok) {
    // Log error but continue (register guard is defensive, not critical)
    rx_log_error("guard", "Register guard init failed");
    // System can still operate, just without register protection
}
```

**Register Corruption Detected:**
- Action: Restore corrupted register(s) from golden values
- Recovery: Automatic (no user intervention needed)
- Logging: Increment correction counter, log via diagnostics
- Prevention: Proper ESD/EMI protection in hardware design

**Refresh Errors:**
- Register guard refresh cannot fail (void return)
- All operations are memory-to-memory (no hardware access failures)
- Worst case: Corrupted register not detected until next refresh (1 second)

## Example Usage

### Complete System Integration

```c
// main.c
int main(void) {
    system_init();

    // Initialize hardware watchdog (1000ms timeout)
    rx_err_t ret = rx_iwdt_init(1000);
    if (ret != k_rx_ok) {
        uart_puts("IWDT init failed\r\n");
        while (1);
    }

    // Check for watchdog reset
    if (rx_iwdt_was_reset()) {
        uart_puts("WARNING: Watchdog reset\r\n");

        rx_iwdt_reset_cause_t cause = rx_iwdt_get_reset_cause();
        if (cause == k_iwdt_reset_underflow) {
            uart_puts("Cause: Counter underflow\r\n");
        } else if (cause == k_iwdt_reset_refresh_error) {
            uart_puts("Cause: Refresh error\r\n");
        }

        const char* failed = rx_iwdt_get_failed_task();
        if (failed != NULL) {
            uart_puts("Failed task: ");
            uart_puts(failed);
            uart_puts("\r\n");
        }

        rx_iwdt_clear_status();
    }

    uart_init();
    tx_kernel_enter();

    while (1);
    return 0;
}

// tx_application_define (called by ThreadX)
void tx_application_define(void *first_unused_memory) {
    (void)first_unused_memory;

    // Initialize shared state
    shared_state_init();

    // Initialize hardware
    hardware_init();

    // Initialize register guard (capture golden values)
    rx_err_t ret = rx_register_guard_init();
    if (ret != k_rx_ok) {
        uart_puts("Register guard init failed\r\n");
    }

    // Create application threads
    motor_controller_create();
    comm_manager_create();
    env_monitor_create();
    system_health_create();
}
```

### High-Priority Task (Motor Controller)

```c
// motor_controller.c
static void motor_controller_entry(ULONG input) {
    (void)input;

    rx_log_info("motor", "Motor_Controller thread started");

    // Register with watchdog (3x period = 12ms)
    rx_err_t ret = rx_iwdt_register_task("Motor_Controller", 12);
    if (ret != k_rx_ok) {
        rx_log_error("motor", "Failed to register with watchdog");
    }

    // Initialize motor subsystem
    ret = init_motor_subsystem();
    if (ret != k_rx_ok) {
        rx_log_error("motor", "Motor init failed");
        while (1) { tx_thread_sleep(100); }
    }

    rx_log_info("motor", "Entering main control loop");

    // Main control loop (250 Hz = 4ms period)
    while (1) {
        // 1. Check emergency stop
        shared_state_t* state = shared_state_get();
        UINT status = tx_mutex_get(&state->safety_mutex, TX_WAIT_FOREVER);
        bool emergency_stop = state->safety.emergency_stop;
        tx_mutex_put(&state->safety_mutex);

        if (emergency_stop) {
            // Hardware-level motor cutoff
            for (uint8_t i = 0; i < k_motor_count; i++) {
                rx_motor_emergency_stop(&s_motors[i]);
            }
            tx_thread_sleep(1);
            continue;
        }

        // 2. Read encoder feedback
        for (uint8_t i = 0; i < k_motor_count; i++) {
            rx_encoder_state_t* encoder = &s_encoder_states[i];
            rx_encoder_read_count((rx_mtu_channel_t)(i + 1), encoder);
        }

        // 3. Update shared encoder state
        status = tx_mutex_get(&state->encoder_mutex, TX_WAIT_FOREVER);
        // ... update encoder data ...
        tx_mutex_put(&state->encoder_mutex);

        // 4. Read motor setpoint
        status = tx_mutex_get(&state->setpoint_mutex, TX_WAIT_FOREVER);
        // ... read setpoint ...
        tx_mutex_put(&state->setpoint_mutex);

        // 5. Compute PID control
        for (uint8_t i = 0; i < k_motor_count; i++) {
            float control = rx_pid_compute(&s_pids[i], setpoint[i], measured[i], dt);
            rx_motor_set_duty_cycle(&s_motors[i], control);
        }

        // 6. Feed hardware watchdog (CRITICAL - prevents reset)
        rx_iwdt_feed();

        // 7. Record task heartbeat for deadlock detection
        rx_iwdt_task_heartbeat("Motor_Controller");

        // 8. Sleep until next cycle (4ms)
        tx_thread_sleep(1);
    }
}
```

### Low-Priority Task (System Health)

```c
// system_health.c
static void system_health_entry(ULONG input) {
    (void)input;

    rx_log_info("health", "System_Health thread started");

    // Register with watchdog (3x period = 3000ms)
    rx_err_t ret = rx_iwdt_register_task("System_Health", 3000);
    if (ret != k_rx_ok) {
        rx_log_error("health", "Failed to register with watchdog");
    }

    // Initialize battery communication
    ret = init_battery_comm();
    if (ret != k_rx_ok) {
        rx_log_error("health", "Battery init failed");
    }

    // Record startup time
    uint32_t startup_time_ms = tx_time_get();

    rx_log_info("health", "Entering main monitoring loop");

    // Main monitoring loop (1 Hz = 1000ms period)
    while (1) {
        // 1. Poll battery (voltage, SOC, current)
        poll_battery();

        // 2. Update diagnostics (uptime, counters)
        update_diagnostics();

        // 3. Restore corrupted registers (1 Hz is sufficient)
        rx_register_guard_refresh();

        // Track register corrections
        uint32_t corrections = rx_register_guard_get_correction_count();
        if (corrections > 0) {
            diagnostics_increment_register_correction();
            rx_register_guard_reset_count();
            rx_log_error("health", "Register corruption detected");
        }

        // 4. Check for task deadlocks
        ret = rx_iwdt_check_tasks();
        if (ret != k_rx_ok) {
            rx_log_error("health", "Task deadlock detected");

            const char* failed = rx_iwdt_get_failed_task();
            if (failed != NULL) {
                diagnostics_log_fault("watchdog", failed, ret);
            }
        }

        // 5. Record task heartbeat
        rx_iwdt_task_heartbeat("System_Health");

        // 6. Sleep until next cycle (1000ms)
        tx_thread_sleep(100);  // 100 ticks = 1 second at 100 Hz
    }
}
```

## References

- [RX72N Hardware Manual](https://www.renesas.com/en/products/rx72n) - IWDT specification (Section 25)
- [NASA Power of 10 Rules](https://en.wikipedia.org/wiki/The_Power_of_10:_Rules_for_Developing_Safety-Critical_Code) - Safety-critical coding standards
- [ThreadX Documentation](https://github.com/eclipse-threadx/rtos-docs) - RTOS API reference
- [STAR Project Documentation](../../docs/sections/06_nasa_power_of_10.tex) - Project-wide safety guidelines
