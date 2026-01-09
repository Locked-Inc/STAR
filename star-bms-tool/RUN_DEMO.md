# Running the Mock BMS Demo

## ✅ Cleanup Complete

The old Go/Wails version has been **removed**. The project structure is now clean:
- Old: `star-bms-tool/` (Go) ❌ DELETED
- New: `star-bms-tool/` (Rust) ✅ ACTIVE

## Quick Start

### Option 1: Using socat (Recommended - Most Realistic)

**Install socat:**
```bash
brew install socat
```

**Run the demo:**
```bash
cd /Users/bsikar/Documents/git/STAR/star-bms-tool
./run_mock_demo.sh
```

This will automatically:
1. Create a PTY pair (/tmp/bms_mock and /tmp/bms_client)
2. Start the mock BMS device
3. Run all CLI commands to demonstrate functionality
4. Clean up

### Option 2: Manual PTY Setup

**Terminal 1 - Create PTY pair:**
```bash
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

This will output two PTY paths like:
```
2024/01/09 ... N PTY is /dev/ttys004
2024/01/09 ... N PTY is /dev/ttys005
```

**Terminal 2 - Run mock device:**
```bash
cd /Users/bsikar/Documents/git/STAR/star-bms-tool
./target/release/mock_device /dev/ttys004
```

**Terminal 3 - Run CLI commands:**
```bash
cd /Users/bsikar/Documents/git/STAR/star-bms-tool
./target/release/app --port /dev/ttys005 telemetry
./target/release/app --port /dev/ttys005 cell-voltages --num-cells 4
./target/release/app --port /dev/ttys005 device-info
./target/release/app --port /dev/ttys005 read-register 0x00
./target/release/app --port /dev/ttys005 write-register 0x10 0x42
```

### Option 3: Real Serial Port

If you have actual BMS hardware:
```bash
./target/release/app --port /dev/ttyUSB0 telemetry
```

## CLI Commands Available

### Telemetry
```bash
./target/release/app --port <PORT> telemetry
```
Shows:
- Voltage, Current, Avg Current
- SOC (Relative & Absolute)
- Temperature
- Remaining/Full Capacity
- Cycle Count

### Cell Voltages
```bash
./target/release/app --port <PORT> cell-voltages [--num-cells 16]
```
Shows:
- Individual cell voltages
- Pack voltage, Min, Max, Delta

### Device Info
```bash
./target/release/app --port <PORT> device-info
```
Shows:
- Manufacturer, Device Name, Chemistry
- Serial Number (hex)
- Firmware/Hardware versions
- Cell count, Capacity, Voltage

### Read Register
```bash
./target/release/app --port <PORT> read-register <ADDRESS> [--num-bytes 1]
```
Examples:
- `read-register 0x00` - Read 1 byte from address 0x00
- `read-register 10` - Decimal address
- `read-register 0x0A --num-bytes 2` - Read 2 bytes

### Write Register
```bash
./target/release/app --port <PORT> write-register <ADDRESS> <VALUE> [--num-bytes 1]
```
Examples:
- `write-register 0x10 0x42` - Write 0x42 to address 0x10
- `write-register 16 66` - Decimal values

## Mock Device Data

The mock device simulates a realistic BMS with:
- **Battery:** 4S Li-ion pack (14.8V nominal)
- **Capacity:** 3.2Ah design, 3.0Ah current
- **SOC:** 75% (2.25Ah remaining)
- **Current:** -1.5A (discharging)
- **Temperature:** 25°C
- **Cycles:** 42
- **Manufacturer:** Texas Instruments
- **Device:** BQ78350-R1A
- **Chemistry:** LION

All responses include proper frame protocol (sync bytes 0x55AA, CRC-32 validation).

## Architecture

```
CLI App (./app) <---Serial/PTY---> Mock Device (./mock_device)
     |                                     |
     |-- Frame Protocol (sync, CRC)       |-- Frame Protocol
     |-- Protobuf Encoding                |-- Protobuf Decoding
     |-- BMS Commands                     |-- Mock Data Generation
```

## Next Steps

1. **Run the demo** to see CLI functionality
2. **Test with real hardware** if available
3. **Start Svelte frontend** development
4. **Add integration tests** using mock device
5. **Implement missing commands** (block read/write, protection status)

## Troubleshooting

**socat not found:**
```bash
brew install socat
```

**Permission denied on serial port:**
```bash
sudo chmod 666 /dev/ttyUSB0
# Or add user to dialout group (Linux):
sudo usermod -a -G dialout $USER
```

**Mock device won't start:**
- Check PTY path is correct
- Ensure no other process is using the PTY
- Try with different PTY pair

**No response from mock:**
- Verify both ends of PTY pair are correct
- Check mock device output for errors
- Use `ls -l /dev/pts/` or `/dev/tty*` to see available PTYs
