# RX72N Package Configuration Guide

This guide explains how to configure the firmware for different RX72N package variants and how the compile-time protection works.

## Quick Start

The firmware defaults to **100-pin LFQFP** (STAR project hardware). To use a different package, define the appropriate macro in `rx_package_config.h`:

```c
// Uncomment ONE of these in rx_package_config.h:
// #define RX72N_PACKAGE_100PIN   // Default - STAR hardware
// #define RX72N_PACKAGE_144PIN
// #define RX72N_PACKAGE_145PIN
// #define RX72N_PACKAGE_176PIN
// #define RX72N_PACKAGE_224PIN
```

Or define via compiler flags:
```bash
cmake -DRX72N_PACKAGE_176PIN=1 ..
```

## Package Variants and Port Availability

### 100-pin LFQFP (R5F572NNHGFP#30) - STAR Default
**Available Ports:**
- Port 0: P05, P07 only (limited)
- Port 1: P12-P17 only (limited)
- Port 2-5: Full or limited pins
- Port A-E: Full (8 pins each)
- Port J: PJ3, PJ5 only (limited)

**NOT Available:** Ports 6, 7, 8, 9, F, G, H, K, L, M, N, Q

### 144/145-pin LFQFP/TFLGA
**Additional Ports:** 8 (limited), 9 (limited), F (PF5 only)

### 176-pin LFQFP/LFBGA
**Additional Ports:** 0-9 (full), A-G (full), J (full)

### 224-pin LFBGA
**All Ports Available:** 0-9, A-Q (182 total I/O pins)

## Compile-Time Protection

### How It Works

The package configuration enables **compile-time protection** against using unavailable ports:

```c
// On 100-pin package:
volatile rx_port_regs_t* p = port6();  // ❌ COMPILE ERROR: port6() not defined!

// On 176-pin package:
volatile rx_port_regs_t* p = port6();  // ✅ OK: port6() is available
```

### Example: Porting from 100-pin to 176-pin

**Step 1:** Change package in `rx_package_config.h`:
```c
// #define RX72N_PACKAGE_100PIN  // Comment out
#define RX72N_PACKAGE_176PIN     // Uncomment
```

**Step 2:** Rebuild - now you can use additional ports:
```c
// These are now available:
port6()->pdr = 0xFF;  // Set all Port 6 pins as outputs
port7()->podr = 0x55; // Set pattern on Port 7
```

**Step 3:** If you try to use 224-pin-only ports, you'll get compile errors:
```c
porth()->pdr = 0xFF;  // ❌ COMPILE ERROR: porth() not available on 176-pin!
```

## Pre-Computed Constants

All port/pin combinations are pre-defined in `rx_port_constants.h`, regardless of package:

```c
// These constants exist for ALL packages (they're just numbers):
gpio_pin_t my_pin = k_rx_p6_3;  // Port 6, Pin 3 = 0x0603
gpio_pin_t another = k_rx_ph_5; // Port H, Pin 5 = 0x1105
```

**Important:** The constants exist for convenience, but **accessing the hardware** through port accessor functions is package-dependent:

```c
// 100-pin package:
gpio_pin_t pin = k_rx_p6_3;  // ✅ OK: constant exists
port6()->pdr |= (1 << 3);    // ❌ COMPILE ERROR: port6() not available!

// 176-pin package:
gpio_pin_t pin = k_rx_p6_3;  // ✅ OK: constant exists
port6()->pdr |= (1 << 3);    // ✅ OK: port6() is available!
```

## Compile Error Examples

### Example 1: Using unavailable port on 100-pin

```c
// config: RX72N_PACKAGE_100PIN
void setup_led(void)
{
  port6()->pdr = 0xFF;  // ❌ ERROR
}
```

**Compiler Error:**
```
error: 'port6' was not declared in this scope
     port6()->pdr = 0xFF;
     ^~~~~
```

**Fix:** Use an available port (Port A, B, C, D, or E) or change to a larger package.

### Example 2: Using 224-pin-only port on 176-pin

```c
// config: RX72N_PACKAGE_176PIN
void setup_sensor(void)
{
  porth()->pdr = 0x01;  // ❌ ERROR: Port H only on 224-pin
}
```

**Compiler Error:**
```
error: 'porth' was not declared in this scope
     porth()->pdr = 0x01;
     ^~~~~
```

**Fix:** Use a different port or change to 224-pin package.

## Best Practices

### 1. Document Package Requirements

Add comments in your code indicating package requirements:

```c
/**
 * @brief Initialize expansion I/O
 * @note Requires 176-pin or 224-pin package (uses Port 6)
 */
void expansion_io_init(void)
{
#ifdef RX72N_PORT6_AVAILABLE
  port6()->pdr = 0xFF;
  port6()->podr = 0x00;
#else
#error "expansion_io_init() requires Port 6 (176-pin or larger package)"
#endif
}
```

### 2. Use Conditional Compilation for Optional Features

```c
void init_all_ports(void)
{
  // Core ports (available on all packages)
  porta()->pdr = 0xFF;
  portb()->pdr = 0xFF;

#ifdef RX72N_PORT6_AVAILABLE
  // Optional: Extra I/O on larger packages
  port6()->pdr = 0xFF;
  port7()->pdr = 0xFF;
#endif

#ifdef RX72N_PORTH_AVAILABLE
  // Optional: Even more I/O on 224-pin
  porth()->pdr = 0xFF;
#endif
}
```

### 3. Check Package at Runtime (for logging/debug)

```c
void print_package_info(void)
{
  uart_puts("Package: " RX72N_PACKAGE_NAME "\r\n");
  uart_puts("I/O pins: ");
  uart_putint(RX72N_IO_PIN_COUNT);
  uart_puts("\r\n");
}
```

Output on 100-pin:
```
Package: 100-pin LFQFP
I/O pins: 63
```

## Common Errors and Solutions

### Error: "Only one RX72N_PACKAGE_* macro should be defined!"

**Cause:** Multiple package macros defined in `rx_package_config.h`

**Solution:** Only uncomment ONE package macro:
```c
// ❌ WRONG:
#define RX72N_PACKAGE_100PIN
#define RX72N_PACKAGE_176PIN

// ✅ CORRECT:
// #define RX72N_PACKAGE_100PIN
#define RX72N_PACKAGE_176PIN
```

### Error: "port6 was not declared in this scope"

**Cause:** Trying to use Port 6 on 100-pin or 144/145-pin package

**Solution:** Either:
1. Use a different port (0-5, A-E, J for 100-pin)
2. Change to 176-pin or 224-pin package
3. Add conditional compilation:
   ```c
   #ifdef RX72N_PORT6_AVAILABLE
     port6()->pdr = 0xFF;
   #endif
   ```

## Summary

- **Default:** 100-pin LFQFP (STAR hardware)
- **Configuration:** Define `RX72N_PACKAGE_*` in `rx_package_config.h`
- **Protection:** Compile errors prevent using unavailable ports
- **Constants:** All port/pin constants exist (they're just numbers)
- **Hardware Access:** Port accessor functions are package-dependent

This ensures you can only use ports that physically exist on your hardware!
