# STAR BMS Tool - UI Automation Tests

Comprehensive Playwright-based UI automation tests for the STAR BMS Tool.

## Test Structure

```
tests/ui/
├── connection.spec.ts      - Connection/disconnection tests
├── telemetry.spec.ts       - Telemetry data reading tests
├── cell-voltages.spec.ts   - Cell voltage tests
├── device-info.spec.ts     - Device information tests
├── registers.spec.ts       - Register read/write tests
└── e2e-workflow.spec.ts    - End-to-end workflow tests
```

## Prerequisites

Before running tests, ensure:

1. **Mock Device Running**: Start the mock device on PTY
   ```bash
   # Terminal 1: Create PTY pair
   socat -d -d pty,raw,echo=0,link=/tmp/bms_mock pty,raw,echo=0,link=/tmp/bms_client

   # Terminal 2: Start mock device
   cargo run --release --bin mock_device /dev/ttys003
   # (Use the PTY path shown by socat for /tmp/bms_mock)
   ```

2. **Tauri Dev Server Running**: Start the application in dev mode
   ```bash
   cargo tauri dev
   ```
   Should be accessible at http://localhost:5173/

## Running Tests

```bash
# Run all tests
npm test

# Run tests with UI mode (interactive)
npm run test:ui

# Run tests in headed mode (see browser)
npm run test:headed

# Run tests in debug mode
npm run test:debug

# Run specific test suites
npm run test:connection
npm run test:telemetry
npm run test:cells
npm run test:info
npm run test:registers
npm run test:e2e

# View test report
npm run test:report
```

## Test Coverage

### Connection Tests (connection.spec.ts)
- ✓ Display application title
- ✓ List available serial ports
- ✓ Allow manual port entry
- ✓ Connect to mock device via PTY
- ✓ Disconnect from device
- ✓ Show error for invalid port
- ✓ Prevent connection when already connected

### Telemetry Tests (telemetry.spec.ts)
- ✓ Display telemetry tab
- ✓ Read and display telemetry data
  - Voltage (14.8V)
  - Current (-1.5A)
  - State of Charge (75%)
  - Temperature (25°C)
  - Remaining capacity (2.25 Ah)
  - Full capacity (3.0 Ah)
  - Cycle count (42)
  - Time to empty (90 min)
  - Charging status (No)
- ✓ Update data on multiple reads

### Cell Voltages Tests (cell-voltages.spec.ts)
- ✓ Display cell voltages tab
- ✓ Read and display 4 cell voltages (3.70V-3.73V)
- ✓ Display pack voltage (14.8V)
- ✓ Display min/max cell voltages
- ✓ Display delta voltage
- ✓ Allow changing number of cells (1-16)
- ✓ Handle minimum cells configuration

### Device Info Tests (device-info.spec.ts)
- ✓ Display device info tab
- ✓ Read and display device information:
  - Manufacturer (Texas Instruments)
  - Device name (BQ78350-R1A)
  - Chemistry (LION)
  - Serial number
  - Firmware version (v1.2.3)
  - Hardware version (v0.1)
  - Design capacity (3.2 Ah)
  - Design voltage (14.8 V)
  - Number of cells (4)
- ✓ Persist data after tab switch

### Registers Tests (registers.spec.ts)
- ✓ Display registers tab
- ✓ Read registers at various addresses (0x00, 0x02, etc.)
- ✓ Accept hexadecimal address format
- ✓ Write register successfully
- ✓ Validate num bytes range
- ✓ Display both decimal and hex values
- ✓ Handle multiple register reads/writes

### E2E Workflow Tests (e2e-workflow.spec.ts)
- ✓ Complete full BMS testing workflow:
  1. Connect to device
  2. Read telemetry
  3. Read cell voltages
  4. Read device info
  5. Read register
  6. Write register
  7. Verify data persistence
  8. Disconnect
- ✓ Handle rapid tab switching
- ✓ Maintain connection across tab switches

## Mock Device Data

The tests verify against these expected mock values:

| Field | Expected Value |
|-------|----------------|
| Voltage | 14.8V |
| Current | -1.5A |
| SOC | 75% |
| Temperature | 25°C |
| Remaining Capacity | 2.25 Ah |
| Full Capacity | 3.0 Ah |
| Cycle Count | 42 |
| Cell 1 Voltage | 3.70V |
| Cell 2 Voltage | 3.71V |
| Cell 3 Voltage | 3.72V |
| Cell 4 Voltage | 3.73V |
| Manufacturer | Texas Instruments |
| Device Name | BQ78350-R1A |
| Chemistry | LION |

## Troubleshooting

### Tests fail with "Failed to connect"
- Ensure socat PTY pair is running
- Ensure mock device is running on correct PTY path
- Verify `/tmp/bms_client` symlink exists

### Tests timeout
- Ensure Tauri dev server is running on port 5173
- Check `cargo tauri dev` output for errors
- Increase timeout in playwright.config.ts if needed

### Mock device not responding
- Check mock device logs for errors
- Restart mock device and PTY pair
- Verify PTY paths match in socat and mock_device commands

## CI/CD Integration

Tests are designed to run in CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Start mock device
  run: |
    socat pty,raw,echo=0,link=/tmp/bms_mock pty,raw,echo=0,link=/tmp/bms_client &
    cargo run --release --bin mock_device $(readlink /tmp/bms_mock) &

- name: Start Tauri dev
  run: cargo tauri dev &

- name: Run tests
  run: npm test
```

## Test Results

Test results are generated in:
- `test-results/html/` - HTML report (view with `npm run test:report`)
- `test-results/results.json` - JSON results for CI integration
- Screenshots/videos on failure in `test-results/`

## Writing New Tests

Follow the existing pattern:

```typescript
import { test, expect } from '@playwright/test';

const MOCK_PTY_PORT = '/tmp/bms_client';

test.describe('My New Tests', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
    // Connect if needed
  });

  test('should do something', async ({ page }) => {
    // Your test code
  });
});
```
