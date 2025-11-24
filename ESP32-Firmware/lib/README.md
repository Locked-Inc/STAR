# STAR Firmware Libraries

This directory contains the modular libraries for the STAR ESP32 firmware. The architecture follows the **Dependency Inversion Principle (DIP)** to ensure loose coupling between components.

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           APPLICATION LAYER                              │
│                              (main.c)                                    │
│  • Sensor initialization    • Business logic    • Task scheduling        │
└────────────────────────────────────┬─────────────────────────────────────┘
                                     │
       ┌─────────────────────────────┼─────────────────────────────┐
       │                             │                             │
       ▼                             ▼                             ▼
┌────────────────┐       ┌────────────────────┐       ┌────────────────┐
│ SENSOR DRIVERS │       │   COMMUNICATION    │       │ POWER/CONTROL  │
│                │       │                    │       │                │
│ • MPU6050      │       │                    │       │ • BQ7850 (BMS) │
│ • BNO055+BMP280│       │                    │       │ • PCA9685 (PWM)│
│ • HC-SR04      │       │                    │       │                │
└───────┬────────┘       └─────────┬──────────┘       └───────┬────────┘
        │                          │                          │
        └──────────────────────────┼──────────────────────────┘
                                   │
                     ┌─────────────┴─────────────┐
                     │      UNIFIED BUS LAYER    │
                     │       (star_bus)          │
                     │  I2C • SPI • UART • SMBus │
                     └─────────────┬─────────────┘
                                   │
        ┌──────────────────────────┼──────────────────────────┐
        │                          │                          │
        ▼                          ▼                          ▼
┌───────────────┐       ┌──────────────────┐       ┌─────────────────┐
│   star_core   │◄──────│ star_error_      │       │ star_pin_       │
│               │       │    handler       │       │   validator     │
│  (Abstract    │       │                  │       │                 │
│  Interfaces)  │       │ (Implementation) │       │ (Implementation)│
└───────────────┘       └──────────────────┘       └─────────────────┘
        ▲
        │
        │ All components depend on interfaces, not implementations
        │ This enables: Testing, Mocking, Swapping, Optional Dependencies
        │
```

## Dependency Inversion Principle (DIP)

### Why DIP?

**Without DIP - Tightly Coupled:**
```c
// star_bus_manager.c - BAD
#include "star_error_handler.h"  // Direct dependency on concrete implementation

void init_bus() {
    error_handler_record_error(...);  // Hardcoded to one implementation
}
```

**Problems:**
- Can't test without real hardware
- Can't swap implementations
- Changes ripple through codebase

**With DIP - Loosely Coupled:**
```c
// star_bus_manager.c - GOOD
#include "star_core/star_error_interface.h"  // Depend on interface only

void star_bus_manager_init(star_bus_manager_t* mgr,
                           star_error_interface_t* error_iface) {
    if (error_iface) {
        error_iface->record_error(error_iface->ctx, ...);  // Use injected implementation
    }
}
```

**Benefits:**
- ✅ Testable with mock interfaces
- ✅ Swappable implementations
- ✅ Optional dependencies (pass NULL)
- ✅ No circular dependencies
- ✅ Each library evolves independently

---

## Library Structure

Each library follows this structure:
```
lib/star_component/
├── CMakeLists.txt       # CMake configuration
├── library.json         # PlatformIO library metadata
├── README.md            # Detailed documentation
├── include/             # Public headers
│   └── star_component.h
└── src/                 # Implementation files
    └── star_component.c
```

---

## Adding New Libraries

1. Create directory structure following the pattern above
2. Define public API in `include/` header
3. Implement in `src/`
4. Add CMakeLists.txt and library.json
5. Document in library-specific README.md
6. If the library needs error handling or pin validation, use dependency injection through `star_core` interfaces

---

## License

MIT License - See top-level LICENSE file for details.
