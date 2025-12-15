# STAR Firmware

A modular, production-ready embedded firmware framework implementing Dependency Inversion Principle (DIP) for loose coupling, testability, and maintainability.

## Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/)

### Build

```bash
# Clone repository
git clone https://github.com/Locked-Inc/STAR.git
cd STAR/ESP32-Firmware

# Build for ESP32-WROOM
pio run -e esp32_wroom

# Build for ESP32-S3
pio run -e esp32s3
```

### Upload

```bash
pio run -e esp32_wroom --target upload
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- ESP-IDF team for the excellent framework
- PlatformIO for seamless build tooling
- All sensor manufacturers for detailed datasheets
