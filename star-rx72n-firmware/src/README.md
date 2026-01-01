# Application Source Code (`src/`)

This directory contains application-specific code for the STAR RX72N motor controller firmware.

## TODO: Implement Application Tasks

### 1. Motor Control Task (Highest Priority, 250 Hz)
- GPTW PWM generation for 4 motors (PH/EN signals)
- MTU encoder quadrature decoding
- PID velocity control
- **Check STOP flag from sensor task** → emergency brake if set
- Driver fault monitoring (nFAULT pins)
- Current sensing via ADC

### 2. Sensor Task (High Priority, 50 Hz)
- Read 4x HC-SR04 ultrasonic sensors via GPIO
- Simple threshold check (e.g., distance < 20cm)
- **Set STOP flag** in shared state when obstacle detected
- Clear STOP flag when all sensors read safe distances
- Publish sensor data to shared state

### 3. Communication Task (Medium Priority)
- SPI (RPi5) OR USB CDC (compile-time choice)
- Receive velocity commands from RPi5 (100 Hz)
- Send encoder telemetry to RPi5 (100 Hz)
- Protocol Buffers messaging (nanopb) with CRC-32

## Architecture

- **Simple STOP**: Sensor task sets flag, motor task checks flag (no path planning)
- **Shared State**: Mutex-protected data structures for inter-task communication
- **No Dynamic Allocation**: All buffers/stacks statically allocated (safety-critical)
