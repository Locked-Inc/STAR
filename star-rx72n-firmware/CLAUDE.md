# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Important

**This firmware follows the STAR project code style guide.** See the main project CLAUDE.md at `/Users/bsikar/Documents/git/STAR/CLAUDE.md` for complete style guidelines.

This document covers RX72N-specific requirements and differences from ESP32 firmware.

## Terminology Standard

**IMPORTANT:** This project uses inclusive terminology following OSHWA standards:

- **Controller/Peripheral** - NOT master/slave (I2C, SPI, 1-Wire)
- **COPI/CIPO** - NOT MOSI/MISO (Controller Out Peripheral In / Controller In Peripheral Out)
- **Primary/Main** - NOT master (for configuration structures)

Note: Renesas APIs and ThreadX may still use legacy terminology internally. Map these to our terminology in comments and documentation.

## Project Overview

**STAR RX72N Firmware** - ThreadX-based motor control firmware for Renesas RX72N microcontroller.

This firmware is part of the STAR (Simultaneous Tracking and Robotics) distributed robotics platform. It runs on the RX72N motor controller PCB and communicates with the Raspberry Pi 5 control system via SPI using Protocol Buffers.

### Key Differences from ESP32 Firmware

| Feature | ESP32-S3 Firmware | RX72N Firmware |
|---------|-------------------|----------------|
| **RTOS** | FreeRTOS | ThreadX (Azure RTOS / Eclipse ThreadX) |
| **MCU** | ESP32-S3 (Xtensa dual-core) | RX72N (RXv3 240MHz single-core) |
| **GPIO** | 35 usable (with mux/decoder) | **182 direct** (no mux needed!) |
| **Build System** | PlatformIO + ESP-IDF | CMake + GNURX GCC |
| **Development** | Native builds | Docker-based cross-compilation |
| **PWM** | MCPWM peripheral | MTU3a peripheral |
| **ADC** | SAR ADC | S12ADFa (12-bit with PWM sync) |
| **Flash/Debug** | USB (ESP-IDF tools) | E2 Lite / J-Link (Renesas tools) |

## Build System

### Docker-Based Workflow

**Professional embedded development pattern:**
- **Build in Docker** (reproducible, cross-platform)
- **Flash/Debug on host** (hardware access via USB)

### Build Scripts

```bash
./build.sh              # Build firmware in Docker
./clean.sh              # Clean build artifacts
./flash.sh              # Flash to RX72N (host, needs E2 Lite)
./debug.sh              # Debug with GDB (host, needs J-Link/E2)
```

### Manual Build

```bash
# Inside Docker
docker run --rm -v "$(pwd):/work" -w /work rx72n-build bash -c "
    mkdir -p build && cd build
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-gnurx.cmake ..
    make -j\$(nproc)
"

# Output files
build/star-rx72n-firmware.elf   # Executable with debug symbols
build/star-rx72n-firmware.hex   # Intel HEX for flashing
build/star-rx72n-firmware.bin   # Raw binary
build/star-rx72n-firmware.map   # Linker map
```

## Code Formatting

### Format C Code

```bash
# Format all C/H files according to .clang-format
./scripts/format_code.sh
```

The formatter uses the same `.clang-format` configuration as ESP32 firmware for consistency.

### Pre-Commit Hook (Recommended)

```bash
# Install pre-commit hook to auto-format on commit
cp scripts/pre-commit .git/hooks/
chmod +x .git/hooks/pre-commit
```

## Code Style

**Follow the main STAR style guide** (see `/Users/bsikar/Documents/git/STAR/CLAUDE.md`)

### RX72N-Specific Style Notes

#### 1. RTOS Differences (ThreadX vs FreeRTOS)

**ThreadX terminology:**
```c
// Thread creation (NOT task)
TX_THREAD led_thread;
tx_thread_create(&led_thread, "LED", led_task_entry, ...);

// Sleep (NOT vTaskDelay)
tx_thread_sleep(50);  // 50 ticks (10ms at 100Hz)

// Semaphores
TX_SEMAPHORE my_semaphore;
tx_semaphore_create(&my_semaphore, "MySem", 1);
tx_semaphore_get(&my_semaphore, TX_WAIT_FOREVER);
tx_semaphore_put(&my_semaphore);
```

**File organization for ThreadX tasks:**
- Still use **one task per file** pattern
- Place in `src/tasks/` and `include/tasks/`
- File naming: `<task_name>_task.c` and `<task_name>_task.h`
- Expose creation function: `<task_name>_task_create()`

#### 2. Hardware Register Access

**Always use register structures, never raw pointers:**

```c
// CORRECT: Use defined register structures
SYSTEM.PRCR = 0xA50F;  // Unlock protection
CMT0.CMCR = 0x0042;    // Configure timer

// WRONG: Don't use raw memory access
*(volatile uint16_t*)0x000803FE = 0xA50F;  // Hard to read, error-prone
```

**Register structures are defined in `include/rx72n_regs.h`:**
- `SYSTEM` - System control registers
- `CMT0`, `CMT1` - Compare Match Timers
- `ICU` - Interrupt Controller
- `PORT0`-`PORTJ` - GPIO ports
- `SCI0`-`SCI12` - Serial Communication Interface (UART)
- `MTU0`-`MTU7` - Multi-Function Timer Units (PWM)
- `CRC` - CRC Calculator peripheral (hardware CRC-32)

#### 3. Interrupt Handlers

**RX interrupt handlers are plain C functions (no special attributes needed):**

```c
// In timer.c
void cmt0_isr(void) {
    // Clear interrupt flag
    CMT0.CMCR;

    // Call ThreadX timer interrupt
    _tx_timer_interrupt();
}
```

**Symbol naming:**
- RX GCC prefixes C functions with `_` (e.g., `cmt0_isr` becomes `_cmt0_isr`)
- Assembly code must reference with underscore: `.long _cmt0_isr`

#### 4. Memory Safety

**No dynamic allocation:**
- ThreadX stacks are **statically allocated** arrays
- All buffers pre-allocated at compile time
- This is safety-critical firmware - zero malloc/free

```c
// CORRECT: Static allocation
static uint8_t led_task_stack[1024];

tx_thread_create(&led_thread, "LED", led_task_entry,
    0, led_task_stack, sizeof(led_task_stack), ...);

// WRONG: Dynamic allocation
uint8_t* stack = malloc(1024);  // Never do this!
```

#### 5. Explicit Integer Types (No size_t)

**Use explicit-width integers instead of `size_t`:**

On the 32-bit RX72N platform, `size_t` is 32 bits, but using explicit types like `uint32_t` is preferred for:
- Clarity about the actual width
- Consistency across the codebase
- Avoiding implicit conversions

```c
// CORRECT: Explicit width
uint32_t rx_crc32_ieee(const uint8_t* data, uint32_t len);

for (uint32_t i = 0; i < len; i++) {
    // ...
}

// AVOID: size_t
size_t rx_crc32_ieee(const uint8_t* data, size_t len);  // Don't use
```

**When to use each type:**
- `uint32_t` - Buffer lengths, loop counters, sizes
- `uint16_t` - Sequence numbers, frame lengths
- `uint8_t` - Byte data, flags, small counters

#### 6. Constants and Macros

**Prefer enums over const over macros** (same as ESP32):

```c
// PREFER: Type-safe enum
typedef enum {
    k_motor_state_idle    = 0,
    k_motor_state_running = 1,
    k_motor_state_error   = 2,
} motor_state_t;

// AVOID: Macros for constants
#define MOTOR_STATE_IDLE 0
```

#### 7. Named Constants for Array Indices

**Use enums for array indices instead of magic numbers:**

When indexing into arrays (especially for byte serialization), use named constants to document what each index represents:

```c
// CORRECT: Named indices document the byte ordering
typedef enum {
    k_be16_byte_high = 0,  /**< High byte (MSB) at index 0 */
    k_be16_byte_low  = 1,  /**< Low byte (LSB) at index 1 */
} be16_byte_idx_t;

static void internal_write_be16(uint8_t* buf, uint16_t val) {
    buf[k_be16_byte_high] = (uint8_t)(val >> 8);
    buf[k_be16_byte_low]  = (uint8_t)(val & 0xFF);
}

// AVOID: Magic number indices
static void internal_write_be16(uint8_t* buf, uint16_t val) {
    buf[0] = (uint8_t)(val >> 8);   // What does 0 mean?
    buf[1] = (uint8_t)(val & 0xFF); // What does 1 mean?
}
```

This improves:
- **Readability** - Index meaning is self-documenting
- **Maintainability** - Easier to understand byte ordering
- **Debuggability** - Enum values visible in debugger

#### 8. ThreadX Configuration

**ThreadX config is in `include/tx_user.h`:**

```c
// Example: Configure system tick
#define TX_TIMER_TICKS_PER_SECOND    100    // 100 Hz (10ms tick)
#define TX_RX72N_CMT_DIVIDER         128    // CMT clock divider
#define TX_RX72N_CMT_CMCOR           4687   // Compare match value

// Enable features
#define TX_ENABLE_STACK_CHECKING          // Catch stack overflow
#define TX_TIMER_PROCESS_IN_ISR           // Timer processing in ISR
```

#### 9. Peripheral Initialization Order

**Critical startup sequence for RX72N:**

```c
// In main.c
int main(void) {
    // 1. System init FIRST (clocks, power)
    system_init();   // 240 MHz PLL, PCLKB 60 MHz

    // 2. Hardware peripherals
    timer_init();    // CMT0 for ThreadX tick
    uart_init();     // Debug UART

    // 3. Print startup banner (after UART init!)
    uart_puts("STAR RX72N Firmware Starting...\r\n");

    // 4. ThreadX kernel (never returns)
    tx_kernel_enter();

    // Should never reach here
    while (1) __asm__ volatile ("wait");
    return 0;
}
```

#### 10. Doxygen File Headers

**All `.c` and `.h` files must include proper Doxygen metadata tags:**

```c
/**
 * @file my_driver.c
 * @brief Brief description of the file
 *
 * Detailed description if needed.
 *
 * @date 2026-01-02
 * @copyright Copyright (c) 2026 STAR Project
 */
```

**Do NOT use plain text dates:**
```c
// WRONG: Plain text, not Doxygen tags
 * STAR Project - Texas A&M University
 * January 2026

// CORRECT: Proper Doxygen tags
 * @date 2026-01-02
 * @copyright Copyright (c) 2026 STAR Project
```

#### 11. Single-Value Enums

**Use `static const` instead of single-value enums:**

```c
// AVOID: Single-value enum (unused typedef)
typedef enum {
  k_timeout_ms = 1000,
} timeout_constants_t;

// PREFER: static const with s_ prefix
static const uint32_t s_timeout_ms = 1000;
```

Enums are for **groups of related constants**. A single constant should use `static const`.

#### 12. Variable Declaration Placement

**Declare all variables at function start, not mid-block:**

```c
// AVOID: Declaration inside loop
static rx_err_t wait_for_event(uint32_t timeout_us)
{
  uint32_t start = get_time_us();
  while (true) {
    uint32_t elapsed = get_time_us() - start;  // declared mid-function
    if (elapsed >= timeout_us) return k_rx_err_timeout;
  }
}

// PREFER: All declarations at function start
static rx_err_t wait_for_event(uint32_t timeout_us)
{
  uint32_t start   = get_time_us();
  uint32_t elapsed = 0;

  while (true) {
    elapsed = get_time_us() - start;
    if (elapsed >= timeout_us) return k_rx_err_timeout;
  }
}
```

#### 13. HAL Wrapper Error Propagation

**HAL wrapper functions must propagate errors, never discard return values:**

```c
// WRONG: Discards HAL errors
static rx_err_t wrapper_set_output(uint8_t port, uint8_t pin)
{
  gpio_set_output(port, pin);  // return value ignored!
  return k_rx_ok;
}

// CORRECT: Propagate HAL errors
static rx_err_t wrapper_set_output(uint8_t port, uint8_t pin)
{
  return gpio_set_output(port, pin);
}
```

#### 14. Range Validation

**Always check both minimum AND maximum bounds:**

```c
// INCOMPLETE: Only checks minimum
if (distance_cm < MIN_DISTANCE_CM) {
  return k_rx_err_out_of_range;
}

// COMPLETE: Checks both bounds
if (distance_cm < MIN_DISTANCE_CM || distance_cm > MAX_DISTANCE_CM) {
  return k_rx_err_out_of_range;
}
```

#### 15. Port/Pin Constants Policy

**CRITICAL: NEVER hardcode port or pin numbers!**

All port and pin numbers MUST use centralized constants from the library layer. Hex values are ONLY allowed in `lib/rx_core/inc/rx_port_constants.h`.

**Architecture:**
- `lib/rx_core/inc/rx_port_constants.h` - Library source of truth (ONLY place with hex values)
- `lib/rx_hal/` - Library code uses constants (NO hex)
- `include/hardware_pinout.h` - Application layer uses constants (NO hex)

**Correct usage (library code):**

```c
#include "rx_port_constants.h"

// Use named constants
switch (port) {
  case k_rx_port_b: return portb();
  case k_rx_port_e: return porte();
  // ...
}
```

**Correct usage (application code):**

```c
#include "rx_port_constants.h"

// Build gpio_pin_t from constants
k_gpio_pb2 = (k_rx_port_b << k_port_shift) | k_rx_pin_2,
```

**WRONG - Never do this:**

```c
// WRONG: Hardcoded hex values
if (port == 0x0B) { ... }           // What is 0x0B?
if (port == 11) { ... }             // Decimal is also wrong!
k_gpio_pb2 = 0x0B02;                // Magic number!
```

**Allowed hex values:**
- `lib/rx_core/inc/rx_port_constants.h` - Port/pin constants (ONLY here!)
- `lib/rx_hal/inc/rx72n_*_regs.h` - Hardware register addresses

**Benefits:**
- Single source of truth for all port/pin numbers
- Compile-time verification via static assertions
- Self-documenting code (`k_rx_port_b` vs `0x0B`)
- Easy to search and maintain
- Type-safe enum constants

## Project Structure

```
star-rx72n-firmware/
├── Dockerfile              # Build environment
├── build.sh                # Build in Docker
├── flash.sh                # Flash via E2 Lite
├── debug.sh                # GDB debugging
├── clean.sh                # Clean build artifacts
│
├── .clang-format           # Code formatting rules
├── scripts/
│   ├── format_code.sh      # Format all C/H files
│   └── compile_doxygen.sh  # Generate documentation
│
├── CMakeLists.txt          # Build configuration
├── cmake/
│   └── toolchain-gnurx.cmake   # Cross-compile settings
│
├── src/
│   ├── main.c              # Application entry, ThreadX init
│   └── hardware/           # Hardware abstraction layer
│       ├── startup_rx72n.S # Reset vector, interrupts
│       ├── system_init.c   # Clock/power initialization
│       ├── gpio.c          # GPIO control
│       ├── timer.c         # CMT0 system tick
│       └── uart.c          # Debug UART (SCI)
│
├── include/
│   ├── rx72n_regs.h        # Hardware register definitions
│   ├── tx_user.h           # ThreadX configuration
│   └── hardware.h          # HAL public API
│
├── lib/
│   └── threadx/            # ThreadX RTOS (submodule)
│
├── linker/
│   └── rx72n.ld            # Memory layout (4MB Flash, 1MB RAM)
│
└── build/                  # Generated (not in git)
    ├── Makefile            # Generated by CMake
    └── *.elf, *.hex, *.bin # Build outputs
```

## Hardware Abstraction Layer

### System Initialization (`system_init.c`)

```c
void system_init(void);         // Initialize clocks (240 MHz) and power
void clock_init(void);          // PLL configuration
```

### GPIO (`gpio.c`)

```c
void gpio_set_output(uint8_t port, uint8_t pin);
void gpio_set_input(uint8_t port, uint8_t pin);
void gpio_write_high(uint8_t port, uint8_t pin);
void gpio_write_low(uint8_t port, uint8_t pin);
void gpio_toggle(uint8_t port, uint8_t pin);
bool gpio_read(uint8_t port, uint8_t pin);
```

### UART (`uart.c`)

```c
void uart_init(void);                       // Initialize SCI1 (115200 baud)
void uart_putc(char c);                     // Send character
void uart_puts(const char* str);            // Send string
void uart_putint(int32_t value);            // Send integer
```

### Timer (`timer.c`)

```c
void timer_init(void);          // Initialize CMT0 for ThreadX tick (100 Hz)
void cmt0_isr(void);            // CMT0 interrupt handler
```

### Defensive Coding Modules

#### IWDT Watchdog (`rx_iwdt.h`)

```c
rx_err_t rx_iwdt_init(uint32_t timeout_ms);  // Initialize with timeout
void rx_iwdt_feed(void);                      // Feed watchdog
bool rx_iwdt_was_reset(void);                 // Check reset cause
```

#### IRQ Filter (`rx_irq_filter.h`)

```c
rx_err_t rx_irq_filter_enable(uint8_t irq_num, rx_irq_filter_clk_t clk);
rx_err_t rx_irq_filter_disable(uint8_t irq_num);
```

#### Register Guard (`rx_register_guard.h`)

```c
rx_err_t rx_register_guard_init(void);        // Capture golden values
void rx_register_guard_refresh(void);          // Restore corrupted registers
uint32_t rx_register_guard_get_correction_count(void);
```

## Protocol Stack Libraries

The firmware includes protocol stack libraries for reliable SPI communication:

| Library | Location | Description |
|---------|----------|-------------|
| `rx_frame` | `lib/rx_frame/` | Frame encoding/decoding with CRC-32 |
| `rx_fec` | `lib/rx_fec/` | Forward Error Correction (Hamming codes) |
| `rx_harq` | `lib/rx_harq/` | Hybrid ARQ with soft combining |

### CRC-32 Module (`rx_frame`)

IEEE 802.3 CRC-32 implementation with **compile-time hardware/software selection**.

**Design Rationale:**

The CRC module uses a compile-time selection strategy with both implementations always compiled:

1. **Hardware CRC** (default on RX72N):
   - Uses RX72N CRC Calculator peripheral (~10x faster for large buffers)
   - Automatically enabled when `__RX__` is defined

2. **Software CRC** (default on host, always available):
   - 256-entry lookup table implementation (1KB table in .rodata)
   - Used for host-side unit testing (no hardware available)
   - `rx_crc32_update_sw()` always compiled for incremental CRC operations
   - Enables A/B comparison testing on hardware for validation

**Why both implementations are always compiled:**
- Host-side tests validate CRC correctness without hardware
- Software can be forced on target (`-DRX_CRC32_USE_SOFTWARE`) for debugging
- Incremental CRC uses software fallback (hardware state management is complex)

**Public API:**
```c
#include "rx_crc.h"

uint32_t rx_crc32_ieee(const uint8_t *data, size_t len);
uint32_t rx_crc32_update(uint32_t crc, const uint8_t *data, size_t len);
```

**Hardware/Software Selection:**

| Build Target | Default | Override |
|--------------|---------|----------|
| RX72N (`__RX__` defined) | Hardware CRC | Define `RX_CRC32_USE_SOFTWARE` |
| Host (testing) | Software CRC | N/A |

```c
// Force software CRC (useful for debugging or comparison)
#define RX_CRC32_USE_SOFTWARE
#include "rx_crc_internal.h"

// Or via compiler flag:
// gcc -DRX_CRC32_USE_SOFTWARE ...
```

**Hardware CRC Details:**
- Peripheral: RX72N CRC Calculator at 0x00088280
- Polynomial: IEEE 802.3 (0x04C11DB7)
- Module stop: MSTPCRB bit 23
- Bit-exact compatible with Go's `crc32.ChecksumIEEE()`

**References:**
- [RX72N Hardware Manual](https://www.renesas.com/en/products/rx72n) - CRC Calculator section
- [Renesas FSP CRC Module](https://renesas.github.io/fsp/group___c_r_c.html)

**File Structure:**
```
lib/rx_crc/
├── inc/
│   ├── rx_crc.h            # Public API
│   └── rx_crc_internal.h   # Internal abstraction (hw/sw selection)
└── src/
    ├── rx_crc32.c          # Public API wrapper
    ├── rx_crc32_sw.c       # Software implementation (lookup table)
    └── rx_crc32_hw.c       # Hardware implementation (RX72N peripheral)

lib/rx_frame/
└── src/
    └── rx_frame.c          # Frame encoding/decoding
```

## ThreadX Patterns

### Creating a Task

```c
// In include/tasks/my_task.h
#ifndef MY_TASK_H
#define MY_TASK_H

#include "tx_api.h"

esp_err_t my_task_create(void);  // Note: Still use esp_err_t for consistency

#endif

// In src/tasks/my_task.c
#include "tasks/my_task.h"
#include "hardware.h"

#define MY_TASK_STACK_SIZE 1024
#define MY_TASK_PRIORITY   5

static TX_THREAD my_thread;
static uint8_t my_task_stack[MY_TASK_STACK_SIZE];

static void my_task_entry(ULONG input) {
    (void)input;

    while (1) {
        // Task logic
        uart_puts("[MyTask] Running\r\n");
        tx_thread_sleep(100);  // 1 second at 100 Hz
    }
}

esp_err_t my_task_create(void) {
    UINT status = tx_thread_create(
        &my_thread,
        "MyTask",
        my_task_entry,
        0,
        my_task_stack,
        MY_TASK_STACK_SIZE,
        MY_TASK_PRIORITY,
        MY_TASK_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START
    );

    return (status == TX_SUCCESS) ? ESP_OK : ESP_FAIL;
}
```

### Using Semaphores

```c
static TX_SEMAPHORE data_ready_sem;

// In initialization:
tx_semaphore_create(&data_ready_sem, "DataReady", 0);

// Producer thread:
// ... prepare data ...
tx_semaphore_put(&data_ready_sem);

// Consumer thread:
tx_semaphore_get(&data_ready_sem, TX_WAIT_FOREVER);
// ... process data ...
```

### Using Mutexes

```c
static TX_MUTEX data_mutex;

// In initialization:
tx_mutex_create(&data_mutex, "DataMutex", TX_NO_INHERIT);

// In task:
tx_mutex_get(&data_mutex, TX_WAIT_FOREVER);
// ... critical section ...
tx_mutex_put(&data_mutex);
```

## Debugging

### Printf Debugging (UART)

```c
// In any file:
#include "hardware.h"

uart_puts("[DEBUG] Task started\r\n");
uart_puts("[DEBUG] Counter: ");
uart_putint(counter);
uart_puts("\r\n");
```

Monitor via serial:
```bash
screen /dev/tty.usbserial-XXXXXXXX 115200
```

### GDB Debugging

```bash
# Terminal 1: Start GDB server
JLinkGDBServer -device R5F572NN -if JTAG -speed 4000 -port 2331

# Terminal 2: Debug
./debug.sh

# In GDB:
(gdb) break main
(gdb) continue
(gdb) print led_thread
(gdb) info threads
(gdb) backtrace
```

See [FLASH.md](FLASH.md) for complete debugging guide.

## Adding New Features

### Adding a New Task

1. Create task files:
   ```
   include/tasks/motor_control_task.h
   src/tasks/motor_control_task.c
   ```

2. Add to CMakeLists.txt:
   ```cmake
   set(SOURCES
       src/main.c
       src/tasks/motor_control_task.c  # Add here
       ...
   )
   ```

3. Create in `tx_application_define()`:
   ```c
   // In main.c
   void tx_application_define(void *first_unused_memory) {
       motor_control_task_create();
       telemetry_task_create();
       // ...
   }
   ```

### Adding a New Driver

1. Create driver files:
   ```
   include/drivers/mtu3a.h
   src/drivers/mtu3a.c
   ```

2. Add register definitions to `rx72n_regs.h`:
   ```c
   typedef struct {
       volatile uint8_t  TCR;
       volatile uint8_t  TMDR;
       // ...
   } MTU_Type;

   #define MTU0 ((MTU_Type*)0x00088600)
   ```

3. Add to CMakeLists.txt and use in tasks

## Memory Layout

```
RX72N Memory Map (R5F572NNHGFP#30):

Flash (4 MB):
0x00000000 - 0x003FFFFF    Code and constants

RAM (1 MB):
0x00000000 - 0x000FFFFF    RAM (shared with Flash address space via bus)
                           Actual RAM at different physical address

Peripherals:
0x00080000 - 0x000FFFFF    Peripheral registers
0x00088000 - ICU
0x00088200 - CMT
0x00088280 - CRC (CRC Calculator)
0x000C0000 - GPIO (PORT)
0x000D0000 - MTU3a
0x000E0000 - S12AD
0x00088000 - SCI

Reset Vectors:
0xFFFFFFFC - Reset vector (points to _PowerON_Reset)
```

Current firmware usage:
```
Text (code):    7,147 bytes  (0.2% of 4MB Flash)
Data (init):        4 bytes
BSS (uninit):  53,172 bytes  (5% of 1MB RAM, mostly ThreadX stacks)
Total:         60,323 bytes
```

## Common Patterns

### Safe Register Modification

```c
// Unlock protection
SYSTEM.PRCR = 0xA50F;

// Modify registers
SYSTEM.MSTPCRA &= ~(1 << 15);  // Enable CMT module

// Lock protection
SYSTEM.PRCR = 0xA500;
```

### Enabling Interrupts

```c
// 1. Configure peripheral interrupt
CMT0.CMCR = 0x0042;  // CKS=10 (PCLK/128), CMIE=1 (enable)

// 2. Set interrupt priority in ICU
ICU.IPR[28] = 3;  // Priority 3 (0-15, higher = higher priority)

// 3. Enable interrupt in ICU
ICU.IER[3] |= (1 << 4);  // Enable CMI0 (vector 28, IER3 bit 4)

// 4. Clear interrupt flag
ICU.IR[28] = 0;

// 5. Enable interrupts globally (if not already enabled)
// ThreadX does this automatically
```

## References

- [RX72N Hardware Manual](https://www.renesas.com/en/products/rx72n) - Complete peripheral reference
- [RX72N CRC Calculator](https://renesas.github.io/fsp/group___c_r_c.html) - Hardware CRC-32 documentation
- [ThreadX Documentation](https://github.com/eclipse-threadx/rtos-docs) - RTOS API reference
- [GNURX Toolchain](https://llvm-gcc-renesas.com/) - Compiler documentation
- [Main STAR Style Guide](../CLAUDE.md) - Project-wide coding standards
- [ESP32 Firmware](../star-esp32-firmware/CLAUDE.md) - Sister project for comparison
