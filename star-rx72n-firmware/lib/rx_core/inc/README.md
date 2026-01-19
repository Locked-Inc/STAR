# RX Core Library - Include Files

This directory contains core library header files that provide fundamental definitions and utilities for the RX72N firmware.

## Files

### `rx_port_constants.h` - Port/Pin Number Constants

**Purpose:** Single source of truth for all RX72N port and pin number definitions.

**Key Points:**
- **ONLY file where hex values for port/pin numbers are allowed**
- Provides `rx_port_number_t` enum for all ports (0-9, A-G, J)
- Provides `rx_pin_number_t` enum for all pin numbers (0-7)
- Provides `k_port_shift` constant for encoding port/pin into gpio_pin_t
- Includes compile-time static assertions to verify encoding correctness

**Usage in Library Code:**
```c
#include "rx_port_constants.h"

switch (port) {
  case k_rx_port_b: return portb();
  case k_rx_port_e: return porte();
  // ...
}
```

**Usage in Application Code:**
```c
#include "rx_port_constants.h"

typedef enum : uint16_t {
  k_gpio_pb2 = (k_rx_port_b << k_port_shift) | k_rx_pin_2,
  k_gpio_pe5 = (k_rx_port_e << k_port_shift) | k_rx_pin_5,
} gpio_pin_t;
```

**Architecture:**
```
rx_port_constants.h           ← Library source of truth (hex values ONLY here)
        ↓
lib/rx_hal/src/gpio.c         ← Library uses constants (NO hex)
lib/rx_hal/src/rx_mpc.c       ← Library uses constants (NO hex)
        ↓
include/hardware_pinout.h     ← Application uses constants (NO hex)
```

**Why This Matters:**
1. **Single Source of Truth:** All port numbers defined in one place
2. **No Magic Numbers:** `k_rx_port_b` is self-documenting vs `0x0B`
3. **Compile-Time Safety:** Static assertions verify correctness
4. **Maintainable:** Change port mapping in one location
5. **Searchable:** Easy to find all uses of a specific port

### Other Core Library Files

- `rx_err.h` - Error code definitions
- `rx_check.h` - Parameter validation macros
- `rx_log.h` - Logging infrastructure
- `rx_infrastructure.h` - Dependency injection framework
- `rx_pin_interface.h` - Pin reservation interface
- `rx_gpio_constants.h` - GPIO direction/mode constants
- `mock_*.h` - Mock implementations for unit testing

## Adding New Constants

If you need to add new port/pin-related constants:

1. Add them ONLY to `rx_port_constants.h`
2. Use enums for integer constants
3. Add static assertions to verify correctness
4. Update this README if adding new enum types
5. Never hardcode hex values in other files

## Policy

**CRITICAL:** Never hardcode port or pin numbers anywhere except `rx_port_constants.h`!

See [CLAUDE.md](../../../CLAUDE.md) section "Port/Pin Constants Policy" for complete guidelines.
