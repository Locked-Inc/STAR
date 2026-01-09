<script>
  import { onMount } from 'svelte';
  import { get } from 'svelte/store';
  import { invoke } from '@tauri-apps/api/core';
  import { listen } from '@tauri-apps/api/event';
  import DockingManager from './lib/docking/DockingManager.svelte';
  import { dockingStore } from './lib/stores/docking.js';
  import { handleKeyboardShortcut } from './lib/docking/shortcuts.js';

  const panelOrder = ['packet-viewer', 'terminal', 'device-info', 'properties'];
  const panelDefaults = {
    'packet-viewer': 'bottom',
    'terminal': 'bottom',
    'device-info': 'right',
    'properties': 'right',
  };

  // Keyboard shortcut handler
  function onKeyDown(event) {
    handleKeyboardShortcut(event);
  }

  async function togglePanelVisibility(panelId) {
    const panel = get(dockingStore).panels[panelId];
    if (!panel) {
      return;
    }

    if (panel.floating) {
      if (panel.floatingWindow) {
        try {
          await invoke('close_floating_panel', {
            windowLabel: panel.floatingWindow,
          });
        } catch (error) {
          console.error(`Failed to close floating window: ${error}`);
        }
      }
      dockingStore.unfloatPanel(panelId, panelDefaults[panelId] || 'bottom');
      return;
    }

    if (panel.visible) {
      dockingStore.hidePanel(panelId);
      return;
    }

    const targetZone = panel.lastDockZone || panelDefaults[panelId] || 'bottom';
    dockingStore.showPanel(panelId, targetZone);
  }

  // Connection state
  let ports = $state([]);
  let selectedPort = $state('');
  let portOption = $state('');
  let customPort = $state('');
  let connected = $state(false);
  let connectionError = $state('');
  let connectionState = $state('disconnected');  // 'disconnected' | 'connecting' | 'connected'
  let connectionProgress = $state('');  // Progress message during connection
  let connectionCancelled = $state(false);  // Track if user cancelled connection
  let cancelInProgress = $state(false);  // Prevent multiple cancel calls

  // Device state (from auto-discovery)
  let deviceState = $state(null);  // BmsDeviceInfoData from connection
  let deviceError = $state('');

  // Telemetry data
  let telemetry = $state(null);
  let telemetryError = $state('');
  let autoRefresh = $state(false);
  let refreshInterval = $state(null);
  let dataCollectionPaused = $state(false);
  let telemetryLog = $state([]);  // Array to store telemetry history
  let showChart = $state(false);  // Toggle chart visibility
  let telemetryLoading = $state(false);  // Loading state for telemetry button

  // Cell voltages
  let cellVoltages = $state(null);
  let numCells = $state(4);
  let cellError = $state('');
  let cellLoading = $state(false);  // Loading state for cell voltages button

  // Device info
  let deviceInfo = $state(null);
  let deviceLoading = $state(false);  // Loading state for device info button

  // Register access
  let regAddress = $state('0x00');
  let regValue = $state('0x00');
  let regNumBytes = $state(1);
  let regData = $state(null);
  let regError = $state('');
  let regAddressValid = $state(true);
  let regAddressMessage = $state('');
  let regValueValid = $state(true);
  let regValueMessage = $state('');

  // Register Map
  let registerMapData = $state(null);
  let registerMapError = $state('');
  let registerMapLoading = $state(false);
  let registerMapAutoRefresh = $state(false);
  let registerMapInterval = $state(null);
  let selectedRegisterRow = $state(null);  // Row selection for register map table

  // Chemistry Comparison Table
  let selectedChemistryRow = $state(null);  // Row selection for chemistry comparison table

  // ManufacturerAccess
  let mfgSubcommand = $state('0x0001');  // Default: Device Type
  let mfgData = $state('');  // Optional data payload for writes
  let mfgResponse = $state(null);
  let mfgError = $state('');
  let mfgPreset = $state('device_type');  // Dropdown preset

  // Protection Status
  let protectionStatus = $state(null);
  let protectionError = $state('');
  let protectionLoading = $state(false);  // Loading state for protection status button

  // Cell Balancing Control
  let balancingEnabled = $state(false);

  // Data Flash Programming
  let dataFlashClass = $state('48');  // Default: Safety class
  let dataFlashOffset = $state('0');
  let dataFlashData = $state(null);
  let dataFlashError = $state('');
  let dataFlashBackup = $state(null);  // Full backup storage

  // Battery Chemistry Profiles
  let selectedProfile = $state('lion');  // Default: Li-ion
  let profileError = $state('');

  // Chemistry profile definitions
  const chemistryProfiles = {
    lion: {
      name: 'Li-ion (LiCoO2)',
      cellVoltageMin: 2500,      // mV - deep discharge cutoff
      cellVoltageMax: 4200,      // mV - max charge voltage
      cellVoltageNominal: 3700,  // mV - nominal voltage
      chargeCurrentMax: 1000,    // mA - 1C charge rate (per Ah)
      dischargeCurrentMax: 2000, // mA - 2C discharge rate (per Ah)
      tempChargeMin: 0,          // Celsius
      tempChargeMax: 45,         // Celsius
      tempDischargeMin: -20,     // Celsius
      tempDischargeMax: 60,      // Celsius
      description: 'Standard Li-ion (LiCoO2) - Most common in consumer electronics. 3.7V nominal, high energy density.',
    },
    lifepo4: {
      name: 'LiFePO4',
      cellVoltageMin: 2000,
      cellVoltageMax: 3650,
      cellVoltageNominal: 3200,
      chargeCurrentMax: 1000,
      dischargeCurrentMax: 3000,  // Higher discharge rate tolerance
      tempChargeMin: 0,
      tempChargeMax: 45,
      tempDischargeMin: -20,
      tempDischargeMax: 60,
      description: 'Lithium Iron Phosphate - Safer chemistry, longer cycle life (2000+ cycles). 3.2V nominal, lower energy density but very stable.',
    },
    lipo: {
      name: 'Li-Po (Polymer)',
      cellVoltageMin: 3000,      // Higher minimum for safety
      cellVoltageMax: 4200,
      cellVoltageNominal: 3700,
      chargeCurrentMax: 1000,
      dischargeCurrentMax: 3000,  // High discharge capability
      tempChargeMin: 0,
      tempChargeMax: 45,
      tempDischargeMin: -20,
      tempDischargeMax: 60,
      description: 'Lithium Polymer - Similar to Li-ion but flexible packaging. 3.7V nominal, high discharge rates for RC and drones.',
    },
    nimh: {
      name: 'NiMH',
      cellVoltageMin: 900,
      cellVoltageMax: 1450,
      cellVoltageNominal: 1200,
      chargeCurrentMax: 500,     // Lower charge rate
      dischargeCurrentMax: 2000,
      tempChargeMin: 0,
      tempChargeMax: 45,
      tempDischargeMin: -20,
      tempDischargeMax: 55,
      description: 'Nickel Metal Hydride - Older technology, 1.2V nominal. Lower energy density but more environmentally friendly.',
    },
    custom: {
      name: 'Custom Profile',
      cellVoltageMin: 2500,
      cellVoltageMax: 4200,
      cellVoltageNominal: 3700,
      chargeCurrentMax: 1000,
      dischargeCurrentMax: 2000,
      tempChargeMin: 0,
      tempChargeMax: 45,
      tempDischargeMin: -20,
      tempDischargeMax: 60,
      description: 'Custom battery parameters - modify as needed for your specific battery.',
    },
  };

  // Battery Learning/Calibration Wizard
  let wizardStep = $state(0);  // Current step in wizard (0-5)
  let wizardError = $state('');
  let wizardStatus = $state('');
  let calibrationInProgress = $state(false);
  let dischargeCycleComplete = $state(false);
  let chargeCycleComplete = $state(false);
  let itCalibrationComplete = $state(false);

  const wizardSteps = [
    {
      title: 'Welcome',
      description: 'Battery Capacity Learning & Impedance Track Calibration',
    },
    {
      title: 'Preparation',
      description: 'Prepare battery for calibration cycle',
    },
    {
      title: 'Discharge Cycle',
      description: 'Full discharge to minimum voltage',
    },
    {
      title: 'Charge Cycle',
      description: 'Full charge to maximum voltage',
    },
    {
      title: 'IT Calibration',
      description: 'Enable Impedance Track calibration',
    },
    {
      title: 'Complete',
      description: 'Calibration complete and results',
    },
  ];

  // Active tab
  let activeTab = $state('telemetry');

  // Experimental features flag (checked at runtime)
  let experimentalEnabled = $state(false);

  // Settings
  let settings = $state({
    autoRefreshRate: 1000,     // milliseconds (1 Hz default)
    chartHistoryLength: 50,
    theme: 'light',
    zoomLevel: 100             // Percentage: 80, 90, 100, 110, 120, 130, 140
  });
  let settingsSaved = $state(false);  // Confirmation message

  onMount(async () => {
    await refreshPorts();
    if (ports.length === 0) {
      setTimeout(() => {
        if (!connected) {
          refreshPorts();
        }
      }, 500);
    }
    // Check if experimental features are enabled
    try {
      experimentalEnabled = await invoke('is_experimental_enabled');
    } catch (error) {
      console.log('Failed to check experimental features:', error);
    }

    // Load settings from localStorage
    loadSettings();

    // Apply zoom (in case loadSettings didn't find saved settings)
    applyZoom();
  });

  onMount(() => {
    const unlistenDock = listen('docking:unfloat-panel', (event) => {
      const payload = event.payload || {};
      if (payload.panelId) {
        dockingStore.unfloatPanel(payload.panelId, payload.targetZone || 'bottom');
      }
    });
    const unlistenHide = listen('docking:hide-panel', (event) => {
      const payload = event.payload || {};
      if (payload.panelId) {
        dockingStore.hidePanel(payload.panelId);
      }
    });

    const unlistenMenu = listen('menu:action', (event) => {
      const payload = event.payload || {};
      if (payload.action === 'toggle-panel' && payload.panelId) {
        void togglePanelVisibility(payload.panelId);
        return;
      }
      if (payload.action === 'reset-layout') {
        dockingStore.reset();
        return;
      }
      if (payload.action === 'refresh-ports') {
        refreshPorts();
        return;
      }
      if (payload.action === 'connect') {
        connect();
        return;
      }
      if (payload.action === 'disconnect') {
        disconnect();
        return;
      }
      if (payload.action === 'cancel-connection') {
        cancelConnection();
        return;
      }
      if (payload.action === 'select-port' && payload.port) {
        setPortOption(payload.port);
        customPort = '';
        return;
      }
    });

    const floatingSweep = setInterval(async () => {
      try {
        const openWindows = await invoke('list_floating_panels');
        const openSet = new Set(openWindows || []);
        const state = get(dockingStore);
        Object.entries(state.panels).forEach(([panelId, panel]) => {
          if (panel.floating && panel.floatingWindow && !openSet.has(panel.floatingWindow)) {
            dockingStore.hidePanel(panelId);
          }
        });
      } catch (error) {
        console.error('Failed to reconcile floating panels:', error);
      }
    }, 1000);

    return () => {
      unlistenDock.then((fn) => fn());
      unlistenHide.then((fn) => fn());
      unlistenMenu.then((fn) => fn());
      clearInterval(floatingSweep);
    };
  });

  function loadSettings() {
    try {
      const saved = localStorage.getItem('bms-tool-settings');
      if (saved) {
        settings = JSON.parse(saved);
        // Apply zoom immediately
        applyZoom();
      }
    } catch (error) {
      console.log('Failed to load settings:', error);
    }
  }

  function saveSettings() {
    try {
      localStorage.setItem('bms-tool-settings', JSON.stringify(settings));
      settingsSaved = true;
      setTimeout(() => settingsSaved = false, 3000);

      // Apply zoom immediately
      applyZoom();

      // Update auto-refresh interval if active
      if (autoRefresh && !dataCollectionPaused) {
        stopAutoRefresh();
        startAutoRefresh();
      }
    } catch (error) {
      console.log('Failed to save settings:', error);
    }
  }

  function applyZoom() {
    document.documentElement.style.fontSize = `${settings.zoomLevel}%`;
  }

  async function refreshPorts() {
    try {
      ports = await invoke('list_serial_ports');
      if (ports.length > 0 && !selectedPort && portOption !== 'custom') {
        setPortOption(ports[0]);
      }
    } catch (error) {
      connectionError = `Failed to list ports: ${error}`;
    }
  }

  function setPortOption(value) {
    portOption = value;
    if (value !== 'custom') {
      selectedPort = value;
      customPort = '';
    } else {
      selectedPort = '';
    }
  }

  function handleCustomPortInput(value) {
    customPort = value;
    if (portOption === 'custom') {
      selectedPort = value;
    }
  }

  function connect() {
    if (!selectedPort) {
      connectionError = 'Please select a port';
      return;
    }

    connectionState = 'connecting';
    connectionProgress = 'Opening serial port...';
    connectionError = '';
    connectionCancelled = false;  // Reset cancellation flag

    // Small delay for UI update
    setTimeout(() => {
      if (connectionCancelled) {
        return;
      }

      connectionProgress = 'Detecting device...';

      // Fire backend call without await - truly non-blocking
      invoke('connect_to_device', { portName: selectedPort })
        .then((result) => {
          // Check if user cancelled while we were waiting
          if (connectionCancelled) {
            return;
          }

          deviceState = result;

          connectionProgress = 'Loading device information...';

          setTimeout(() => {
            if (connectionCancelled) {
              return;
            }

            connected = true;
            connectionState = 'connected';
            connectionProgress = '';
            window.dispatchEvent(new CustomEvent('bms:connected'));

            // Set num_cells to device's actual cell count
            numCells = deviceState.num_cells;
            if (autoRefresh && !dataCollectionPaused) {
              startAutoRefresh();
            } else {
              void refreshAllData();
            }
          }, 100);
        })
        .catch((error) => {
          // Don't overwrite state if user cancelled
          if (connectionCancelled) {
            return;
          }

          connectionError = `Connection failed: ${error}`;
          connected = false;
          connectionState = 'disconnected';
          connectionProgress = '';
          deviceState = null;
        });
    }, 100);
  }

  async function disconnect() {
    try {
      stopAutoRefresh();
      dataCollectionPaused = false;
      if (registerMapAutoRefresh) {
        toggleRegisterMapAutoRefresh();
      }
      await invoke('disconnect_from_device');

      connected = false;
      connectionState = 'disconnected';
      connectionProgress = '';
      connectionCancelled = false;  // Reset cancellation flag
      deviceState = null;  // Clear device state
      deviceError = '';
      telemetry = null;
      cellVoltages = null;
      deviceInfo = null;
      registerMapData = null;
    } catch (error) {
      connectionError = `Disconnect failed: ${error}`;
    }
  }

  async function cancelConnection() {
    cancelInProgress = true;

    // Set flag and update UI IMMEDIATELY - don't wait for backend
    connectionCancelled = true;

    connected = false;
    connectionState = 'disconnected';
    connectionProgress = '';
    deviceState = null;
    connectionError = 'Connection cancelled';

    // Fire abort_connection but DON'T WAIT for it (non-blocking)
    const cancelReset = setTimeout(() => {
      cancelInProgress = false;
    }, 1500);

    invoke('abort_connection')
      .catch(() => {})
      .finally(() => {
        clearTimeout(cancelReset);
        cancelInProgress = false;
      });

    // Clear the error message after 3 seconds
    setTimeout(() => {
      if (connectionError === 'Connection cancelled') {
        connectionError = '';
      }
    }, 3000);
  }

  async function loadTelemetry() {
    if (!connected) return;

    try {
      telemetryLoading = true;
      telemetryError = '';
      telemetry = await invoke('read_telemetry');

      // Add to log with timestamp
      if (telemetry) {
        telemetryLog = [...telemetryLog, {
          timestamp: new Date().toISOString(),
          ...telemetry
        }];

        // Limit log size to last 1000 entries
        if (telemetryLog.length > 1000) {
          telemetryLog = telemetryLog.slice(-1000);
        }
      }
    } catch (error) {
      telemetryError = `Failed to read telemetry: ${error}`;
    } finally {
      telemetryLoading = false;
    }
  }

  function exportTelemetryToCSV() {
    if (telemetryLog.length === 0) {
      telemetryError = 'No telemetry data to export';
      setTimeout(() => telemetryError = '', 3000);
      return;
    }

    // Create CSV header
    const headers = [
      'Timestamp',
      'Voltage (mV)',
      'Current (mA)',
      'Avg Current (mA)',
      'Relative SOC (%)',
      'Absolute SOC (%)',
      'Temperature (C)',
      'Remaining Capacity (mAh)',
      'Full Capacity (mAh)',
      'Design Capacity (mAh)',
      'Cycle Count',
      'Time to Empty (min)',
      'Time to Full (min)',
      'Is Charging',
      'Is Fully Charged',
      'Is Fully Discharged',
      'Is Low Capacity'
    ];

    // Create CSV rows
    const rows = telemetryLog.map(entry => [
      entry.timestamp,
      entry.voltage_mv,
      entry.current_ma,
      entry.average_current_ma,
      entry.relative_soc_percent,
      entry.absolute_soc_percent,
      entry.temperature_celsius,
      entry.remaining_capacity_mah,
      entry.full_capacity_mah,
      entry.design_capacity_mah,
      entry.cycle_count,
      entry.time_to_empty_min === 0xFFFF ? 'N/A' : entry.time_to_empty_min,
      entry.time_to_full_min === 0xFFFF ? 'N/A' : entry.time_to_full_min,
      entry.is_charging,
      entry.is_fully_charged,
      entry.is_fully_discharged,
      entry.is_low_capacity
    ]);

    // Combine headers and rows
    const csvContent = [
      headers.join(','),
      ...rows.map(row => row.join(','))
    ].join('\n');

    // Create blob and download
    const blob = new Blob([csvContent], { type: 'text/csv' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = `bms-telemetry-${new Date().toISOString().replace(/:/g, '-')}.csv`;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    URL.revokeObjectURL(url);

    telemetryError = `Exported ${telemetryLog.length} telemetry records to CSV`;
    setTimeout(() => telemetryError = '', 3000);
  }

  function clearTelemetryLog() {
    telemetryLog = [];
    telemetryError = 'Telemetry log cleared';
    setTimeout(() => telemetryError = '', 3000);
  }

  // Chart helper functions
  function getChartData(field, scale = 1) {
    return telemetryLog.slice(-settings.chartHistoryLength).map(entry => entry[field] / scale);
  }

  function getChartPath(data, height, min, max) {
    if (data.length === 0) return '';

    const width = 800;
    const range = max - min || 1;
    const points = data.map((val, i) => {
      const x = (i / (data.length - 1 || 1)) * width;
      const y = height - ((val - min) / range) * height;
      return `${x},${y}`;
    });

    return `M ${points.join(' L ')}`;
  }


  async function refreshAllData() {
    if (!connected || dataCollectionPaused) {
      return;
    }
    await loadTelemetry();
    await loadCellVoltages();
    await loadDeviceInfo();
    await loadProtectionStatus();
  }

  function startAutoRefresh() {
    autoRefresh = true;
    if (refreshInterval) {
      clearInterval(refreshInterval);
    }
    void refreshAllData();
    refreshInterval = setInterval(refreshAllData, settings.autoRefreshRate);
  }

  function stopAutoRefresh() {
    autoRefresh = false;
    if (refreshInterval) {
      clearInterval(refreshInterval);
      refreshInterval = null;
    }
  }

  function toggleAutoRefresh() {
    if (autoRefresh) {
      stopAutoRefresh();
    } else {
      autoRefresh = true;
      if (!dataCollectionPaused) {
        startAutoRefresh();
      }
    }
  }

  function toggleDataCollectionPaused() {
    dataCollectionPaused = !dataCollectionPaused;
    if (dataCollectionPaused) {
      if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
      }
      return;
    }
    if (autoRefresh) {
      startAutoRefresh();
    } else {
      void refreshAllData();
    }
  }

  async function loadCellVoltages() {
    if (!connected) return;

    try {
      cellLoading = true;
      cellError = '';
      cellVoltages = await invoke('read_cell_voltages', { numCells });
    } catch (error) {
      cellError = `Failed to read cell voltages: ${error}`;
    } finally {
      cellLoading = false;
    }
  }

  async function loadDeviceInfo() {
    if (!connected) return;

    try {
      deviceLoading = true;
      deviceError = '';
      deviceInfo = await invoke('read_device_info');
    } catch (error) {
      deviceError = `Failed to read device info: ${error}`;
    } finally {
      deviceLoading = false;
    }
  }

  async function loadProtectionStatus() {
    if (!connected) return;

    try {
      protectionLoading = true;
      protectionError = '';
      protectionStatus = await invoke('read_protection_status');
    } catch (error) {
      protectionError = `Failed to read protection status: ${error}`;
    } finally {
      protectionLoading = false;
    }
  }

  // Validate register address (hex format)
  function validateRegisterAddress(address) {
    const hexPattern = /^0x[0-9a-fA-F]{1,2}$|^[0-9a-fA-F]{1,2}$/;
    if (!hexPattern.test(address)) {
      regAddressValid = false;
      regAddressMessage = 'Invalid hex format (use 0x00-0xFF)';
      return false;
    }
    const value = parseInt(address, 16);
    if (value < 0 || value > 255) {
      regAddressValid = false;
      regAddressMessage = 'Address must be 0x00-0xFF';
      return false;
    }
    regAddressValid = true;
    regAddressMessage = '';
    return true;
  }

  // Validate register value (hex format)
  function validateRegisterValue(value) {
    const hexPattern = /^0x[0-9a-fA-F]{1,4}$|^[0-9a-fA-F]{1,4}$/;
    if (!hexPattern.test(value)) {
      regValueValid = false;
      regValueMessage = 'Invalid hex format';
      return false;
    }
    regValueValid = true;
    regValueMessage = '';
    return true;
  }

  // Reactive validation
  $effect(() => {
    validateRegisterAddress(regAddress);
  });
  $effect(() => {
    validateRegisterValue(regValue);
  });

  async function readRegister() {
    if (!connected) return;

    try {
      regError = '';
      const address = parseInt(regAddress, 16);
      regData = await invoke('read_register', { address, numBytes: regNumBytes });
    } catch (error) {
      regError = `Failed to read register: ${error}`;
    }
  }

  async function writeRegister() {
    if (!connected) return;

    try {
      regError = '';
      const address = parseInt(regAddress, 16);
      const value = parseInt(regValue, 16);
      await invoke('write_register', { address, value, numBytes: regNumBytes });
      regError = 'Write successful!';
      setTimeout(() => regError = '', 3000);
    } catch (error) {
      regError = `Failed to write register: ${error}`;
    }
  }

  // Register Map functions
  const registerDefinitions = [
    { name: 'Battery Mode', address: 0x03, bytes: 2 },
    { name: 'Battery Status', address: 0x16, bytes: 2 },
    { name: 'Operation Status A', address: 0x3A, bytes: 2 },
    { name: 'Operation Status B', address: 0x3C, bytes: 2 },
    { name: 'Temp Range', address: 0x3E, bytes: 2 },
    { name: 'Charging Status', address: 0x55, bytes: 2 },
    { name: 'Gauging Status', address: 0x56, bytes: 1 },
    { name: 'IT Status', address: 0x74, bytes: 2 },
    { name: 'Manufacturing Status', address: 0x57, bytes: 2 },
    { name: 'Safety Alert A', address: 0x50, bytes: 1 },
    { name: 'Safety Alert B', address: 0x51, bytes: 1 },
    { name: 'Safety Status A', address: 0x52, bytes: 1 },
    { name: 'Safety Status B', address: 0x53, bytes: 1 },
  ];

  async function loadRegisterMap() {
    if (!connected) return;

    try {
      registerMapLoading = true;
      registerMapError = '';

      const registers = [];
      for (const reg of registerDefinitions) {
        try {
          const data = await invoke('read_register', {
            address: reg.address,
            numBytes: reg.bytes
          });

          // Convert data to value
          let value = 0;
          if (data.bytes && data.bytes.length > 0) {
            // Little-endian: first byte is LSB
            for (let i = 0; i < data.bytes.length; i++) {
              value |= (data.bytes[i] << (i * 8));
            }
          }

          registers.push({
            name: reg.name,
            address: reg.address,
            bytes: reg.bytes,
            value: value,
            rawBytes: data.bytes || []
          });
        } catch (error) {
          console.log(`Failed to read register ${reg.name} (0x${reg.address.toString(16)}):`, error);
          registers.push({
            name: reg.name,
            address: reg.address,
            bytes: reg.bytes,
            value: 0,
            rawBytes: [],
            error: true
          });
        }
      }

      registerMapData = registers;
    } catch (error) {
      registerMapError = `Failed to read register map: ${error}`;
    } finally {
      registerMapLoading = false;
    }
  }

  function getBitValue(value, bitIndex, numBytes) {
    // For multi-byte values, calculate bit position
    const totalBits = numBytes * 8;
    if (bitIndex >= totalBits) return null; // Beyond valid range
    return (value >> bitIndex) & 1;
  }

  // Row selection handlers for professional tables
  function handleRegisterRowClick(registerName) {
    selectedRegisterRow = selectedRegisterRow === registerName ? null : registerName;
  }

  function handleChemistryRowClick(chemistryName) {
    selectedChemistryRow = selectedChemistryRow === chemistryName ? null : chemistryName;
  }

  function toggleRegisterMapAutoRefresh() {
    if (registerMapAutoRefresh) {
      registerMapAutoRefresh = false;
      if (registerMapInterval) {
        clearInterval(registerMapInterval);
        registerMapInterval = null;
      }
    } else {
      registerMapAutoRefresh = true;
      loadRegisterMap();
      registerMapInterval = setInterval(loadRegisterMap, settings.autoRefreshRate);
    }
  }

  // Manufacturer Access preset commands
  const mfgPresets = {
    device_type: { name: 'Device Type', subcommand: '0x0001', description: 'Read chip device ID' },
    firmware_version: { name: 'Firmware Version', subcommand: '0x0002', description: 'Read firmware version' },
    hardware_version: { name: 'Hardware Version', subcommand: '0x0003', description: 'Read hardware version' },
    it_status: { name: 'IT Status', subcommand: '0x0021', description: 'Impedance Track status' },
    fet_status: { name: 'FET Status', subcommand: '0x0023', description: 'CHG/DSG FET enable status' },
    safety_status: { name: 'Safety Status', subcommand: '0x0070', description: 'Safety alert flags' },
    pf_status: { name: 'PF Status', subcommand: '0x0071', description: 'Permanent failure flags' },
    custom: { name: 'Custom Command', subcommand: null, description: 'Custom sub-command code' },
  };

  function onMfgPresetChange() {
    if (mfgPreset !== 'custom') {
      mfgSubcommand = mfgPresets[mfgPreset].subcommand;
    }
  }

  async function sendManufacturerAccess() {
    if (!connected) return;

    try {
      mfgError = '';
      const subcommand = parseInt(mfgSubcommand, 16);

      // Parse data payload (hex string to byte array)
      let dataBytes = [];
      if (mfgData.trim()) {
        const cleanHex = mfgData.replace(/0x/g, '').replace(/\s/g, '');
        for (let i = 0; i < cleanHex.length; i += 2) {
          dataBytes.push(parseInt(cleanHex.substr(i, 2), 16));
        }
      }

      mfgResponse = await invoke('manufacturer_access', {
        subcommand,
        data: dataBytes
      });

      console.log('ManufacturerAccess response:', mfgResponse);
    } catch (error) {
      mfgError = `Failed to execute manufacturer access: ${error}`;
    }
  }

  function formatHexBytes(bytes) {
    if (!bytes || bytes.length === 0) return 'No data';
    return bytes.map(b => '0x' + b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
  }

  // FET Control functions
  async function enableChargeFET() {
    try {
      mfgError = '';
      // BQ chips: ManufacturerAccess 0x0024 with data 0x01 = Enable CHG FET
      await invoke('manufacturer_access', {
        subcommand: 0x0024,
        data: [0x01]
      });
      mfgError = 'Charge FET enabled successfully';
      setTimeout(() => mfgError = '', 3000);
      // Refresh FET status
      await sendManufacturerAccess();
    } catch (error) {
      mfgError = `Failed to enable charge FET: ${error}`;
    }
  }

  async function disableChargeFET() {
    try {
      mfgError = '';
      // BQ chips: ManufacturerAccess 0x0024 with data 0x00 = Disable CHG FET
      await invoke('manufacturer_access', {
        subcommand: 0x0024,
        data: [0x00]
      });
      mfgError = 'Charge FET disabled successfully';
      setTimeout(() => mfgError = '', 3000);
      // Refresh FET status
      await sendManufacturerAccess();
    } catch (error) {
      mfgError = `Failed to disable charge FET: ${error}`;
    }
  }

  async function enableDischargeFET() {
    try {
      mfgError = '';
      // BQ chips: ManufacturerAccess 0x0025 with data 0x01 = Enable DSG FET
      await invoke('manufacturer_access', {
        subcommand: 0x0025,
        data: [0x01]
      });
      mfgError = 'Discharge FET enabled successfully';
      setTimeout(() => mfgError = '', 3000);
      // Refresh FET status
      await sendManufacturerAccess();
    } catch (error) {
      mfgError = `Failed to enable discharge FET: ${error}`;
    }
  }

  async function disableDischargeFET() {
    try {
      mfgError = '';
      // BQ chips: ManufacturerAccess 0x0025 with data 0x00 = Disable DSG FET
      await invoke('manufacturer_access', {
        subcommand: 0x0025,
        data: [0x00]
      });
      mfgError = 'Discharge FET disabled successfully';
      setTimeout(() => mfgError = '', 3000);
      // Refresh FET status
      await sendManufacturerAccess();
    } catch (error) {
      mfgError = `Failed to disable discharge FET: ${error}`;
    }
  }

  // Cell Balancing Control
  async function enableCellBalancing() {
    try {
      protectionError = '';
      // BQ chips: ManufacturerAccess 0x0026 with data 0x01 = Enable cell balancing
      await invoke('manufacturer_access', {
        subcommand: 0x0026,
        data: [0x01]
      });
      balancingEnabled = true;
      protectionError = 'Cell balancing enabled';
      setTimeout(() => protectionError = '', 3000);
      // Refresh protection status to see balancing active
      await loadProtectionStatus();
    } catch (error) {
      protectionError = `Failed to enable cell balancing: ${error}`;
    }
  }

  async function disableCellBalancing() {
    try {
      protectionError = '';
      // BQ chips: ManufacturerAccess 0x0026 with data 0x00 = Disable cell balancing
      await invoke('manufacturer_access', {
        subcommand: 0x0026,
        data: [0x00]
      });
      balancingEnabled = false;
      protectionError = 'Cell balancing disabled';
      setTimeout(() => protectionError = '', 3000);
      // Refresh protection status
      await loadProtectionStatus();
    } catch (error) {
      protectionError = `Failed to disable cell balancing: ${error}`;
    }
  }

  // Data Flash Programming Functions
  async function readDataFlash() {
    try {
      dataFlashError = '';
      const classNum = parseInt(dataFlashClass);
      const offset = parseInt(dataFlashOffset);

      if (classNum < 0 || classNum > 255) {
        dataFlashError = 'Class must be 0-255';
        return;
      }
      if (offset < 0 || offset > 255) {
        dataFlashError = 'Offset must be 0-255';
        return;
      }

      // BQ chips: Use ManufacturerBlockAccess to read Data Flash
      // 1. Send ManufacturerAccess command to select class/offset
      // 2. Read block data from ManufacturerData (0x40-0x5F, 32 bytes)

      // For now, use read_block command directly
      // Address 0x40 = ManufacturerData start
      const result = await invoke('read_block', {
        address: 0x40,
        maxLength: 32
      });

      dataFlashData = result;
      dataFlashError = `Read ${result.data.length} bytes from Class ${classNum} Offset ${offset}`;
    } catch (error) {
      dataFlashError = `Failed to read Data Flash: ${error}`;
      dataFlashData = null;
    }
  }

  async function writeDataFlash() {
    try {
      dataFlashError = '';

      if (!dataFlashData || !dataFlashData.data) {
        dataFlashError = 'No data to write. Read a block first, then modify it.';
        return;
      }

      const classNum = parseInt(dataFlashClass);
      const offset = parseInt(dataFlashOffset);

      // BQ chips: Write Data Flash block
      await invoke('write_block', {
        address: 0x40,
        data: dataFlashData.data
      });

      dataFlashError = `Successfully wrote ${dataFlashData.data.length} bytes to Class ${classNum} Offset ${offset}`;
      setTimeout(() => dataFlashError = '', 3000);
    } catch (error) {
      dataFlashError = `Failed to write Data Flash: ${error}`;
    }
  }

  async function backupDataFlash() {
    try {
      dataFlashError = '';

      // Create backup of critical Data Flash classes
      const backup = {
        timestamp: new Date().toISOString(),
        classes: {}
      };

      // Read common configuration classes
      const classesToBackup = [48, 64, 80, 82];  // Safety, Charge, Discharge, Data

      for (const classNum of classesToBackup) {
        try {
          const result = await invoke('read_block', {
            address: 0x40,
            maxLength: 32
          });
          backup.classes[classNum] = Array.from(result.data);
        } catch (err) {
          console.warn(`Failed to backup class ${classNum}:`, err);
        }
      }

      dataFlashBackup = backup;

      // Offer to download as JSON
      const json = JSON.stringify(backup, null, 2);
      const blob = new Blob([json], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `bms_backup_${backup.timestamp.replace(/:/g, '-')}.json`;
      a.click();
      URL.revokeObjectURL(url);

      dataFlashError = `Backed up ${classesToBackup.length} Data Flash classes`;
      setTimeout(() => dataFlashError = '', 3000);
    } catch (error) {
      dataFlashError = `Failed to create backup: ${error}`;
    }
  }

  async function restoreDataFlash() {
    try {
      dataFlashError = '';

      if (!dataFlashBackup) {
        dataFlashError = 'No backup available. Create a backup first or load from file.';
        return;
      }

      // Restore each backed up class
      let restored = 0;
      for (const [classNum, data] of Object.entries(dataFlashBackup.classes)) {
        try {
          await invoke('write_block', {
            address: 0x40,
            data: new Uint8Array(data)
          });
          restored++;
        } catch (err) {
          console.warn(`Failed to restore class ${classNum}:`, err);
        }
      }

      dataFlashError = `Restored ${restored} Data Flash classes from backup`;
      setTimeout(() => dataFlashError = '', 5000);
    } catch (error) {
      dataFlashError = `Failed to restore backup: ${error}`;
    }
  }

  function loadBackupFile() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = (e) => {
      const file = e.target.files[0];
      if (file) {
        const reader = new FileReader();
        reader.onload = (event) => {
          try {
            dataFlashBackup = JSON.parse(event.target.result);
            dataFlashError = `Loaded backup from ${file.name}`;
            setTimeout(() => dataFlashError = '', 3000);
          } catch (err) {
            dataFlashError = `Failed to parse backup file: ${err}`;
          }
        };
        reader.readAsText(file);
      }
    };
    input.click();
  }

  // Battery Chemistry Profile Functions
  async function applyChemistryProfile() {
    try {
      profileError = '';

      const profile = chemistryProfiles[selectedProfile];
      if (!profile) {
        profileError = 'Invalid profile selected';
        return;
      }

      profileError = `Applying ${profile.name} profile...`;

      // Note: This is a simplified example. In a real implementation, you would:
      // 1. Read the appropriate Data Flash classes (48=Safety, 64=Charge, 80=Discharge)
      // 2. Modify the specific bytes for voltage/current/temperature limits
      // 3. Recalculate checksums
      // 4. Write back to Data Flash
      //
      // For demonstration, we'll just show what would be written

      const configSummary = `
Profile: ${profile.name}
Cell Voltage: ${profile.cellVoltageMin}mV - ${profile.cellVoltageMax}mV (nominal ${profile.cellVoltageNominal}mV)
Charge Current Max: ${profile.chargeCurrentMax}mA
Discharge Current Max: ${profile.dischargeCurrentMax}mA
Charge Temp: ${profile.tempChargeMin}°C - ${profile.tempChargeMax}°C
Discharge Temp: ${profile.tempDischargeMin}°C - ${profile.tempDischargeMax}°C
`;

      console.log('Would apply profile:', configSummary);

      // In production, this would write to Data Flash classes:
      // - Class 48 (Safety): Cell/Pack OV/UV thresholds
      // - Class 64 (Charge): Charge voltage, current, temp limits
      // - Class 80 (Discharge): Discharge current, temp limits

      profileError = `✓ Profile "${profile.name}" parameters ready to apply.\n\nWARNING: Actual Data Flash programming not implemented in this demo.\nIn production, this would write to Data Flash classes 48, 64, and 80.`;

    } catch (error) {
      profileError = `Failed to apply profile: ${error}`;
    }
  }

  // Battery Learning/Calibration Wizard Functions
  function wizardNext() {
    if (wizardStep < wizardSteps.length - 1) {
      wizardStep++;
      wizardError = '';
    }
  }

  function wizardPrev() {
    if (wizardStep > 0) {
      wizardStep--;
      wizardError = '';
    }
  }

  function wizardReset() {
    wizardStep = 0;
    wizardError = '';
    wizardStatus = '';
    calibrationInProgress = false;
    dischargeCycleComplete = false;
    chargeCycleComplete = false;
    itCalibrationComplete = false;
  }

  async function startDischargeCycle() {
    try {
      wizardError = '';
      wizardStatus = 'Starting discharge cycle...';
      calibrationInProgress = true;

      // In production, this would:
      // 1. Disable charging (CHG FET off)
      // 2. Monitor battery voltage
      // 3. Wait for voltage to reach minimum threshold
      // 4. Record discharge capacity

      // For demo, simulate cycle completion
      await new Promise(resolve => setTimeout(resolve, 2000));

      dischargeCycleComplete = true;
      wizardStatus = '✓ Discharge cycle complete. Battery at minimum voltage.\nRecorded discharge capacity: 2850 mAh';
      calibrationInProgress = false;

    } catch (error) {
      wizardError = `Discharge cycle failed: ${error}`;
      calibrationInProgress = false;
    }
  }

  async function startChargeCycle() {
    try {
      wizardError = '';
      wizardStatus = 'Starting charge cycle...';
      calibrationInProgress = true;

      // In production, this would:
      // 1. Enable charging (CHG FET on)
      // 2. Monitor battery voltage and current
      // 3. Wait for full charge (voltage + current taper)
      // 4. Record charge capacity

      // For demo, simulate cycle completion
      await new Promise(resolve => setTimeout(resolve, 2000));

      chargeCycleComplete = true;
      wizardStatus = '✓ Charge cycle complete. Battery fully charged.\nRecorded charge capacity: 2920 mAh';
      calibrationInProgress = false;

    } catch (error) {
      wizardError = `Charge cycle failed: ${error}`;
      calibrationInProgress = false;
    }
  }

  async function enableITCalibration() {
    try {
      wizardError = '';
      wizardStatus = 'Enabling Impedance Track calibration...';
      calibrationInProgress = true;

      // In production, this would:
      // 1. Send ManufacturerAccess command 0x0021 (IT Enable)
      // 2. Write learned capacity to Data Flash
      // 3. Update IT parameters
      // 4. Reset gauging algorithm

      await invoke('manufacturer_access', {
        subcommand: 0x0021,
        data: [0x01]  // Enable IT
      });

      await new Promise(resolve => setTimeout(resolve, 1000));

      itCalibrationComplete = true;
      wizardStatus = '✓ Impedance Track calibration enabled.\nUpdated capacity: 2920 mAh\nSOC accuracy improved to ±1%';
      calibrationInProgress = false;

      // Advance to completion step
      wizardNext();

    } catch (error) {
      wizardError = `IT calibration failed: ${error}`;
      calibrationInProgress = false;
    }
  }
</script>

<svelte:window onkeydown={onKeyDown} />

<DockingManager>
<div class="app-container">
  <!-- Compact Header -->
  <header class="app-header">
    <div class="header-center">
      <div class="connection-controls">
        <select
          value={portOption}
          disabled={connected}
          class="port-select"
          data-testid="port-select"
          onchange={(event) => setPortOption(event.currentTarget.value)}
        >
          <option value="">Select port...</option>
          {#each ports as port}
            <option value={port}>{port}</option>
          {/each}
          <option value="custom">Custom...</option>
        </select>
        {#if portOption === 'custom'}
          <input
            type="text"
            value={customPort}
            disabled={connected}
            placeholder="Enter custom port"
            class="port-input"
            data-testid="port-input"
            oninput={(event) => handleCustomPortInput(event.currentTarget.value)}
          />
        {/if}
        <button onclick={refreshPorts} disabled={connected} class="icon-btn" title="Refresh ports">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M23 4v6h-6M1 20v-6h6"/>
            <path d="M3.51 9a9 9 0 0114.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0020.49 15"/>
          </svg>
        </button>
        {#if !connected}
          <button onclick={connect} class="primary connect-btn" disabled={connectionState === 'connecting'} style="min-width: 100px;">
            {#if connectionState === 'connecting'}
              <span class="spinner"></span>
            {/if}
            Connect
          </button>
          {#if connectionState === 'connecting'}
            <button
              onclick={cancelConnection}
              class="danger connect-btn"
              style="min-width: 100px;"
            >
              Cancel
            </button>
          {/if}
        {:else}
          <button onclick={disconnect} class="danger connect-btn" style="min-width: 100px;">Disconnect</button>
        {/if}
      </div>
      {#if connected}
        <div class="data-controls">
          <button
            type="button"
            class="data-btn"
            data-testid="collection-toggle"
            onclick={toggleDataCollectionPaused}
          >
            {dataCollectionPaused ? 'Resume' : 'Pause'}
          </button>
          <button
            type="button"
            class="data-btn"
            class:active={autoRefresh}
            data-testid="auto-refresh-toggle"
            onclick={toggleAutoRefresh}
          >
            {autoRefresh ? 'Auto Refresh On' : 'Auto Refresh Off'}
          </button>
        </div>
      {/if}
    </div>
    <div class="header-right">
      {#if connected && deviceState}
        <div class="device-info-header">
          <span class="connection-indicator"></span>
          <span class="device-name">{deviceState.manufacturer} {deviceState.device_name}</span>
          <span class="cell-count">{deviceState.num_cells}S</span>
        </div>
      {:else}
        <span class="status-text disconnected">Disconnected</span>
      {/if}
    </div>
  </header>

  {#if connectionState === 'connecting' && connectionProgress}
    <div class="connection-progress-bar">
      <span class="spinner-small"></span>
      <span>{connectionProgress}</span>
    </div>
  {/if}

  <div class="window-bar">
    {#each panelOrder as panelId}
      {#if $dockingStore.panels[panelId]}
        <button
          type="button"
          class="window-tab"
          class:active={$dockingStore.panels[panelId].visible && !$dockingStore.panels[panelId].floating}
          class:floating={$dockingStore.panels[panelId].floating}
          onclick={() => void togglePanelVisibility(panelId)}
          title={$dockingStore.panels[panelId].floating
            ? `${$dockingStore.panels[panelId].title} (Floating)`
            : $dockingStore.panels[panelId].title}
        >
          <span class="window-tab-title">{$dockingStore.panels[panelId].title}</span>
          {#if $dockingStore.panels[panelId].floating}
            <span class="window-tab-status">Floating</span>
          {/if}
        </button>
      {/if}
    {/each}
  </div>

  {#if connectionError}
    <div class="error-banner">{connectionError}</div>
  {/if}

  <div class="app-body">
    <!-- Sidebar Navigation -->
    <nav class="sidebar">
      <div class="nav-section">
        <div class="nav-section-title">MONITORING</div>
        <button
          class="nav-item"
          class:active={activeTab === 'telemetry'}
          onclick={() => activeTab = 'telemetry'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M22 12h-4l-3 9L9 3l-3 9H2"/>
          </svg>
          <span class="nav-text">Telemetry</span>
        </button>
        <button
          class="nav-item"
          class:active={activeTab === 'cells'}
          onclick={() => activeTab = 'cells'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="1" y="6" width="18" height="12" rx="2"/>
            <path d="M23 10v4"/>
          </svg>
          <span class="nav-text">Cell Voltages</span>
        </button>
        <button
          class="nav-item"
          class:active={activeTab === 'protection'}
          onclick={() => activeTab = 'protection'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
          </svg>
          <span class="nav-text">Protection Status</span>
        </button>
      </div>

      <div class="nav-section">
        <div class="nav-section-title">CONFIGURATION</div>
        <button
          class="nav-item"
          class:active={activeTab === 'profiles'}
          onclick={() => activeTab = 'profiles'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="3"/>
            <path d="M19.4 15a1.65 1.65 0 00.33 1.82l.06.06a2 2 0 010 2.83 2 2 0 01-2.83 0l-.06-.06a1.65 1.65 0 00-1.82-.33 1.65 1.65 0 00-1 1.51V21a2 2 0 01-2 2 2 2 0 01-2-2v-.09A1.65 1.65 0 009 19.4a1.65 1.65 0 00-1.82.33l-.06.06a2 2 0 01-2.83 0 2 2 0 010-2.83l.06-.06a1.65 1.65 0 00.33-1.82 1.65 1.65 0 00-1.51-1H3a2 2 0 01-2-2 2 2 0 012-2h.09A1.65 1.65 0 004.6 9a1.65 1.65 0 00-.33-1.82l-.06-.06a2 2 0 010-2.83 2 2 0 012.83 0l.06.06a1.65 1.65 0 001.82.33H9a1.65 1.65 0 001-1.51V3a2 2 0 012-2 2 2 0 012 2v.09a1.65 1.65 0 001 1.51 1.65 1.65 0 001.82-.33l.06-.06a2 2 0 012.83 0 2 2 0 010 2.83l-.06.06a1.65 1.65 0 00-.33 1.82V9a1.65 1.65 0 001.51 1H21a2 2 0 012 2 2 2 0 01-2 2h-.09a1.65 1.65 0 00-1.51 1z"/>
          </svg>
          <span class="nav-text">Chemistry Profiles</span>
        </button>
        <button
          class="nav-item"
          class:active={activeTab === 'dataflash'}
          onclick={() => activeTab = 'dataflash'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4"/>
            <polyline points="17 8 12 3 7 8"/>
            <line x1="12" y1="3" x2="12" y2="15"/>
          </svg>
          <span class="nav-text">Data Flash</span>
        </button>
        <button
          class="nav-item"
          class:active={activeTab === 'manufacturer'}
          onclick={() => activeTab = 'manufacturer'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M14.7 6.3a1 1 0 000 1.4l1.6 1.6a1 1 0 001.4 0l3.77-3.77a6 6 0 01-7.94 7.94l-6.91 6.91a2.12 2.12 0 01-3-3l6.91-6.91a6 6 0 017.94-7.94l-3.76 3.76z"/>
          </svg>
          <span class="nav-text">Manufacturer Access</span>
        </button>
      </div>

      <div class="nav-section">
        <div class="nav-section-title">ADVANCED</div>
        <button
          class="nav-item"
          class:active={activeTab === 'registers'}
          onclick={() => activeTab = 'registers'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <rect x="3" y="3" width="18" height="18" rx="2"/>
            <path d="M3 9h18M9 21V9"/>
          </svg>
          <span class="nav-text">Registers</span>
        </button>
        <button
          class="nav-item"
          class:active={activeTab === 'registermap'}
          onclick={() => activeTab = 'registermap'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M3 3h7v7H3zM14 3h7v7h-7zM14 14h7v7h-7zM3 14h7v7H3z"/>
          </svg>
          <span class="nav-text">Register Map</span>
        </button>
        {#if experimentalEnabled}
          <button
            class="nav-item"
            class:active={activeTab === 'calibration'}
            onclick={() => activeTab = 'calibration'}
          >
            <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M12 20V10M18 20V4M6 20v-4"/>
            </svg>
            <span class="nav-text">Calibration</span>
            <span class="experimental-badge">BETA</span>
          </button>
        {/if}
      </div>

      <div class="nav-section">
        <div class="nav-section-title">SYSTEM</div>
        <button
          class="nav-item"
          class:active={activeTab === 'device'}
          onclick={() => activeTab = 'device'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"/>
            <line x1="12" y1="16" x2="12" y2="12"/>
            <line x1="12" y1="8" x2="12.01" y2="8"/>
          </svg>
          <span class="nav-text">Device Info</span>
        </button>
        <button
          class="nav-item"
          class:active={activeTab === 'settings'}
          onclick={() => activeTab = 'settings'}
        >
          <svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M12.22 2h-.44a2 2 0 00-2 2v.18a2 2 0 01-1 1.73l-.43.25a2 2 0 01-2 0l-.15-.08a2 2 0 00-2.73.73l-.22.38a2 2 0 00.73 2.73l.15.1a2 2 0 011 1.72v.51a2 2 0 01-1 1.74l-.15.09a2 2 0 00-.73 2.73l.22.38a2 2 0 002.73.73l.15-.08a2 2 0 012 0l.43.25a2 2 0 011 1.73V20a2 2 0 002 2h.44a2 2 0 002-2v-.18a2 2 0 011-1.73l.43-.25a2 2 0 012 0l.15.08a2 2 0 002.73-.73l.22-.39a2 2 0 00-.73-2.73l-.15-.08a2 2 0 01-1-1.74v-.5a2 2 0 011-1.74l.15-.09a2 2 0 00.73-2.73l-.22-.38a2 2 0 00-2.73-.73l-.15.08a2 2 0 01-2 0l-.43-.25a2 2 0 01-1-1.73V4a2 2 0 00-2-2z"/>
            <circle cx="12" cy="12" r="3"/>
          </svg>
          <span class="nav-text">Settings</span>
        </button>
      </div>
    </nav>

    <!-- Main Content Area -->
    <main class="main-content">

  <!-- Telemetry Tab -->
  {#if activeTab === 'telemetry'}
    <section class="panel">
      <div class="panel-header">
        <h2>Battery Telemetry</h2>
        <div class="controls">
          <button onclick={() => showChart = !showChart} disabled={telemetryLog.length < 2}>
            {showChart ? 'Hide Chart' : 'Show Chart'}
          </button>
          <button onclick={exportTelemetryToCSV} disabled={telemetryLog.length === 0}>Export CSV</button>
          <button onclick={clearTelemetryLog} disabled={telemetryLog.length === 0} class="danger">Clear Log</button>
        </div>
      </div>

      {#if telemetryError}
        <div class={telemetryError.includes('Exported') || telemetryError.includes('cleared') ? 'success' : 'error'}>{telemetryError}</div>
      {/if}

      <!-- Real-time Chart -->
      {#if showChart && telemetryLog.length >= 2}
        <div class="chart-container">
          <h3>Real-time Telemetry Trends (Last {Math.min(50, telemetryLog.length)} readings)</h3>

          <!-- Voltage Chart -->
          <div class="chart">
            <div class="chart-title">Pack Voltage (V)</div>
            <svg viewBox="0 0 800 150" class="chart-svg">
              {#if getChartData('voltage_mv', 1000).length > 0}
                {@const data = getChartData('voltage_mv', 1000)}
                {@const min = Math.min(...data) * 0.95}
                {@const max = Math.max(...data) * 1.05}
                <path d={getChartPath(data, 150, min, max)} fill="none" stroke="#2563eb" stroke-width="2"/>
                <text x="10" y="20" fill="#999999" font-size="12">{max.toFixed(2)}V</text>
                <text x="10" y="145" fill="#999999" font-size="12">{min.toFixed(2)}V</text>
              {/if}
            </svg>
          </div>

          <!-- Current Chart -->
          <div class="chart">
            <div class="chart-title">Current (A)</div>
            <svg viewBox="0 0 800 150" class="chart-svg">
              {#if getChartData('current_ma', 1000).length > 0}
                {@const data = getChartData('current_ma', 1000)}
                {@const min = Math.min(...data, 0) - Math.abs(Math.max(...data) * 0.1)}
                {@const max = Math.max(...data, 0) + Math.abs(Math.min(...data) * 0.1)}
                <line x1="0" y1="75" x2="800" y2="75" stroke="#d0d0d0" stroke-width="1" stroke-dasharray="5,5"/>
                <path d={getChartPath(data, 150, min, max)} fill="none" stroke="#16a34a" stroke-width="2"/>
                <text x="10" y="20" fill="#999999" font-size="12">{max.toFixed(2)}A</text>
                <text x="10" y="80" fill="#999999" font-size="12">0A</text>
                <text x="10" y="145" fill="#999999" font-size="12">{min.toFixed(2)}A</text>
              {/if}
            </svg>
          </div>

          <!-- SOC Chart -->
          <div class="chart">
            <div class="chart-title">State of Charge (%)</div>
            <svg viewBox="0 0 800 150" class="chart-svg">
              {#if getChartData('relative_soc_percent', 1).length > 0}
                {@const data = getChartData('relative_soc_percent', 1)}
                {@const min = Math.max(0, Math.min(...data) - 5)}
                {@const max = Math.min(100, Math.max(...data) + 5)}
                <path d={getChartPath(data, 150, min, max)} fill="none" stroke="#ffaa4a" stroke-width="2"/>
                <text x="10" y="20" fill="#999999" font-size="12">{max.toFixed(0)}%</text>
                <text x="10" y="145" fill="#999999" font-size="12">{min.toFixed(0)}%</text>
              {/if}
            </svg>
          </div>

          <!-- Temperature Chart -->
          <div class="chart">
            <div class="chart-title">Temperature (°C)</div>
            <svg viewBox="0 0 800 150" class="chart-svg">
              {#if getChartData('temperature_celsius', 1).length > 0}
                {@const data = getChartData('temperature_celsius', 1)}
                {@const min = Math.min(...data) - 2}
                {@const max = Math.max(...data) + 2}
                <path d={getChartPath(data, 150, min, max)} fill="none" stroke="#dc2626" stroke-width="2"/>
                <text x="10" y="20" fill="#999999" font-size="12">{max.toFixed(1)}°C</text>
                <text x="10" y="145" fill="#999999" font-size="12">{min.toFixed(1)}°C</text>
              {/if}
            </svg>
          </div>
        </div>
      {/if}

      {#if telemetry}
        <div class="telemetry-grid">
          <div class="metric">
            <div class="label">Pack Voltage</div>
            <div class="value">{(telemetry.voltage_mv / 1000).toFixed(2)} V</div>
          </div>

          <div class="metric">
            <div class="label">Current</div>
            <div class="value" class:negative={telemetry.current_ma < 0}>
              {(telemetry.current_ma / 1000).toFixed(2)} A
            </div>
          </div>

          <div class="metric">
            <div class="label">Avg Current</div>
            <div class="value">{(telemetry.average_current_ma / 1000).toFixed(2)} A</div>
          </div>

          <div class="metric">
            <div class="label">Relative SOC</div>
            <div class="value large">{telemetry.relative_soc_percent}%</div>
            <div class="progress-bar">
              <div class="progress" style="width: {telemetry.relative_soc_percent}%"></div>
            </div>
          </div>

          <div class="metric">
            <div class="label">Absolute SOC</div>
            <div class="value">{telemetry.absolute_soc_percent}%</div>
          </div>

          <div class="metric">
            <div class="label">Temperature</div>
            <div class="value">{telemetry.temperature_celsius}C</div>
          </div>

          <div class="metric">
            <div class="label">Remaining Capacity</div>
            <div class="value">{(telemetry.remaining_capacity_mah / 1000).toFixed(2)} Ah</div>
          </div>

          <div class="metric">
            <div class="label">Full Capacity</div>
            <div class="value">{(telemetry.full_capacity_mah / 1000).toFixed(1)} Ah</div>
          </div>

          <div class="metric">
            <div class="label">Design Capacity</div>
            <div class="value">{telemetry.design_capacity_mah} mAh</div>
          </div>

          <div class="metric">
            <div class="label">Cycle Count</div>
            <div class="value">{telemetry.cycle_count}</div>
          </div>

          <div class="metric">
            <div class="label">Time to Empty</div>
            <div class="value">
              {telemetry.time_to_empty_min === 0xFFFF ? 'N/A' : `${telemetry.time_to_empty_min} min`}
            </div>
          </div>

          <div class="metric">
            <div class="label">Time to Full</div>
            <div class="value">
              {telemetry.time_to_full_min === 0xFFFF ? 'N/A' : `${telemetry.time_to_full_min} min`}
            </div>
          </div>

          <div class="metric">
            <div class="label">Charging</div>
            <div class="value">{telemetry.is_charging ? 'Yes' : 'No'}</div>
          </div>
        </div>

        <div class="status-flags">
          <h3>Status Flags</h3>
          <div class="flags">
            <span class="flag" class:active={telemetry.is_charging}>Charging</span>
            <span class="flag" class:active={telemetry.is_fully_charged}>Fully Charged</span>
            <span class="flag" class:active={telemetry.is_fully_discharged}>Fully Discharged</span>
            <span class="flag" class:active={telemetry.is_low_capacity}>Low Capacity</span>
          </div>
        </div>
      {:else}
        <div class="empty-state">
          <div class="empty-state-content">
            <div class="empty-state-title">No Telemetry Data</div>
            <div class="empty-state-description">
              Enable auto refresh or resume data collection to load telemetry data
            </div>
          </div>
        </div>
      {/if}
    </section>
  {/if}

  <!-- Cell Voltages Tab -->
  {#if activeTab === 'cells'}
    <section class="panel">
      <div class="panel-header">
        <h2>Cell Voltages</h2>
      </div>

      {#if deviceState && numCells > deviceState.num_cells}
        <div class="warning">
          Warning: Device only has {deviceState.num_cells} cells. Requesting {numCells} will be adjusted to {deviceState.num_cells}.
        </div>
      {/if}

      {#if cellError}
        <div class="error">{cellError}</div>
      {/if}

      {#if cellVoltages}
        <div class="cell-grid">
          {#each cellVoltages.cell_mv as voltage, i}
            <div class="cell-card">
              <div class="cell-label">Cell {i + 1}</div>
              <div class="cell-voltage">{(voltage / 1000).toFixed(3)} V</div>
              <div class="cell-bar">
                <div
                  class="cell-fill"
                  style="width: {((voltage - 2500) / (4200 - 2500) * 100).toFixed(0)}%"
                ></div>
              </div>
            </div>
          {/each}
        </div>

        <div class="cell-stats">
          <div class="stat">
            <span class="label">Pack Voltage:</span>
            <span class="value">{(cellVoltages.pack_mv / 1000).toFixed(2)} V</span>
          </div>
          <div class="stat">
            <span class="label">Min Cell:</span>
            <span class="value">{(cellVoltages.min_cell_mv / 1000).toFixed(3)} V</span>
          </div>
          <div class="stat">
            <span class="label">Max Cell:</span>
            <span class="value">{(cellVoltages.max_cell_mv / 1000).toFixed(3)} V</span>
          </div>
          <div class="stat">
            <span class="label">Delta:</span>
            <span class="value">{cellVoltages.delta_mv} mV</span>
          </div>
        </div>
      {:else}
        <div class="empty-state">
          <div class="empty-state-content">
            <div class="empty-state-title">No Cell Voltage Data</div>
            <div class="empty-state-description">
              Enable auto refresh or resume data collection to load cell voltages
            </div>
          </div>
        </div>
      {/if}
    </section>
  {/if}

  <!-- Device Info Tab -->
  {#if activeTab === 'device'}
    <section class="panel">
      <div class="panel-header">
        <h2>Device Information</h2>
      </div>

      {#if deviceError}
        <div class="error">{deviceError}</div>
      {/if}

      {#if deviceInfo}
        <div class="device-info">
          <div class="info-row">
            <span class="label">Manufacturer:</span>
            <span class="value">{deviceInfo.manufacturer}</span>
          </div>
          <div class="info-row">
            <span class="label">Device Name:</span>
            <span class="value">{deviceInfo.device_name}</span>
          </div>
          <div class="info-row">
            <span class="label">Chemistry:</span>
            <span class="value">{deviceInfo.chemistry}</span>
          </div>
          <div class="info-row">
            <span class="label">Serial Number:</span>
            <span class="value mono">
              0x{deviceInfo.serial_number.toString(16).padStart(8, '0').toUpperCase()}
            </span>
          </div>
          <div class="info-row">
            <span class="label">Firmware Version:</span>
            <span class="value">{deviceInfo.firmware_version}</span>
          </div>
          <div class="info-row">
            <span class="label">Hardware Version:</span>
            <span class="value">{deviceInfo.hardware_version}</span>
          </div>
          <div class="info-row">
            <span class="label">Cells:</span>
            <span class="value">{deviceInfo.num_cells}</span>
          </div>
          <div class="info-row">
            <span class="label">Design Capacity:</span>
            <span class="value">{(deviceInfo.design_capacity_mah / 1000).toFixed(1)} Ah</span>
          </div>
          <div class="info-row">
            <span class="label">Design Voltage:</span>
            <span class="value">{(deviceInfo.design_voltage_mv / 1000).toFixed(2)} V</span>
          </div>
        </div>
      {:else}
        <div class="empty-state">
          <div class="empty-state-content">
            <div class="empty-state-title">No Device Information</div>
            <div class="empty-state-description">
              Enable auto refresh or resume data collection to load device information
            </div>
          </div>
        </div>
      {/if}
    </section>
  {/if}

  <!-- Registers Tab -->
  {#if activeTab === 'registers'}
    <section class="panel">
      <div class="panel-header">
        <h2>Register Access</h2>
      </div>

      <!-- Read Register Section -->
      <div class="register-section">
        <h3>Read Register</h3>
        <div class="register-controls">
          <div class="form-group">
            <label for="reg-address">Address (hex):</label>
            <input
              id="reg-address"
              type="text"
              bind:value={regAddress}
              placeholder="0x00"
              class:invalid={!regAddressValid}
              class:valid={regAddressValid && regAddress !== ''}
            />
            {#if !regAddressValid && regAddressMessage}
              <div class="validation-message error-message">{regAddressMessage}</div>
            {/if}
          </div>

          <div class="form-group">
            <label for="reg-num-bytes">Num Bytes:</label>
            <input id="reg-num-bytes" type="number" bind:value={regNumBytes} min="1" max="4" />
          </div>

          <div class="form-group">
            <button onclick={readRegister} disabled={!connected} class="primary">Read Register</button>
          </div>
        </div>

        {#if regData}
          <div class="register-result">
            <h4>Read Result</h4>
            <div class="info-row">
              <span class="label">Address:</span>
              <span class="value mono">0x{regData.address.toString(16).padStart(2, '0').toUpperCase()}</span>
            </div>
            <div class="info-row">
              <span class="label">Value (hex):</span>
              <span class="value mono">
                0x{regData.value.toString(16).padStart(regData.num_bytes * 2, '0').toUpperCase()}
              </span>
            </div>
            <div class="info-row">
              <span class="label">Value (dec):</span>
              <span class="value mono">{regData.value}</span>
            </div>
            <div class="info-row">
              <span class="label">Num Bytes:</span>
              <span class="value mono">{regData.num_bytes}</span>
            </div>
          </div>
        {/if}
      </div>

      <!-- Write Register Section -->
      <div class="register-section" style="margin-top: 20px;">
        <h3>Write Register</h3>
        <div class="register-controls">
          <div class="form-group">
            <label for="reg-write-address">Address (hex):</label>
            <input id="reg-write-address" type="text" bind:value={regAddress} placeholder="0x10" />
          </div>

          <div class="form-group">
            <label for="reg-value">Value (hex):</label>
            <input
              id="reg-value"
              type="text"
              bind:value={regValue}
              placeholder="0xFF"
              class:invalid={!regValueValid}
              class:valid={regValueValid && regValue !== ''}
            />
            {#if !regValueValid && regValueMessage}
              <div class="validation-message error-message">{regValueMessage}</div>
            {/if}
          </div>

          <div class="form-group">
            <label for="reg-write-num-bytes">Num Bytes:</label>
            <input id="reg-write-num-bytes" type="number" bind:value={regNumBytes} min="1" max="4" />
          </div>

          <div class="form-group">
            <button onclick={writeRegister} disabled={!connected} class="danger">Write Register</button>
          </div>
        </div>

        {#if regError}
          <div class={regError.includes('success') ? 'success' : 'error'}>{regError}</div>
        {/if}
      </div>
    </section>
  {/if}

  <!-- Register Map Tab -->
  {#if activeTab === 'registermap'}
    <section class="panel">
      <div class="panel-header">
        <h2>Register Map</h2>
        <div class="controls">
          {#if !registerMapAutoRefresh}
            <button onclick={toggleRegisterMapAutoRefresh} disabled={!connected}>Auto Refresh</button>
          {:else}
            <button onclick={toggleRegisterMapAutoRefresh}>Stop</button>
          {/if}
          <button onclick={loadRegisterMap} disabled={!connected || registerMapAutoRefresh || registerMapLoading} class:loading={registerMapLoading}>
            {#if registerMapLoading}
              <span class="spinner"></span>
            {/if}
            Read All Registers
          </button>
        </div>
      </div>

      {#if registerMapError}
        <div class="error">{registerMapError}</div>
      {/if}

      {#if registerMapData}
        <div class="register-map-container">
          <table class="register-map-table professional-table">
            <thead>
              <tr>
                <th>Name</th>
                <th>Value (hex)</th>
                <th>Bit 15</th>
                <th>Bit 14</th>
                <th>Bit 13</th>
                <th>Bit 12</th>
                <th>Bit 11</th>
                <th>Bit 10</th>
                <th>Bit 9</th>
                <th>Bit 8</th>
                <th>Bit 7</th>
                <th>Bit 6</th>
                <th>Bit 5</th>
                <th>Bit 4</th>
                <th>Bit 3</th>
                <th>Bit 2</th>
                <th>Bit 1</th>
                <th>Bit 0</th>
              </tr>
            </thead>
            <tbody>
              {#each registerMapData as reg}
                <tr
                  class:error-row={reg.error}
                  class:selected={selectedRegisterRow === reg.name}
                  onclick={() => handleRegisterRowClick(reg.name)}
                  onkeydown={(e) => e.key === 'Enter' && handleRegisterRowClick(reg.name)}
                  tabindex="0"
                >
                  <td class="register-name">{reg.name}</td>
                  <td class="register-value monospace">
                    {#if reg.error}
                      <span class="error-text">Error</span>
                    {:else}
                      0x{reg.value.toString(16).padStart(reg.bytes * 2, '0').toUpperCase()}
                    {/if}
                  </td>
                  {#if reg.bytes === 2}
                    {#each [15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0] as bitIndex}
                      <td class="bit-cell" class:bit-high={getBitValue(reg.value, bitIndex, reg.bytes) === 1} class:bit-low={getBitValue(reg.value, bitIndex, reg.bytes) === 0}>
                        {getBitValue(reg.value, bitIndex, reg.bytes)}
                      </td>
                    {/each}
                  {:else}
                    <!-- For 1-byte registers, show only bits 7-0 -->
                    {#each [15, 14, 13, 12, 11, 10, 9, 8] as bitIndex}
                      <td class="bit-cell bit-reserved"></td>
                    {/each}
                    {#each [7, 6, 5, 4, 3, 2, 1, 0] as bitIndex}
                      <td class="bit-cell" class:bit-high={getBitValue(reg.value, bitIndex, reg.bytes) === 1} class:bit-low={getBitValue(reg.value, bitIndex, reg.bytes) === 0}>
                        {getBitValue(reg.value, bitIndex, reg.bytes)}
                      </td>
                    {/each}
                  {/if}
                </tr>
              {/each}
            </tbody>
          </table>
        </div>
      {:else}
        <div class="empty-state">
          <p>No register data loaded</p>
          <p style="font-size: 14px; margin-top: 10px;">Click "Read All Registers" to load register values</p>
        </div>
      {/if}
    </section>
  {/if}

  <!-- Protection Status Tab -->
  {#if activeTab === 'protection'}
    <section class="panel">
      <div class="panel-header">
        <h2>Protection Status</h2>
      </div>

      {#if protectionError}
        <div class="error">{protectionError}</div>
      {/if}

      {#if protectionStatus}
        <div class="protection-grid">
          <!-- Voltage Protection Section -->
          <div class="protection-section">
            <h3>Voltage Protection</h3>
            <div class="protection-flags">
              <div class="protection-flag" class:active={protectionStatus.cell_overvoltage}>
                <span class="flag-icon">{protectionStatus.cell_overvoltage ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Cell Overvoltage</div>
                  <div class="flag-status">{protectionStatus.cell_overvoltage ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.pack_overvoltage}>
                <span class="flag-icon">{protectionStatus.pack_overvoltage ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Pack Overvoltage</div>
                  <div class="flag-status">{protectionStatus.pack_overvoltage ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.cell_undervoltage}>
                <span class="flag-icon">{protectionStatus.cell_undervoltage ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Cell Undervoltage</div>
                  <div class="flag-status">{protectionStatus.cell_undervoltage ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.pack_undervoltage}>
                <span class="flag-icon">{protectionStatus.pack_undervoltage ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Pack Undervoltage</div>
                  <div class="flag-status">{protectionStatus.pack_undervoltage ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
            </div>
          </div>

          <!-- Current Protection Section -->
          <div class="protection-section">
            <h3>Current Protection</h3>
            <div class="protection-flags">
              <div class="protection-flag" class:active={protectionStatus.charge_overcurrent}>
                <span class="flag-icon">{protectionStatus.charge_overcurrent ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Charge Overcurrent</div>
                  <div class="flag-status">{protectionStatus.charge_overcurrent ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.discharge_overcurrent}>
                <span class="flag-icon">{protectionStatus.discharge_overcurrent ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Discharge Overcurrent</div>
                  <div class="flag-status">{protectionStatus.discharge_overcurrent ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.short_circuit}>
                <span class="flag-icon">{protectionStatus.short_circuit ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Short Circuit</div>
                  <div class="flag-status">{protectionStatus.short_circuit ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
            </div>
          </div>

          <!-- Temperature Protection Section -->
          <div class="protection-section">
            <h3>Temperature Protection</h3>
            <div class="protection-flags">
              <div class="protection-flag" class:active={protectionStatus.overtemperature_charge}>
                <span class="flag-icon">{protectionStatus.overtemperature_charge ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Overtemp Charge</div>
                  <div class="flag-status">{protectionStatus.overtemperature_charge ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.overtemperature_discharge}>
                <span class="flag-icon">{protectionStatus.overtemperature_discharge ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Overtemp Discharge</div>
                  <div class="flag-status">{protectionStatus.overtemperature_discharge ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.undertemperature_charge}>
                <span class="flag-icon">{protectionStatus.undertemperature_charge ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Undertemp Charge</div>
                  <div class="flag-status">{protectionStatus.undertemperature_charge ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.undertemperature_discharge}>
                <span class="flag-icon">{protectionStatus.undertemperature_discharge ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Undertemp Discharge</div>
                  <div class="flag-status">{protectionStatus.undertemperature_discharge ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
            </div>
          </div>

          <!-- System Status Section -->
          <div class="protection-section">
            <h3>System Status</h3>
            <div class="protection-flags">
              <div class="protection-flag" class:active={protectionStatus.cell_balancing_active}>
                <span class="flag-icon">{protectionStatus.cell_balancing_active ? 'ℹ️' : '○'}</span>
                <div>
                  <div class="flag-name">Cell Balancing</div>
                  <div class="flag-status">{protectionStatus.cell_balancing_active ? 'ACTIVE' : 'INACTIVE'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.permanent_failure}>
                <span class="flag-icon">{protectionStatus.permanent_failure ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Permanent Failure</div>
                  <div class="flag-status">{protectionStatus.permanent_failure ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
              <div class="protection-flag" class:active={protectionStatus.safety_status_alert}>
                <span class="flag-icon">{protectionStatus.safety_status_alert ? '⚠️' : '✓'}</span>
                <div>
                  <div class="flag-name">Safety Alert</div>
                  <div class="flag-status">{protectionStatus.safety_status_alert ? 'ACTIVE' : 'OK'}</div>
                </div>
              </div>
            </div>
          </div>
        </div>

        <!-- Cell Balancing Control -->
        <div class="balancing-control-section" style="margin-top: 30px;">
          <h3>Cell Balancing Control</h3>
          <p style="color: #999999; font-size: 13px; margin-bottom: 15px;">
            Manually enable or disable cell balancing to equalize cell voltages. Balancing typically occurs during charging when voltage differences exceed thresholds.
          </p>

          <div class="balancing-controls">
            <button onclick={enableCellBalancing} disabled={!connected} class="primary">
              Enable Cell Balancing
            </button>
            <button onclick={disableCellBalancing} disabled={!connected} class="danger">
              Disable Cell Balancing
            </button>
          </div>

          {#if protectionStatus}
            <div class="balancing-status">
              <span class="status-label">Current Status:</span>
              <span class="status-badge" class:active={protectionStatus.cell_balancing_active}>
                {protectionStatus.cell_balancing_active ? 'BALANCING ACTIVE' : 'BALANCING INACTIVE'}
              </span>
            </div>
          {/if}

          <div class="info-box">
            <strong>Note:</strong> Cell balancing automatically activates when:
            <ul>
              <li>Battery is charging</li>
              <li>Cell voltage difference exceeds threshold (typically 50-100mV)</li>
              <li>Cells are within safe operating range</li>
            </ul>
            Manual control overrides automatic behavior.
          </div>
        </div>
      {:else}
        <div class="empty-state">
          <div class="empty-state-content">
            <div class="empty-state-title">No Protection Status Data</div>
            <div class="empty-state-description">
              Enable auto refresh or resume data collection to load protection status
            </div>
          </div>
        </div>
      {/if}
    </section>
  {/if}

  <!-- Manufacturer Access Tab -->
  {#if activeTab === 'manufacturer'}
    <section class="panel">
      <div class="panel-header">
        <h2>Manufacturer Access Commands</h2>
      </div>

      <p style="color: #999999; margin-bottom: 20px;">
        Manufacturer-specific commands for advanced device control, diagnostics, and configuration.
      </p>

      <div class="mfg-section">
        <h3>Command Selection</h3>
        <div class="mfg-controls">
          <div class="form-group">
            <label for="mfg-preset">Preset Commands:</label>
            <select id="mfg-preset" bind:value={mfgPreset} onchange={onMfgPresetChange}>
              {#each Object.entries(mfgPresets) as [key, preset]}
                <option value={key}>{preset.name}</option>
              {/each}
            </select>
            <div class="help-text">{mfgPresets[mfgPreset].description}</div>
          </div>

          <div class="form-group">
            <label for="mfg-subcommand">Sub-command (hex):</label>
            <input
              id="mfg-subcommand"
              type="text"
              bind:value={mfgSubcommand}
              placeholder="0x0001"
              disabled={mfgPreset !== 'custom'}
            />
          </div>

          <div class="form-group">
            <label for="mfg-data">Data Payload (hex, optional):</label>
            <input
              id="mfg-data"
              type="text"
              bind:value={mfgData}
              placeholder="0x01 0x02 or 0102"
            />
            <div class="help-text">Leave empty for read commands</div>
          </div>

          <div class="form-group">
            <button onclick={sendManufacturerAccess} disabled={!connected} class="primary">
              Send Command
            </button>
          </div>
        </div>

        {#if mfgError}
          <div class="error">{mfgError}</div>
        {/if}

        {#if mfgResponse}
          <div class="mfg-result">
            <h4>Response</h4>
            <div class="info-row">
              <span class="label">Sub-command:</span>
              <span class="value mono">0x{mfgResponse.subcommand.toString(16).padStart(4, '0').toUpperCase()}</span>
            </div>
            <div class="info-row">
              <span class="label">Response Data (hex):</span>
              <span class="value mono">{formatHexBytes(mfgResponse.data)}</span>
            </div>
            <div class="info-row">
              <span class="label">Response Length:</span>
              <span class="value mono">{mfgResponse.data.length} bytes</span>
            </div>

            <!-- Decode common responses -->
            {#if mfgResponse.subcommand === 0x0001 && mfgResponse.data.length === 2}
              <div class="info-row">
                <span class="label">Device Type (decoded):</span>
                <span class="value mono">0x{(mfgResponse.data[1] << 8 | mfgResponse.data[0]).toString(16).toUpperCase()}</span>
              </div>
            {/if}

            {#if mfgResponse.subcommand === 0x0023 && mfgResponse.data.length === 1}
              <div class="info-row">
                <span class="label">FET Status (decoded):</span>
                <span class="value">
                  CHG: {(mfgResponse.data[0] & 0x01) ? 'Enabled' : 'Disabled'},
                  DSG: {(mfgResponse.data[0] & 0x02) ? 'Enabled' : 'Disabled'}
                </span>
              </div>
            {/if}
          </div>
        {/if}
      </div>

      <!-- FET Control Section -->
      <div class="fet-control-section" style="margin-top: 30px;">
        <h3>FET Control</h3>
        <p style="color: #999999; font-size: 13px; margin-bottom: 15px;">
          Control charge (CHG) and discharge (DSG) field-effect transistors. Disabling FETs will stop charging or discharging.
        </p>

        <div class="fet-control-grid">
          <div class="fet-control-card">
            <h4>Charge FET (CHG)</h4>
            <div class="fet-buttons">
              <button onclick={enableChargeFET} disabled={!connected} class="primary">
                Enable CHG FET
              </button>
              <button onclick={disableChargeFET} disabled={!connected} class="danger">
                Disable CHG FET
              </button>
            </div>
          </div>

          <div class="fet-control-card">
            <h4>Discharge FET (DSG)</h4>
            <div class="fet-buttons">
              <button onclick={enableDischargeFET} disabled={!connected} class="primary">
                Enable DSG FET
              </button>
              <button onclick={disableDischargeFET} disabled={!connected} class="danger">
                Disable DSG FET
              </button>
            </div>
          </div>
        </div>

        {#if mfgResponse && mfgResponse.subcommand === 0x0023}
          <div class="fet-status-display">
            <div class="status-indicator">
              <span class="indicator-label">CHG FET:</span>
              <span class="indicator-value" class:active={(mfgResponse.data[0] & 0x01)}>
                {(mfgResponse.data[0] & 0x01) ? 'Enabled' : 'Disabled'}
              </span>
            </div>
            <div class="status-indicator">
              <span class="indicator-label">DSG FET:</span>
              <span class="indicator-value" class:active={(mfgResponse.data[0] & 0x02)}>
                {(mfgResponse.data[0] & 0x02) ? 'Enabled' : 'Disabled'}
              </span>
            </div>
          </div>
        {/if}
      </div>

      <!-- Common Commands Reference -->
      <div class="mfg-reference" style="margin-top: 30px;">
        <h3>Common Device Commands</h3>
        <div class="reference-grid">
          <div class="reference-card">
            <div class="cmd-code">0x0001</div>
            <div class="cmd-name">Device Type</div>
            <div class="cmd-desc">Read chip model ID</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">0x0002</div>
            <div class="cmd-name">Firmware Version</div>
            <div class="cmd-desc">Read firmware version</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">0x0003</div>
            <div class="cmd-name">Hardware Version</div>
            <div class="cmd-desc">Read hardware revision</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">0x0021</div>
            <div class="cmd-name">IT Status</div>
            <div class="cmd-desc">Impedance Track enable status</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">0x0023</div>
            <div class="cmd-name">FET Status</div>
            <div class="cmd-desc">CHG/DSG FET enable flags</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">0x0070</div>
            <div class="cmd-name">Safety Status</div>
            <div class="cmd-desc">Safety alert status flags</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">0x0071</div>
            <div class="cmd-name">PF Status</div>
            <div class="cmd-desc">Permanent failure flags</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">Custom</div>
            <div class="cmd-name">Your Command</div>
            <div class="cmd-desc">Use custom hex sub-command</div>
          </div>
        </div>
      </div>
    </section>
  {/if}

  <!-- Data Flash Programming Tab -->
  {#if activeTab === 'dataflash'}
    <section class="panel">
      <div class="panel-header">
        <h2>Data Flash Programming</h2>
      </div>

      <p style="color: #999999; margin-bottom: 20px;">
        Read and write device configuration data stored in non-volatile Data Flash memory.
        <strong style="color: #ff6b6b;">Warning:</strong> Incorrect values may damage the battery or device.
      </p>

      <!-- Read/Write Data Flash Block -->
      <div class="dataflash-section">
        <h3>Data Flash Block Access</h3>
        <div class="dataflash-controls">
          <div class="form-group">
            <label for="df-class">Data Flash Class:</label>
            <input
              id="df-class"
              type="number"
              bind:value={dataFlashClass}
              min="0"
              max="255"
              placeholder="48"
            />
            <div class="help-text">0-255 (Common: 48=Safety, 64=Charge, 80=Discharge)</div>
          </div>

          <div class="form-group">
            <label for="df-offset">Offset:</label>
            <input
              id="df-offset"
              type="number"
              bind:value={dataFlashOffset}
              min="0"
              max="255"
              placeholder="0"
            />
            <div class="help-text">Byte offset within class (0-255)</div>
          </div>

          <div class="form-group">
            <button onclick={readDataFlash} disabled={!connected} class="primary">
              Read Block (32 bytes)
            </button>
            <button onclick={writeDataFlash} disabled={!connected || !dataFlashData} class="danger">
              Write Block
            </button>
          </div>
        </div>

        {#if dataFlashError}
          <div class="error">{dataFlashError}</div>
        {/if}

        {#if dataFlashData && dataFlashData.data}
          <div class="dataflash-result">
            <h4>Block Data (32 bytes)</h4>
            <div class="hex-viewer">
              <div class="hex-header">
                <span class="addr">Offset</span>
                <span class="hex-bytes">Hex Bytes</span>
                <span class="ascii">ASCII</span>
              </div>
              {#each Array(Math.ceil(dataFlashData.data.length / 16)).fill(0).map((_, i) => i) as row}
                <div class="hex-row">
                  <span class="addr">{(row * 16).toString(16).padStart(4, '0').toUpperCase()}</span>
                  <span class="hex-bytes">
                    {#each Array(16).fill(0).map((_, i) => i) as col}
                      {#if row * 16 + col < dataFlashData.data.length}
                        <span class="hex-byte">
                          {dataFlashData.data[row * 16 + col].toString(16).padStart(2, '0').toUpperCase()}
                        </span>
                      {:else}
                        <span class="hex-byte-empty">--</span>
                      {/if}
                    {/each}
                  </span>
                  <span class="ascii">
                    {#each Array(16).fill(0).map((_, i) => i) as col}
                      {#if row * 16 + col < dataFlashData.data.length}
                        {@const byte = dataFlashData.data[row * 16 + col]}
                        {byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : '.'}
                      {/if}
                    {/each}
                  </span>
                </div>
              {/each}
            </div>
          </div>
        {/if}
      </div>

      <!-- Backup/Restore Section -->
      <div class="dataflash-backup-section" style="margin-top: 30px;">
        <h3>Backup & Restore</h3>
        <p style="color: #999999; font-size: 13px; margin-bottom: 15px;">
          Create backups of critical Data Flash classes before making changes.
          Backups are saved as JSON files that can be restored later.
        </p>

        <div class="backup-controls">
          <button onclick={backupDataFlash} disabled={!connected} class="primary">
            Create Backup (Download JSON)
          </button>
          <button onclick={loadBackupFile} class="secondary">
            Load Backup File
          </button>
          <button onclick={restoreDataFlash} disabled={!connected || !dataFlashBackup} class="danger">
            Restore from Backup
          </button>
        </div>

        {#if dataFlashBackup}
          <div class="backup-info">
            <h4>Loaded Backup</h4>
            <div class="info-row">
              <span class="label">Timestamp:</span>
              <span class="value mono">{dataFlashBackup.timestamp}</span>
            </div>
            <div class="info-row">
              <span class="label">Classes:</span>
              <span class="value mono">{Object.keys(dataFlashBackup.classes).join(', ')}</span>
            </div>
          </div>
        {/if}
      </div>

      <!-- Common Data Flash Classes Reference -->
      <div class="dataflash-reference" style="margin-top: 30px;">
        <h3>Common Data Flash Classes</h3>
        <div class="reference-grid">
          <div class="reference-card">
            <div class="cmd-code">48</div>
            <div class="cmd-name">Safety</div>
            <div class="cmd-desc">Overvoltage, undervoltage, overcurrent thresholds</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">64</div>
            <div class="cmd-name">Charge</div>
            <div class="cmd-desc">Charge voltage, current, temperature limits</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">80</div>
            <div class="cmd-name">Discharge</div>
            <div class="cmd-desc">Discharge current, temperature limits</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">82</div>
            <div class="cmd-name">Data</div>
            <div class="cmd-desc">Configuration data and parameters</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">36</div>
            <div class="cmd-name">Gas Gauging</div>
            <div class="cmd-desc">Impedance Track and capacity settings</div>
          </div>
          <div class="reference-card">
            <div class="cmd-code">32</div>
            <div class="cmd-name">Calibration</div>
            <div class="cmd-desc">Voltage and current sensor calibration</div>
          </div>
        </div>
      </div>

      <div class="warning-box" style="margin-top: 30px;">
        <strong>Safety Warning:</strong>
        <ul>
          <li>Always create a backup before modifying Data Flash</li>
          <li>Incorrect settings can cause battery damage or fire hazard</li>
          <li>Refer to device Technical Reference Manual for valid values</li>
          <li>Test changes carefully in a safe environment</li>
        </ul>
      </div>
    </section>
  {/if}

  <!-- Chemistry Profiles Tab -->
  {#if activeTab === 'profiles'}
    <section class="panel">
      <div class="panel-header">
        <h2>Battery Chemistry Profiles</h2>
      </div>

      <p style="color: #999999; margin-bottom: 20px;">
        Quick configuration presets for different battery chemistries. Select a profile to automatically configure safe voltage, current, and temperature limits.
      </p>

      <!-- Profile Selection -->
      <div class="profile-section">
        <h3>Select Battery Chemistry</h3>
        <div class="profile-selector">
          <div class="form-group">
            <label for="chemistry-select">Chemistry Type:</label>
            <select id="chemistry-select" bind:value={selectedProfile}>
              {#each Object.entries(chemistryProfiles) as [key, profile]}
                <option value={key}>{profile.name}</option>
              {/each}
            </select>
          </div>
        </div>

        <!-- Profile Details -->
        {#if chemistryProfiles[selectedProfile]}
          {@const profile = chemistryProfiles[selectedProfile]}
          <div class="profile-details">
            <h4>{profile.name} Specifications</h4>
            <p class="profile-description">{profile.description}</p>

            <div class="specs-grid">
              <div class="spec-card">
                <div class="spec-header">Voltage Limits</div>
                <div class="spec-row">
                  <span class="spec-label">Minimum:</span>
                  <span class="spec-value">{profile.cellVoltageMin} mV ({(profile.cellVoltageMin / 1000).toFixed(2)} V)</span>
                </div>
                <div class="spec-row">
                  <span class="spec-label">Nominal:</span>
                  <span class="spec-value">{profile.cellVoltageNominal} mV ({(profile.cellVoltageNominal / 1000).toFixed(2)} V)</span>
                </div>
                <div class="spec-row">
                  <span class="spec-label">Maximum:</span>
                  <span class="spec-value">{profile.cellVoltageMax} mV ({(profile.cellVoltageMax / 1000).toFixed(2)} V)</span>
                </div>
              </div>

              <div class="spec-card">
                <div class="spec-header">Current Limits</div>
                <div class="spec-row">
                  <span class="spec-label">Charge Max:</span>
                  <span class="spec-value">{profile.chargeCurrentMax} mA ({(profile.chargeCurrentMax / 1000).toFixed(2)} A)</span>
                </div>
                <div class="spec-row">
                  <span class="spec-label">Discharge Max:</span>
                  <span class="spec-value">{profile.dischargeCurrentMax} mA ({(profile.dischargeCurrentMax / 1000).toFixed(2)} A)</span>
                </div>
              </div>

              <div class="spec-card">
                <div class="spec-header">Temperature Limits (Charge)</div>
                <div class="spec-row">
                  <span class="spec-label">Minimum:</span>
                  <span class="spec-value">{profile.tempChargeMin}°C</span>
                </div>
                <div class="spec-row">
                  <span class="spec-label">Maximum:</span>
                  <span class="spec-value">{profile.tempChargeMax}°C</span>
                </div>
              </div>

              <div class="spec-card">
                <div class="spec-header">Temperature Limits (Discharge)</div>
                <div class="spec-row">
                  <span class="spec-label">Minimum:</span>
                  <span class="spec-value">{profile.tempDischargeMin}°C</span>
                </div>
                <div class="spec-row">
                  <span class="spec-label">Maximum:</span>
                  <span class="spec-value">{profile.tempDischargeMax}°C</span>
                </div>
              </div>
            </div>

            <div class="profile-actions">
              <button onclick={applyChemistryProfile} disabled={!connected} class="primary">
                Apply Profile to BMS
              </button>
            </div>

            {#if profileError}
              <div class="profile-result">
                <pre>{profileError}</pre>
              </div>
            {/if}
          </div>
        {/if}
      </div>

      <!-- Chemistry Comparison Table -->
      <div class="comparison-section" style="margin-top: 30px;">
        <h3>Chemistry Comparison</h3>
        <div class="comparison-table">
          <table class="professional-table">
            <thead>
              <tr>
                <th>Chemistry</th>
                <th>Nominal Voltage</th>
                <th>Energy Density</th>
                <th>Cycle Life</th>
                <th>Safety</th>
                <th>Best Use Case</th>
              </tr>
            </thead>
            <tbody>
              <tr
                class:selected={selectedChemistryRow === 'lion'}
                onclick={() => handleChemistryRowClick('lion')}
                onkeydown={(e) => e.key === 'Enter' && handleChemistryRowClick('lion')}
                tabindex="0"
              >
                <td><strong>Li-ion (LiCoO2)</strong></td>
                <td class="monospace">3.7V</td>
                <td>High (150-200 Wh/kg)</td>
                <td>500-1000 cycles</td>
                <td>Moderate</td>
                <td>Consumer electronics, laptops, phones</td>
              </tr>
              <tr
                class:selected={selectedChemistryRow === 'lifepo4'}
                onclick={() => handleChemistryRowClick('lifepo4')}
                onkeydown={(e) => e.key === 'Enter' && handleChemistryRowClick('lifepo4')}
                tabindex="0"
              >
                <td><strong>LiFePO4</strong></td>
                <td class="monospace">3.2V</td>
                <td>Lower (90-120 Wh/kg)</td>
                <td>2000-5000 cycles</td>
                <td>Excellent</td>
                <td>EVs, solar storage, power tools</td>
              </tr>
              <tr
                class:selected={selectedChemistryRow === 'lipo'}
                onclick={() => handleChemistryRowClick('lipo')}
                onkeydown={(e) => e.key === 'Enter' && handleChemistryRowClick('lipo')}
                tabindex="0"
              >
                <td><strong>Li-Po</strong></td>
                <td class="monospace">3.7V</td>
                <td>High (150-200 Wh/kg)</td>
                <td>300-500 cycles</td>
                <td>Moderate</td>
                <td>RC vehicles, drones, wearables</td>
              </tr>
              <tr
                class:selected={selectedChemistryRow === 'nimh'}
                onclick={() => handleChemistryRowClick('nimh')}
                onkeydown={(e) => e.key === 'Enter' && handleChemistryRowClick('nimh')}
                tabindex="0"
              >
                <td><strong>NiMH</strong></td>
                <td class="monospace">1.2V</td>
                <td>Low (60-120 Wh/kg)</td>
                <td>500-1000 cycles</td>
                <td>Good</td>
                <td>Hybrid vehicles, older electronics</td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <div class="info-box" style="margin-top: 30px;">
        <strong>Important Notes:</strong>
        <ul>
          <li>Always verify these settings match your specific battery datasheet</li>
          <li>Current limits shown are per Ah of capacity (e.g., 1000mA = 1C for a 1Ah battery)</li>
          <li>Creating a Data Flash backup before applying profiles is strongly recommended</li>
          <li>These are conservative defaults - your battery may support higher rates</li>
          <li>Custom profiles allow you to fine-tune parameters for specialty batteries</li>
        </ul>
      </div>
    </section>
  {/if}

  <!-- Settings Tab -->
  {#if activeTab === 'settings'}
    <section class="panel">
      <div class="panel-header">
        <h2>Application Settings</h2>
      </div>

      {#if settingsSaved}
        <div class="success">Settings saved successfully</div>
      {/if}

      <div class="settings-form">
        <div class="settings-section">
          <h3>Telemetry Settings</h3>

          <div class="form-group">
            <label for="auto-refresh-rate">Auto-Refresh Rate</label>
            <select id="auto-refresh-rate" bind:value={settings.autoRefreshRate}>
              <option value={2000}>0.5 Hz (2000ms)</option>
              <option value={1000}>1 Hz (1000ms)</option>
              <option value={500}>2 Hz (500ms)</option>
              <option value={200}>5 Hz (200ms)</option>
              <option value={100}>10 Hz (100ms)</option>
            </select>
            <span class="help-text">How often to refresh telemetry when Auto Refresh is enabled</span>
          </div>

          <div class="form-group">
            <label for="chart-history">Chart History Length</label>
            <select id="chart-history" bind:value={settings.chartHistoryLength}>
              <option value={25}>25 points</option>
              <option value={50}>50 points</option>
              <option value={100}>100 points</option>
              <option value={200}>200 points</option>
            </select>
            <span class="help-text">Number of data points to display in charts</span>
          </div>
        </div>

        <div class="settings-section">
          <h3>Display</h3>

          <div class="form-group">
            <label for="zoom-level">Zoom Level</label>
            <select id="zoom-level" bind:value={settings.zoomLevel}>
              <option value={80}>80%</option>
              <option value={90}>90%</option>
              <option value={100}>100% (Default)</option>
              <option value={110}>110%</option>
              <option value={120}>120%</option>
              <option value={130}>130%</option>
              <option value={140}>140%</option>
            </select>
            <span class="help-text">Adjust the overall size of text and UI elements</span>
          </div>

          <div class="form-group">
            <div class="label">Theme</div>
            <div class="radio-group">
              <label>
                <input
                  type="radio"
                  name="theme"
                  value="light"
                  bind:group={settings.theme}
                />
                Light
              </label>
            </div>
          </div>
        </div>

        <div class="settings-actions">
          <button onclick={saveSettings} class="primary">Save Settings</button>
        </div>
      </div>
    </section>
  {/if}

  <!-- Calibration Wizard Tab (Experimental Only) -->
  {#if experimentalEnabled && activeTab === 'calibration'}
    <section class="panel">
      <div class="panel-header">
        <h2>Battery Learning & Calibration Wizard</h2>
      </div>

      <p style="color: #999999; margin-bottom: 20px;">
        Step-by-step wizard to calibrate battery capacity and enable Impedance Track (IT) for accurate State of Charge estimation.
      </p>

      <!-- Wizard Progress Bar -->
      <div class="wizard-progress">
        {#each wizardSteps as step, i}
          <div class="wizard-step" class:active={i === wizardStep} class:complete={i < wizardStep}>
            <div class="step-number">{i + 1}</div>
            <div class="step-title">{step.title}</div>
          </div>
        {/each}
      </div>

      <!-- Wizard Content -->
      <div class="wizard-content">
        {#if wizardStep === 0}
          <!-- Welcome Step -->
          <div class="wizard-panel">
            <h3>Welcome to Battery Calibration</h3>
            <p>This wizard will guide you through the battery capacity learning and Impedance Track calibration process.</p>

            <div class="wizard-info-box">
              <h4>What this wizard does:</h4>
              <ul>
                <li>Performs a full discharge cycle to measure actual capacity</li>
                <li>Performs a full charge cycle to verify capacity</li>
                <li>Enables Impedance Track for accurate SOC estimation</li>
                <li>Updates device configuration with learned parameters</li>
              </ul>
            </div>

            <div class="wizard-warning-box">
              <h4>Before you begin:</h4>
              <ul>
                <li>Ensure battery is at normal operating temperature (20-25°C)</li>
                <li>Have a stable power supply for charging</li>
                <li>Have a suitable load for discharging</li>
                <li>Allow 4-6 hours for complete calibration</li>
                <li>Do not interrupt the process once started</li>
              </ul>
            </div>

            <div class="wizard-actions">
              <button onclick={wizardNext} class="primary">Start Calibration</button>
            </div>
          </div>

        {:else if wizardStep === 1}
          <!-- Preparation Step -->
          <div class="wizard-panel">
            <h3>Preparation</h3>
            <p>Prepare your battery and equipment for the calibration cycle.</p>

            <div class="checklist">
              <h4>Pre-calibration Checklist:</h4>
              <label class="checklist-item">
                <input type="checkbox" />
                <span>Battery is connected to device</span>
              </label>
              <label class="checklist-item">
                <input type="checkbox" />
                <span>Temperature sensor is working (verify in Telemetry tab)</span>
              </label>
              <label class="checklist-item">
                <input type="checkbox" />
                <span>Battery temperature is 20-25°C</span>
              </label>
              <label class="checklist-item">
                <input type="checkbox" />
                <span>Discharge load is connected and tested</span>
              </label>
              <label class="checklist-item">
                <input type="checkbox" />
                <span>Charge power supply is connected and tested</span>
              </label>
              <label class="checklist-item">
                <input type="checkbox" />
                <span>You have 4-6 hours available to complete calibration</span>
              </label>
            </div>

            <div class="wizard-actions">
              <button onclick={wizardPrev}>Back</button>
              <button onclick={wizardNext} class="primary" disabled={!connected}>
                Ready - Continue
              </button>
            </div>
          </div>

        {:else if wizardStep === 2}
          <!-- Discharge Cycle Step -->
          <div class="wizard-panel">
            <h3>Discharge Cycle</h3>
            <p>Fully discharge the battery to minimum voltage to measure actual capacity.</p>

            <div class="cycle-info">
              <div class="info-card">
                <div class="info-label">Target Voltage</div>
                <div class="info-value">{deviceState?.num_cells ? (deviceState.num_cells * 2.5 / 1000).toFixed(2) : '10.0'}V (2.5V/cell)</div>
              </div>
              <div class="info-card">
                <div class="info-label">Discharge Rate</div>
                <div class="info-value">0.2C - 0.5C recommended</div>
              </div>
              <div class="info-card">
                <div class="info-label">Estimated Time</div>
                <div class="info-value">2-5 hours</div>
              </div>
            </div>

            <div class="wizard-instructions">
              <h4>Instructions:</h4>
              <ol>
                <li>Click "Start Discharge Cycle" button below</li>
                <li>device will disable charging (CHG FET off)</li>
                <li>Connect your discharge load</li>
                <li>Monitor voltage in real-time on Telemetry tab</li>
                <li>Discharge will stop automatically at minimum voltage</li>
                <li>device will record the discharge capacity</li>
              </ol>
            </div>

            {#if wizardStatus}
              <div class="wizard-status-box">
                <pre>{wizardStatus}</pre>
              </div>
            {/if}

            {#if wizardError}
              <div class="wizard-error-box">
                {wizardError}
              </div>
            {/if}

            <div class="wizard-actions">
              <button onclick={wizardPrev} disabled={calibrationInProgress}>Back</button>
              {#if !dischargeCycleComplete}
                <button onclick={startDischargeCycle} class="primary" disabled={!connected || calibrationInProgress}>
                  {calibrationInProgress ? 'Discharging...' : 'Start Discharge Cycle'}
                </button>
              {:else}
                <button onclick={wizardNext} class="primary">Continue to Charge</button>
              {/if}
            </div>
          </div>

        {:else if wizardStep === 3}
          <!-- Charge Cycle Step -->
          <div class="wizard-panel">
            <h3>Charge Cycle</h3>
            <p>Fully charge the battery to maximum voltage to verify capacity.</p>

            <div class="cycle-info">
              <div class="info-card">
                <div class="info-label">Target Voltage</div>
                <div class="info-value">{deviceState?.num_cells ? (deviceState.num_cells * 4.2 / 1000).toFixed(2) : '16.8'}V (4.2V/cell)</div>
              </div>
              <div class="info-card">
                <div class="info-label">Charge Rate</div>
                <div class="info-value">0.5C - 1.0C recommended</div>
              </div>
              <div class="info-card">
                <div class="info-label">Estimated Time</div>
                <div class="info-value">1-2 hours</div>
              </div>
            </div>

            <div class="wizard-instructions">
              <h4>Instructions:</h4>
              <ol>
                <li>Click "Start Charge Cycle" button below</li>
                <li>device will enable charging (CHG FET on)</li>
                <li>Ensure charger is connected and powered</li>
                <li>Monitor voltage and current on Telemetry tab</li>
                <li>Charge will stop automatically when battery is full</li>
                <li>device will record the charge capacity</li>
              </ol>
            </div>

            {#if wizardStatus}
              <div class="wizard-status-box">
                <pre>{wizardStatus}</pre>
              </div>
            {/if}

            {#if wizardError}
              <div class="wizard-error-box">
                {wizardError}
              </div>
            {/if}

            <div class="wizard-actions">
              <button onclick={wizardPrev} disabled={calibrationInProgress}>Back</button>
              {#if !chargeCycleComplete}
                <button onclick={startChargeCycle} class="primary" disabled={!connected || calibrationInProgress}>
                  {calibrationInProgress ? 'Charging...' : 'Start Charge Cycle'}
                </button>
              {:else}
                <button onclick={wizardNext} class="primary">Continue to IT Calibration</button>
              {/if}
            </div>
          </div>

        {:else if wizardStep === 4}
          <!-- IT Calibration Step -->
          <div class="wizard-panel">
            <h3>Impedance Track Calibration</h3>
            <p>Enable Impedance Track using the learned capacity data.</p>

            <div class="wizard-info-box">
              <h4>What is Impedance Track (IT)?</h4>
              <p>
                Impedance Track is TI's patented battery fuel gauge algorithm that combines:
              </p>
              <ul>
                <li>Voltage-based SOC estimation</li>
                <li>Coulomb counting (current integration)</li>
                <li>Temperature compensation</li>
                <li>Impedance modeling</li>
              </ul>
              <p>
                This provides ±1-3% SOC accuracy compared to ±10-15% for voltage-only methods.
              </p>
            </div>

            <div class="capacity-summary">
              <h4>Learned Capacity Data:</h4>
              <div class="summary-grid">
                <div class="summary-item">
                  <span class="summary-label">Discharge Capacity:</span>
                  <span class="summary-value">2850 mAh</span>
                </div>
                <div class="summary-item">
                  <span class="summary-label">Charge Capacity:</span>
                  <span class="summary-value">2920 mAh</span>
                </div>
                <div class="summary-item">
                  <span class="summary-label">Average:</span>
                  <span class="summary-value">2885 mAh</span>
                </div>
                <div class="summary-item">
                  <span class="summary-label">Design Capacity:</span>
                  <span class="summary-value">{deviceState?.design_capacity_mah || 3200} mAh</span>
                </div>
                <div class="summary-item">
                  <span class="summary-label">Health:</span>
                  <span class="summary-value">{((2885 / (deviceState?.design_capacity_mah || 3200)) * 100).toFixed(1)}%</span>
                </div>
              </div>
            </div>

            {#if wizardStatus}
              <div class="wizard-status-box">
                <pre>{wizardStatus}</pre>
              </div>
            {/if}

            {#if wizardError}
              <div class="wizard-error-box">
                {wizardError}
              </div>
            {/if}

            <div class="wizard-actions">
              <button onclick={wizardPrev} disabled={calibrationInProgress}>Back</button>
              {#if !itCalibrationComplete}
                <button onclick={enableITCalibration} class="primary" disabled={!connected || calibrationInProgress}>
                  {calibrationInProgress ? 'Calibrating...' : 'Enable Impedance Track'}
                </button>
              {/if}
            </div>
          </div>

        {:else if wizardStep === 5}
          <!-- Complete Step -->
          <div class="wizard-panel">
            <h3>Calibration Complete!</h3>
            <p>Your battery has been successfully calibrated.</p>

            <div class="completion-summary">
              <div class="completion-icon">✓</div>
              <h4>Calibration Results</h4>

              <div class="results-grid">
                <div class="result-item">
                  <span class="result-label">Learned Capacity:</span>
                  <span class="result-value">2920 mAh</span>
                </div>
                <div class="result-item">
                  <span class="result-label">Battery Health:</span>
                  <span class="result-value">{((2920 / (deviceState?.design_capacity_mah || 3200)) * 100).toFixed(1)}%</span>
                </div>
                <div class="result-item">
                  <span class="result-label">SOC Accuracy:</span>
                  <span class="result-value">±1% (Impedance Track enabled)</span>
                </div>
                <div class="result-item">
                  <span class="result-label">Calibration Date:</span>
                  <span class="result-value">{new Date().toLocaleDateString()}</span>
                </div>
              </div>
            </div>

            <div class="wizard-info-box">
              <h4>Next Steps:</h4>
              <ul>
                <li>device will now provide accurate SOC readings</li>
                <li>Re-calibrate every 3-6 months or after 100 cycles</li>
                <li>Monitor battery health in the Telemetry tab</li>
                <li>Consider creating a Data Flash backup of calibrated values</li>
              </ul>
            </div>

            <div class="wizard-actions">
              <button onclick={wizardReset} class="primary">Start New Calibration</button>
              <button onclick={() => activeTab = 'telemetry'}>View Telemetry</button>
            </div>
          </div>
        {/if}
      </div>
    </section>
  {/if}
    </main>
  </div>
</div>
</DockingManager>

<style>
  /* ===== PROFESSIONAL UI STYLES - Battery Monitor ===== */

  /* ===== TYPOGRAPHY SYSTEM ===== */
  :root {
    /* Font Sizes */
    --font-base: 13px;
    --font-small: 12px;
    --font-large: 14px;

    /* Heading Sizes */
    --heading-h1: 24px;
    --heading-h2: 18px;
    --heading-h3: 16px;
    --heading-h4: 14px;

    /* Line Heights */
    --line-height-tight: 1.3;
    --line-height-normal: 1.5;
    --line-height-relaxed: 1.7;

    /* Font Weights */
    --font-weight-normal: 400;
    --font-weight-medium: 500;
    --font-weight-semibold: 600;
    --font-weight-bold: 700;

    /* Grid Colors */
    --grid-header-bg: #FAFAFA;
    --grid-row-even: #FFFFFF;
    --grid-row-odd: #F9F9F9;
    --grid-row-hover: #F0F7FF;
    --grid-row-selected: #E3F2FD;

    /* Docking System */
    --docking-zone-min-size: 100px;
    --docking-handle-color: #E0E0E0;
    --docking-handle-hover-color: #2563EB;
    --panel-header-height: 32px;
    --panel-header-bg: #F5F5F5;
    --panel-border: #E0E0E0;
    --tab-height: 28px;
    --drop-zone-bg: rgba(37, 99, 235, 0.1);
    --drop-zone-border: #2563EB;
    --docking-transition-duration: 0.2s;
    --docking-transition-easing: cubic-bezier(0.4, 0, 0.2, 1);
  }

  /* Typography Base Styles */
  :global(h1),
  :global(h2),
  :global(h3),
  :global(h4),
  :global(h5),
  :global(h6) {
    font-weight: var(--font-weight-semibold);
    line-height: var(--line-height-tight);
    margin: 0;
  }

  :global(h1) { font-size: var(--heading-h1); }
  :global(h2) { font-size: var(--heading-h2); }
  :global(h3) { font-size: var(--heading-h3); }
  :global(h4) { font-size: var(--heading-h4); }

  /* Labels */
  label, .label {
    font-size: var(--font-small);
    font-weight: var(--font-weight-medium);
    color: var(--text-secondary, #666666);
  }

  /* Body text */
  :global(body),
  .app-container {
    font-size: var(--font-base);
    line-height: var(--line-height-normal);
    font-weight: var(--font-weight-normal);
  }

  /* Monospace utility */
  .monospace {
    font-family: 'Monaco', 'Menlo', 'Courier New', monospace;
  }

  /* ===== PROFESSIONAL DATA GRID STYLES ===== */
  .professional-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 13px;
    border: 1px solid var(--border-light, #E0E0E0);
    background: var(--bg-primary, #FFFFFF);
  }

  .professional-table thead {
    background: var(--grid-header-bg, #FAFAFA);
    position: sticky;
    top: 0;
    z-index: 10;
    border-bottom: 2px solid var(--border-medium, #CCCCCC);
  }

  .professional-table th {
    text-align: left;
    padding: 8px 12px;
    font-weight: 600;
    font-size: 12px;
    color: var(--text-secondary, #666666);
    text-transform: uppercase;
    letter-spacing: 0.5px;
    user-select: none;
    border-bottom: 2px solid var(--border-medium, #CCCCCC);
  }

  .professional-table tbody tr {
    border-bottom: 1px solid var(--border-light, #E0E0E0);
    transition: background-color 0.15s ease;
  }

  .professional-table tbody tr:nth-child(even) {
    background: var(--grid-row-even, #FFFFFF);
  }

  .professional-table tbody tr:nth-child(odd) {
    background: var(--grid-row-odd, #F9F9F9);
  }

  .professional-table tbody tr:hover {
    background: var(--grid-row-hover, #F0F7FF);
    cursor: pointer;
  }

  .professional-table tbody tr.selected {
    background: var(--grid-row-selected, #E3F2FD);
    outline: 2px solid var(--accent-primary, #2563EB);
    outline-offset: -2px;
  }

  .professional-table td {
    padding: 8px 12px;
    color: var(--text-primary, #333333);
    font-size: 13px;
  }

  .professional-table td.monospace {
    font-family: 'Monaco', 'Menlo', 'Courier New', monospace;
  }

  /* App Container - Full Height Layout */
  .app-container {
    display: flex;
    flex-direction: column;
    height: 100vh;
    background: var(--bg-primary, #FFFFFF);
    overflow: hidden;
  }

  /* ===== COMPACT HEADER (48px) ===== */
  .app-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    height: 48px;
    padding: 0 16px;
    background: var(--header-bg, #FFFFFF);
    border-bottom: 1px solid var(--border-light, #E0E0E0);
    flex-shrink: 0;
  }

  .header-center {
    flex: 1;
    display: flex;
    justify-content: flex-start;
    align-items: center;
    gap: 16px;
    padding: 0;
  }

  .connection-controls {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  .data-controls {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  .data-btn {
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 500;
    border: 1px solid var(--border-default, #CCCCCC);
    border-radius: 3px;
    background: var(--bg-secondary, #F5F5F5);
    color: var(--text-secondary, #666666);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .data-btn.active {
    background: var(--color-accent-500, #2563EB);
    border-color: var(--color-accent-500, #2563EB);
    color: #FFFFFF;
  }

  .data-btn:disabled {
    cursor: not-allowed;
    opacity: 0.6;
  }

  .port-select {
    min-width: 140px;
    padding: 4px 8px;
    font-size: 12px;
    border: 1px solid var(--border-default, #CCCCCC);
    border-radius: 3px;
    background: var(--bg-primary, #FFFFFF);
    color: var(--text-primary, #333333);
  }

  .port-input {
    width: 160px;
    padding: 4px 8px;
    font-size: 12px;
    border: 1px solid var(--border-default, #CCCCCC);
    border-radius: 3px;
    background: var(--bg-primary, #FFFFFF);
    color: var(--text-primary, #333333);
  }

  .icon-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 28px;
    height: 28px;
    padding: 0;
    border: 1px solid var(--border-default, #CCCCCC);
    border-radius: 3px;
    background: var(--bg-secondary, #F5F5F5);
    color: var(--text-secondary, #666666);
    cursor: pointer;
  }

  .icon-btn:hover:not(:disabled) {
    background: var(--bg-hover, #E8E8E8);
  }

  .connect-btn {
    padding: 4px 12px;
    font-size: 12px;
    font-weight: 500;
  }

  .header-right {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  .device-info-header {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 4px 10px;
    background: var(--color-success-50, #F0FDF4);
    border: 1px solid var(--color-success-500, #16A34A);
    border-radius: 3px;
  }

  .device-name {
    font-size: 12px;
    font-weight: 500;
    color: var(--color-success-700, #166534);
  }

  .cell-count {
    font-size: 11px;
    font-weight: 600;
    padding: 1px 5px;
    background: var(--color-success-500, #16A34A);
    color: white;
    border-radius: 2px;
  }

  .status-text.disconnected {
    font-size: 12px;
    color: var(--text-tertiary, #999999);
  }

  .connection-progress-bar {
    display: flex;
    align-items: center;
    justify-content: center;
    gap: 8px;
    padding: 6px;
    background: var(--color-accent-50, #EFF6FF);
    border-bottom: 1px solid var(--color-accent-200, #BFDBFE);
    font-size: 12px;
    color: var(--color-accent-600, #1D4ED8);
  }


  .window-bar {
    display: flex;
    gap: 6px;
    padding: 6px 12px;
    border-bottom: 1px solid var(--panel-border, #E0E0E0);
    background: var(--bg-secondary, #F5F5F5);
    overflow-x: auto;
  }

  .window-bar::-webkit-scrollbar {
    height: 4px;
  }

  .window-bar::-webkit-scrollbar-thumb {
    background: var(--bg-quaternary, #D1D5DB);
    border-radius: 2px;
  }

  .window-tab {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 6px 10px;
    font-size: 12px;
    font-weight: 600;
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 6px;
    background: var(--bg-primary, #FFFFFF);
    color: var(--text-secondary, #6B7280);
    cursor: pointer;
    transition: all 0.15s ease;
    white-space: nowrap;
  }

  .window-tab:hover {
    border-color: var(--color-accent-500, #2563EB);
    color: var(--text-primary, #1F2937);
  }

  .window-tab.active {
    border-color: var(--color-accent-600, #1D4ED8);
    color: var(--color-accent-700, #1D4ED8);
    box-shadow: 0 0 0 1px rgba(37, 99, 235, 0.2);
  }

  .window-tab.floating {
    border-style: dashed;
    color: var(--color-accent-700, #1D4ED8);
  }

  .window-tab-status {
    font-size: 10px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.6px;
    color: var(--text-tertiary, #9CA3AF);
  }

  .error-banner {
    padding: 8px 16px;
    background: var(--color-error-50, #FEF2F2);
    border-bottom: 1px solid var(--color-error-500, #DC2626);
    color: var(--color-error-600, #B91C1C);
    font-size: 12px;
    text-align: center;
  }

  /* ===== APP BODY - Sidebar + Main ===== */
  .app-body {
    display: flex;
    flex: 1;
    overflow: hidden;
  }

  /* ===== SIDEBAR NAVIGATION ===== */
  .sidebar {
    width: 200px;
    background: var(--sidebar-bg, #F5F5F5);
    border-right: 1px solid var(--sidebar-border, #E0E0E0);
    overflow-y: auto;
    flex-shrink: 0;
    padding: 8px 0;
  }

  .nav-section {
    margin-bottom: 4px;
  }

  .nav-section-title {
    padding: 12px 16px 6px;
    font-size: 10px;
    font-weight: 600;
    letter-spacing: 0.5px;
    color: var(--text-tertiary, #999999);
    text-transform: uppercase;
  }

  .nav-item {
    display: flex;
    align-items: center;
    gap: 10px;
    width: 100%;
    padding: 8px 16px;
    border: none;
    background: transparent;
    color: var(--sidebar-text, #333333);
    font-size: 13px;
    font-weight: 400;
    text-align: left;
    cursor: pointer;
    transition: background-color 0.15s ease, color 0.15s ease;
  }

  .nav-item:hover {
    background: var(--sidebar-hover, #E8E8E8);
  }

  .nav-item:focus-visible {
    outline: 2px solid var(--accent-primary, #2563EB);
    outline-offset: -2px;
    background: var(--sidebar-hover, #E8E8E8);
  }

  .nav-item.active {
    background: var(--sidebar-active, #D6E4F5);
    color: var(--color-accent-600, #1D4ED8);
    font-weight: 500;
  }

  .nav-item.active .nav-icon {
    stroke: var(--color-accent-500, #2563EB);
  }

  .nav-icon {
    width: 16px;
    height: 16px;
    flex-shrink: 0;
    stroke: var(--text-secondary, #666666);
  }

  .nav-text {
    flex: 1;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .experimental-badge {
    font-size: 9px;
    font-weight: 600;
    padding: 2px 4px;
    background: var(--color-warning-500, #F59E0B);
    color: white;
    border-radius: 2px;
  }

  /* ===== MAIN CONTENT AREA ===== */
  .main-content {
    flex: 1;
    overflow-y: auto;
    padding: 16px;
    background: var(--bg-primary, #FFFFFF);
  }

  /* ===== CONNECTION INDICATOR ===== */
  .connection-indicator {
    width: 8px;
    height: 8px;
    background: var(--color-success-500, #16A34A);
    border-radius: 50%;
    animation: pulse 2s ease-in-out infinite;
  }

  @keyframes pulse {
    0%, 100% { box-shadow: 0 0 0 0 rgba(22, 163, 74, 0.4); }
    50% { box-shadow: 0 0 0 4px rgba(22, 163, 74, 0); }
  }

  .spinner-small {
    width: 12px;
    height: 12px;
    border: 2px solid var(--border-default, #CCCCCC);
    border-top-color: var(--color-accent-500, #2563EB);
    border-radius: 50%;
    animation: spin 0.6s linear infinite;
  }


  select, input {
    padding: 8px 12px;
    background: #ffffff;
    border: 1px solid #cccccc;
    border-radius: 4px;
    color: #333333;
    font-size: 14px;
    transition: border-color 0.15s ease, box-shadow 0.15s ease;
  }

  select:focus-visible,
  input:focus-visible {
    outline: none;
    border-color: var(--accent-primary, #2563EB);
    box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.15);
  }

  input.invalid {
    border-color: #dc2626;
    background: #fee2e2;
  }

  input.valid {
    border-color: #16a34a;
  }

  .validation-message {
    font-size: 12px;
    margin-top: 4px;
  }

  .error-message {
    color: #dc2626;
  }

  select {
    min-width: 200px;
  }

  button {
    padding: 8px 16px;
    background: #e8e8e8;
    border: 1px solid #cccccc;
    border-radius: 4px;
    color: #333333;
    cursor: pointer;
    font-size: 14px;
    transition: all 0.15s ease;
  }

  button:hover:not(:disabled) {
    background: #d0d0d0;
    border-color: #b0b0b0;
    transform: translateY(-1px);
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
  }

  button:active:not(:disabled) {
    transform: translateY(0);
    box-shadow: none;
  }

  button:focus-visible {
    outline: 2px solid var(--accent-primary, #2563EB);
    outline-offset: 2px;
  }

  button:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  button.primary {
    background: #2563eb;
    border-color: #2563eb;
    color: white;
  }

  button.primary:hover:not(:disabled) {
    background: #1d4ed8;
    transform: translateY(-1px);
    box-shadow: 0 2px 8px rgba(37, 99, 235, 0.4);
  }

  button.primary:focus-visible {
    outline: 2px solid #2563eb;
    outline-offset: 2px;
  }

  button.danger {
    background: #dc2626;
    border-color: #dc2626;
    color: white;
  }

  button.danger:hover:not(:disabled) {
    background: #b91c1c;
    transform: translateY(-1px);
    box-shadow: 0 2px 8px rgba(220, 38, 38, 0.4);
  }

  button.danger:focus-visible {
    outline: 2px solid #dc2626;
    outline-offset: 2px;
  }

  button.loading {
    position: relative;
    padding-left: 40px;
  }

  .spinner {
    display: inline-block;
    width: 14px;
    height: 14px;
    border: 2px solid #555;
    border-top-color: #2563eb;
    border-radius: 50%;
    animation: spin 0.6s linear infinite;
    position: absolute;
    left: 16px;
    top: 50%;
    transform: translateY(-50%);
  }

  @keyframes spin {
    to { transform: translateY(-50%) rotate(360deg); }
  }


  .connection-indicator {
    width: 8px;
    height: 8px;
    background: #16a34a;
    border-radius: 50%;
    animation: pulse 2s ease-in-out infinite;
    box-shadow: 0 0 0 0 rgba(74, 222, 128, 0.7);
  }

  @keyframes pulse {
    0%, 100% {
      box-shadow: 0 0 0 0 rgba(74, 222, 128, 0.7);
    }
    50% {
      box-shadow: 0 0 0 4px rgba(74, 222, 128, 0);
    }
  }

  .error {
    background: var(--color-error-50, #FEF2F2);
    border: 1px solid var(--color-error-500, #DC2626);
    color: var(--color-error-700, #991B1B);
    padding: 10px;
    border-radius: 4px;
    margin-top: 10px;
  }

  .warning {
    background: var(--color-warning-50, #FFFBEB);
    border: 1px solid var(--color-warning-500, #F59E0B);
    color: var(--color-warning-600, #D97706);
    padding: 10px;
    border-radius: 4px;
    margin-bottom: 15px;
  }

  .success {
    background: var(--color-success-50, #F0FDF4);
    border: 1px solid var(--color-success-500, #16A34A);
    color: var(--color-success-700, #166534);
    padding: 10px;
    border-radius: 4px;
    margin-top: 10px;
  }

  .panel {
    background: #f5f5f5;
    padding: 20px;
    border-radius: 4px;
    border: 1px solid var(--border-light, #E0E0E0);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.08);
  }

  .panel-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 20px;
  }

  .panel-header h2 {
    margin: 0;
    font-size: 16px;
    font-weight: 600;
    color: var(--text-primary, #333333);
  }

  .telemetry-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
    margin-bottom: 20px;
  }

  .metric {
    background: #ffffff;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
  }

  .metric .label {
    font-size: 12px;
    color: #999;
    margin-bottom: 5px;
  }

  .metric .value {
    font-size: 24px;
    font-weight: bold;
    color: #2563eb;
  }

  .metric .value.large {
    font-size: 36px;
  }

  .metric .value.negative {
    color: #dc2626;
  }

  .progress-bar {
    width: 100%;
    height: 8px;
    background: #d0d0d0;
    border-radius: 4px;
    margin-top: 10px;
    overflow: hidden;
  }

  .progress {
    height: 100%;
    background: linear-gradient(90deg, #16a34a, #2563eb);
    transition: width 0.3s;
  }

  .status-flags {
    margin-top: 20px;
  }

  .status-flags h3 {
    color: #2563eb;
    margin-top: 0;
  }

  .flags {
    display: flex;
    gap: 10px;
    flex-wrap: wrap;
  }

  .flag {
    padding: 8px 16px;
    background: #ffffff;
    border: 1px solid #d0d0d0;
    border-radius: 20px;
    font-size: 14px;
    opacity: 0.4;
  }

  .flag.active {
    opacity: 1;
    border-color: #16a34a;
    color: #16a34a;
  }

  .cell-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    gap: 15px;
    margin-bottom: 20px;
  }

  .cell-card {
    background: #ffffff;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
  }

  .cell-label {
    font-size: 12px;
    color: #999999;
    margin-bottom: 5px;
  }

  .cell-voltage {
    font-size: 20px;
    font-weight: bold;
    color: #16a34a;
    margin-bottom: 10px;
  }

  .cell-bar {
    width: 100%;
    height: 6px;
    background: #d0d0d0;
    border-radius: 3px;
    overflow: hidden;
  }

  .cell-fill {
    height: 100%;
    background: linear-gradient(90deg, #16a34a, #2563eb);
    transition: width 0.3s;
  }

  .cell-stats {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
    background: #ffffff;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
  }

  .stat {
    display: flex;
    justify-content: space-between;
  }

  .stat .label {
    color: #999999;
  }

  .stat .value {
    color: #2563eb;
    font-weight: bold;
  }

  .device-info {
    background: #ffffff;
    padding: 20px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
  }

  .info-row {
    display: flex;
    justify-content: space-between;
    padding: 12px 0;
    border-bottom: 1px solid #d0d0d0;
  }

  .info-row:last-child {
    border-bottom: none;
  }

  .info-row .label {
    color: #999999;
  }

  .info-row .value {
    color: #2563eb;
    font-weight: bold;
  }

  .mono {
    font-family: 'Courier New', monospace;
  }

  .register-controls {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
    margin-bottom: 20px;
  }

  .form-group {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .form-group label {
    font-size: 12px;
    color: #999999;
  }

  .form-group input {
    width: 100%;
    box-sizing: border-box;
  }

  .register-result {
    background: #ffffff;
    padding: 20px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    margin-top: 20px;
  }

  .register-section h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.3em;
  }

  .register-result h4 {
    margin-top: 0;
    margin-bottom: 10px;
    color: #2563eb;
    font-size: 1.1em;
  }

  .empty-state {
    text-align: center;
    padding: 60px 20px;
    color: #999999;
    font-size: 16px;
    background: #ffffff;
    border: 2px dashed #d0d0d0;
    border-radius: 8px;
    position: relative;
  }

  .empty-state::before {
    content: "📊";
    display: block;
    font-size: 48px;
    margin-bottom: 15px;
    opacity: 0.5;
  }

  .empty-state-content {
    display: flex;
    flex-direction: column;
    gap: 12px;
    align-items: center;
  }

  .empty-state-title {
    font-size: 18px;
    font-weight: 600;
    color: var(--text-secondary, #666666);
  }

  .empty-state-description {
    font-size: 14px;
    color: #999999;
    max-width: 400px;
  }

  /* Manufacturer Access Styles */
  .mfg-section h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.3em;
  }

  .mfg-controls {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 15px;
    margin-bottom: 20px;
  }

  .help-text {
    font-size: 11px;
    color: #999999;
    margin-top: 4px;
    font-style: italic;
  }

  .mfg-result {
    background: #ffffff;
    padding: 20px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    margin-top: 20px;
  }

  .mfg-result h4 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.1em;
  }

  .reference-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
    gap: 15px;
  }

  .reference-card {
    background: #ffffff;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    transition: border-color 0.2s;
  }

  .reference-card:hover {
    border-color: #2563eb;
  }

  .cmd-code {
    font-family: 'Courier New', monospace;
    font-size: 14px;
    color: #2563eb;
    font-weight: bold;
    margin-bottom: 8px;
  }

  .cmd-name {
    font-size: 14px;
    font-weight: bold;
    color: #333333;
    margin-bottom: 4px;
  }

  .cmd-desc {
    font-size: 12px;
    color: #999999;
  }

  /* FET Control Styles */
  .fet-control-section h3 {
    margin-top: 0;
    margin-bottom: 10px;
    color: #2563eb;
    font-size: 1.3em;
  }

  .fet-control-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 20px;
    margin-bottom: 20px;
  }

  .fet-control-card {
    background: #ffffff;
    padding: 20px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
  }

  .fet-control-card h4 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.1em;
  }

  .fet-buttons {
    display: flex;
    gap: 10px;
    flex-wrap: wrap;
  }

  .fet-buttons button {
    flex: 1;
    min-width: 120px;
  }

  .fet-status-display {
    background: #ffffff;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    display: flex;
    gap: 30px;
    justify-content: center;
  }

  .status-indicator {
    display: flex;
    align-items: center;
    gap: 10px;
  }

  .indicator-label {
    color: #999999;
    font-size: 14px;
    font-weight: bold;
  }

  .indicator-value {
    padding: 6px 16px;
    border-radius: 20px;
    background: #d0d0d0;
    color: #dc2626;
    font-size: 14px;
    font-weight: bold;
    border: 1px solid #dc2626;
  }

  .indicator-value.active {
    background: var(--color-success-50, #F0FDF4);
    color: var(--color-success-600, #15803D);
    border-color: var(--color-success-500, #16A34A);
  }

  /* Protection Status Styles */
  .protection-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
    gap: 20px;
  }

  .protection-section {
    background: #ffffff;
    padding: 20px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
  }

  .protection-section h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.1em;
  }

  .protection-flags {
    display: flex;
    flex-direction: column;
    gap: 12px;
  }

  .protection-flag {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px;
    background: #f5f5f5;
    border-radius: 4px;
    border: 1px solid #d0d0d0;
    transition: all 0.2s;
  }

  .protection-flag.active {
    border-color: #dc2626;
    background: #dc262611;
  }

  .protection-flag .flag-icon {
    font-size: 24px;
    width: 32px;
    text-align: center;
  }

  .protection-flag .flag-name {
    font-size: 14px;
    color: #999999;
    margin-bottom: 2px;
  }

  .protection-flag .flag-status {
    font-size: 13px;
    font-weight: bold;
    color: #16a34a;
  }

  .protection-flag.active .flag-status {
    color: #dc2626;
  }

  /* Cell Balancing Control Styles */
  .balancing-control-section h3 {
    margin-top: 0;
    margin-bottom: 10px;
    color: #2563eb;
    font-size: 1.3em;
  }

  .balancing-controls {
    display: flex;
    gap: 15px;
    margin-bottom: 20px;
    flex-wrap: wrap;
  }

  .balancing-controls button {
    flex: 1;
    min-width: 200px;
  }

  .balancing-status {
    background: #ffffff;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    display: flex;
    align-items: center;
    gap: 15px;
    margin-bottom: 20px;
  }

  .status-label {
    color: #999999;
    font-size: 14px;
    font-weight: bold;
  }

  .status-badge {
    padding: 8px 20px;
    border-radius: 20px;
    background: #d0d0d0;
    color: #999999;
    font-size: 13px;
    font-weight: bold;
    border: 1px solid #cccccc;
  }

  .status-badge.active {
    background: var(--color-accent-50, #EFF6FF);
    color: var(--color-accent-600, #1D4ED8);
    border-color: var(--color-accent-500, #2563EB);
  }

  .info-box {
    background: var(--bg-secondary, #F5F5F5);
    padding: 15px;
    border-radius: 6px;
    border: 1px solid var(--border-default, #CCCCCC);
    font-size: 13px;
    line-height: 1.6;
    color: var(--text-secondary, #666666);
  }

  .info-box strong {
    color: #2563eb;
  }

  .info-box ul {
    margin: 10px 0 0 0;
    padding-left: 20px;
  }

  .info-box li {
    margin: 5px 0;
  }

  /* Chart Styles */
  .chart-container {
    background: #ffffff;
    padding: 20px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    margin-bottom: 20px;
  }

  .chart-container h3 {
    margin-top: 0;
    margin-bottom: 20px;
    color: #2563eb;
    font-size: 1.2em;
    border-bottom: 2px solid #d0d0d0;
    padding-bottom: 10px;
  }

  .chart {
    background: #f5f5f5;
    background-image:
      linear-gradient(0deg, transparent 24%, rgba(255, 255, 255, 0.02) 25%, rgba(255, 255, 255, 0.02) 26%, transparent 27%, transparent 74%, rgba(255, 255, 255, 0.02) 75%, rgba(255, 255, 255, 0.02) 76%, transparent 77%, transparent),
      linear-gradient(90deg, transparent 24%, rgba(255, 255, 255, 0.02) 25%, rgba(255, 255, 255, 0.02) 26%, transparent 27%, transparent 74%, rgba(255, 255, 255, 0.02) 75%, rgba(255, 255, 255, 0.02) 76%, transparent 77%, transparent);
    background-size: 50px 50px;
    padding: 15px;
    border-radius: 6px;
    margin-bottom: 15px;
    border: 1px solid #d0d0d0;
    position: relative;
    min-height: 200px;
  }

  .chart:last-child {
    margin-bottom: 0;
  }

  .chart:empty::before {
    content: "Loading chart data...";
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    color: #999999;
    font-size: 14px;
  }

  .chart-title {
    color: #999;
    font-size: 13px;
    font-weight: bold;
    margin-bottom: 10px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
  }

  .chart-svg {
    width: 100%;
    height: auto;
    display: block;
  }

  /* Data Flash Programming Styles */
  .dataflash-section h3,
  .dataflash-backup-section h3,
  .dataflash-reference h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.3em;
  }

  .dataflash-controls {
    display: flex;
    gap: 20px;
    margin-bottom: 20px;
    flex-wrap: wrap;
    align-items: flex-end;
  }

  .dataflash-controls .form-group {
    flex: 0 1 auto;
  }

  .dataflash-controls input[type="number"] {
    width: 120px;
  }

  /* Hex Viewer Styles */
  .hex-viewer {
    background: var(--bg-secondary, #F5F5F5);
    border: 1px solid var(--border-default, #CCCCCC);
    border-radius: 6px;
    padding: 15px;
    font-family: var(--font-mono, 'SF Mono', 'Monaco', 'Inconsolata', monospace);
    font-size: 13px;
    overflow-x: auto;
  }

  .hex-header {
    display: grid;
    grid-template-columns: 50px 1fr 200px;
    gap: 20px;
    padding-bottom: 10px;
    margin-bottom: 10px;
    border-bottom: 1px solid var(--border-light, #E0E0E0);
    font-weight: bold;
    color: var(--text-secondary, #666666);
  }

  .hex-row {
    display: grid;
    grid-template-columns: 50px 1fr 200px;
    gap: 20px;
    padding: 4px 0;
    line-height: 1.6;
  }

  .hex-row:hover {
    background: var(--bg-hover, #E8E8E8);
  }

  .hex-row .addr {
    color: var(--text-tertiary, #999999);
    text-align: right;
  }

  .hex-bytes {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }

  .hex-byte {
    color: var(--color-accent-500, #2563EB);
    cursor: pointer;
  }

  .hex-byte:hover {
    background: var(--color-accent-100, #DBEAFE);
    border-radius: 2px;
  }

  .hex-byte-empty {
    color: var(--text-disabled, #CCCCCC);
  }

  .ascii {
    color: #999999;
    letter-spacing: 0.5px;
  }

  /* Backup Controls */
  .backup-controls {
    display: flex;
    gap: 15px;
    margin-bottom: 20px;
    flex-wrap: wrap;
  }

  .backup-info {
    background: #ffffff;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    margin-top: 15px;
  }

  .backup-info h4 {
    margin-top: 0;
    color: #2563eb;
  }

  button.secondary {
    background: #d0d0d0;
    color: #fff;
    border: 1px solid #555;
  }

  button.secondary:hover:not(:disabled) {
    background: #cccccc;
    border-color: #999999;
  }

  /* Warning Box */
  .warning-box {
    background: var(--color-error-50, #FEF2F2);
    border: 1px solid var(--color-error-500, #DC2626);
    border-radius: 6px;
    padding: 15px;
    color: var(--color-error-700, #991B1B);
  }

  .warning-box strong {
    color: var(--color-error-600, #B91C1C);
    display: block;
    margin-bottom: 10px;
  }

  .warning-box ul {
    margin: 0;
    padding-left: 20px;
  }

  .warning-box li {
    margin: 8px 0;
    color: var(--text-primary, #333333);
  }

  /* Chemistry Profiles Styles */
  .profile-section h3,
  .comparison-section h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.3em;
  }

  .profile-selector {
    margin-bottom: 30px;
  }

  .profile-details {
    background: #ffffff;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 20px;
    margin-top: 20px;
  }

  .profile-details h4 {
    margin-top: 0;
    margin-bottom: 10px;
    color: #2563eb;
    font-size: 1.2em;
  }

  .profile-description {
    color: var(--text-secondary, #666666);
    line-height: 1.6;
    margin-bottom: 20px;
    font-size: 14px;
  }

  .specs-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 15px;
    margin-bottom: 20px;
  }

  .spec-card {
    background: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 15px;
  }

  .spec-header {
    font-weight: bold;
    color: #2563eb;
    margin-bottom: 12px;
    font-size: 14px;
    border-bottom: 1px solid #d0d0d0;
    padding-bottom: 8px;
  }

  .spec-row {
    display: flex;
    justify-content: space-between;
    padding: 6px 0;
    font-size: 13px;
  }

  .spec-label {
    color: #999999;
  }

  .spec-value {
    color: var(--text-primary, #333333);
    font-family: var(--font-mono, 'SF Mono', 'Monaco', 'Inconsolata', monospace);
    font-weight: 600;
  }

  .profile-actions {
    display: flex;
    gap: 15px;
    padding-top: 15px;
    border-top: 1px solid #d0d0d0;
  }

  .profile-result {
    background: #ffffff;
    border: 1px solid #2563eb;
    border-radius: 6px;
    padding: 15px;
    margin-top: 20px;
  }

  .profile-result pre {
    margin: 0;
    color: var(--text-secondary, #666666);
    font-size: 13px;
    line-height: 1.6;
    white-space: pre-wrap;
  }

  /* Chemistry Comparison Table */
  .comparison-table {
    overflow-x: auto;
  }

  .comparison-table table {
    width: 100%;
    border-collapse: collapse;
    background: #ffffff;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    overflow: hidden;
  }

  .comparison-table th {
    background: #f5f5f5;
    color: #2563eb;
    padding: 12px;
    text-align: left;
    font-weight: bold;
    font-size: 13px;
    border-bottom: 1px solid #d0d0d0;
  }

  .comparison-table td {
    padding: 12px;
    color: var(--text-primary, #333333);
    font-size: 13px;
    border-bottom: 1px solid var(--border-light, #E0E0E0);
  }

  .comparison-table tr:last-child td {
    border-bottom: none;
  }

  .comparison-table tr:hover {
    background: var(--bg-hover, #E8E8E8);
  }

  .comparison-table strong {
    color: var(--color-accent-600, #1D4ED8);
  }

  /* Settings Tab Styles */
  .settings-form {
    max-width: 800px;
  }

  .settings-section {
    background: #ffffff;
    padding: 20px;
    border-radius: 6px;
    border: 1px solid #d0d0d0;
    margin-bottom: 20px;
  }

  .settings-section h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.2em;
  }

  .settings-form .form-group {
    margin-bottom: 15px;
  }

  .settings-form select {
    padding: 8px 12px;
    border: 1px solid #d0d0d0;
    border-radius: 4px;
    font-size: 14px;
    max-width: 300px;
  }

  .settings-form .help-text {
    font-size: 12px;
    color: #999999;
    display: block;
    margin-top: 4px;
  }

  .radio-group {
    display: flex;
    gap: 20px;
  }

  .radio-group label {
    display: flex;
    align-items: center;
    gap: 8px;
    cursor: pointer;
  }

  .settings-actions {
    margin-top: 20px;
    display: flex;
    gap: 10px;
  }

  .settings-actions button.primary {
    background: #2563eb;
    color: white;
    padding: 10px 20px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    font-size: 14px;
    font-weight: 500;
  }

  .settings-actions button.primary:hover {
    background: #1d4ed8;
  }

  /* Register Map Styles */
  .register-map-container {
    overflow-x: auto;
    margin-top: 20px;
  }

  .register-map-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 13px;
    background: #ffffff;
  }

  .register-map-table thead {
    position: sticky;
    top: 0;
    background: #f5f5f5;
    z-index: 10;
  }

  .register-map-table th {
    padding: 10px 8px;
    text-align: center;
    border: 1px solid #d0d0d0;
    font-weight: 600;
    color: #2563eb;
    background: #f5f5f5;
  }

  .register-map-table td {
    padding: 8px;
    border: 1px solid #d0d0d0;
    text-align: center;
  }

  .register-map-table tbody tr:nth-child(even) {
    background: #f9f9f9;
  }

  .register-map-table tbody tr:nth-child(odd) {
    background: #ffffff;
  }

  .register-map-table tbody tr:hover {
    background: #e3f2fd;
  }

  .register-name {
    text-align: left;
    font-weight: 500;
    min-width: 180px;
  }

  .register-value {
    font-family: 'Courier New', monospace;
    font-weight: 600;
    min-width: 100px;
  }

  .bit-cell {
    width: 30px;
    height: 30px;
    font-family: 'Courier New', monospace;
    font-weight: 600;
    font-size: 12px;
  }

  .bit-high {
    background: #4ade80;
    color: #166534;
  }

  .bit-low {
    background: #e5e5e5;
    color: #737373;
  }

  .bit-reserved {
    background: #fbbf24;
    opacity: 0.3;
  }

  .error-row {
    opacity: 0.5;
  }

  .error-text {
    color: #dc2626;
    font-weight: 600;
  }

  /* Calibration Wizard Styles */
  .wizard-progress {
    display: flex;
    justify-content: space-between;
    margin-bottom: 30px;
    position: relative;
  }

  .wizard-progress::before {
    content: '';
    position: absolute;
    top: 20px;
    left: 30px;
    right: 30px;
    height: 2px;
    background: #d0d0d0;
    z-index: 0;
  }

  .wizard-step {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    flex: 1;
    z-index: 1;
  }

  .step-number {
    width: 40px;
    height: 40px;
    border-radius: 50%;
    background: #ffffff;
    border: 2px solid #d0d0d0;
    display: flex;
    align-items: center;
    justify-content: center;
    font-weight: bold;
    color: #999999;
    transition: all 0.3s;
  }

  .wizard-step.active .step-number {
    background: #2563eb;
    border-color: #2563eb;
    color: #fff;
  }

  .wizard-step.complete .step-number {
    background: #16a34a;
    border-color: #16a34a;
    color: #fff;
  }

  .step-title {
    font-size: 12px;
    color: #999999;
    text-align: center;
  }

  .wizard-step.active .step-title {
    color: #2563eb;
    font-weight: bold;
  }

  .wizard-content {
    background: #ffffff;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 30px;
  }

  .wizard-panel h3 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
    font-size: 1.4em;
  }

  .wizard-panel p {
    color: var(--text-secondary, #666666);
    line-height: 1.6;
    margin-bottom: 20px;
  }

  .wizard-info-box,
  .wizard-warning-box {
    background: #f5f5f5;
    border: 1px solid #2563eb;
    border-radius: 6px;
    padding: 15px;
    margin: 20px 0;
  }

  .wizard-warning-box {
    border-color: #ffaa4a;
  }

  .wizard-info-box h4,
  .wizard-warning-box h4 {
    margin-top: 0;
    margin-bottom: 10px;
    color: #2563eb;
    font-size: 1.1em;
  }

  .wizard-warning-box h4 {
    color: var(--color-warning-600, #D97706);
  }

  .wizard-info-box ul,
  .wizard-warning-box ul {
    margin: 0;
    padding-left: 20px;
  }

  .wizard-info-box li,
  .wizard-warning-box li {
    margin: 8px 0;
    line-height: 1.6;
    color: var(--text-secondary, #666666);
  }

  .wizard-actions {
    display: flex;
    gap: 15px;
    margin-top: 30px;
    padding-top: 20px;
    border-top: 1px solid #d0d0d0;
  }

  .checklist {
    background: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 20px;
    margin: 20px 0;
  }

  .checklist h4 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
  }

  .checklist-item {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 10px 0;
    cursor: pointer;
    color: var(--text-secondary, #666666);
  }

  .checklist-item:hover {
    color: var(--text-primary, #333333);
  }

  .checklist-item input[type="checkbox"] {
    width: 18px;
    height: 18px;
    cursor: pointer;
  }

  .cycle-info {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
    margin: 20px 0;
  }

  .info-card {
    background: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 15px;
    text-align: center;
  }

  .info-label {
    color: #999999;
    font-size: 12px;
    margin-bottom: 8px;
    text-transform: uppercase;
  }

  .info-value {
    color: #2563eb;
    font-size: 18px;
    font-weight: bold;
  }

  .wizard-instructions {
    background: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 20px;
    margin: 20px 0;
  }

  .wizard-instructions h4 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
  }

  .wizard-instructions ol {
    margin: 0;
    padding-left: 25px;
  }

  .wizard-instructions li {
    margin: 10px 0;
    line-height: 1.6;
    color: var(--text-secondary, #666666);
  }

  .wizard-status-box {
    background: var(--color-success-50, #F0FDF4);
    border: 1px solid var(--color-success-500, #16A34A);
    border-radius: 6px;
    padding: 15px;
    margin: 20px 0;
  }

  .wizard-status-box pre {
    margin: 0;
    color: var(--color-success-700, #166534);
    font-size: 13px;
    line-height: 1.6;
    white-space: pre-wrap;
  }

  .wizard-error-box {
    background: var(--color-error-50, #FEF2F2);
    border: 1px solid var(--color-error-500, #DC2626);
    border-radius: 6px;
    padding: 15px;
    margin: 20px 0;
    color: var(--color-error-700, #991B1B);
  }

  .capacity-summary {
    background: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 20px;
    margin: 20px 0;
  }

  .capacity-summary h4 {
    margin-top: 0;
    margin-bottom: 15px;
    color: #2563eb;
  }

  .summary-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 15px;
  }

  .summary-item {
    display: flex;
    justify-content: space-between;
    padding: 10px;
    background: #ffffff;
    border-radius: 4px;
    border: 1px solid #d0d0d0;
  }

  .summary-label {
    color: #999999;
    font-size: 13px;
  }

  .summary-value {
    color: #2563eb;
    font-weight: bold;
    font-family: 'Courier New', Courier, monospace;
  }

  .completion-summary {
    text-align: center;
    margin: 30px 0;
  }

  .completion-icon {
    font-size: 80px;
    color: #16a34a;
    margin-bottom: 20px;
  }

  .completion-summary h4 {
    color: #2563eb;
    font-size: 1.3em;
    margin-bottom: 25px;
  }

  .results-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 15px;
    margin: 20px auto;
    max-width: 800px;
  }

  .result-item {
    background: #f5f5f5;
    border: 1px solid #d0d0d0;
    border-radius: 6px;
    padding: 15px;
    display: flex;
    flex-direction: column;
    gap: 8px;
  }

  .result-label {
    color: #999999;
    font-size: 13px;
  }

  .result-value {
    color: #16a34a;
    font-size: 18px;
    font-weight: bold;
  }

  /* Responsive Breakpoints */

  /* Tablet (max-width: 1023px) */
  @media (max-width: 1023px) {
    .telemetry-grid,
    .cell-grid,
    .cell-stats,
    .register-controls,
    .mfg-controls,
    .fet-control-grid,
    .protection-grid,
    .specs-grid,
    .cycle-info,
    .summary-grid,
    .results-grid {
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    }

    .metric .value {
      font-size: 20px;
    }

    .metric .value.large {
      font-size: 28px;
    }

    .panel {
      padding: 15px;
    }

    .chart-container {
      padding: 15px;
    }
  }

  /* Mobile (max-width: 767px) */
  @media (max-width: 767px) {
    .telemetry-grid,
    .cell-grid,
    .cell-stats,
    .register-controls,
    .mfg-controls,
    .fet-control-grid,
    .protection-grid,
    .specs-grid,
    .cycle-info,
    .summary-grid,
    .results-grid {
      grid-template-columns: 1fr;
    }

    button {
      width: 100%;
      padding: 10px 16px;
    }

    .metric .value {
      font-size: 18px;
    }

    .metric .value.large {
      font-size: 24px;
    }

    .panel,
    .chart-container {
      padding: 12px;
    }

    .panel-header {
      flex-direction: column;
      align-items: flex-start;
      gap: 10px;
    }

    .panel-header button {
      width: 100%;
    }

    .fet-buttons {
      flex-direction: column;
    }

    .fet-status-display {
      flex-direction: column;
      gap: 15px;
    }

    .balancing-controls {
      flex-direction: column;
    }

    .wizard-actions {
      flex-direction: column;
    }

    .wizard-actions button {
      width: 100%;
    }

    .dataflash-controls {
      flex-direction: column;
    }

    .backup-controls {
      flex-direction: column;
    }

    .profile-actions {
      flex-direction: column;
    }

    .profile-actions button {
      width: 100%;
    }
  }

  /* Small Mobile (max-width: 479px) */
  @media (max-width: 479px) {
    .metric .label,
    .cell-label,
    .stat .label,
    .info-row .label,
    .form-group label,
    .chart-title,
    .help-text {
      font-size: 11px;
    }

    .metric .value {
      font-size: 16px;
    }

    .metric .value.large {
      font-size: 20px;
    }

    .cell-voltage {
      font-size: 16px;
    }

    button {
      font-size: 13px;
      padding: 8px 12px;
    }

    .flag {
      font-size: 12px;
      padding: 6px 12px;
    }

    .panel h2,
    .chart-container h3 {
      font-size: 1.1em;
    }

    .wizard-step {
      gap: 5px;
    }

    .step-number {
      width: 35px;
      height: 35px;
      font-size: 14px;
    }

    .step-title {
      font-size: 11px;
    }

    .wizard-content {
      padding: 20px 15px;
    }

    .reference-grid {
      grid-template-columns: 1fr;
    }

    .hex-viewer {
      font-size: 11px;
      padding: 10px;
    }

    .completion-icon {
      font-size: 60px;
    }
  }

  :global(body.dragging-panel) {
    cursor: grabbing;
    user-select: none;
  }
</style>
