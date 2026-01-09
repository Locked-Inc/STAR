// Test helpers for Playwright browser testing
// This file should ONLY be imported by Playwright tests, NEVER by production code

export function createMockInvoke() {
  return async function mockInvoke(cmd, args = {}) {
    console.warn(`[TEST MOCK] invoke: ${cmd}`, args);

    switch (cmd) {
      case 'list_serial_ports':
        return ['/tmp/bms_client', '/dev/ttyUSB0', '/dev/ttyUSB1'];

      case 'connect_to_device':
        // Auto-discovery returns device info
        return {
          manufacturer: 'Texas Instruments',
          device_name: 'BQ78350-R1A',
          chemistry: 'LION',
          serial_number: 0x12345678,
          firmware_version: 'v1.2.3',
          hardware_version: 'v0.1',
          design_capacity_mah: 3200,
          design_voltage_mv: 14800,
          num_cells: 4,
        };

      case 'disconnect_from_device':
        return null;

      case 'is_connected':
        return false;

      case 'get_device_state':
        return {
          manufacturer: 'Texas Instruments',
          device_name: 'BQ78350-R1A',
          chemistry: 'LION',
          serial_number: 0x12345678,
          firmware_version: 'v1.2.3',
          hardware_version: 'v0.1',
          design_capacity_mah: 3200,
          design_voltage_mv: 14800,
          num_cells: 4,
        };

      case 'read_telemetry':
        return {
          voltage_mv: 14800,
          current_ma: -1500,
          average_current_ma: -1200,
          relative_soc_percent: 75,
          absolute_soc_percent: 73,
          temperature_celsius: 25,
          remaining_capacity_mah: 2250,
          full_capacity_mah: 3000,
          design_capacity_mah: 3200,
          cycle_count: 42,
          time_to_empty_min: 90,
          time_to_full_min: 0xffff,
          is_charging: false,
          is_fully_charged: false,
          is_fully_discharged: false,
          is_low_capacity: false,
        };

      case 'read_cell_voltages':
        const numCells = args.numCells || 4;
        const cells = [];
        for (let i = 0; i < numCells; i++) {
          cells.push(3700 + i * 10);
        }
        return {
          cell_mv: cells,
          pack_mv: cells.reduce((a, b) => a + b, 0),
          min_cell_mv: Math.min(...cells),
          max_cell_mv: Math.max(...cells),
          delta_mv: Math.max(...cells) - Math.min(...cells),
        };

      case 'read_device_info':
        return {
          manufacturer: 'Texas Instruments',
          device_name: 'BQ78350-R1A',
          chemistry: 'LION',
          serial_number: 0x12345678,
          firmware_version: 'v1.2.3',
          hardware_version: 'v0.1',
          design_capacity_mah: 3200,
          design_voltage_mv: 14800,
          num_cells: 4,
        };

      case 'read_register':
        return {
          address: args.address || 0,
          value: 0x0350,
          num_bytes: args.numBytes || 1,
        };

      case 'write_register':
        return null;

      default:
        throw new Error(`Unknown mock command: ${cmd}`);
    }
  };
}

// For Playwright: Inject mock invoke into window before loading the app
export function injectMockInvoke(page) {
  return page.addInitScript(() => {
    // Override Tauri detection
    window.__TAURI__ = {
      invoke: window.mockInvoke || (async () => {
        throw new Error('mockInvoke not initialized');
      }),
    };

    // Create mock invoke function
    window.mockInvoke = async function (cmd, args = {}) {
      console.warn(`[TEST MOCK] invoke: ${cmd}`, args);

      switch (cmd) {
        case 'list_serial_ports':
          return ['/tmp/bms_client'];

        case 'connect_to_device':
          // Auto-discovery returns device info
          return {
            manufacturer: 'Texas Instruments',
            device_name: 'BQ78350-R1A',
            chemistry: 'LION',
            serial_number: 0x12345678,
            firmware_version: 'v1.2.3',
            hardware_version: 'v0.1',
            design_capacity_mah: 3200,
            design_voltage_mv: 14800,
            num_cells: 4,
          };

        case 'disconnect_from_device':
          return null;

        case 'is_connected':
          return window.__mockConnected || false;

        case 'get_device_state':
          return {
            manufacturer: 'Texas Instruments',
            device_name: 'BQ78350-R1A',
            chemistry: 'LION',
            serial_number: 0x12345678,
            firmware_version: 'v1.2.3',
            hardware_version: 'v0.1',
            design_capacity_mah: 3200,
            design_voltage_mv: 14800,
            num_cells: 4,
          };

        case 'read_telemetry':
          return {
            voltage_mv: 14800,
            current_ma: -1500,
            average_current_ma: -1200,
            relative_soc_percent: 75,
            absolute_soc_percent: 73,
            temperature_celsius: 25,
            remaining_capacity_mah: 2250,
            full_capacity_mah: 3000,
            design_capacity_mah: 3200,
            cycle_count: 42,
            time_to_empty_min: 90,
            time_to_full_min: 0xffff,
            is_charging: false,
            is_fully_charged: false,
            is_fully_discharged: false,
            is_low_capacity: false,
          };

        case 'read_cell_voltages':
          const numCells = args.numCells || 4;
          const cells = [];
          for (let i = 0; i < numCells; i++) {
            cells.push(3700 + i * 10);
          }
          return {
            cell_mv: cells,
            pack_mv: cells.reduce((a, b) => a + b, 0),
            min_cell_mv: Math.min(...cells),
            max_cell_mv: Math.max(...cells),
            delta_mv: Math.max(...cells) - Math.min(...cells),
          };

        case 'read_device_info':
          return {
            manufacturer: 'Texas Instruments',
            device_name: 'BQ78350-R1A',
            chemistry: 'LION',
            serial_number: 0x12345678,
            firmware_version: 'v1.2.3',
            hardware_version: 'v0.1',
            design_capacity_mah: 3200,
            design_voltage_mv: 14800,
            num_cells: 4,
          };

        case 'read_register':
          return {
            address: args.address || 0,
            value: 0x0350,
            num_bytes: args.numBytes || 1,
          };

        case 'write_register':
          return null;

        default:
          throw new Error(`Unknown mock command: ${cmd}`);
      }
    };

    // Make mockInvoke available to __TAURI__.invoke
    window.__TAURI__.invoke = window.mockInvoke;
  });
}
