# Technology Stack

## High-Level Architecture
- **Main Controller:** Raspberry Pi 5 (quad-core ARM Cortex-A76)
- **Real-Time Controller:** Renesas RX72N (RXv3 core)
- **UI Architecture:** TypeScript-based web interface communicating via WebSocket/HTTP to the Gateway.

## Programming Languages
- **Go:** Core language for the `star-gateway` service, bridging high-level and low-level communication.
- **C11 / Assembly:** Used for performance-critical real-time firmware on the RX72N.
- **C++:** Used for ROS2 Jazzy node implementation on the Raspberry Pi 5.
- **Python 3.11:** Primary scripting language for ROS2, system-level automation, and Buildroot integration.
- **TypeScript:** Used for the user interface and cross-platform communication types.

## Frameworks & Operating Systems
- **ThreadX (Azure RTOS):** Real-time operating system for the motor control firmware, providing deterministic task scheduling.
- **Buildroot Linux:** Custom-tailored embedded Linux distribution for the Raspberry Pi 5.
- **ROS2 Jazzy:** The primary robotics middleware for navigation, sensing, and higher-level behavior orchestration.

## Communication Protocols
- **Protocol Buffers (nanopb):** Unified serialization format for data exchange across all components (Go, C, TypeScript).
- **SPI (10 Mbps):** High-speed serial interface for the primary RPi5 ↔ RX72N data link.
- **gRPC / WebSockets:** Used for high-level command and telemetry streams between the UI and Gateway.
    - `/ws/controller`: Dedicated 60Hz binary WebSocket for real-time gamepad control.

## Engineering & Design Tools
- **MATLAB/Simulink:** For system identification, motor modeling, and discrete PID controller design.
- **KiCad:** Professional EDA for PCB design and schematic capture.
- **Buf CLI:** For standardized Protobuf management and code generation.
- **CMake:** For cross-platform build management of the embedded C/C++ components.
