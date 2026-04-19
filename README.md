# STAR

**Spatial Topography Accessibility Robot** - A distributed robotics platform for autonomous indoor ADA-compliance auditing.

## Overview

STAR is a complete robotics system combining custom hardware, embedded firmware, control software, and user interfaces. The platform uses a distributed architecture with dedicated microcontrollers for real-time motor control and a high-level compute platform for navigation and decision-making.

## Project Structure

- **`star-rx72n-firmware/`** - Motor controller firmware (Renesas RX72N)
- **`star-gateway/`** - Gateway service bridging UI and robot control
- **`star-proto/`** - Protocol Buffers schemas for inter-component communication
- **`star-ui/`** - User interface
- **`schematic/`** - PCB designs and hardware schematics
- **`matlab/`** - Control system design and analysis
- **`docs/`** - System documentation

## Documentation

Detailed documentation is available in the `docs/` directory:

- **System Architecture:** See `docs/sections/` for component specifications
- **Hardware Design:** Reference schematics in `schematic/` and pinout documentation
- **Firmware Development:** See `star-rx72n-firmware/CLAUDE.md` for build instructions
- **Gateway Service:** See `star-gateway/CLAUDE.md` for architecture details
- **Protocol Specifications:** See `docs/sections/01_nanopb_protocol.tex` and related files

Build the full documentation:
```bash
cd docs && ./compile.sh
```

## Getting Started

Each component has its own build system and documentation:

1. **Firmware:** See `star-rx72n-firmware/` for embedded development
2. **Gateway:** See `star-gateway/` for Go service development
3. **Protocols:** See `star-proto/` for Protocol Buffers schemas
4. **Hardware:** See `schematic/` for PCB designs

Refer to component-specific README or CLAUDE.md files for detailed setup instructions.

## GitHub CLI Authentication

The first time you open this devcontainer, you'll need to authenticate GitHub CLI:

```bash
gh auth login
```

Follow the web browser flow. Your authentication will persist across container rebuilds thanks to the mounted `~/.config/gh` directory.

## Technology Stack

- **Embedded:** C with ThreadX RTOS, Protocol Buffers (nanopb)
- **Control:** Go, ROS2
- **Hardware:** Custom PCBs with Renesas RX72N microcontroller
- **Communication:** SPI with Forward Error Correction

## License

This project is licensed under the MIT License.

Copyright (c) 2026 STAR Project

See the [LICENSE](LICENSE) file for details.
