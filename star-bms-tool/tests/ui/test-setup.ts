// Shared test setup for Playwright tests
// Injects mock Tauri API for browser testing
//
// IMPORTANT: Tauri v2 uses window.__TAURI_INTERNALS__.invoke, not window.__TAURI__.invoke
// See: https://github.com/tauri-apps/tauri/blob/v2/packages/api/src/mocks.ts

import { Page } from '@playwright/test';

const DEFAULT_DOCKING_LAYOUT = {
  zones: {
    top: { visible: false, size: 200, panels: [], splits: [] },
    bottom: { visible: true, size: 300, panels: ['packet-viewer'], splits: [] },
    left: { visible: false, size: 250, panels: [], splits: [] },
    right: { visible: false, size: 250, panels: [], splits: [] },
  },
  panels: {
    'packet-viewer': {
      id: 'packet-viewer',
      title: 'Packet Viewer',
      icon: null,
      zone: 'bottom',
      lastDockZone: 'bottom',
      visible: true,
      floating: false,
      floatingWindow: null,
    },
    terminal: {
      id: 'terminal',
      title: 'Terminal',
      icon: null,
      zone: null,
      lastDockZone: 'bottom',
      visible: false,
      floating: false,
      floatingWindow: null,
    },
    'device-info': {
      id: 'device-info',
      title: 'Device Info',
      icon: null,
      zone: null,
      lastDockZone: 'right',
      visible: false,
      floating: false,
      floatingWindow: null,
    },
    properties: {
      id: 'properties',
      title: 'Properties',
      icon: null,
      zone: null,
      lastDockZone: 'right',
      visible: false,
      floating: false,
      floatingWindow: null,
    },
  },
  dragState: {
    isDragging: false,
    panelId: null,
    sourceZone: null,
    didDrop: false,
  },
};

export async function setupMockTauri(page: Page) {
  await page.addInitScript((layout) => {
    try {
      localStorage.clear();
      localStorage.setItem('bms-tool-docking-layout', JSON.stringify(layout));
    } catch {
      // Ignore if storage is unavailable in this context
    }

    // Initialize __TAURI_INTERNALS__ as expected by @tauri-apps/api v2
    (window as any).__TAURI_INTERNALS__ = (window as any).__TAURI_INTERNALS__ || {};

    // Mock the invoke function that @tauri-apps/api/core uses
    (window as any).__TAURI_INTERNALS__.invoke = async function (
      cmd: string,
      args: any = {},
      _options?: any
    ) {
      switch (cmd) {
        case 'list_serial_ports':
          return ['/tmp/bms_client'];

        case 'connect_to_device':
          // Auto-discovery returns device info
          (window as any).__mockConnected = true;
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
          (window as any).__mockConnected = false;
          return null;

        case 'is_connected':
          return (window as any).__mockConnected || false;

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
          // Simulate backend validation logic: clamp to device maximum (4 cells)
          const DEVICE_CELLS = 4;
          const PROTOCOL_MAX = 16;
          let requested = args.numCells || DEVICE_CELLS;

          // Clamp to device capabilities first
          const clamped = requested > DEVICE_CELLS ? DEVICE_CELLS : requested;

          // Check protocol maximum (would throw error in real backend)
          if (clamped > PROTOCOL_MAX) {
            throw new Error(`Device has ${clamped} cells which exceeds protocol maximum of ${PROTOCOL_MAX}`);
          }

          const cells: number[] = [];
          for (let i = 0; i < clamped; i++) {
            cells.push(3700 + i * 10);
          }
          return {
            cell_mv: cells,
            pack_mv: cells.reduce((a: number, b: number) => a + b, 0),
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

        case 'read_block':
          // Mock Data Flash read - return pattern data
          const maxLength = args.maxLength || 32;
          const blockData = new Array(maxLength).fill(0).map((_, i) => (args.address + i) & 0xFF);
          return {
            address: args.address,
            data: blockData,
          };

        case 'write_block':
          return null;

        case 'is_experimental_enabled':
          // Return false by default in tests (experimental features disabled)
          return false;

        case 'read_protection_status':
          return {
            cell_overvoltage: false,
            pack_overvoltage: false,
            cell_undervoltage: false,
            pack_undervoltage: false,
            charge_overcurrent: false,
            discharge_overcurrent: false,
            short_circuit: false,
            overtemperature_charge: false,
            overtemperature_discharge: false,
            undertemperature_charge: false,
            undertemperature_discharge: false,
            cell_balancing_active: true,
            permanent_failure: false,
            safety_status_alert: false,
          };

        case 'get_raw_packets':
          // Mock raw packet capture - return sample packets
          const rawPacketStore = (window as any).__mockRawPackets || [];
          const rawLimit = args.limit || 1000;
          const rawOffset = args.offset || 0;
          return rawPacketStore.slice(rawOffset, rawOffset + rawLimit);

        case 'get_parsed_packets':
          // Mock parsed packet capture - return sample parsed packets
          const parsedPacketStore = (window as any).__mockParsedPackets || [];
          const parsedLimit = args.limit || 1000;
          const parsedOffset = args.offset || 0;
          return parsedPacketStore.slice(parsedOffset, parsedOffset + parsedLimit);

        case 'get_packet_count':
          const rawCount = ((window as any).__mockRawPackets || []).length;
          const parsedCount = ((window as any).__mockParsedPackets || []).length;
          return [rawCount, parsedCount];

        case 'clear_packet_capture':
          // Clear mock packet stores
          (window as any).__mockRawPackets = [];
          (window as any).__mockParsedPackets = [];
          return null;

        case 'set_packet_capture_enabled':
          // Toggle packet capture state
          (window as any).__mockCaptureEnabled = args.enabled;
          return null;

        case 'create_floating_panel':
          // Simulate a floating panel window label
          return `floating-${args.panelId || 'panel'}`;

        case 'close_floating_panel':
          return null;

        case 'abort_connection':
          // Cancel ongoing connection attempt
          (window as any).__mockConnected = false;
          return null;

        case 'manufacturer_access':
          // Mock BQ4050/BQ78350 ManufacturerAccess commands
          const subcommand = args.subcommand || 0x0001;
          let responseData: number[] = [];

          switch (subcommand) {
            case 0x0001: // Device Type
              responseData = [0x50, 0x40]; // BQ4050 device ID
              break;
            case 0x0002: // Firmware Version
              responseData = [0x01, 0x02, 0x03]; // v1.2.3
              break;
            case 0x0003: // Hardware Version
              responseData = [0x00, 0x01]; // v0.1
              break;
            case 0x0021: // IT Status
              responseData = [0x01]; // IT enabled
              break;
            case 0x0023: // FET Status
              responseData = [0x03]; // Both CHG and DSG FETs enabled
              break;
            case 0x0024: // CHG FET Control (write)
              responseData = [0x00]; // ACK
              break;
            case 0x0025: // DSG FET Control (write)
              responseData = [0x00]; // ACK
              break;
            case 0x0026: // Cell Balancing Control (write)
              responseData = [0x00]; // ACK
              break;
            case 0x0070: // Safety Status
              responseData = [0x00, 0x00]; // No safety alerts
              break;
            case 0x0071: // PF Status
              responseData = [0x00, 0x00]; // No permanent failures
              break;
            default:
              responseData = [0x00, 0x00]; // Unknown sub-command
          }

          return {
            subcommand: subcommand,
            data: responseData,
          };

        default:
          throw new Error(`Unknown mock command: ${cmd}`);
      }
    };

    // Also mock transformCallback which is needed for Channel class
    const callbacks = new Map<number, Function>();
    (window as any).__TAURI_INTERNALS__.transformCallback = function (
      callback: Function,
      once: boolean = false
    ): number {
      const identifier = window.crypto.getRandomValues(new Uint32Array(1))[0];
      callbacks.set(identifier, (data: any) => {
        if (once) {
          callbacks.delete(identifier);
        }
        return callback && callback(data);
      });
      return identifier;
    };

    (window as any).__TAURI_INTERNALS__.unregisterCallback = function (id: number) {
      callbacks.delete(id);
    };

    (window as any).__TAURI_INTERNALS__.callbacks = callbacks;
  }, DEFAULT_DOCKING_LAYOUT);
}
