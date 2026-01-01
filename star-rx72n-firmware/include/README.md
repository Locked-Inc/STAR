# Configuration Headers (`include/`)

This directory contains application-wide configuration headers for the STAR RX72N firmware.

## TODO: Define Configuration Constants

### `tx_user.h`
ThreadX RTOS configuration:
- System tick rate (e.g., 100 Hz)
- Timer configuration (CMT divider, compare match)
- Stack checking enablement
- Timer processing mode

### `system_config.h`
Task and system configuration:
- Task priorities (motor > sensor > communication)
- Stack sizes for each task
- Control loop frequencies (250 Hz motor, 50 Hz sensor, 100 Hz comm)
- **Obstacle detection threshold** (e.g., 20cm for HC-SR04 STOP trigger)
- Emergency stop parameters

### `hardware_config.h`
Pin mappings and peripheral assignments:
- **Reference `docs/sections/03_hardware_pinout.tex` for complete pin table**
- Motor PWM pins (GPTW: PE0-PE7)
- Encoder pins (MTU: P14-P15, P22-P27, PA3, PC0-PC1, PD0)
- SPI pins (RPi5: PA4-PA7)
- HC-SR04 sonar pins (4 sensors: trigger + echo pins)
- Motor driver nFAULT pins (P44-P47)
- ADC current sense pins (P40-P43)

## Key Principles

- **Single Source of Truth**: Pin mappings match hardware documentation in `docs/`
- **Compile-Time Configuration**: All constants defined at compile time (no runtime allocation)
- **Type Safety**: Use enums for related constants, const for single values
- **Documentation**: Reference section numbers from `docs/sections/*.tex`
