# OTA Documentation Index

This directory contains comprehensive Q&A documentation for the ESP32 OTA (Over-The-Air) update system.

## Documents

### [01-overview.md](01-overview.md)
**Question:** How is OTA implemented in this ESP32 project?

Covers:
- Core components (OTA Manager, Protocol, Handlers)
- Update process flow
- Two update modes (Manual vs Auto)
- Safety features
- Configuration options
- Current implementation status

### [02-making-requests.md](02-making-requests.md)
**Question:** How do I make a request to update/downgrade the firmware version?

Covers:
- Protocol structure and packet format
- Python implementation for sending OTA commands
- Usage examples (basic, secure, downgrade)
- Complete workflow with progress monitoring
- Common errors and solutions

### [03-sha256-verification.md](03-sha256-verification.md)
**Question:** The SHA256 check is done on the ESP32, right? How does this work exactly? When we download the update, where does it temporarily get saved before we check the hash?

Covers:
- Flash memory architecture
- Direct-to-flash writing during download
- SHA256 verification process
- Memory usage breakdown
- What happens on hash match/mismatch
- Why this design is efficient

### [04-status-monitoring.md](04-status-monitoring.md)
**Question:** How will the Raspberry Pi know if the ESP has been updated or if the update failed? And if the update failed, how would the Raspberry Pi know what type of failure it was?

Covers:
- Status request/response protocol
- OTA state definitions
- Progress monitoring implementation
- Failure detection methods
- Error parsing from logs
- Enhanced error reporting (future)
- Complete monitoring workflow

### [05-auto-update.md](05-auto-update.md)
**Question:** When the ESP32 is set to auto-update, how does this work with getting the hash of the file?

Covers:
- Auto-update flow diagram
- Backend version API implementation
- SHA256 hash generation
- Current implementation gap and fix
- Configuration via Kconfig
- Complete auto-update example
- Security considerations

## Quick Navigation

### For Raspberry Pi Developers
1. Start with [Making Requests](02-making-requests.md) to learn the protocol
2. Read [Status Monitoring](04-status-monitoring.md) to track updates
3. Reference [SHA256 Verification](03-sha256-verification.md) for security details

### For Backend Developers
1. Read [Auto-Update](05-auto-update.md) for version API requirements
2. Check [SHA256 Verification](03-sha256-verification.md) for hash generation
3. See [Overview](01-overview.md) for system architecture

### For ESP32 Firmware Developers
1. Start with [Overview](01-overview.md) for architecture
2. Read [SHA256 Verification](03-sha256-verification.md) for implementation details
3. Check [Auto-Update](05-auto-update.md) for enhancement opportunities

## Related Files

- **Implementation:** `components/star_wifi_bridge/pynq_ota_manager.c`
- **API:** `components/star_wifi_bridge/include/pynq_ota_manager.h`
- **Protocol:** `components/star_wifi_bridge/include/pynq_wifi_protocol.h`
- **Tests:** `components/star_wifi_bridge/test/test_ota_*.c`
- **Configuration:** `main/Kconfig.projbuild`
- **Main Docs:** `OTA_IMPLEMENTATION.md`, `OTA_IMPROVEMENTS.md`

## Common Use Cases

### Update ESP32 from Raspberry Pi with Hash Verification
See: [02-making-requests.md - Example 2](02-making-requests.md#example-2-secure-update-with-sha256-verification)

### Roll Back to Previous Version
See: [02-making-requests.md - Example 3](02-making-requests.md#example-3-version-downgrade-rollback)

### Monitor Update Progress
See: [04-status-monitoring.md - Basic Progress Monitor](04-status-monitoring.md#basic-progress-monitor)

### Set Up Auto-Update Server
See: [05-auto-update.md - Backend Version API](05-auto-update.md#backend-version-api-implementation)

### Debug Failed Updates
See: [04-status-monitoring.md - Failure Detection](04-status-monitoring.md#failure-detection-methods)

## Key Concepts

- **Dual Boot Partitions:** ota_0 and ota_1 alternate for safe updates
- **Direct-to-Flash:** Firmware written directly to flash, not buffered in RAM
- **SHA256 Verification:** Cryptographic hash ensures firmware integrity
- **Automatic Rollback:** Bootloader reverts after 3 failed boots
- **Non-Blocking:** Update happens in background while ESP32 runs

## Getting Help

1. Check the relevant document above
2. Search for error messages in [Status Monitoring](04-status-monitoring.md)
3. Review test files: `test_ota_manager.c` and `test_ota_handler.c`
4. Check ESP32 UART logs for detailed error messages

## Version

Documentation created: 2025-01-15
Firmware version: 0.1.0
Protocol version: 1.0
