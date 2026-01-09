# BQ Studio Comprehensive Feature Reference & STAR BMS Tool Checklist

**Document Purpose**: Complete reference for Texas Instruments BQ Studio functionality, with detailed checklist for STAR BMS Tool feature parity and enhancements.

**Date Created**: 2026-01-11
**Based On**: Analysis of ESET453 course materials, BQ76940 evaluation software, and LabVIEW/TestStand automation
**Target**: STAR BMS Tool (Rust + Tauri + Svelte) replacing TI BQ Studio
**Hardware**: RX72N firmware replacing EV2300/EV2400 USB interface

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [BQ Studio Architecture](#bq-studio-architecture)
3. [Core Features Breakdown](#core-features-breakdown)
4. [EV2300/EV2400 Interface Protocol](#ev2300ev2400-interface-protocol)
5. [Automation & Scripting (.bqseq Files)](#automation--scripting-bqseq-files)
6. [LabVIEW Integration](#labview-integration)
7. [TestStand Integration](#teststand-integration)
8. [Data Logging & Export](#data-logging--export)
9. [Configuration Management](#configuration-management)
10. [STAR BMS Tool Feature Checklist](#star-bms-tool-feature-checklist)
11. [Implementation Priorities](#implementation-priorities)
12. [Testing Requirements](#testing-requirements)

---

## Executive Summary

### What is BQ Studio?

**BQ Studio** is Texas Instruments' Windows-only GUI application for evaluating and configuring BQ-series battery management ICs (BQ76920/30/40, BQ78350, BQ4050, etc.). It provides:
- Real-time battery telemetry monitoring
- Register read/write access
- Data Flash programming
- Automated test sequences
- Configuration management
- Production test integration

### What is EV2300/EV2400?

**EV2300/EV2400** are USB-to-I2C/SMBus interface boards that connect PC to BMS evaluation modules:
- **Hardware**: USB interface to I2C/SMBus bridge
- **Protocol**: Proprietary USB protocol (vendor-specific HID or bulk transfer)
- **DLLs**: `bq80xusb.dll`, `bq80xrw.dll`, `bq80xSim.dll`, `CMAPI.dll`
- **Software**: `commmgr.exe` for device management
- **Price**: $99-$149 USD (cost barrier for students/hobbyists)

### Your RX72N Replacement

Your **RX72N firmware** + **STAR BMS Tool** replaces this entire ecosystem:
- ✅ **Open source** vs proprietary
- ✅ **Cross-platform** (macOS/Linux/Windows) vs Windows-only
- ✅ **Low cost** ($30 RX72N eval board) vs $150 EV2300
- ✅ **Modern stack** (Rust/Tauri/Svelte) vs legacy .NET
- ✅ **Extensible** (Protocol Buffers) vs closed protocol
- ✅ **Real-time capable** (ThreadX RTOS) vs PC-based timing

---

## BQ Studio Architecture

### Software Components

```
┌─────────────────────────────────────────────────────────────┐
│                    BQ Studio GUI (bq76940.exe)              │
│  ┌──────────────┬──────────────┬──────────────┬───────────┐ │
│  │ Connection   │ Telemetry    │ Register     │ Data      │ │
│  │ Manager      │ Dashboard    │ Editor       │ Flash     │ │
│  └──────────────┴──────────────┴──────────────┴───────────┘ │
│  ┌──────────────┬──────────────┬──────────────┬───────────┐ │
│  │ Manufacturer │ Protection   │ Cell         │ Sequence  │ │
│  │ Access       │ Settings     │ Balancing    │ Runner    │ │
│  └──────────────┴──────────────┴──────────────┴───────────┘ │
└─────────────────────────────────────────────────────────────┘
                            ↓ API Calls
┌─────────────────────────────────────────────────────────────┐
│                     DLL Layer                                │
│  ┌─────────────────┬──────────────────┬────────────────────┐│
│  │ bq80xusb.dll    │ bq80xrw.dll      │ bq80xSim.dll       ││
│  │ (USB comms)     │ (Read/Write)     │ (Simulation)       ││
│  └─────────────────┴──────────────────┴────────────────────┘│
│  ┌─────────────────┬──────────────────┬────────────────────┐│
│  │ CMAPI.dll       │ commmgr.exe      │ EV2300a Driver     ││
│  │ (Common API)    │ (Device Mgr)     │ (USB HID)          ││
│  └─────────────────┴──────────────────┴────────────────────┘│
└─────────────────────────────────────────────────────────────┘
                            ↓ USB
┌─────────────────────────────────────────────────────────────┐
│                    EV2300/EV2400 Interface Board             │
│                    (USB-to-I2C Bridge)                       │
└─────────────────────────────────────────────────────────────┘
                            ↓ I2C/SMBus
┌─────────────────────────────────────────────────────────────┐
│              BQ76940 Evaluation Module / BMS IC              │
│                   (Battery Management System)                │
└─────────────────────────────────────────────────────────────┘
```

### Your STAR Architecture

```
┌─────────────────────────────────────────────────────────────┐
│            STAR BMS Tool (Tauri App)                         │
│  ┌──────────────┬──────────────┬──────────────┬───────────┐ │
│  │ Svelte UI    │ Real-time    │ Register     │ Device    │ │
│  │ (Frontend)   │ Telemetry    │ Editor       │ Info      │ │
│  └──────────────┴──────────────┴──────────────┴───────────┘ │
│  ┌──────────────┬──────────────┬──────────────┬───────────┐ │
│  │ Rust Backend │ BMS Manager  │ Frame        │ Protobuf  │ │
│  │ (Tauri)      │ (Threading)  │ Protocol     │ Codec     │ │
│  └──────────────┴──────────────┴──────────────┴───────────┘ │
└─────────────────────────────────────────────────────────────┘
                            ↓ USB CDC Serial
┌─────────────────────────────────────────────────────────────┐
│                   RX72N Firmware                             │
│  ┌──────────────┬──────────────┬──────────────┬───────────┐ │
│  │ USB Task     │ BMS Task     │ SMBus Driver │ ThreadX   │ │
│  │ (rx_usb_comm)│ (Commands)   │ (rx_bq78350) │ (RTOS)    │ │
│  └──────────────┴──────────────┴──────────────┴───────────┘ │
└─────────────────────────────────────────────────────────────┘
                            ↓ SMBus (I2C)
┌─────────────────────────────────────────────────────────────┐
│              BQ78350 / BQ76940 / BQ4050 IC                   │
│                   (Battery Management System)                │
└─────────────────────────────────────────────────────────────┘
```

---

## Core Features Breakdown

### 1. Connection Management

#### BQ Studio Features
- **Device Detection**: Auto-scan COM ports for EV2300
- **Manual Connection**: Select specific COM port
- **I2C Address Configuration**: Set device address (default 0x08 for BQ76920, 0x0B for BQ78350)
- **Connection Status Indicator**: LED/icon showing connection state
- **Auto-reconnect**: On communication failure
- **Multiple Device Support**: Switch between multiple connected devices

#### STAR BMS Tool Status
```
✅ Device Detection - Manual port selection (auto-detect possible)
✅ Manual Connection - Full implementation
✅ I2C Address Configuration - Hardcoded in RX72N firmware (configurable)
✅ Connection Status - UI indicator present
❌ Auto-reconnect - Not implemented (TODO)
❌ Multiple Device Support - Single device only (TODO)
```

#### Implementation Notes
**Auto-reconnect**: Add retry logic in `src/bms.rs`:
```rust
pub async fn connect_with_retry(port: &str, max_retries: u32) -> Result<BmsConnection> {
    for attempt in 0..max_retries {
        match connect(port).await {
            Ok(conn) => return Ok(conn),
            Err(e) if attempt < max_retries - 1 => {
                tokio::time::sleep(Duration::from_millis(1000)).await;
                continue;
            }
            Err(e) => return Err(e),
        }
    }
}
```

**Multiple Devices**: Extend `BmsManager` to maintain map of port → connection:
```rust
pub struct BmsManager {
    connections: Arc<Mutex<HashMap<String, SerialPort>>>,
    active_port: Arc<Mutex<Option<String>>>,
}
```

---

### 2. Telemetry Dashboard

#### BQ Studio Features
| Parameter | Unit | BQ Studio Display | STAR Status |
|-----------|------|-------------------|-------------|
| Voltage | V | Numeric + Gauge | ✅ Numeric |
| Current | A | Numeric + Sign | ✅ Numeric + Sign |
| State of Charge (SOC) | % | Numeric + Bar | ✅ Numeric |
| Temperature | °C | Numeric + Alert | ✅ Numeric |
| Remaining Capacity | mAh | Numeric | ✅ Numeric |
| Design Capacity | mAh | Numeric | ✅ Numeric |
| Cycle Count | # | Numeric | ✅ Numeric |
| Average Power | W | Calculated | ❌ Not implemented |
| Time to Empty | min | Calculated | ❌ Not implemented |
| Time to Full | min | Calculated | ❌ Not implemented |
| Cell Voltages | mV | Individual bars | ✅ Bars |
| Max Cell Voltage | mV | Numeric | ❌ Calculated in UI? |
| Min Cell Voltage | mV | Numeric | ❌ Calculated in UI? |
| Delta Cell Voltage | mV | Numeric | ❌ Not shown |
| FET Status | On/Off | LED indicators | ❌ Not displayed |
| Protection Flags | Bits | Individual LEDs | ❌ Not displayed |

#### Missing Calculations
Add to Svelte UI (`ui/src/App.svelte`):
```javascript
$: averagePower = (telemetry?.voltage_v || 0) * (telemetry?.current_a || 0);
$: timeToEmpty = telemetry?.current_a > 0 ?
    (telemetry?.remaining_capacity_mah / (telemetry.current_a * 1000)) * 60 :
    null;
$: timeToFull = telemetry?.current_a < 0 ?
    ((telemetry?.full_charge_capacity_mah - telemetry?.remaining_capacity_mah) /
     (Math.abs(telemetry.current_a) * 1000)) * 60 :
    null;
$: maxCellVoltage = cellVoltages ? Math.max(...cellVoltages.map(c => c.voltage_mv)) : 0;
$: minCellVoltage = cellVoltages ? Math.min(...cellVoltages.map(c => c.voltage_mv)) : 0;
$: deltaCellVoltage = maxCellVoltage - minCellVoltage;
```

#### Auto-Refresh
BQ Studio: **Configurable 100ms - 5000ms interval**
STAR Status: **✅ 1Hz fixed** (make configurable)

Add refresh rate selector to UI:
```svelte
<select bind:value={refreshInterval}>
  <option value={100}>Fast (100ms)</option>
  <option value={500}>Normal (500ms)</option>
  <option value={1000}>Slow (1s)</option>
  <option value={0}>Manual</option>
</select>
```

---

### 3. Cell Voltage Monitoring

#### BQ Studio Features
- **Individual Cell Display**: Up to 16 cells shown as vertical bars
- **Color Coding**:
  - Green: Normal (within 50mV of average)
  - Yellow: Warning (50-100mV from average)
  - Red: Critical (>100mV from average)
- **Numeric Values**: mV precision next to bars
- **Cell Balancing Status**: Indicator showing which cells are balancing
- **Historical Min/Max**: Track highest and lowest voltage per cell
- **Export Cell Data**: CSV export of cell voltages over time

#### STAR Status
```
✅ Individual Cell Display (1-16 cells)
❌ Color Coding (all same color currently)
✅ Numeric Values (mV shown)
❌ Cell Balancing Status (not shown)
❌ Historical Min/Max (not tracked)
❌ Export Cell Data (CSV not implemented for cells)
```

#### Implementation: Color Coding
```svelte
<!-- ui/src/App.svelte -->
<script>
function getCellColor(voltage_mv, avgVoltage) {
  const delta = Math.abs(voltage_mv - avgVoltage);
  if (delta > 100) return '#ff4444'; // Red
  if (delta > 50) return '#ffaa00';  // Yellow
  return '#44ff44';                  // Green
}

$: avgCellVoltage = cellVoltages ?
  cellVoltages.reduce((sum, c) => sum + c.voltage_mv, 0) / cellVoltages.length :
  0;
</script>

{#each cellVoltages as cell, i}
  <div class="cell-bar" style="background-color: {getCellColor(cell.voltage_mv, avgCellVoltage)}">
    <span>Cell {i+1}: {cell.voltage_mv}mV</span>
  </div>
{/each}
```

#### Implementation: Cell Balancing Status
Need to add to Protocol Buffers:
```protobuf
// In star-proto/proto/star/v1/bms.proto
message BmsCellVoltage {
  uint32 cell_number = 1;
  uint32 voltage_mv = 2;
  bool balancing_active = 3;  // NEW
}
```

Update RX72N firmware to read balancing status from BQ chip and include in response.

---

### 4. Register Editor (Direct Memory Access)

#### BQ Studio Features
- **Register Map Table**: All registers displayed in table
- **Read Single Register**: Click to read current value
- **Write Single Register**: Editable field with validation
- **Hex/Decimal Toggle**: Display values in hex or decimal
- **Register Descriptions**: Tooltip/sidebar showing register purpose
- **Bulk Read**: Read all registers at once
- **Bulk Write**: Write multiple registers from file
- **Watch List**: Monitor specific registers continuously
- **Register Compare**: Compare current vs. default/saved values
- **Bit Field Editor**: For registers with multiple bit fields

#### STAR Status
```
✅ Read Single Register (0x00-0xFF)
✅ Write Single Register
✅ Hex/Decimal Input (both accepted)
❌ Register Map Table (not displayed)
❌ Register Descriptions (no tooltips)
❌ Bulk Read (one at a time only)
❌ Bulk Write (no batch operations)
❌ Watch List (no register monitoring)
❌ Register Compare (no comparison feature)
❌ Bit Field Editor (treat as single value)
```

#### Implementation: Register Map Table

Create register definitions file:
```typescript
// ui/src/bms_register_map.ts
export interface RegisterDef {
  address: number;
  name: string;
  description: string;
  access: 'R' | 'W' | 'RW';
  size: 1 | 2; // bytes
  fields?: BitField[];
}

export interface BitField {
  name: string;
  bit_start: number;
  bit_end: number;
  description: string;
}

export const BQ76920_REGISTERS: RegisterDef[] = [
  {
    address: 0x00,
    name: 'SYS_STAT',
    description: 'System Status Register',
    access: 'RW',
    size: 1,
    fields: [
      { name: 'OCD', bit_start: 0, bit_end: 0, description: 'Overcurrent Discharge' },
      { name: 'SCD', bit_start: 1, bit_end: 1, description: 'Short Circuit Discharge' },
      { name: 'OV', bit_start: 2, bit_end: 2, description: 'Overvoltage' },
      { name: 'UV', bit_start: 3, bit_end: 3, description: 'Undervoltage' },
      // ... more fields
    ],
  },
  // ... more registers
];
```

Add register map component:
```svelte
<!-- ui/src/components/RegisterMap.svelte -->
<table class="register-map">
  <thead>
    <tr>
      <th>Address</th>
      <th>Name</th>
      <th>Value</th>
      <th>Description</th>
      <th>Actions</th>
    </tr>
  </thead>
  <tbody>
    {#each BQ76920_REGISTERS as reg}
      <tr>
        <td>0x{reg.address.toString(16).padStart(2, '0')}</td>
        <td>{reg.name}</td>
        <td><input type="text" value={registerValues[reg.address] || '--'} /></td>
        <td>{reg.description}</td>
        <td>
          <button on:click={() => readRegister(reg.address)}>Read</button>
          <button on:click={() => writeRegister(reg.address)}>Write</button>
        </td>
      </tr>
    {/each}
  </tbody>
</table>
```

---

### 5. Manufacturer Access Commands

#### BQ Studio Features
- **Command Library**: Pre-defined manufacturer access commands
- **Custom Command**: Send arbitrary 2-byte command code
- **Block Read Response**: Display multi-byte responses
- **Command History**: Log of all executed commands
- **Common Commands**:
  - ManufacturerAccess (0x00/0x01)
  - DeviceType (0x0001)
  - FirmwareVersion (0x0002)
  - HardwareVersion (0x0003)
  - ChemistryID (0x0006)
  - Reset (0x0012)
  - IT Enable (0x0021)
  - CAL_ENABLE (0x002D)
  - SEAL (0x0030)
  - IT_ENABLE2 (0x00B9)
  - And 100+ device-specific commands

#### STAR Status
```
✅ Custom Command (via ReadManufacturerAccess)
✅ Block Read Support (subcommand + data)
❌ Command Library (no pre-defined commands)
❌ Command History (not logged in UI)
❌ Common Commands UI (requires manual entry)
```

#### Implementation: Manufacturer Access Command Library

```typescript
// ui/src/manufacturer_commands.ts
export interface MfgCommand {
  code: number;
  name: string;
  description: string;
  responseType: 'uint16' | 'string' | 'block';
  responseLength?: number;
}

export const BQ78350_MFG_COMMANDS: MfgCommand[] = [
  { code: 0x0001, name: 'DeviceType', description: 'Returns device type ID', responseType: 'uint16' },
  { code: 0x0002, name: 'FirmwareVersion', description: 'Firmware version', responseType: 'uint16' },
  { code: 0x0003, name: 'HardwareVersion', description: 'Hardware version', responseType: 'uint16' },
  { code: 0x0006, name: 'ChemistryID', description: 'Battery chemistry ID', responseType: 'uint16' },
  { code: 0x0012, name: 'Reset', description: 'Soft reset device', responseType: 'uint16' },
  { code: 0x0021, name: 'IT_ENABLE', description: 'Enable impedance track', responseType: 'uint16' },
  { code: 0x002D, name: 'CAL_ENABLE', description: 'Enable calibration mode', responseType: 'uint16' },
  { code: 0x0030, name: 'SEAL', description: 'Seal device (disable writes)', responseType: 'uint16' },
  { code: 0x0041, name: 'DeviceReset', description: 'Full device reset', responseType: 'uint16' },
  { code: 0x0044, name: 'ManufacturerBlockAccess', description: 'Access data flash', responseType: 'block', responseLength: 32 },
  { code: 0x00B9, name: 'IT_ENABLE2', description: 'IT Enable confirmation', responseType: 'uint16' },
  // ... add all 100+ commands from datasheet
];
```

Add UI dropdown for command selection:
```svelte
<select bind:value={selectedMfgCommand}>
  <option value="">-- Select Command --</option>
  {#each BQ78350_MFG_COMMANDS as cmd}
    <option value={cmd.code}>{cmd.name} (0x{cmd.code.toString(16).padStart(4, '0')}) - {cmd.description}</option>
  {/each}
</select>
<button on:click={executeMfgCommand}>Execute</button>
```

---

### 6. Data Flash Programming

#### BQ Studio Features
- **Data Flash Explorer**: Tree view of all data flash classes and subclasses
- **Read Data Flash**: Read block (32 bytes) from any address
- **Write Data Flash**: Write block with automatic checksum calculation
- **Golden Image**: Load/save complete data flash configuration
- **Configuration Wizard**: Step-by-step configuration for common parameters
- **Parameter Categories**:
  - **Gas Gauging**: QMax, impedance track parameters, Ra tables
  - **Protection**: OV/UV/OC thresholds, delays, recovery
  - **Cell Balancing**: Min/max thresholds, cell delta
  - **Charge Control**: Charge voltage, current, temperature limits
  - **Discharge Control**: Discharge current, temperature limits
  - **Manufacturer Info**: Device name, serial number, manufacturing date
  - **Advanced Config**: IT configuration, lifetime data, calibration

#### STAR Status
```
❌ Data Flash Explorer (not implemented)
❌ Read Data Flash (no UI for block read)
❌ Write Data Flash (no UI for block write)
❌ Golden Image (no save/load)
❌ Configuration Wizard (experimental only)
❌ All Parameter Categories (not accessible)
```

#### Data Flash Structure (BQ78350)

```
Data Flash Memory Map:
0x4000-0x4FFF: Configuration Data (4KB)
  Class 48 (0x30): Gas Gauging
    - Subclass 0: Design Capacity, Design Energy
    - Subclass 1: Cell Count, Series Cell Count
    - Subclass 2: Pack Configuration

  Class 49 (0x31): Gas Gauging IT Cfg
    - Subclass 0: Load Select, Load Mode
    - Subclass 1: Reserve Cap mAh

  Class 64 (0x40): Registers
    - Subclass 0: Operation Configuration
    - Subclass 1: SOC Flag Configuration

  Class 68 (0x44): Power
    - Subclass 0: Shutdown voltage, sleep current

  Class 80 (0x50): System Data
    - Subclass 0: Manufacturing date, serial number

  Class 81 (0x51): Manufacturer Info
    - Subclass 0: Device name, chemistry

  Class 82 (0x52): Safety (BQ78350)
    - Subclass 0: OV, UV, OCC, OCD thresholds
    - Subclass 1: Protection delays
```

#### Implementation: Data Flash Access

Add to Protocol Buffers:
```protobuf
message ReadDataFlashCommand {
  uint32 address = 1;  // Data flash address (0x4000-0x4FFF)
  uint32 length = 2;   // Number of bytes (typically 32)
}

message WriteDataFlashCommand {
  uint32 address = 1;
  bytes data = 2;      // Block data (up to 32 bytes)
  bool verify = 3;     // Verify write after completion
}

message BmsDataFlashData {
  uint32 address = 1;
  bytes data = 2;
  uint32 checksum = 3; // Calculated checksum
}
```

Add RX72N firmware support:
```c
// examples/bms_evaluation/main.c
static void handle_read_data_flash(const BmsCommandRequest *request, BmsCommandResponse *response) {
  uint16_t address = request->command.read_data_flash.address;
  uint8_t length = request->command.read_data_flash.length;

  // Use ManufacturerBlockAccess (0x0044) to read data flash
  // 1. Write subcommand 0x0044
  // 2. Write address LSB to 0x3F
  // 3. Write address MSB to 0x40
  // 4. Read block from 0x41-0x60 (32 bytes)

  uint8_t block_data[32];
  rx_bq78350_read_data_flash(address, block_data, length);

  response->response.data_flash_data.address = address;
  memcpy(response->response.data_flash_data.data, block_data, length);
  response->response.data_flash_data.checksum = calculate_checksum(block_data, length);
}
```

---

### 7. Protection Configuration

#### BQ Studio Features
- **Overvoltage (OV) Settings**:
  - Threshold (mV per cell): 3000-5000mV
  - Delay (ms): 1-1000ms
  - Recovery threshold (mV)
- **Undervoltage (UV) Settings**:
  - Threshold (mV per cell): 2000-3500mV
  - Delay (ms): 1-1000ms
  - Recovery threshold (mV)
- **Overcurrent Charge (OCC) Settings**:
  - Threshold (mV across sense resistor): -200 to 200mV
  - Delay (ms): 1-1000ms
- **Overcurrent Discharge (OCD) Settings**:
  - Threshold (mV): 0-200mV
  - Delay (ms): 1-1000ms
- **Short Circuit Discharge (SCD) Settings**:
  - Threshold (mV): 0-200mV
  - Delay (µs): 10-500µs
- **Temperature Protection**:
  - Overtemperature charge/discharge (°C)
  - Undertemperature charge/discharge (°C)
- **FET Control**:
  - CHG FET manual control (on/off)
  - DSG FET manual control (on/off)
  - PCHG FET control (if available)
- **Protection Status Display**:
  - Real-time flag display (LED indicators)
  - Protection event log
  - Clear faults button

#### STAR Status
```
❌ All Protection Settings (not accessible in UI)
❌ FET Control (not implemented)
❌ Protection Status Display (flags not shown)
❌ Event Log (not tracked)
❌ Clear Faults (not implemented)
```

#### Implementation: Protection Settings UI

Create protection configuration component:
```svelte
<!-- ui/src/components/ProtectionSettings.svelte -->
<script>
let ovThreshold = 4200; // mV
let ovDelay = 2000;     // ms
let uvThreshold = 3000;
let uvDelay = 2000;
let ocdThreshold = 100; // mV
let ocdDelay = 320;     // ms

async function updateProtection() {
  // Convert thresholds to register values using datasheet formulas
  const ovTripReg = Math.round((ovThreshold - 1500) / 50);
  const uvTripReg = Math.round((uvThreshold - 1500) / 50);

  // Write to protection registers
  await invoke('write_register', { address: 0x09, value: ovTripReg });  // OV_TRIP
  await invoke('write_register', { address: 0x0A, value: uvTripReg });  // UV_TRIP
  // ... write other protection registers
}
</script>

<div class="protection-settings">
  <h3>Overvoltage Protection</h3>
  <label>Threshold (mV): <input type="number" bind:value={ovThreshold} min={3000} max={5000} step={50} /></label>
  <label>Delay (ms): <input type="number" bind:value={ovDelay} min={1} max={1000} /></label>

  <h3>Undervoltage Protection</h3>
  <label>Threshold (mV): <input type="number" bind:value={uvThreshold} min={2000} max={3500} step={50} /></label>
  <label>Delay (ms): <input type="number" bind:value={uvDelay} min={1} max={1000} /></label>

  <!-- ... more protection settings -->

  <button on:click={updateProtection}>Apply Settings</button>
</div>
```

---

### 8. Cell Balancing Control

#### BQ Studio Features
- **Auto Balancing**: Enable/disable automatic balancing
- **Manual Balancing**: Select specific cells to balance
- **Balancing Threshold**: Set minimum cell voltage difference (mV)
- **Balancing Current Display**: Show actual balancing current per cell
- **Balancing Status LEDs**: Visual indication of which cells are balancing
- **Balancing Duration**: Track how long each cell has been balancing
- **Safety Interlocks**: Disable balancing during charge/discharge
- **Temperature Monitoring**: Stop balancing if overtemperature

#### STAR Status
```
❌ Auto Balancing (not implemented)
❌ Manual Balancing (no cell selection)
❌ Balancing Threshold (not configurable)
❌ Balancing Current Display (not shown)
❌ Balancing Status LEDs (not shown)
❌ Balancing Duration (not tracked)
❌ Safety Interlocks (not enforced)
❌ Temperature Monitoring (not integrated)
```

#### Implementation: Cell Balancing UI

```svelte
<!-- ui/src/components/CellBalancing.svelte -->
<script>
let autoBalancing = true;
let balancingThreshold = 50; // mV
let manualCellSelection = [false, false, false, false]; // 4S battery

async function enableAutoBalancing() {
  // Write to CELLBAL register to enable automatic balancing
  await invoke('write_register', { address: 0x01, value: 0x01 });
}

async function balanceCell(cellIndex) {
  // Set bit in CELLBAL register to force balance specific cell
  const cellBit = 1 << cellIndex;
  await invoke('write_register', { address: 0x01, value: cellBit });
}

async function readBalancingStatus() {
  // Read CELLBAL register to get current balancing status
  const status = await invoke('read_register', { address: 0x01 });
  // Update UI based on status bits
}

setInterval(readBalancingStatus, 1000); // Update every second
</script>

<div class="balancing-control">
  <h3>Cell Balancing</h3>

  <label>
    <input type="checkbox" bind:checked={autoBalancing} on:change={enableAutoBalancing} />
    Auto Balancing
  </label>

  <label>
    Balancing Threshold (mV):
    <input type="number" bind:value={balancingThreshold} min={10} max={200} step={10} />
  </label>

  <h4>Manual Cell Selection</h4>
  {#each cellVoltages as cell, i}
    <div class="cell-balance-control">
      <span>Cell {i+1}: {cell.voltage_mv}mV</span>
      <input type="checkbox" bind:checked={manualCellSelection[i]} on:change={() => balanceCell(i)} />
      {#if cell.balancing_active}
        <span class="balancing-indicator">⚡ Balancing</span>
      {/if}
    </div>
  {/each}
</div>
```

---

## Automation & Scripting (.bqseq Files)

### BQSeq File Format Specification

#### File Structure
```
# Comments start with '#'
Name = <Sequence Name>
Description = <Sequence Description>

# Commands (case insensitive, whitespace delimited)
ReadByte <address>
WriteByte <address> <data>
Delay <milliseconds>
```

#### Example Sequences

**Clear Faults:**
```
Name = Clear All Faults
Description = Clears all protection fault flags
WriteByte 0x00 0x3F
ReadByte 0x00
Delay 20
```

**Set UV Trip Threshold:**
```
Name = Set UV Trip to 2.75V
Description = Configure undervoltage trip to 2750mV
WriteByte 0x0A 0xA0
ReadByte 0x0A
```

**Complete Initialization:**
```
Name = BQ76920 Initialization
Description = Complete startup sequence

# Clear all faults
WriteByte 0x00 0x3F
Delay 50

# Set OV trip (4.2V = 0xAA)
WriteByte 0x09 0xAA
ReadByte 0x09
Delay 10

# Set UV trip (3.0V = 0x64)
WriteByte 0x0A 0x64
ReadByte 0x0A
Delay 10

# Enable ADC and coulomb counter
WriteByte 0x04 0x19
Delay 100

# Read status
ReadByte 0x00
```

### BQ Studio Sequence Runner Features

- **Sequence Library**: Store multiple .bqseq files
- **Auto-discovery**: Scan `Documents\Texas Instruments\bq76940\sequence\` for .bqseq files
- **UI Integration**: Dropdown to select sequence
- **One-click Execution**: Run button to execute selected sequence
- **Progress Indicator**: Show current command being executed
- **Result Display**: Show read values and status
- **Error Handling**: Stop on error or continue
- **Loop Support**: Repeat sequence N times
- **Conditional Execution**: Skip commands based on conditions

### STAR Implementation Plan

#### Phase 1: Basic Sequence Support
```rust
// src/sequence.rs
pub struct BqSequence {
    name: String,
    description: String,
    commands: Vec<SequenceCommand>,
}

pub enum SequenceCommand {
    ReadByte { address: u8 },
    WriteByte { address: u8, value: u8 },
    Delay { milliseconds: u32 },
}

impl BqSequence {
    pub fn from_file(path: &Path) -> Result<Self> {
        let content = std::fs::read_to_string(path)?;
        let mut name = String::new();
        let mut description = String::new();
        let mut commands = Vec::new();

        for line in content.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('#') {
                continue;
            }

            if line.starts_with("Name") {
                name = line.split('=').nth(1).unwrap_or("").trim().to_string();
            } else if line.starts_with("Description") {
                description = line.split('=').nth(1).unwrap_or("").trim().to_string();
            } else {
                commands.push(Self::parse_command(line)?);
            }
        }

        Ok(Self { name, description, commands })
    }

    fn parse_command(line: &str) -> Result<SequenceCommand> {
        let parts: Vec<&str> = line.split_whitespace().collect();
        match parts[0].to_lowercase().as_str() {
            "readbyte" => {
                let addr = u8::from_str_radix(parts[1].trim_start_matches("0x"), 16)?;
                Ok(SequenceCommand::ReadByte { address: addr })
            }
            "writebyte" => {
                let addr = u8::from_str_radix(parts[1].trim_start_matches("0x"), 16)?;
                let val = u8::from_str_radix(parts[2].trim_start_matches("0x"), 16)?;
                Ok(SequenceCommand::WriteByte { address: addr, value: val })
            }
            "delay" => {
                let ms = parts[1].parse()?;
                Ok(SequenceCommand::Delay { milliseconds: ms })
            }
            _ => Err(anyhow!("Unknown command: {}", parts[0])),
        }
    }

    pub async fn execute(&self, bms: &BmsConnection) -> Result<Vec<SequenceResult>> {
        let mut results = Vec::new();

        for cmd in &self.commands {
            match cmd {
                SequenceCommand::ReadByte { address } => {
                    let value = bms.read_register(*address).await?;
                    results.push(SequenceResult::ReadByte { address: *address, value });
                }
                SequenceCommand::WriteByte { address, value } => {
                    bms.write_register(*address, *value).await?;
                    results.push(SequenceResult::WriteByte { address: *address, value: *value });
                }
                SequenceCommand::Delay { milliseconds } => {
                    tokio::time::sleep(Duration::from_millis(*milliseconds as u64)).await;
                    results.push(SequenceResult::Delay { milliseconds: *milliseconds });
                }
            }
        }

        Ok(results)
    }
}
```

#### Phase 2: UI Integration
```svelte
<!-- ui/src/components/SequenceRunner.svelte -->
<script>
let sequences = [];
let selectedSequence = null;
let executing = false;
let results = [];

async function loadSequences() {
  // Load .bqseq files from user directory or bundled resources
  sequences = await invoke('list_sequences');
}

async function executeSequence() {
  executing = true;
  results = [];

  try {
    results = await invoke('execute_sequence', {
      sequencePath: selectedSequence.path
    });
  } catch (e) {
    console.error('Sequence execution failed:', e);
  } finally {
    executing = false;
  }
}

onMount(loadSequences);
</script>

<div class="sequence-runner">
  <h3>Sequence Runner</h3>

  <select bind:value={selectedSequence}>
    <option value={null}>-- Select Sequence --</option>
    {#each sequences as seq}
      <option value={seq}>{seq.name} - {seq.description}</option>
    {/each}
  </select>

  <button on:click={executeSequence} disabled={!selectedSequence || executing}>
    {executing ? 'Executing...' : 'Run Sequence'}
  </button>

  {#if results.length > 0}
    <div class="results">
      <h4>Execution Results</h4>
      {#each results as result}
        <div class="result-item">
          {#if result.type === 'ReadByte'}
            ReadByte 0x{result.address.toString(16)} = 0x{result.value.toString(16)}
          {:else if result.type === 'WriteByte'}
            WriteByte 0x{result.address.toString(16)} ← 0x{result.value.toString(16)}
          {:else if result.type === 'Delay'}
            Delay {result.milliseconds}ms
          {/if}
        </div>
      {/each}
    </div>
  {/if}
</div>
```

---

## LabVIEW Integration

### LabVIEW VI Architecture (from ESET453 Labs)

#### Common LabVIEW Pattern
```
Main VI (Front Panel)
  ├── State Machine (While Loop)
  │   ├── Initialize State
  │   │   ├── DMM_Init.vi
  │   │   ├── PSU_Init.vi
  │   │   └── EV2300_Init.vi (YOUR REPLACEMENT: USB CDC)
  │   │
  │   ├── Configure State
  │   │   ├── DMM_Config.vi
  │   │   ├── PSU_Config.vi
  │   │   └── Set Test Parameters
  │   │
  │   ├── Measure State
  │   │   ├── EV2300_ReadRegister.vi → YOUR: invoke_serial_command()
  │   │   ├── DMM_Measure.vi
  │   │   └── Store Data in Array
  │   │
  │   ├── Analyze State
  │   │   ├── Calculate Statistics
  │   │   ├── Check Pass/Fail Limits
  │   │   └── Update Graphs
  │   │
  │   ├── Log State
  │   │   ├── Excel_Report.vi
  │   │   └── Save to Database
  │   │
  │   └── Cleanup State
  │       ├── DMM_Close.vi
  │       ├── PSU_Close.vi
  │       └── EV2300_Close.vi → YOUR: close_serial_port()
```

### Replacing EV2300 VIs with STAR BMS Tool

#### Option 1: Create LabVIEW Serial Wrapper VIs
Create custom VIs that call your RX72N device:

**EV2300_Init.vi → STAR_BMS_Init.vi**
```labview
Inputs:
  - COM Port (String): e.g., "COM3" or "/dev/ttyUSB0"
  - Baud Rate (U32): 115200
  - Timeout (U32): 5000 ms

Outputs:
  - Connection Handle (RefNum)
  - Error Out (Error Cluster)

Implementation:
  1. Open VISA Serial Session
  2. Configure 8N1, no flow control
  3. Send initial handshake (optional)
  4. Return session reference
```

**EV2300_ReadRegister.vi → STAR_BMS_ReadRegister.vi**
```labview
Inputs:
  - Connection Handle (RefNum)
  - Register Address (U8)

Outputs:
  - Register Value (U16)
  - Error Out (Error Cluster)

Implementation:
  1. Build Protobuf ReadRegisterCommand
  2. Frame with sync bytes + CRC-32
  3. VISA Write to serial port
  4. VISA Read response (blocking, with timeout)
  5. Parse Protobuf response
  6. Extract register value
  7. Return value
```

**EV2300_WriteRegister.vi → STAR_BMS_WriteRegister.vi**
```labview
Inputs:
  - Connection Handle (RefNum)
  - Register Address (U8)
  - Value (U16)

Outputs:
  - Success (Boolean)
  - Error Out (Error Cluster)

Implementation:
  1. Build Protobuf WriteRegisterCommand
  2. Frame and send via VISA
  3. Wait for acknowledgment
  4. Return success status
```

#### Option 2: Use Command-Line Interface from LabVIEW

Alternative: Call your CLI tool from LabVIEW using System Exec:

```labview
System Exec.vi
  Command Line: "star-bms-tool --port COM3 read-register 0x00"
  Wait for completion: TRUE
  Parse stdout: Extract value from JSON/text output
```

This is simpler but less efficient (spawns process per call).

#### Option 3: Create LabVIEW-Native Protobuf Support

Use LabVIEW's Call Library Function Node to call your Rust library directly:
1. Build `star-bms-tool` as C-compatible shared library (.dll/.so)
2. Export C functions with `#[no_mangle]` and `extern "C"`
3. Call from LabVIEW using CLF Node

```rust
#[no_mangle]
pub extern "C" fn bms_init(port: *const c_char) -> *mut BmsConnection {
    // ...
}

#[no_mangle]
pub extern "C" fn bms_read_register(conn: *mut BmsConnection, address: u8) -> u16 {
    // ...
}
```

### STAR Status for LabVIEW Integration
```
❌ LabVIEW Serial Wrapper VIs (not created)
✅ CLI Interface (can be called via System Exec)
❌ Shared Library Export (Rust not compiled as .dll)
❌ Example LabVIEW Project (not provided)
```

---

## TestStand Integration

### TestStand Sequence Structure (from ESET453 Labs)

TestStand sequences (`.seq` files) follow this pattern:

```
MainSequence
  ├── Setup
  │   ├── Initialize DMM (LabVIEW VI)
  │   ├── Initialize PSU (LabVIEW VI)
  │   ├── Initialize BMS Interface (YOUR RX72N)
  │   └── Load Test Limits from File
  │
  ├── Test UUT (Unit Under Test)
  │   ├── Pre-Test
  │   │   ├── ID Verification
  │   │   └── Continuity Check
  │   │
  │   ├── Parametric Tests
  │   │   ├── IDDQ Test
  │   │   │   ├── Set BMS to SHIP mode
  │   │   │   ├── Measure current (DMM)
  │   │   │   ├── Numeric Limit Test (< 10µA)
  │   │   │   └── Log Result
  │   │   │
  │   │   ├── OV Protection Test
  │   │   │   ├── Ramp PSU voltage
  │   │   │   ├── Monitor BMS registers
  │   │   │   ├── Detect OV trip
  │   │   │   ├── Measure trip voltage
  │   │   │   └── Numeric Limit Test (4.20V ±50mV)
  │   │   │
  │   │   └── ADC Accuracy Test
  │   │       ├── Apply known voltage
  │   │       ├── Read BMS voltage register
  │   │       ├── Calculate error
  │   │       └── Numeric Limit Test (±10mV)
  │   │
  │   └── Functional Tests
  │       ├── Cell Balancing Test
  │       ├── Protection Recovery Test
  │       └── Communication Reliability Test
  │
  ├── Post-Test
  │   ├── Generate Report
  │   ├── Log Data to Database
  │   └── Print Label (if pass)
  │
  └── Cleanup
      ├── Power Down UUT
      ├── Close DMM
      ├── Close PSU
      └── Close BMS Connection
```

### Integrating STAR BMS Tool with TestStand

#### Method 1: Create TestStand Step Types

Create custom step types that call your tool:

**Read BMS Register Step**
```
Step Type: Action
Adapter: LabVIEW or Command Line Adapter
Module: star_bms_read_register.vi or "star-bms-tool read-register"
Parameters:
  - Port: String (COM3)
  - Address: Number (0x00)
Result:
  - RegisterValue: Number
```

**Write BMS Register Step**
```
Step Type: Action
Adapter: LabVIEW or Command Line
Module: star_bms_write_register.vi
Parameters:
  - Port: String
  - Address: Number
  - Value: Number
Result:
  - Success: Boolean
```

#### Method 2: Use TestStand's ActiveX/COM Interface

If you build your Rust tool with COM support, TestStand can call it directly:

```csharp
// Pseudo-code for COM interface
[ComVisible(true)]
[Guid("...")]
public interface IBmsConnection {
    ushort ReadRegister(byte address);
    void WriteRegister(byte address, ushort value);
    BmsTelemetry ReadTelemetry();
}
```

#### Method 3: REST API Server Mode

Run your tool as a REST API server, TestStand calls HTTP endpoints:

```rust
// src/api.rs (NEW)
use axum::{Router, routing::get, routing::post, Json};

#[tokio::main]
async fn start_api_server() {
    let app = Router::new()
        .route("/telemetry", get(get_telemetry))
        .route("/register/:addr", get(read_register))
        .route("/register/:addr", post(write_register));

    axum::Server::bind(&"127.0.0.1:3000".parse().unwrap())
        .serve(app.into_make_service())
        .await
        .unwrap();
}

async fn get_telemetry() -> Json<BmsTelemetry> {
    // Read telemetry and return JSON
}
```

TestStand can then use HTTP steps to interact with your tool.

### STAR Status for TestStand Integration
```
❌ Custom TestStand Step Types (not created)
❌ COM Interface (not implemented)
❌ REST API Mode (not implemented)
✅ CLI Mode (usable but inefficient)
```

---

## Data Logging & Export

### BQ Studio Features

#### Real-Time Data Logging
- **Log to CSV**: Continuous logging to CSV file
- **Configurable Parameters**: Select which telemetry fields to log
- **Timestamp**: Each sample timestamped
- **Sample Rate**: 100ms - 60s intervals
- **Auto-filename**: Based on date/time
- **File Size Limit**: Auto-rotate when size exceeded

#### Data Export Formats
- **CSV**: Comma-separated values
- **Excel**: .xlsx with charts
- **JSON**: Structured data export
- **XML**: For automated parsing
- **TI Format**: Proprietary format for post-processing

#### Logged Parameters
- Voltage, current, SOC, temperature, capacity
- Cell voltages (all cells)
- Protection flags (binary state)
- FET status
- Cycle count
- Register snapshots

### STAR Implementation

#### Phase 1: CSV Logging
```rust
// src/logger.rs
pub struct BmsLogger {
    file: Option<File>,
    sample_interval: Duration,
    parameters: Vec<TelemetryParameter>,
}

impl BmsLogger {
    pub fn new(path: &Path, interval_ms: u64) -> Result<Self> {
        let file = File::create(path)?;
        let mut wtr = csv::Writer::from_writer(file);

        // Write header
        wtr.write_record(&["Timestamp", "Voltage_V", "Current_A", "SOC_%", "Temp_C",
                          "Cell1_mV", "Cell2_mV", "Cell3_mV", "Cell4_mV"])?;

        Ok(Self {
            file: Some(wtr.into_inner()?),
            sample_interval: Duration::from_millis(interval_ms),
            parameters: vec![],
        })
    }

    pub async fn log_sample(&mut self, telemetry: &BmsTelemetry, cell_voltages: &[BmsCellVoltage]) {
        let timestamp = chrono::Utc::now().to_rfc3339();

        let mut wtr = csv::Writer::from_writer(self.file.as_ref().unwrap());
        wtr.write_record(&[
            timestamp,
            telemetry.voltage_v.to_string(),
            telemetry.current_a.to_string(),
            telemetry.state_of_charge_percent.to_string(),
            telemetry.temperature_c.to_string(),
            cell_voltages.get(0).map_or("".to_string(), |c| c.voltage_mv.to_string()),
            cell_voltages.get(1).map_or("".to_string(), |c| c.voltage_mv.to_string()),
            cell_voltages.get(2).map_or("".to_string(), |c| c.voltage_mv.to_string()),
            cell_voltages.get(3).map_or("".to_string(), |c| c.voltage_mv.to_string()),
        ])?;
    }
}
```

#### Phase 2: Tauri Command for Logging
```rust
#[tauri::command]
async fn start_logging(path: String, interval_ms: u64, state: State<'_, BmsManager>) -> Result<(), String> {
    let mut logger = BmsLogger::new(Path::new(&path), interval_ms)
        .map_err(|e| e.to_string())?;

    // Spawn background task to log continuously
    tokio::spawn(async move {
        loop {
            if let Ok(telemetry) = read_telemetry_internal().await {
                if let Ok(cells) = read_cell_voltages_internal(4).await {
                    logger.log_sample(&telemetry, &cells).await;
                }
            }
            tokio::time::sleep(Duration::from_millis(interval_ms)).await;
        }
    });

    Ok(())
}

#[tauri::command]
fn stop_logging() {
    // Stop logging task
}
```

#### Phase 3: UI Controls
```svelte
<script>
let logging = false;
let logPath = '';
let logInterval = 1000; // ms

async function startLogging() {
  const filePath = await save({
    defaultPath: `bms_log_${Date.now()}.csv`,
    filters: [{ name: 'CSV', extensions: ['csv'] }]
  });

  if (filePath) {
    await invoke('start_logging', { path: filePath, intervalMs: logInterval });
    logging = true;
    logPath = filePath;
  }
}

async function stopLogging() {
  await invoke('stop_logging');
  logging = false;
}
</script>

<div class="logging-controls">
  <label>Sample Interval (ms): <input type="number" bind:value={logInterval} disabled={logging} /></label>

  {#if !logging}
    <button on:click={startLogging}>Start Logging</button>
  {:else}
    <button on:click={stopLogging}>Stop Logging</button>
    <span>Logging to: {logPath}</span>
  {/if}
</div>
```

### STAR Status for Logging
```
❌ CSV Logging (not implemented)
❌ Real-time Export (no background task)
❌ Configurable Parameters (all or nothing)
❌ File Rotation (no size limits)
❌ Excel/JSON/XML Export (CSV only planned)
```

---

## Configuration Management

### BQ Studio Features

#### Configuration Profiles
- **Save Configuration**: Export complete BMS settings to file
- **Load Configuration**: Import settings from file
- **Profile Library**: Store multiple configurations
- **Profile Comparison**: Compare two configurations side-by-side
- **Template Profiles**: Pre-configured templates for common use cases
  - Li-ion 4S laptop battery
  - Li-ion 6S power tool
  - Li-ion 12S e-bike
  - LiFePO4 8S storage
  - Custom templates

#### Configuration Includes
- All protection thresholds
- Cell balancing settings
- Gas gauging parameters
- Data flash configuration
- Register values
- Calibration data (optional)

#### File Format
Typically XML or JSON:
```xml
<BQConfiguration>
  <DeviceType>BQ76920</DeviceType>
  <Version>1.0</Version>
  <Protection>
    <Overvoltage>
      <Threshold_mV>4200</Threshold_mV>
      <Delay_ms>2000</Delay_ms>
    </Overvoltage>
    <Undervoltage>
      <Threshold_mV>3000</Threshold_mV>
      <Delay_ms>2000</Delay_ms>
    </Undervoltage>
  </Protection>
  <CellBalancing>
    <AutoEnable>true</AutoEnable>
    <Threshold_mV>50</Threshold_mV>
  </CellBalancing>
  <!-- ... more settings -->
</BQConfiguration>
```

### STAR Implementation

```rust
// src/config.rs
#[derive(Serialize, Deserialize)]
pub struct BmsConfiguration {
    pub device_type: String,
    pub version: String,
    pub protection: ProtectionConfig,
    pub cell_balancing: CellBalancingConfig,
    pub gas_gauging: Option<GasGaugingConfig>,
}

#[derive(Serialize, Deserialize)]
pub struct ProtectionConfig {
    pub overvoltage_mv: u16,
    pub overvoltage_delay_ms: u16,
    pub undervoltage_mv: u16,
    pub undervoltage_delay_ms: u16,
    // ... more fields
}

impl BmsConfiguration {
    pub fn from_device(bms: &BmsConnection) -> Result<Self> {
        // Read all relevant registers
        let ov_trip = bms.read_register(0x09)?;
        let uv_trip = bms.read_register(0x0A)?;

        // Convert register values to human-readable config
        Ok(Self {
            device_type: "BQ76920".to_string(),
            version: "1.0".to_string(),
            protection: ProtectionConfig {
                overvoltage_mv: (ov_trip as u16) * 50 + 1500,
                undervoltage_mv: (uv_trip as u16) * 50 + 1500,
                // ... more fields
            },
            // ... more fields
        })
    }

    pub fn apply_to_device(&self, bms: &mut BmsConnection) -> Result<()> {
        // Convert config back to register values and write
        let ov_trip = ((self.protection.overvoltage_mv - 1500) / 50) as u8;
        bms.write_register(0x09, ov_trip as u16)?;

        let uv_trip = ((self.protection.undervoltage_mv - 1500) / 50) as u8;
        bms.write_register(0x0A, uv_trip as u16)?;

        // ... write other registers
        Ok(())
    }

    pub fn save_to_file(&self, path: &Path) -> Result<()> {
        let json = serde_json::to_string_pretty(self)?;
        std::fs::write(path, json)?;
        Ok(())
    }

    pub fn load_from_file(path: &Path) -> Result<Self> {
        let json = std::fs::read_to_string(path)?;
        let config: Self = serde_json::from_str(&json)?;
        Ok(config)
    }
}
```

### STAR Status for Configuration Management
```
❌ Save Configuration (not implemented)
❌ Load Configuration (not implemented)
❌ Profile Library (no UI)
❌ Profile Comparison (not available)
❌ Template Profiles (none provided)
```

---

## STAR BMS Tool Feature Checklist

### ✅ = Implemented | ⚠️ = Partial | ❌ = Not Implemented | 🔄 = In Progress

### Connection & Communication
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Serial port selection | ✅ | Manual selection working | - |
| Auto-detect ports | ⚠️ | Can list ports, no auto-select | Medium |
| Connection status indicator | ✅ | UI shows connected/disconnected | - |
| Auto-reconnect on failure | ❌ | No retry logic | Medium |
| Multiple device support | ❌ | Single device only | Low |
| Custom baud rate | ⚠️ | Hardcoded 115200 | Low |
| Protocol Buffer encoding | ✅ | nanopb implementation | - |
| Frame sync + CRC-32 | ✅ | Full implementation | - |

### Telemetry Monitoring
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Read voltage | ✅ | Working | - |
| Read current | ✅ | Working | - |
| Read SOC | ✅ | Working | - |
| Read temperature | ✅ | Working | - |
| Read capacity | ✅ | Working | - |
| Read cycle count | ✅ | Working | - |
| Auto-refresh | ✅ | 1Hz fixed | - |
| Configurable refresh rate | ❌ | Hardcoded 1Hz | High |
| Average power calculation | ❌ | Not displayed | Medium |
| Time to empty | ❌ | Not calculated | Medium |
| Time to full | ❌ | Not calculated | Medium |
| Real-time graphing | ⚠️ | 4 SVG charts exist | - |
| Export graph data | ❌ | No export | Medium |

### Cell Voltage Monitoring
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Read cell voltages (1-16 cells) | ✅ | Working | - |
| Visual bars display | ✅ | SVG bars | - |
| Numeric values (mV) | ✅ | Displayed | - |
| Color coding (normal/warning/critical) | ❌ | All same color | High |
| Cell balancing status | ❌ | Not shown | High |
| Historical min/max | ❌ | Not tracked | Low |
| Export cell data | ❌ | No export | Medium |
| Max cell voltage | ❌ | Not calculated | Medium |
| Min cell voltage | ❌ | Not calculated | Medium |
| Delta cell voltage | ❌ | Not shown | Medium |

### Register Access
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Read single register | ✅ | Working | - |
| Write single register | ✅ | Working | - |
| Hex/decimal input | ✅ | Both supported | - |
| Register map table | ❌ | No table view | High |
| Register descriptions | ❌ | No tooltips | High |
| Bulk read | ❌ | One at a time | Medium |
| Bulk write | ❌ | One at a time | Medium |
| Watch list | ❌ | No monitoring | Low |
| Register compare | ❌ | No comparison | Low |
| Bit field editor | ❌ | Treat as single value | Medium |

### Manufacturer Access Commands
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Custom command execution | ✅ | ReadManufacturerAccess | - |
| Block read support | ✅ | Implemented | - |
| Command library | ❌ | No pre-defined commands | High |
| Command history | ❌ | Not logged | Medium |
| Common commands UI | ❌ | Manual entry only | High |
| DeviceType command | ❌ | No UI shortcut | Medium |
| FirmwareVersion command | ❌ | No UI shortcut | Medium |
| Reset command | ❌ | No UI shortcut | High |
| SEAL/UNSEAL commands | ❌ | No UI shortcut | Medium |

### Data Flash Programming
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Data flash explorer | ❌ | Not implemented | High |
| Read data flash block | ❌ | No UI | High |
| Write data flash block | ❌ | No UI | High |
| Golden image save | ❌ | No save | Medium |
| Golden image load | ❌ | No load | Medium |
| Configuration wizard | ⚠️ | Experimental only | Low |
| Gas gauging parameters | ❌ | No access | Medium |
| Protection parameters | ❌ | No access | High |
| Cell balancing config | ❌ | No access | High |
| Manufacturer info | ❌ | No access | Low |

### Protection Features
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| OV threshold config | ❌ | No UI | High |
| UV threshold config | ❌ | No UI | High |
| OCC threshold config | ❌ | No UI | High |
| OCD threshold config | ❌ | No UI | High |
| SCD threshold config | ❌ | No UI | High |
| Temperature limits | ❌ | No UI | High |
| FET control | ❌ | No manual control | High |
| Protection status display | ❌ | Flags not shown | High |
| Event log | ❌ | Not tracked | Medium |
| Clear faults button | ❌ | Not implemented | High |

### Cell Balancing
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Auto balancing enable/disable | ❌ | No control | High |
| Manual cell selection | ❌ | No UI | Medium |
| Balancing threshold config | ❌ | Not configurable | High |
| Balancing current display | ❌ | Not shown | Medium |
| Balancing status LEDs | ❌ | Not shown | High |
| Balancing duration tracking | ❌ | Not tracked | Low |
| Safety interlocks | ❌ | Not enforced | Critical |
| Temperature monitoring | ❌ | Not integrated | Critical |

### Automation & Scripting
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| .bqseq file parser | ❌ | Not implemented | Medium |
| Sequence library | ❌ | No UI | Medium |
| Auto-discovery of sequences | ❌ | No scanning | Low |
| One-click execution | ❌ | No runner | Medium |
| Progress indicator | ❌ | No feedback | Low |
| Result display | ❌ | No output | Medium |
| Error handling | ❌ | No recovery | Medium |
| Loop support | ❌ | No repeats | Low |
| Conditional execution | ❌ | No conditionals | Low |

### Data Logging & Export
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| CSV logging | ❌ | Not implemented | High |
| Real-time export | ❌ | No background task | High |
| Configurable parameters | ❌ | All or nothing | Medium |
| Timestamp | ❌ | Not added | High |
| Sample rate config | ❌ | No UI | High |
| Auto-filename | ❌ | Manual selection | Low |
| File rotation | ❌ | No size limits | Low |
| Excel export | ❌ | CSV only planned | Low |
| JSON export | ❌ | Not supported | Low |

### Configuration Management
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Save configuration to file | ❌ | Not implemented | High |
| Load configuration from file | ❌ | Not implemented | High |
| Profile library | ❌ | No UI | Medium |
| Profile comparison | ❌ | Not available | Low |
| Template profiles | ❌ | None provided | Medium |
| Li-ion templates | ❌ | Not included | Medium |
| LiFePO4 templates | ❌ | Not included | Medium |

### LabVIEW Integration
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Serial wrapper VIs | ❌ | Not created | Medium |
| CLI interface | ✅ | Can be called | - |
| Shared library export | ❌ | Not compiled as .dll | Low |
| Example project | ❌ | Not provided | Low |
| Documentation | ❌ | No LabVIEW guide | Medium |

### TestStand Integration
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Custom step types | ❌ | Not created | Low |
| COM interface | ❌ | Not implemented | Low |
| REST API mode | ❌ | Not implemented | Medium |
| Example sequences | ❌ | Not provided | Low |

### User Interface
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Dark theme | ✅ | Implemented | - |
| Light theme toggle | ❌ | Dark only | Low |
| Responsive layout | ✅ | Fixed 1280x800 | - |
| Tab navigation | ✅ | 4-5 tabs | - |
| Keyboard shortcuts | ❌ | Mouse only | Low |
| Accessibility (ARIA) | ❌ | Not implemented | Low |
| Multi-language | ❌ | English only | Low |
| Tooltips/help | ❌ | No inline help | Medium |

### Testing & Quality
| Feature | Status | Notes | Priority |
|---------|--------|-------|----------|
| Unit tests | ✅ | 23 tests | - |
| Integration tests | ✅ | 4 tests | - |
| UI automation tests | ✅ | 171+ Playwright tests | - |
| Mock device | ✅ | PTY-based simulator | - |
| Code coverage | ⚠️ | ~65% passing | - |
| CI/CD pipeline | ❌ | No automation | Medium |
| Cross-platform testing | ❌ | macOS only | Medium |

---

## Implementation Priorities

### Phase 1: Critical Features (Complete BQ Studio Parity)
**Timeline: 2-4 weeks**

#### 1.1 Protection Configuration (Week 1)
- ✅ Create ProtectionSettings.svelte component
- ✅ Implement threshold configuration UI
- ✅ Add FET manual control
- ✅ Display protection flags in real-time
- ✅ Implement clear faults command

#### 1.2 Cell Balancing Control (Week 1)
- ✅ Create CellBalancing.svelte component
- ✅ Add auto/manual balancing toggle
- ✅ Show balancing status per cell
- ✅ Implement safety interlocks
- ✅ Add temperature monitoring integration

#### 1.3 Register Map Table (Week 2)
- ✅ Create register definitions (bms_register_map.ts)
- ✅ Build RegisterMap.svelte component with table view
- ✅ Add register descriptions/tooltips
- ✅ Implement bulk read/write
- ✅ Add bit field editor

#### 1.4 Manufacturer Command Library (Week 2)
- ✅ Create manufacturer_commands.ts with full command list
- ✅ Build UI dropdown for command selection
- ✅ Add command history logging
- ✅ Implement common command shortcuts

#### 1.5 Color-Coded Cell Voltage Display (Week 3)
- ✅ Implement getCellColor() function
- ✅ Add green/yellow/red color coding
- ✅ Show balancing indicator per cell
- ✅ Display max/min/delta voltages

#### 1.6 Calculated Telemetry Fields (Week 3)
- ✅ Add average power (V × I)
- ✅ Calculate time to empty
- ✅ Calculate time to full
- ✅ Show max/min/delta cell voltages

#### 1.7 Data Logging (Week 4)
- ✅ Implement CSV logger backend (logger.rs)
- ✅ Create logging UI controls
- ✅ Add configurable sample rate
- ✅ Implement file rotation (optional)

### Phase 2: Advanced Features (Beyond BQ Studio)
**Timeline: 4-6 weeks**

#### 2.1 Data Flash Programming (Weeks 5-6)
- ✅ Implement ManufacturerBlockAccess (0x0044) in RX72N firmware
- ✅ Create Protocol Buffer messages for data flash
- ✅ Build data flash explorer UI (tree view)
- ✅ Implement read/write with checksum validation
- ✅ Add golden image save/load

#### 2.2 Sequence Automation (Week 7)
- ✅ Create .bqseq parser (sequence.rs)
- ✅ Implement sequence executor
- ✅ Build sequence runner UI
- ✅ Add auto-discovery of sequence files
- ✅ Implement progress indicator

#### 2.3 Configuration Management (Week 8)
- ✅ Create BmsConfiguration struct
- ✅ Implement from_device() and apply_to_device()
- ✅ Add save/load to JSON
- ✅ Build configuration UI
- ✅ Create template profiles

#### 2.4 Enhanced Graphing (Weeks 9-10)
- ✅ Replace SVG charts with Chart.js
- ✅ Add historical data storage
- ✅ Implement zoom/pan controls
- ✅ Add export graph as PNG/CSV
- ✅ Multiple chart types (line, area, scatter)

### Phase 3: Integration & Polish
**Timeline: 2-3 weeks**

#### 3.1 LabVIEW Integration (Week 11)
- ✅ Create STAR_BMS_*.vi wrappers
- ✅ Test with existing LabVIEW VIs from ESET453 labs
- ✅ Write integration guide
- ✅ Provide example LabVIEW project

#### 3.2 Production Test Features (Week 12)
- ✅ Add batch testing mode (test multiple devices)
- ✅ Implement test report generation
- ✅ Add pass/fail binning
- ✅ Database integration for traceability

#### 3.3 Documentation & Release (Week 13)
- ✅ Complete user manual
- ✅ Record video tutorials
- ✅ Build installers for macOS/Windows/Linux
- ✅ Create GitHub releases with binaries

---

## Testing Requirements

### Functional Testing Checklist

#### Connection Tests
- [ ] Connect to RX72N device on macOS
- [ ] Connect to RX72N device on Windows
- [ ] Connect to RX72N device on Linux
- [ ] Auto-reconnect after cable disconnect
- [ ] Handle device not present gracefully
- [ ] Multiple connect/disconnect cycles

#### Telemetry Tests
- [ ] Read all telemetry fields correctly
- [ ] Auto-refresh at configurable rates (100ms, 500ms, 1s, 5s)
- [ ] Graphs update in real-time
- [ ] No memory leaks during extended operation (8+ hours)
- [ ] Accurate calculations (power, time to empty/full)

#### Register Tests
- [ ] Read all registers (0x00-0xFF)
- [ ] Write all writable registers
- [ ] Hex and decimal input both work
- [ ] Register map table displays correctly
- [ ] Tooltips show register descriptions
- [ ] Bulk read reads all registers
- [ ] Bulk write writes multiple registers from file
- [ ] Bit field editor correctly manipulates individual bits

#### Protection Tests
- [ ] Configure OV threshold and verify write
- [ ] Configure UV threshold and verify write
- [ ] Configure OCC/OCD/SCD thresholds
- [ ] Manual FET control (CHG/DSG on/off)
- [ ] Protection flags display in real-time
- [ ] Clear faults command works
- [ ] Event log records all protection events

#### Cell Balancing Tests
- [ ] Enable/disable auto balancing
- [ ] Manual cell selection works
- [ ] Balancing status shows correct cells
- [ ] Safety interlock stops balancing during charge/discharge
- [ ] Temperature monitoring prevents overheating

#### Data Flash Tests
- [ ] Read data flash blocks
- [ ] Write data flash blocks with checksum
- [ ] Save golden image to file
- [ ] Load golden image from file
- [ ] Data flash explorer displays all classes/subclasses

#### Sequence Tests
- [ ] Parse .bqseq files correctly
- [ ] Execute ReadByte commands
- [ ] Execute WriteByte commands
- [ ] Execute Delay commands
- [ ] Handle errors gracefully
- [ ] Display execution results

#### Logging Tests
- [ ] CSV logging starts/stops correctly
- [ ] Sample rate is accurate
- [ ] All selected parameters logged
- [ ] Timestamps are correct
- [ ] File rotation works at size limit
- [ ] No data loss during long sessions

#### Configuration Tests
- [ ] Save configuration to JSON
- [ ] Load configuration from JSON
- [ ] Apply configuration to device
- [ ] Template profiles load correctly
- [ ] Configuration comparison shows differences

### Performance Testing

#### Latency
- [ ] Register read < 50ms
- [ ] Register write < 50ms
- [ ] Telemetry read < 100ms
- [ ] Cell voltages read < 100ms
- [ ] Manufacturer command < 200ms

#### Throughput
- [ ] 10 Hz telemetry refresh stable
- [ ] Bulk read 256 registers < 5s
- [ ] CSV logging 1 Hz for 24 hours

#### Reliability
- [ ] Zero crashes in 100 connect/disconnect cycles
- [ ] Zero CRC errors in 10,000 frames
- [ ] Zero memory leaks in 8-hour session

---

## Missing from STAR BMS Tool (Priority Ordered)

### Critical (Must Have for Production)
1. **Protection Configuration UI** - Users cannot configure OV/UV/OC thresholds
2. **FET Manual Control** - Cannot control CHG/DSG FETs
3. **Protection Flags Display** - No visibility into protection status
4. **Cell Balancing Control** - Cannot enable/disable balancing
5. **Safety Interlocks** - No enforcement of balancing safety rules
6. **Clear Faults Command** - Cannot recover from protection events

### High Priority (Needed for Full Parity)
7. **Register Map Table** - No convenient register browsing
8. **Manufacturer Command Library** - Tedious to look up command codes
9. **Color-Coded Cell Voltages** - Hard to spot imbalanced cells
10. **Calculated Telemetry** - Missing power, time to empty/full
11. **CSV Data Logging** - No way to record test data
12. **Data Flash Access** - Cannot configure gas gauging or advanced features

### Medium Priority (Nice to Have)
13. **Configurable Refresh Rate** - Stuck at 1Hz
14. **Auto-Reconnect** - Requires manual reconnect on failure
15. **Sequence Automation** - Cannot run .bqseq files
16. **Configuration Save/Load** - No way to backup settings
17. **Register Descriptions** - No inline help
18. **Command History** - Hard to remember what was done

### Low Priority (Future Enhancements)
19. **Multiple Device Support** - One device at a time
20. **LabVIEW VIs** - Integration not packaged
21. **Bit Field Editor** - Register bits not easily accessible
22. **Profile Templates** - No pre-configured settings
23. **Historical Min/Max** - Cell voltage trends not tracked

---

## Conclusion

Your **STAR BMS Tool** has achieved ~40% feature parity with TI BQ Studio in core functionality (connection, telemetry, registers, cell voltages). The remaining ~60% consists of:
- **Protection configuration and monitoring** (critical for safety)
- **Data flash programming** (critical for configuration)
- **Cell balancing control** (critical for battery longevity)
- **Automation and scripting** (productivity)
- **Data logging and export** (testing and validation)

Your **RX72N firmware** successfully replaces the EV2300/EV2400 hardware with a superior, open-source, cross-platform solution. The Protocol Buffer-based architecture is excellent and extensible.

**Recommended Next Steps:**
1. Implement Phase 1 features (Weeks 1-4) for critical protection and balancing
2. Add data flash support (Weeks 5-6) for complete configuration access
3. Implement logging and sequences (Weeks 7-8) for test automation
4. Polish UI and add LabVIEW integration (Weeks 9-13)

With these additions, STAR BMS Tool will be a **production-ready, superior replacement** for BQ Studio with modern architecture, cross-platform support, and open-source accessibility.

---

**Document Version**: 1.0
**Last Updated**: 2026-01-11
**Maintainer**: STAR Project Team
**Related Projects**:
- star-bms-tool (Tauri + Rust + Svelte)
- star-rx72n-firmware (RX72N + ThreadX + nanopb)
- star-proto (Protocol Buffer definitions)
