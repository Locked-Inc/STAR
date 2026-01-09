<script>
  import { onMount, onDestroy } from 'svelte';
  import { invoke } from '@tauri-apps/api/core';
  import { save } from '@tauri-apps/plugin-dialog';
  import { writeTextFile } from '@tauri-apps/plugin-fs';

  // State
  let activeTab = 'raw'; // 'raw' | 'parsed' | 'console'
  let rawPackets = [];
  let parsedPackets = [];
  let autoScroll = true;
  let captureEnabled = true;
  let pollInterval;
  let scrollContainer;
  let lastRawCount = 0;
  let lastParsedCount = 0;

  // Console state
  let consoleOutput = [];
  let consoleInput = '';
  let commandHistory = [];
  let historyIndex = -1;
  let consoleScrollContainer;
  let consoleAutoScroll = true;
  let consoleLineNumber = 0;

  // RAW view enhanced state
  let rawFilter = 'all'; // 'all' | 'tx' | 'rx'
  let rawSearchQuery = '';
  let selectedPacketIndices = new Set();
  let lastRawSelectedIndex = null;
  let expandedPacketIndices = new Set();
  let rawListContainer;
  let rawScrollTop = 0;
  let rawWindowStart = 0;
  let rawWindowEnd = 0;

  // PARSED view state
  let parsedFilter = 'all'; // 'all' | 'tx' | 'rx'
  let parsedSearchQuery = '';
  let selectedParsedKeys = new Set();
  let lastParsedSelectedIndex = null;
  let expandedParsedKeys = new Set();
  let parsedListContainer;
  let parsedScrollTop = 0;
  let parsedWindowStart = 0;
  let parsedWindowEnd = 0;
  let expandedFields = {}; // Track which JSON fields are expanded

  const RAW_ROW_HEIGHT = 36;
  const PARSED_ROW_HEIGHT = 34;
  const VIRTUAL_OVERSCAN = 8;
  const isTauri = typeof window !== 'undefined' && '__TAURI_INTERNALS__' in window;

  // Filter packets based on direction and search query
  $: filteredRawPackets = rawPackets.filter(packet => {
    const direction = (packet.direction || '').toLowerCase();
    if (rawFilter === 'tx') return direction === 'tx';
    if (rawFilter === 'rx') return direction === 'rx';
    return true;
  }).filter(packet => {
    if (!rawSearchQuery) return true;
    const hexString = formatHex(packet.data).toLowerCase();
    const search = rawSearchQuery.toLowerCase().replace(/\s+/g, '');
    return hexString.replace(/\s+/g, '').includes(search);
  });

  // Filter parsed packets based on direction and search query
  $: filteredParsedPackets = parsedPackets.filter(packet => {
    const direction = (packet.direction || '').toLowerCase();
    if (parsedFilter === 'tx') return direction === 'tx';
    if (parsedFilter === 'rx') return direction === 'rx';
    return true;
  }).filter(packet => {
    if (!parsedSearchQuery) return true;
    const searchLower = parsedSearchQuery.toLowerCase();
    const payloadType = packet.payload_type.toLowerCase();
    const fieldsStr = JSON.stringify(packet.fields).toLowerCase();
    return payloadType.includes(searchLower) || fieldsStr.includes(searchLower);
  });

  // Load settings from localStorage
  onMount(() => {
    const savedAutoScroll = localStorage.getItem('bottomPanelAutoScroll');
    if (savedAutoScroll !== null) autoScroll = savedAutoScroll === 'true';

    invoke('set_packet_capture_enabled', { enabled: captureEnabled })
      .catch((error) => console.error('Failed to sync packet capture state:', error));

    fetchPackets();
    // Start polling for packets
    pollInterval = setInterval(fetchPackets, 100);

    const refreshNow = () => {
      fetchPackets();
    };
    window.addEventListener('bms:connected', refreshNow);
    window.addEventListener('bms:packets', refreshNow);
    return () => {
      window.removeEventListener('bms:connected', refreshNow);
      window.removeEventListener('bms:packets', refreshNow);
    };
  });

  onDestroy(() => {
    if (pollInterval) clearInterval(pollInterval);
  });

  async function fetchPackets() {
    try {
      const counts = await invoke('get_packet_count');
      const rawCount = Array.isArray(counts) ? counts[0] : (counts?.raw ?? counts?.[0] ?? 0);
      const parsedCount = Array.isArray(counts) ? counts[1] : (counts?.parsed ?? counts?.[1] ?? 0);

      if (rawCount < rawPackets.length) {
        rawPackets = await invoke('get_raw_packets', { limit: rawCount, offset: 0 });
      } else if (rawCount > rawPackets.length) {
        const delta = rawCount - rawPackets.length;
        const newRaw = await invoke('get_raw_packets', { limit: delta, offset: rawPackets.length });
        rawPackets = [...rawPackets, ...newRaw];
      }

      if (parsedCount < parsedPackets.length) {
        parsedPackets = await invoke('get_parsed_packets', { limit: parsedCount, offset: 0 });
      } else if (parsedCount > parsedPackets.length) {
        const delta = parsedCount - parsedPackets.length;
        const newParsed = await invoke('get_parsed_packets', { limit: delta, offset: parsedPackets.length });
        parsedPackets = [...parsedPackets, ...newParsed];
      }

      lastRawCount = rawCount;
      lastParsedCount = parsedCount;

      if (rawListContainer) {
        if (autoScroll && activeTab === 'raw') {
          rawListContainer.scrollTop = rawListContainer.scrollHeight;
        }
        rawScrollTop = rawListContainer.scrollTop;
      }
      if (parsedListContainer) {
        if (autoScroll && activeTab === 'parsed') {
          parsedListContainer.scrollTop = parsedListContainer.scrollHeight;
        }
        parsedScrollTop = parsedListContainer.scrollTop;
      }
    } catch (error) {
      console.error('Failed to fetch packets:', error);
    }
  }

  async function handleClear() {
    try {
      await invoke('clear_packet_capture');
      rawPackets = [];
      parsedPackets = [];
      selectedPacketIndices = new Set();
      selectedParsedKeys = new Set();
      expandedPacketIndices = new Set();
      expandedParsedKeys = new Set();
      expandedFields = {};
    } catch (error) {
      console.error('Failed to clear packets:', error);
    }
  }

  async function toggleCapture() {
    captureEnabled = !captureEnabled;
    try {
      await invoke('set_packet_capture_enabled', { enabled: captureEnabled });
      if (!captureEnabled) {
        autoScroll = false;
        localStorage.setItem('bottomPanelAutoScroll', 'false');
      }
    } catch (error) {
      console.error('Failed to toggle capture:', error);
    }
  }


  function handleTabChange(tab) {
    activeTab = tab;
    selectedPacketIndices = new Set();
    selectedParsedKeys = new Set();
    expandedPacketIndices = new Set();
    expandedParsedKeys = new Set();
    expandedFields = {};
    fetchPackets();
  }

  function toggleAutoScroll() {
    autoScroll = !autoScroll;
    localStorage.setItem('bottomPanelAutoScroll', autoScroll.toString());
  }

  function formatTimestamp(timestampUs) {
    const date = new Date(timestampUs / 1000);
    return date.toISOString().substr(11, 12);
  }

  function formatHex(data) {
    return data.map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
  }

  function formatHexDump(data) {
    const rows = [];
    for (let i = 0; i < data.length; i += 16) {
      const chunk = data.slice(i, i + 16);
      const offset = i.toString(16).padStart(4, '0').toUpperCase();

      // Hex bytes with spacing every 8 bytes
      const hex1 = chunk.slice(0, 8).map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
      const hex2 = chunk.slice(8, 16).map(b => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
      const hexLine = hex1 + (chunk.length > 8 ? '  ' + hex2 : '');

      // ASCII representation
      const ascii = chunk.map(b => (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.').join('');

      rows.push({ offset, hex: hexLine, ascii });
    }
    return rows;
  }

  function handleRawScroll() {
    if (!rawListContainer) return;
    rawScrollTop = rawListContainer.scrollTop;
  }

  function handleParsedScroll() {
    if (!parsedListContainer) return;
    parsedScrollTop = parsedListContainer.scrollTop;
  }

  function computeWindow(scrollTop, containerHeight, rowHeight, total) {
    const start = Math.max(0, Math.floor(scrollTop / rowHeight) - VIRTUAL_OVERSCAN);
    const visibleCount = Math.ceil(containerHeight / rowHeight) + VIRTUAL_OVERSCAN * 2;
    const end = Math.min(total, start + visibleCount);
    return { start, end };
  }

  $: rawVirtualEnabled = expandedPacketIndices.size === 0 && !!rawListContainer && rawListContainer.clientHeight > 0;
  $: parsedVirtualEnabled = expandedParsedKeys.size === 0 && !!parsedListContainer && parsedListContainer.clientHeight > 0;

  $: if (rawListContainer && rawVirtualEnabled) {
    const { start, end } = computeWindow(rawScrollTop, rawListContainer.clientHeight, RAW_ROW_HEIGHT, filteredRawPackets.length);
    rawWindowStart = start;
    rawWindowEnd = end;
  } else if (!rawVirtualEnabled) {
    rawWindowStart = 0;
    rawWindowEnd = filteredRawPackets.length;
  }

  $: if (parsedListContainer && parsedVirtualEnabled) {
    const { start, end } = computeWindow(parsedScrollTop, parsedListContainer.clientHeight, PARSED_ROW_HEIGHT, filteredParsedPackets.length);
    parsedWindowStart = start;
    parsedWindowEnd = end;
  } else if (!parsedVirtualEnabled) {
    parsedWindowStart = 0;
    parsedWindowEnd = filteredParsedPackets.length;
  }

  $: rawVisiblePackets = rawVirtualEnabled
    ? filteredRawPackets.slice(rawWindowStart, rawWindowEnd)
    : filteredRawPackets;
  $: parsedVisiblePackets = parsedVirtualEnabled
    ? filteredParsedPackets.slice(parsedWindowStart, parsedWindowEnd)
    : filteredParsedPackets;

  function expandAllRaw() {
    expandedPacketIndices = new Set(filteredRawPackets.map((_, index) => index));
  }

  function collapseAllRaw() {
    expandedPacketIndices = new Set();
  }

  function selectAllRaw() {
    selectedPacketIndices = new Set(filteredRawPackets.map((_, index) => index));
  }

  function deselectAllRaw() {
    selectedPacketIndices = new Set();
  }

  function deleteSelectedRaw() {
    if (selectedPacketIndices.size === 0) return;
    const selectedPackets = new Set(
      Array.from(selectedPacketIndices)
        .map((index) => filteredRawPackets[index])
        .filter(Boolean)
    );
    rawPackets = rawPackets.filter((packet) => !selectedPackets.has(packet));
    selectedPacketIndices = new Set();
    expandedPacketIndices = new Set();
  }

  function toggleRawExpanded(index) {
    if (expandedPacketIndices.has(index)) {
      expandedPacketIndices.delete(index);
    } else {
      expandedPacketIndices.add(index);
    }
    expandedPacketIndices = new Set(expandedPacketIndices);
  }

  function toggleRawSelection(index, event) {
    if (event.shiftKey && lastRawSelectedIndex !== null) {
      const start = Math.min(lastRawSelectedIndex, index);
      const end = Math.max(lastRawSelectedIndex, index);
      for (let i = start; i <= end; i += 1) {
        selectedPacketIndices.add(i);
      }
    } else if (selectedPacketIndices.has(index)) {
      selectedPacketIndices.delete(index);
    } else {
      selectedPacketIndices.add(index);
    }
    lastRawSelectedIndex = index;
    selectedPacketIndices = new Set(selectedPacketIndices);
  }

  async function copyPacketData() {
    const selected = selectedPacketIndices.size
      ? Array.from(selectedPacketIndices).sort((a, b) => a - b).map((index) => filteredRawPackets[index]).filter(Boolean)
      : filteredRawPackets;
    if (selected.length === 0) return;
    const hexData = selected.map((packet) => {
      const timestamp = formatTimestamp(packet.timestamp_us);
      const direction = packet.direction;
      const description = packet.description;
      const hexDump = formatHexDump(packet.data);
      let output = `[${timestamp}] ${direction} - ${description}\n`;
      output += `Raw: ${formatHex(packet.data)}\n`;
      output += `Hex Dump:\n`;
      hexDump.forEach(row => {
        output += `  ${row.offset}  ${row.hex.padEnd(49)}  ${row.ascii}\n`;
      });
      return output.trimEnd();
    }).join('\n\n');
    try {
      await navigator.clipboard.writeText(hexData);
    } catch (error) {
      console.error('Failed to copy to clipboard:', error);
    }
  }

  async function exportToFile() {
    const selected = selectedPacketIndices.size
      ? Array.from(selectedPacketIndices).sort((a, b) => a - b).map((index) => filteredRawPackets[index]).filter(Boolean)
      : filteredRawPackets;
    const content = selected.map(packet => {
      const timestamp = formatTimestamp(packet.timestamp_us);
      const direction = packet.direction;
      const description = packet.description;
      const hexData = formatHex(packet.data);
      const hexDump = formatHexDump(packet.data);

      let output = `[${timestamp}] ${direction} - ${description}\n`;
      output += `Raw: ${hexData}\n`;
      output += `Hex Dump:\n`;
      hexDump.forEach(row => {
        output += `  ${row.offset}  ${row.hex.padEnd(49)}  ${row.ascii}\n`;
      });
      output += '\n';
      return output;
    }).join('');

    const fileName = `bms-packets-${Date.now()}.txt`;
    if (isTauri) {
      const filePath = await save({
        defaultPath: fileName,
        filters: [{ name: 'Text', extensions: ['txt'] }],
      });
      if (!filePath) return;
      await writeTextFile(filePath, content);
      return;
    }
    const blob = new Blob([content], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName;
    a.click();
    URL.revokeObjectURL(url);
  }

  // PARSED view helper functions
  function toggleFieldExpansion(packetKey, path) {
    if (path === null || path === undefined) return;
    const key = `${packetKey}::${path}`;
    expandedFields = { ...expandedFields, [key]: !expandedFields[key] };
  }

  function isFieldExpanded(packetKey, path) {
    const key = `${packetKey}::${path}`;
    return expandedFields[key] || false;
  }

  function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  function getParsedKey(packet) {
    const seq = packet.frame_seq ?? 'na';
    const type = packet.payload_type ?? 'unknown';
    return `${packet.timestamp_us}-${seq}-${type}`;
  }

  function toggleParsedSelection(packet, index, event) {
    const key = getParsedKey(packet);
    if (event.shiftKey && lastParsedSelectedIndex !== null && index >= 0) {
      const start = Math.min(lastParsedSelectedIndex, index);
      const end = Math.max(lastParsedSelectedIndex, index);
      for (let i = start; i <= end; i += 1) {
        const candidate = filteredParsedPackets[i];
        if (candidate) {
          selectedParsedKeys.add(getParsedKey(candidate));
        }
      }
    } else if (selectedParsedKeys.has(key)) {
      selectedParsedKeys.delete(key);
    } else {
      selectedParsedKeys.add(key);
    }
    if (index >= 0) {
      lastParsedSelectedIndex = index;
    }
    selectedParsedKeys = new Set(selectedParsedKeys);
    expandedFields = {};
  }

  async function copyParsedData() {
    const selected = selectedParsedKeys.size
      ? filteredParsedPackets.filter((packet) => selectedParsedKeys.has(getParsedKey(packet)))
      : filteredParsedPackets;
    if (selected.length === 0) return;
    const jsonData = JSON.stringify(selected, null, 2);
    try {
      await navigator.clipboard.writeText(jsonData);
    } catch (error) {
      console.error('Failed to copy to clipboard:', error);
    }
  }

  async function exportParsedToFile() {
    const selected = selectedParsedKeys.size
      ? filteredParsedPackets.filter((packet) => selectedParsedKeys.has(getParsedKey(packet)))
      : filteredParsedPackets;
    const content = JSON.stringify(selected, null, 2);
    const fileName = `bms-parsed-packets-${Date.now()}.json`;
    if (isTauri) {
      const filePath = await save({
        defaultPath: fileName,
        filters: [{ name: 'JSON', extensions: ['json'] }],
      });
      if (!filePath) return;
      await writeTextFile(filePath, content);
      return;
    }
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName;
    a.click();
    URL.revokeObjectURL(url);
  }

  function renderJsonTree(obj, packetKey, path = '', expandedState = expandedFields) {
    if (obj === null || obj === undefined) {
      return `<span class="json-null">null</span>`;
    }

    if (typeof obj !== 'object') {
      if (typeof obj === 'string') return `<span class="json-string">"${escapeHtml(obj)}"</span>`;
      if (typeof obj === 'number') return `<span class="json-number">${obj}</span>`;
      if (typeof obj === 'boolean') return `<span class="json-boolean">${obj}</span>`;
      return escapeHtml(String(obj));
    }

    if (Array.isArray(obj)) {
      if (obj.length === 0) return `<span class="json-bracket">[]</span>`;
      const isExpanded = isFieldExpanded(packetKey, path);
      let html = `<span class="json-expandable" data-path="${path}">`;
      html += `<span class="json-toggle">${isExpanded ? '[-]' : '[+]'}</span>`;
      html += `<span class="json-bracket">[</span>`;
      if (isExpanded) {
        html += `<div class="json-children">`;
        obj.forEach((item, index) => {
          const itemPath = `${path}[${index}]`;
          html += `<div class="json-item">`;
          html += `<span class="json-index">${index}:</span> `;
          html += renderJsonTree(item, packetKey, itemPath, expandedState);
          html += `</div>`;
        });
        html += `</div>`;
      } else {
        html += `<span class="json-collapsed">... ${obj.length} items</span>`;
      }
      html += `<span class="json-bracket">]</span>`;
      html += `</span>`;
      return html;
    }

    // Object
    const keys = Object.keys(obj);
    if (keys.length === 0) return `<span class="json-brace">{}</span>`;

    const isExpanded = isFieldExpanded(packetKey, path);
    let html = `<span class="json-expandable" data-path="${path}">`;
    html += `<span class="json-toggle">${isExpanded ? '[-]' : '[+]'}</span>`;
    html += `<span class="json-brace">{</span>`;
    if (isExpanded) {
      html += `<div class="json-children">`;
      keys.forEach(key => {
        const value = obj[key];
        const fieldPath = path ? `${path}.${key}` : key;
        html += `<div class="json-field">`;
        html += `<span class="json-key">"${escapeHtml(key)}"</span>: `;
        html += renderJsonTree(value, packetKey, fieldPath, expandedState);
        html += `</div>`;
      });
      html += `</div>`;
    } else {
      html += `<span class="json-collapsed">... ${keys.length} fields</span>`;
    }
    html += `<span class="json-brace">}</span>`;
    html += `</span>`;
    return html;
  }

  function collectExpandablePaths(obj, path, acc) {
    if (obj === null || obj === undefined) return;
    if (typeof obj !== 'object') return;

    const isArray = Array.isArray(obj);
    const keys = isArray ? obj.map((_, index) => index) : Object.keys(obj);
    if (keys.length === 0) return;

    acc.push(path);

    if (isArray) {
      obj.forEach((item, index) => {
        const itemPath = `${path}[${index}]`;
        collectExpandablePaths(item, itemPath, acc);
      });
      return;
    }

    keys.forEach(key => {
      const value = obj[key];
      const fieldPath = path ? `${path}.${key}` : key;
      collectExpandablePaths(value, fieldPath, acc);
    });
  }

  function expandAllParsed() {
    const targetKeys = filteredParsedPackets.map((packet) => getParsedKey(packet));
    const next = { ...expandedFields };

    expandedParsedKeys = new Set(targetKeys);

    targetKeys.forEach(key => {
      const packet = filteredParsedPackets.find((candidate) => getParsedKey(candidate) === key);
      if (!packet) return;
      const paths = [];
      collectExpandablePaths(packet.fields, '', paths);
      paths.forEach(path => {
        next[`${key}::${path}`] = true;
      });
    });

    expandedFields = next;
  }

  function collapseAllParsed() {
    expandedParsedKeys = new Set();
    expandedFields = {};
  }

  function selectAllParsed() {
    selectedParsedKeys = new Set(filteredParsedPackets.map((packet) => getParsedKey(packet)));
  }

  function deselectAllParsed() {
    selectedParsedKeys = new Set();
  }

  function deleteSelectedParsed() {
    if (selectedParsedKeys.size === 0) return;
    parsedPackets = parsedPackets.filter((packet) => !selectedParsedKeys.has(getParsedKey(packet)));
    selectedParsedKeys = new Set();
    expandedParsedKeys = new Set();
    expandedFields = {};
  }

  function toggleParsedExpanded(packetKey) {
    if (expandedParsedKeys.has(packetKey)) {
      expandedParsedKeys.delete(packetKey);
    } else {
      expandedParsedKeys.add(packetKey);
    }
    expandedParsedKeys = new Set(expandedParsedKeys);
  }

  // Console helper functions
  function addConsoleOutput(type, text) {
    const timestamp = formatConsoleTimestamp();
    consoleLineNumber += 1;
    consoleOutput = [...consoleOutput, { line: consoleLineNumber, timestamp, type, text }];

    if (consoleAutoScroll && consoleScrollContainer) {
      setTimeout(() => {
        consoleScrollContainer.scrollTop = consoleScrollContainer.scrollHeight;
      }, 0);
    }
  }

  async function executeCommand(cmd) {
    if (!cmd.trim()) return;

    // Add to output
    addConsoleOutput('command', cmd);

    // Add to history
    commandHistory = [...commandHistory, cmd];
    historyIndex = -1;

    // Parse command
    const parts = cmd.trim().split(/\s+/);
    const command = parts[0].toLowerCase();
    const args = parts.slice(1);

    try {
      switch (command) {
        case 'help':
          addConsoleOutput('info', 'Available commands:');
          addConsoleOutput('info', '  help                          - Show this help message');
          addConsoleOutput('info', '  clear                         - Clear console output');
          addConsoleOutput('info', '  connect <port>                - Connect to device on port');
          addConsoleOutput('info', '  disconnect                    - Disconnect from device');
          addConsoleOutput('info', '  status                        - Show connection status');
          addConsoleOutput('info', '  telemetry                     - Read telemetry data');
          addConsoleOutput('info', '  cells [count]                 - Read cell voltages');
          addConsoleOutput('info', '  info                          - Read device info');
          addConsoleOutput('info', '  read <address> [bytes]        - Read register (hex address)');
          addConsoleOutput('info', '  write <address> <value>       - Write register (hex address and value)');
          break;

        case 'clear':
          consoleOutput = [];
          consoleLineNumber = 0;
          addConsoleOutput('success', 'Console cleared');
          break;

        case 'connect':
          if (args.length === 0) {
            addConsoleOutput('error', 'Usage: connect <port>');
            break;
          }
          addConsoleOutput('info', `Connecting to ${args[0]}...`);
          try {
            await invoke('connect_to_device', { portName: args[0] });
            const deviceState = await invoke('get_device_state');
            addConsoleOutput('success', `Connected to ${deviceState.manufacturer} ${deviceState.device_name}`);
            addConsoleOutput('info', `Device has ${deviceState.num_cells} cells`);
          } catch (error) {
            addConsoleOutput('error', `Connection failed: ${error}`);
          }
          break;

        case 'disconnect':
          addConsoleOutput('info', 'Disconnecting...');
          try {
            await invoke('disconnect_from_device');
            addConsoleOutput('success', 'Disconnected');
          } catch (error) {
            addConsoleOutput('error', `Disconnect failed: ${error}`);
          }
          break;

        case 'status':
          try {
            const isConnected = await invoke('is_connected');
            if (isConnected) {
              const deviceState = await invoke('get_device_state');
              addConsoleOutput('success', 'Device is connected');
              addConsoleOutput('info', `  Manufacturer: ${deviceState.manufacturer}`);
              addConsoleOutput('info', `  Device: ${deviceState.device_name}`);
              addConsoleOutput('info', `  Chemistry: ${deviceState.chemistry}`);
              addConsoleOutput('info', `  Cells: ${deviceState.num_cells}`);
            } else {
              addConsoleOutput('error', 'Not connected');
            }
          } catch (error) {
            addConsoleOutput('error', 'Not connected');
          }
          break;

        case 'telemetry':
          addConsoleOutput('info', 'Reading telemetry...');
          try {
            const telemetry = await invoke('read_telemetry');
            addConsoleOutput('success', 'Telemetry data:');
            addConsoleOutput('info', `  Voltage: ${telemetry.voltage_mv} mV`);
            addConsoleOutput('info', `  Current: ${telemetry.current_ma} mA`);
            addConsoleOutput('info', `  Temperature: ${telemetry.temperature_celsius} C`);
            addConsoleOutput('info', `  State of Charge: ${telemetry.relative_soc_percent}%`);
            addConsoleOutput('info', `  Remaining Capacity: ${telemetry.remaining_capacity_mah} mAh`);
            addConsoleOutput('info', `  Cycle Count: ${telemetry.cycle_count}`);
          } catch (error) {
            addConsoleOutput('error', `Failed to read telemetry: ${error}`);
          }
          break;

        case 'cells':
          const numCells = args.length > 0 ? parseInt(args[0]) : 4;
          addConsoleOutput('info', `Reading ${numCells} cell voltages...`);
          try {
            const cells = await invoke('read_cell_voltages', { numCells });
            addConsoleOutput('success', 'Cell voltages:');
            cells.voltages_mv.forEach((voltage, index) => {
              addConsoleOutput('info', `  Cell ${index + 1}: ${voltage} mV`);
            });
          } catch (error) {
            addConsoleOutput('error', `Failed to read cells: ${error}`);
          }
          break;

        case 'info':
          addConsoleOutput('info', 'Reading device info...');
          try {
            const info = await invoke('read_device_info');
            addConsoleOutput('success', 'Device information:');
            addConsoleOutput('info', `  Manufacturer: ${info.manufacturer}`);
            addConsoleOutput('info', `  Device Name: ${info.device_name}`);
            addConsoleOutput('info', `  Chemistry: ${info.chemistry}`);
            addConsoleOutput('info', `  Serial Number: 0x${info.serial_number.toString(16).toUpperCase()}`);
            addConsoleOutput('info', `  Firmware Version: ${info.firmware_version}`);
            addConsoleOutput('info', `  Hardware Version: ${info.hardware_version}`);
            addConsoleOutput('info', `  Design Capacity: ${info.design_capacity_mah} mAh`);
            addConsoleOutput('info', `  Design Voltage: ${info.design_voltage_mv} mV`);
            addConsoleOutput('info', `  Number of Cells: ${info.num_cells}`);
          } catch (error) {
            addConsoleOutput('error', `Failed to read device info: ${error}`);
          }
          break;

        case 'read':
          if (args.length === 0) {
            addConsoleOutput('error', 'Usage: read <address> [bytes]');
            break;
          }
          const readAddr = parseInt(args[0], 16);
          const readBytes = args.length > 1 ? parseInt(args[1]) : 2;
          if (isNaN(readAddr)) {
            addConsoleOutput('error', 'Invalid address. Use hex format (e.g., 0x09 or 09)');
            break;
          }
          addConsoleOutput('info', `Reading ${readBytes} byte(s) from 0x${readAddr.toString(16).toUpperCase()}...`);
          try {
            const data = await invoke('read_register', { address: readAddr, numBytes: readBytes });
            addConsoleOutput('success', `Read 0x${readAddr.toString(16).toUpperCase()}: ${data.value}`);
            addConsoleOutput('info', `  Raw bytes: ${data.data.map(b => '0x' + b.toString(16).padStart(2, '0').toUpperCase()).join(' ')}`);
          } catch (error) {
            addConsoleOutput('error', `Failed to read register: ${error}`);
          }
          break;

        case 'write':
          if (args.length < 2) {
            addConsoleOutput('error', 'Usage: write <address> <value>');
            break;
          }
          const writeAddr = parseInt(args[0], 16);
          const writeValue = parseInt(args[1], 16);
          if (isNaN(writeAddr) || isNaN(writeValue)) {
            addConsoleOutput('error', 'Invalid address or value. Use hex format (e.g., 0x10 0xFF)');
            break;
          }
          const numBytes = args.length > 2 ? parseInt(args[2]) : 2;
          addConsoleOutput('info', `Writing 0x${writeValue.toString(16).toUpperCase()} to 0x${writeAddr.toString(16).toUpperCase()}...`);
          try {
            await invoke('write_register', { address: writeAddr, value: writeValue, numBytes });
            addConsoleOutput('success', `Wrote 0x${writeValue.toString(16).toUpperCase()} to 0x${writeAddr.toString(16).toUpperCase()}`);
          } catch (error) {
            addConsoleOutput('error', `Failed to write register: ${error}`);
          }
          break;

        default:
          addConsoleOutput('error', `Unknown command: ${command}`);
          addConsoleOutput('info', 'Type "help" for available commands');
      }
    } catch (error) {
      addConsoleOutput('error', `Command failed: ${error}`);
    }

    consoleInput = '';
  }

  function handleConsoleKeydown(event) {
    if (event.key === 'Enter') {
      executeCommand(consoleInput);
    } else if (event.key === 'ArrowUp') {
      event.preventDefault();
      if (commandHistory.length === 0) return;

      if (historyIndex === -1) {
        historyIndex = commandHistory.length - 1;
      } else if (historyIndex > 0) {
        historyIndex--;
      }
      consoleInput = commandHistory[historyIndex];
    } else if (event.key === 'ArrowDown') {
      event.preventDefault();
      if (historyIndex === -1) return;

      if (historyIndex < commandHistory.length - 1) {
        historyIndex++;
        consoleInput = commandHistory[historyIndex];
      } else {
        historyIndex = -1;
        consoleInput = '';
      }
    } else if (event.key === 'Tab') {
      event.preventDefault();
      // Simple auto-complete for command names
      const commands = ['help', 'clear', 'connect', 'disconnect', 'status', 'telemetry', 'cells', 'info', 'read', 'write'];
      const partial = consoleInput.trim().toLowerCase();
      const matches = commands.filter(cmd => cmd.startsWith(partial));
      if (matches.length === 1) {
        consoleInput = matches[0];
      } else if (matches.length > 1) {
        addConsoleOutput('info', `Possible completions: ${matches.join(', ')}`);
      }
    }
  }

  function clearConsole() {
    consoleOutput = [];
    consoleLineNumber = 0;
    addConsoleOutput('success', 'Console cleared');
  }

  function formatConsoleLine(line) {
    return `[${line.line}] [${line.timestamp}] ${line.text}`;
  }

  function formatConsoleTimestamp(date = new Date()) {
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    const seconds = String(date.getSeconds()).padStart(2, '0');
    const millis = String(date.getMilliseconds()).padStart(3, '0');
    return `${hours}:${minutes}:${seconds}.${millis}`;
  }

  async function copyConsole() {
    const text = consoleOutput.map((line) => formatConsoleLine(line)).join('\n');
    try {
      await navigator.clipboard.writeText(text);
      addConsoleOutput('success', 'Console copied to clipboard');
    } catch (error) {
      addConsoleOutput('error', 'Failed to copy to clipboard');
    }
  }

  function exportConsole() {
    const text = consoleOutput.map((line) => formatConsoleLine(line)).join('\n');
    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `bms-console-${Date.now()}.txt`;
    a.click();
    URL.revokeObjectURL(url);
    addConsoleOutput('success', 'Console exported to file');
  }

  function toggleConsoleAutoScroll() {
    consoleAutoScroll = !consoleAutoScroll;
  }
</script>

<div class="packet-viewer-panel">
    <div class="panel-header">
      <div class="tabs" role="tablist" aria-label="Packet viewer tabs">
        <button
          class="tab"
          class:active={activeTab === 'raw'}
          on:click={() => handleTabChange('raw')}
          role="tab"
          aria-selected={activeTab === 'raw'}
        >
          RAW
        </button>
        <button
          class="tab"
          class:active={activeTab === 'parsed'}
          on:click={() => handleTabChange('parsed')}
          role="tab"
          aria-selected={activeTab === 'parsed'}
        >
          PARSED
        </button>
        <button
          class="tab"
          class:active={activeTab === 'console'}
          on:click={() => handleTabChange('console')}
          role="tab"
          aria-selected={activeTab === 'console'}
        >
          CONSOLE
        </button>
      </div>

      <div class="controls">
        <button class="control-btn" on:click={handleClear} title="Clear packets">
          Clear
        </button>
        <button
          class="control-btn"
          class:active={autoScroll}
          on:click={toggleAutoScroll}
          title="Toggle auto-scroll"
        >
          Auto-scroll
        </button>
        <button
          class="control-btn"
          class:active={captureEnabled}
          on:click={toggleCapture}
          title="Toggle packet capture"
        >
          {captureEnabled ? 'Capturing' : 'Paused'}
        </button>
      </div>
    </div>

    <div class="panel-content" bind:this={scrollContainer}>
      {#if activeTab === 'raw'}
        <div class="raw-view-enhanced">
          <div class="raw-toolbar">
            <div class="filter-group">
              <span class="label">Filter:</span>
              <button
                class="filter-btn"
                class:active={rawFilter === 'all'}
                on:click={() => rawFilter = 'all'}
              >
                All
              </button>
              <button
                class="filter-btn tx"
                class:active={rawFilter === 'tx'}
                on:click={() => rawFilter = 'tx'}
              >
                TX
              </button>
              <button
                class="filter-btn rx"
                class:active={rawFilter === 'rx'}
                on:click={() => rawFilter = 'rx'}
              >
                RX
              </button>
            </div>

            <div class="search-group">
              <input
                type="text"
                placeholder="Search hex (e.g., FF 00)"
                bind:value={rawSearchQuery}
                class="search-input"
              />
            </div>

            <div class="action-group">
              <button
                class="action-btn"
                on:click={copyPacketData}
                disabled={selectedPacketIndices.size === 0}
                title="Copy selected packet"
              >
                Copy
              </button>
              <button
                class="action-btn"
                on:click={exportToFile}
                title="Export to file"
              >
                Export
              </button>
              <button
                class="action-btn"
                on:click={selectAllRaw}
                title="Select all packets"
              >
                Select all
              </button>
              <button
                class="action-btn"
                on:click={deselectAllRaw}
                title="Deselect all packets"
              >
                Deselect all
              </button>
              <button
                class="action-btn danger"
                on:click={deleteSelectedRaw}
                disabled={selectedPacketIndices.size === 0}
                title="Delete selected packets"
              >
                Delete
              </button>
              <button
                class="action-btn"
                on:click={expandAllRaw}
                title="Expand all packets"
              >
                Expand all
              </button>
              <button
                class="action-btn"
                on:click={collapseAllRaw}
                title="Collapse all packets"
              >
                Collapse all
              </button>
            </div>
          </div>

          <div class="packet-list" bind:this={rawListContainer} on:scroll={handleRawScroll}>
            {#if rawVirtualEnabled}
              <div class="virtual-spacer" style={`height: ${rawWindowStart * RAW_ROW_HEIGHT}px`}></div>
            {/if}
            {#each rawVisiblePackets as packet, index}
              {@const packetIndex = rawVirtualEnabled ? rawWindowStart + index : index}
              <div
                class="packet-item {packet.direction.toLowerCase()}"
                class:selected={selectedPacketIndices.has(packetIndex)}
                on:click={() => toggleRawExpanded(packetIndex)}
                role="button"
                tabindex="0"
                on:keydown={(e) => e.key === 'Enter' && toggleRawExpanded(packetIndex)}
              >
                <div class="packet-header">
                  <input
                    class="packet-select"
                    type="checkbox"
                    checked={selectedPacketIndices.has(packetIndex)}
                    on:click|stopPropagation={(event) => toggleRawSelection(packetIndex, event)}
                    aria-label="Select packet"
                  />
                  <span class="packet-timestamp">{formatTimestamp(packet.timestamp_us)}</span>
                  <span class="packet-direction {packet.direction.toLowerCase()}">{packet.direction}</span>
                  <span class="packet-description">{packet.description}</span>
                  <span class="packet-length">{packet.data.length} bytes</span>
                </div>

                {#if expandedPacketIndices.has(packetIndex)}
                  <div class="packet-details">
                    <div class="hex-dump">
                      {#each formatHexDump(packet.data) as row}
                        <div class="hex-row">
                          <span class="hex-offset">{row.offset}</span>
                          <span class="hex-bytes">{row.hex}</span>
                          <span class="hex-ascii">{row.ascii}</span>
                        </div>
                      {/each}
                    </div>
                  </div>
                {/if}
              </div>
            {/each}
            {#if rawVirtualEnabled}
              <div class="virtual-spacer" style={`height: ${(filteredRawPackets.length - rawWindowEnd) * RAW_ROW_HEIGHT}px`}></div>
            {/if}

            {#if filteredRawPackets.length === 0}
              <div class="empty-state">
                <p>No packets captured yet</p>
                <p class="empty-hint">Packets will appear here during connection attempts</p>
              </div>
            {/if}
          </div>
        </div>
      {:else if activeTab === 'parsed'}
        <div class="parsed-view-enhanced">
          <div class="parsed-toolbar">
            <div class="filter-group">
              <span class="label">Filter:</span>
              <button
                class="filter-btn"
                class:active={parsedFilter === 'all'}
                on:click={() => parsedFilter = 'all'}
              >
                All
              </button>
              <button
                class="filter-btn tx"
                class:active={parsedFilter === 'tx'}
                on:click={() => parsedFilter = 'tx'}
              >
                TX
              </button>
              <button
                class="filter-btn rx"
                class:active={parsedFilter === 'rx'}
                on:click={() => parsedFilter = 'rx'}
              >
                RX
              </button>
            </div>

            <div class="search-group">
              <input
                type="text"
                placeholder="Search fields and values"
                bind:value={parsedSearchQuery}
                class="search-input"
              />
            </div>

            <div class="action-group">
              <button
                class="action-btn"
                on:click={copyParsedData}
                disabled={selectedParsedKeys.size === 0}
                title="Copy selected packet JSON"
              >
                Copy
              </button>
              <button
                class="action-btn"
                on:click={exportParsedToFile}
                title="Export to JSON file"
              >
                Export
              </button>
              <button
                class="action-btn"
                on:click={selectAllParsed}
                title="Select all packets"
              >
                Select all
              </button>
              <button
                class="action-btn"
                on:click={deselectAllParsed}
                title="Deselect all packets"
              >
                Deselect all
              </button>
              <button
                class="action-btn danger"
                on:click={deleteSelectedParsed}
                disabled={selectedParsedKeys.size === 0}
                title="Delete selected packets"
              >
                Delete
              </button>
              <button
                class="action-btn"
                on:click={expandAllParsed}
                title="Expand all fields"
              >
                Expand all
              </button>
              <button
                class="action-btn"
                on:click={collapseAllParsed}
                title="Collapse all fields"
              >
                Collapse all
              </button>
            </div>
          </div>

          <div class="parsed-list" bind:this={parsedListContainer} on:scroll={handleParsedScroll}>
            {#if parsedVirtualEnabled}
              <div class="virtual-spacer" style={`height: ${parsedWindowStart * PARSED_ROW_HEIGHT}px`}></div>
            {/if}
            {#each parsedVisiblePackets as packet, index}
              {@const packetKey = getParsedKey(packet)}
              <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
              <div
                class="parsed-item {packet.direction.toLowerCase()}"
                class:selected={selectedParsedKeys.has(packetKey)}
                on:click={() => toggleParsedExpanded(packetKey)}
                role="button"
                tabindex="0"
                on:keydown={(e) => e.key === 'Enter' && toggleParsedExpanded(packetKey)}
              >
                <div class="parsed-header">
                  <input
                    class="packet-select"
                    type="checkbox"
                    checked={selectedParsedKeys.has(packetKey)}
                    on:click|stopPropagation={(event) => toggleParsedSelection(packet, parsedVirtualEnabled ? parsedWindowStart + index : index, event)}
                    aria-label="Select parsed packet"
                  />
                  <span class="parsed-timestamp">{formatTimestamp(packet.timestamp_us)}</span>
                  <span class="parsed-direction {packet.direction.toLowerCase()}">{packet.direction}</span>
                  <span class="parsed-seq">Seq: {packet.frame_seq}</span>
                  <span class="parsed-type">{packet.payload_type}</span>
                </div>

                {#if expandedParsedKeys.has(packetKey)}
                  <div class="parsed-details">
                    <!-- svelte-ignore a11y_click_events_have_key_events a11y_no_static_element_interactions -->
                    <div class="json-viewer" on:click={(e) => {
                      const target = e.target instanceof Element ? e.target : e.target.parentElement;
                      if (!target) return;
                      const expandable = target.closest('.json-expandable');
                      if (!expandable) return;
                      const path = expandable.getAttribute('data-path');
                      toggleFieldExpansion(packetKey, path);
                      e.stopPropagation();
                    }}>
                      {@html renderJsonTree(packet.fields, packetKey, '', expandedFields)}
                    </div>
                  </div>
                {/if}
              </div>
            {/each}
            {#if parsedVirtualEnabled}
              <div class="virtual-spacer" style={`height: ${(filteredParsedPackets.length - parsedWindowEnd) * PARSED_ROW_HEIGHT}px`}></div>
            {/if}

            {#if filteredParsedPackets.length === 0}
              <div class="empty-state">
                <p>No parsed packets captured yet</p>
                <p class="empty-hint">Packets will appear here during connection attempts</p>
              </div>
            {/if}
          </div>
        </div>
      {:else if activeTab === 'console'}
        <div class="console-view">
          <div class="console-toolbar">
            <div class="console-info">
              <span class="console-prompt-indicator">$</span>
              <span class="console-hint">Type "help" for commands</span>
            </div>

            <div class="console-controls">
              <button
                class="control-btn"
                class:active={consoleAutoScroll}
                on:click={toggleConsoleAutoScroll}
                title="Toggle auto-scroll"
              >
                Auto-scroll
              </button>
              <button class="control-btn" on:click={clearConsole} title="Clear console">
                Clear
              </button>
              <button class="control-btn" on:click={copyConsole} title="Copy to clipboard">
                Copy
              </button>
              <button class="control-btn" on:click={exportConsole} title="Export to file">
                Export
              </button>
            </div>
          </div>

          <div class="console-output" bind:this={consoleScrollContainer}>
            {#each consoleOutput as line}
              <div class="console-line {line.type}">
                <span class="console-line-number">{line.line}</span>
                <span class="console-text">{line.text}</span>
              </div>
            {/each}
            {#if consoleOutput.length === 0}
              <div class="console-welcome">
                <p>BMS Tool Console</p>
                <p class="console-welcome-hint">Type "help" for available commands</p>
              </div>
            {/if}
          </div>

          <div class="console-input-container">
            <span class="console-prompt-indicator">$</span>
            <input
              type="text"
              class="console-input"
              bind:value={consoleInput}
              on:keydown={handleConsoleKeydown}
              placeholder="Enter command..."
              autocomplete="off"
            />
          </div>
        </div>
      {/if}
    </div>
</div>

<style>
  .packet-viewer-panel {
    display: flex;
    flex-direction: column;
    height: 100%;
    width: 100%;
    background: var(--bg-primary, #FFFFFF);
    overflow: hidden;
  }

  .panel-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 8px 16px;
    background: var(--bg-secondary, #F8F9FA);
    border-bottom: 1px solid var(--border-light, #E0E0E0);
  }

  .tabs {
    display: flex;
    gap: 4px;
  }

  .tab {
    padding: 6px 16px;
    background: transparent;
    border: none;
    font-size: 13px;
    font-weight: 500;
    color: var(--text-secondary, #6B7280);
    cursor: pointer;
    border-radius: 4px;
    transition: all 0.15s ease;
  }

  .tab:hover {
    background: var(--bg-hover, #E5E7EB);
  }

  .tab.active {
    background: var(--bg-primary, #FFFFFF);
    color: var(--accent-primary, #2563EB);
    box-shadow: 0 1px 3px rgba(0,0,0,0.1);
  }

  .controls {
    display: flex;
    gap: 8px;
  }

  .control-btn {
    padding: 6px 12px;
    background: var(--bg-primary, #FFFFFF);
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    font-size: 12px;
    font-weight: 500;
    color: var(--text-primary, #1F2937);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .control-btn:hover {
    background: var(--bg-hover, #F3F4F6);
    border-color: var(--border-medium, #D1D5DB);
  }

  .control-btn.active {
    background: var(--accent-primary, #2563EB);
    color: white;
    border-color: var(--accent-primary, #2563EB);
  }

  .panel-content {
    flex: 1;
    overflow: hidden;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 12px;
    background: var(--bg-primary, #FFFFFF);
  }

  /* RAW View Enhanced Styles */
  .raw-view-enhanced {
    display: flex;
    flex-direction: column;
    height: 100%;
  }

  .raw-toolbar {
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 8px 12px;
    background: var(--bg-secondary, #F8F9FA);
    border-bottom: 1px solid var(--border-light, #E0E0E0);
    flex-wrap: wrap;
  }

  .filter-group,
  .search-group,
  .action-group {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  .filter-group .label {
    font-size: 12px;
    font-weight: 600;
    color: var(--text-secondary, #6B7280);
  }

  .filter-btn {
    padding: 4px 12px;
    background: var(--bg-primary, #FFFFFF);
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    font-size: 11px;
    font-weight: 600;
    color: var(--text-secondary, #6B7280);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .filter-btn:hover {
    background: var(--bg-hover, #F3F4F6);
    border-color: var(--border-medium, #D1D5DB);
  }

  .filter-btn.active {
    background: var(--accent-primary, #2563EB);
    color: white;
    border-color: var(--accent-primary, #2563EB);
  }

  .filter-btn.tx.active {
    background: var(--accent-success, #10B981);
    border-color: var(--accent-success, #10B981);
  }

  .filter-btn.rx.active {
    background: var(--accent-primary, #2563EB);
    border-color: var(--accent-primary, #2563EB);
  }

  .search-input {
    padding: 6px 12px;
    background: var(--bg-primary, #FFFFFF);
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    font-size: 12px;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    width: 200px;
    transition: border-color 0.15s ease;
  }

  .search-input:focus {
    outline: none;
    border-color: var(--accent-primary, #2563EB);
  }

  .action-btn {
    padding: 6px 12px;
    background: var(--bg-primary, #FFFFFF);
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    font-size: 12px;
    font-weight: 500;
    color: var(--text-primary, #1F2937);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .action-btn:hover:not(:disabled) {
    background: var(--bg-hover, #F3F4F6);
    border-color: var(--border-medium, #D1D5DB);
  }

  .action-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .action-btn.danger {
    border-color: #ef4444;
    color: #ef4444;
  }

  .action-btn.danger:hover:not(:disabled) {
    background: #fee2e2;
    border-color: #dc2626;
    color: #dc2626;
  }

  .packet-list {
    flex: 1;
    overflow-y: auto;
    padding: 8px;
  }

  .packet-item {
    margin-bottom: 8px;
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    background: var(--bg-primary, #FFFFFF);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .packet-item:hover {
    border-color: var(--border-medium, #D1D5DB);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
  }

  .packet-item.selected {
    border-color: var(--accent-primary, #2563EB);
    box-shadow: 0 2px 4px rgba(37, 99, 235, 0.2);
  }

  .packet-item.tx {
    border-left: 3px solid var(--accent-success, #10B981);
  }

  .packet-item.rx {
    border-left: 3px solid var(--accent-primary, #2563EB);
  }

  .packet-header {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 8px 12px;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 11px;
  }

  .packet-select {
    width: 14px;
    height: 14px;
    margin: 0;
    flex: 0 0 auto;
  }

  .packet-timestamp {
    color: var(--text-secondary, #6B7280);
    min-width: 100px;
  }

  .packet-direction {
    font-weight: 700;
    min-width: 30px;
  }

  .packet-direction.tx {
    color: var(--accent-success, #10B981);
  }

  .packet-direction.rx {
    color: var(--accent-primary, #2563EB);
  }

  .packet-description {
    flex: 1;
    color: var(--text-secondary, #6B7280);
  }

  .packet-length {
    color: var(--text-tertiary, #9CA3AF);
    font-size: 10px;
  }

  .packet-details {
    padding: 12px;
    background: var(--bg-secondary, #F8F9FA);
    border-top: 1px solid var(--border-light, #E0E0E0);
  }

  .hex-dump {
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 11px;
    line-height: 1.6;
  }

  .hex-row {
    display: flex;
    gap: 12px;
    color: var(--text-primary, #1F2937);
  }

  .hex-offset {
    color: var(--text-secondary, #6B7280);
    font-weight: 600;
    min-width: 40px;
  }

  .hex-bytes {
    flex: 1;
    color: var(--text-primary, #1F2937);
    min-width: 400px;
  }

  .hex-ascii {
    color: var(--accent-primary, #2563EB);
    min-width: 120px;
  }

  .empty-state {
    text-align: center;
    padding: 48px 24px;
    color: var(--text-secondary, #6B7280);
  }

  .empty-state p {
    margin: 0;
    font-size: 14px;
  }

  .empty-hint {
    font-size: 12px;
    color: var(--text-tertiary, #9CA3AF);
    margin-top: 8px;
  }

  /* PARSED View Enhanced Styles */
  .parsed-view-enhanced {
    display: flex;
    flex-direction: column;
    height: 100%;
  }

  .parsed-toolbar {
    display: flex;
    align-items: center;
    gap: 16px;
    padding: 8px 12px;
    background: var(--bg-secondary, #F8F9FA);
    border-bottom: 1px solid var(--border-light, #E0E0E0);
    flex-wrap: wrap;
  }

  .parsed-list {
    flex: 1;
    overflow-y: auto;
    padding: 8px;
  }

  .virtual-spacer {
    height: 0;
  }

  .parsed-item {
    margin-bottom: 8px;
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    background: var(--bg-primary, #FFFFFF);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .parsed-item:hover {
    border-color: var(--border-medium, #D1D5DB);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
  }

  .parsed-item.selected {
    border-color: var(--accent-primary, #2563EB);
    box-shadow: 0 2px 4px rgba(37, 99, 235, 0.2);
  }

  .parsed-item.tx {
    border-left: 3px solid var(--accent-success, #10B981);
  }

  .parsed-item.rx {
    border-left: 3px solid var(--accent-primary, #2563EB);
  }

  .parsed-header {
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 8px 12px;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 11px;
  }

  .parsed-timestamp {
    color: var(--text-secondary, #6B7280);
    min-width: 100px;
  }

  .parsed-direction {
    font-weight: 700;
    min-width: 30px;
  }

  .parsed-direction.tx {
    color: var(--accent-success, #10B981);
  }

  .parsed-direction.rx {
    color: var(--accent-primary, #2563EB);
  }

  .parsed-seq {
    color: var(--text-secondary, #6B7280);
    min-width: 60px;
  }

  .parsed-type {
    color: var(--accent-warning, #F59E0B);
    font-weight: 600;
    flex: 1;
  }

  .parsed-details {
    padding: 12px;
    background: var(--bg-secondary, #F8F9FA);
    border-top: 1px solid var(--border-light, #E0E0E0);
  }

  .json-viewer {
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 12px;
    line-height: 1.6;
    color: var(--text-primary, #1F2937);
  }

  :global(.json-expandable) {
    cursor: pointer;
    user-select: none;
  }

  :global(.json-expandable:hover > .json-toggle) {
    color: var(--accent-primary, #2563EB);
  }

  :global(.json-toggle) {
    color: var(--text-tertiary, #9CA3AF);
    font-weight: 600;
    font-size: 10px;
    margin-right: 4px;
    display: inline-block;
    min-width: 20px;
  }

  .json-children {
    padding-left: 20px;
    border-left: 1px solid var(--border-light, #E0E0E0);
    margin-left: 4px;
  }

  .json-field,
  .json-item {
    padding: 2px 0;
  }

  .json-key {
    color: #0451A5;
    font-weight: 600;
  }

  .json-string {
    color: #0A7D00;
  }

  .json-number {
    color: #09885A;
  }

  .json-boolean {
    color: #0000FF;
    font-weight: 600;
  }

  .json-null {
    color: #808080;
    font-style: italic;
  }

  .json-bracket,
  .json-brace {
    color: var(--text-secondary, #6B7280);
    font-weight: 600;
  }

  .json-collapsed {
    color: var(--text-tertiary, #9CA3AF);
    font-style: italic;
    font-size: 11px;
    margin: 0 4px;
  }

  .json-index {
    color: var(--text-secondary, #6B7280);
    font-weight: 500;
  }

  /* Console View Styles - Light Theme */
  .console-view {
    display: flex;
    flex-direction: column;
    height: 100%;
    background: var(--bg-primary, #FFFFFF);
  }

  .console-toolbar {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 8px 12px;
    background: var(--bg-secondary, #F8F9FA);
    border-bottom: 1px solid var(--border-light, #E0E0E0);
  }

  .console-info {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  .console-prompt-indicator {
    color: #047857;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 13px;
    font-weight: 700;
  }

  .console-hint {
    color: var(--text-secondary, #6B7280);
    font-size: 11px;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
  }

  .console-controls {
    display: flex;
    gap: 8px;
  }

  .console-output {
    flex: 1;
    overflow-y: auto;
    padding: 12px;
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 12px;
    line-height: 1.6;
    background: var(--bg-primary, #FFFFFF);
  }

  .console-line {
    display: flex;
    gap: 12px;
    padding: 2px 0;
  }

  .console-line-number {
    color: var(--text-tertiary, #9CA3AF);
    font-size: 10px;
    min-width: 32px;
    text-align: right;
    flex-shrink: 0;
  }

  .console-text {
    color: var(--text-primary, #1F2937);
    flex: 1;
    white-space: pre-wrap;
    word-break: break-word;
  }

  .console-line.command .console-text {
    color: #1F2937;
    font-weight: 600;
  }

  .console-line.command .console-text::before {
    content: '$ ';
    color: #047857;
    font-weight: 700;
  }

  .console-line.success .console-text {
    color: #047857;
  }

  .console-line.error .console-text {
    color: #DC2626;
  }

  .console-line.info .console-text {
    color: #2563EB;
  }

  .console-input-container {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 12px;
    background: var(--bg-secondary, #F8F9FA);
    border-top: 1px solid var(--border-light, #E0E0E0);
  }

  .console-input {
    flex: 1;
    background: var(--bg-primary, #FFFFFF);
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    padding: 6px 12px;
    color: var(--text-primary, #1F2937);
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-size: 12px;
    outline: none;
    transition: border-color 0.15s ease;
  }

  .console-input:focus {
    border-color: var(--accent-primary, #2563EB);
  }

  .console-input::placeholder {
    color: var(--text-tertiary, #9CA3AF);
  }

  .console-welcome {
    text-align: center;
    padding: 48px 24px;
    color: var(--text-secondary, #6B7280);
  }

  .console-welcome p {
    margin: 0;
    font-size: 14px;
    color: var(--text-primary, #1F2937);
  }

  .console-welcome-hint {
    font-size: 12px !important;
    color: var(--text-tertiary, #9CA3AF) !important;
    margin-top: 8px !important;
  }
</style>
