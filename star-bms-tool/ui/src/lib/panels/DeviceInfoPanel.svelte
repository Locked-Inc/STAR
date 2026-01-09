<script>
  import { onMount, onDestroy } from 'svelte';
  import { invoke } from '@tauri-apps/api/core';

  let deviceState = $state(null);
  let loading = $state(false);
  let error = $state('');
  let autoRefresh = $state(false);
  let refreshInterval = null;

  onMount(() => {
    refreshDeviceInfo();
  });

  onDestroy(() => {
    if (refreshInterval) {
      clearInterval(refreshInterval);
    }
  });

  async function refreshDeviceInfo() {
    loading = true;
    error = '';
    try {
      deviceState = await invoke('get_device_state');
    } catch (e) {
      error = `Failed to get device info: ${e}`;
      deviceState = null;
    } finally {
      loading = false;
    }
  }

  function toggleAutoRefresh() {
    autoRefresh = !autoRefresh;
    if (autoRefresh) {
      refreshInterval = setInterval(refreshDeviceInfo, 1000);
    } else {
      clearInterval(refreshInterval);
      refreshInterval = null;
    }
  }

  function formatHex(value) {
    if (value === null || value === undefined) return 'N/A';
    return `0x${value.toString(16).toUpperCase().padStart(8, '0')}`;
  }
</script>

<div class="device-info-panel">
  <div class="panel-header">
    <div class="panel-title">Device Information</div>
    <div class="panel-controls">
      <button class="control-btn" onclick={refreshDeviceInfo} disabled={loading} title="Refresh">
        {loading ? 'Refreshing...' : 'Refresh'}
      </button>
      <button
        class="control-btn"
        class:active={autoRefresh}
        onclick={toggleAutoRefresh}
        title="Auto-refresh (1s)"
      >
        Auto-refresh
      </button>
    </div>
  </div>

  <div class="panel-content">
    {#if error}
      <div class="error-message">
        <strong>Error:</strong> {error}
      </div>
    {:else if !deviceState}
      <div class="empty-state">
        <p>No device connected</p>
        <p class="hint">Connect to a device to view information</p>
      </div>
    {:else}
      <div class="info-grid">
        <div class="info-section">
          <h3>Device Identity</h3>
          <div class="info-row">
            <span class="info-label">Manufacturer:</span>
            <span class="info-value">{deviceState.manufacturer || 'Unknown'}</span>
          </div>
          <div class="info-row">
            <span class="info-label">Device Name:</span>
            <span class="info-value">{deviceState.device_name || 'Unknown'}</span>
          </div>
          <div class="info-row">
            <span class="info-label">Serial Number:</span>
            <span class="info-value">{formatHex(deviceState.serial_number)}</span>
          </div>
        </div>

        <div class="info-section">
          <h3>Firmware</h3>
          <div class="info-row">
            <span class="info-label">Version:</span>
            <span class="info-value">{deviceState.firmware_version || 'Unknown'}</span>
          </div>
          <div class="info-row">
            <span class="info-label">Hardware:</span>
            <span class="info-value">{deviceState.hardware_version || 'Unknown'}</span>
          </div>
        </div>

        <div class="info-section">
          <h3>Battery Specifications</h3>
          <div class="info-row">
            <span class="info-label">Chemistry:</span>
            <span class="info-value">{deviceState.chemistry || 'Unknown'}</span>
          </div>
          <div class="info-row">
            <span class="info-label">Cell Count:</span>
            <span class="info-value">{deviceState.num_cells || 0}</span>
          </div>
          <div class="info-row">
            <span class="info-label">Design Capacity:</span>
            <span class="info-value">{deviceState.design_capacity_mah || 0} mAh</span>
          </div>
          <div class="info-row">
            <span class="info-label">Design Voltage:</span>
            <span class="info-value">{deviceState.design_voltage_mv || 0} mV</span>
          </div>
        </div>
      </div>
    {/if}
  </div>
</div>

<style>
  .device-info-panel {
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
    flex-shrink: 0;
  }

  .panel-title {
    font-weight: 600;
    font-size: 13px;
    color: var(--text-primary, #1F2937);
  }

  .panel-controls {
    display: flex;
    gap: 8px;
  }

  .control-btn {
    padding: 4px 12px;
    font-size: 12px;
    border: 1px solid var(--border-light, #E0E0E0);
    background: var(--bg-primary, #FFFFFF);
    border-radius: 4px;
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .control-btn:hover:not(:disabled) {
    background: var(--bg-secondary, #F8F9FA);
    border-color: var(--color-accent-500, #2563EB);
  }

  .control-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .control-btn.active {
    background: var(--color-accent-500, #2563EB);
    color: white;
    border-color: var(--color-accent-500, #2563EB);
  }

  .panel-content {
    flex: 1;
    overflow-y: auto;
    padding: 16px;
  }

  .error-message {
    padding: 12px;
    background: #FEF2F2;
    border: 1px solid #FCA5A5;
    border-radius: 4px;
    color: #991B1B;
    font-size: 13px;
  }

  .empty-state {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: var(--text-tertiary, #9CA3AF);
    text-align: center;
  }

  .empty-state p {
    margin: 4px 0;
  }

  .empty-state .hint {
    font-size: 12px;
    font-style: italic;
  }

  .info-grid {
    display: flex;
    flex-direction: column;
    gap: 20px;
  }

  .info-section {
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 6px;
    padding: 16px;
    background: var(--bg-secondary, #FAFAFA);
  }

  .info-section h3 {
    margin: 0 0 12px 0;
    font-size: 14px;
    font-weight: 600;
    color: var(--text-primary, #1F2937);
    border-bottom: 1px solid var(--border-light, #E0E0E0);
    padding-bottom: 8px;
  }

  .info-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 6px 0;
    font-size: 13px;
  }

  .info-label {
    color: var(--text-secondary, #6B7280);
    font-weight: 500;
  }

  .info-value {
    color: var(--text-primary, #1F2937);
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-weight: 600;
  }
</style>
