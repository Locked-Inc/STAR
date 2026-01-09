<script>
  import { invoke } from '@tauri-apps/api/core';

  let selectedProperty = $state('register');
  let regAddress = $state('0x00');
  let regValue = $state('0x00');
  let regNumBytes = $state(1);
  let regData = $state(null);
  let error = $state('');
  let loading = $state(false);

  async function handleReadRegister() {
    loading = true;
    error = '';
    try {
      const address = parseInt(regAddress, 16);
      regData = await invoke('read_register', {
        address,
        numBytes: regNumBytes
      });
    } catch (e) {
      error = `Failed to read register: ${e}`;
      regData = null;
    } finally {
      loading = false;
    }
  }

  async function handleWriteRegister() {
    loading = true;
    error = '';
    try {
      const address = parseInt(regAddress, 16);
      const value = parseInt(regValue, 16);
      await invoke('write_register', {
        address,
        value,
        numBytes: regNumBytes
      });
      await handleReadRegister();
    } catch (e) {
      error = `Failed to write register: ${e}`;
    } finally {
      loading = false;
    }
  }

  function formatHex(value, bytes = 1) {
    if (value === null || value === undefined) return 'N/A';
    const padLength = bytes * 2;
    return `0x${value.toString(16).toUpperCase().padStart(padLength, '0')}`;
  }
</script>

<div class="properties-panel">
  <div class="panel-header">
    <div class="panel-title">Properties</div>
    <div class="property-tabs">
      <button
        class="tab-btn"
        class:active={selectedProperty === 'register'}
        onclick={() => selectedProperty = 'register'}
      >
        Registers
      </button>
      <button
        class="tab-btn"
        class:active={selectedProperty === 'config'}
        onclick={() => selectedProperty = 'config'}
      >
        Configuration
      </button>
    </div>
  </div>

  <div class="panel-content">
    {#if selectedProperty === 'register'}
      <div class="register-editor">
        <h3>Register Access</h3>

        <div class="form-group">
          <label for="reg-address">Address (hex)</label>
          <input
            id="reg-address"
            type="text"
            bind:value={regAddress}
            placeholder="0x00"
            disabled={loading}
          />
        </div>

        <div class="form-group">
          <label for="reg-bytes">Number of Bytes</label>
          <select id="reg-bytes" bind:value={regNumBytes} disabled={loading}>
            <option value={1}>1 byte</option>
            <option value={2}>2 bytes</option>
            <option value={4}>4 bytes</option>
          </select>
        </div>

        <div class="button-group">
          <button class="btn-primary" onclick={handleReadRegister} disabled={loading}>
            {loading ? 'Reading...' : 'Read'}
          </button>
        </div>

        {#if regData}
          <div class="register-result">
            <h4>Register Data</h4>
            <div class="data-row">
              <span class="data-label">Address:</span>
              <span class="data-value">{formatHex(regData.address, 1)}</span>
            </div>
            <div class="data-row">
              <span class="data-label">Value:</span>
              <span class="data-value">{formatHex(regData.value, regData.num_bytes)}</span>
            </div>
            <div class="data-row">
              <span class="data-label">Decimal:</span>
              <span class="data-value">{regData.value}</span>
            </div>
          </div>
        {/if}

        <div class="form-divider"></div>

        <h3>Write Register</h3>
        <div class="form-group">
          <label for="reg-value">Value (hex)</label>
          <input
            id="reg-value"
            type="text"
            bind:value={regValue}
            placeholder="0x00"
            disabled={loading}
          />
        </div>

        <div class="button-group">
          <button class="btn-danger" onclick={handleWriteRegister} disabled={loading}>
            {loading ? 'Writing...' : 'Write'}
          </button>
        </div>

        {#if error}
          <div class="error-message">{error}</div>
        {/if}
      </div>
    {:else if selectedProperty === 'config'}
      <div class="config-editor">
        <h3>Device Configuration</h3>
        <p class="placeholder-text">Configuration editor coming soon...</p>
        <p class="placeholder-hint">
          This panel will allow editing device settings, thresholds, and protection parameters.
        </p>
      </div>
    {/if}
  </div>
</div>

<style>
  .properties-panel {
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

  .property-tabs {
    display: flex;
    gap: 4px;
  }

  .tab-btn {
    padding: 4px 12px;
    font-size: 12px;
    border: 1px solid var(--border-light, #E0E0E0);
    background: var(--bg-primary, #FFFFFF);
    border-radius: 4px;
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .tab-btn:hover {
    background: var(--bg-secondary, #F8F9FA);
  }

  .tab-btn.active {
    background: var(--color-accent-500, #2563EB);
    color: white;
    border-color: var(--color-accent-500, #2563EB);
  }

  .panel-content {
    flex: 1;
    overflow-y: auto;
    padding: 16px;
  }

  .register-editor,
  .config-editor {
    max-width: 500px;
  }

  h3 {
    margin: 0 0 16px 0;
    font-size: 14px;
    font-weight: 600;
    color: var(--text-primary, #1F2937);
  }

  h4 {
    margin: 0 0 8px 0;
    font-size: 13px;
    font-weight: 600;
    color: var(--text-secondary, #6B7280);
  }

  .form-group {
    margin-bottom: 16px;
  }

  .form-group label {
    display: block;
    margin-bottom: 6px;
    font-size: 12px;
    font-weight: 500;
    color: var(--text-secondary, #6B7280);
  }

  .form-group input,
  .form-group select {
    width: 100%;
    padding: 8px 12px;
    font-size: 13px;
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 4px;
    background: var(--bg-primary, #FFFFFF);
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
  }

  .form-group input:focus,
  .form-group select:focus {
    outline: none;
    border-color: var(--color-accent-500, #2563EB);
    box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.1);
  }

  .form-group input:disabled,
  .form-group select:disabled {
    background: var(--bg-secondary, #F8F9FA);
    cursor: not-allowed;
    opacity: 0.6;
  }

  .button-group {
    display: flex;
    gap: 8px;
    margin-bottom: 16px;
  }

  .btn-primary,
  .btn-danger {
    padding: 8px 16px;
    font-size: 13px;
    font-weight: 500;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .btn-primary {
    background: var(--color-accent-500, #2563EB);
    color: white;
  }

  .btn-primary:hover:not(:disabled) {
    background: var(--color-accent-600, #1D4ED8);
  }

  .btn-danger {
    background: #EF4444;
    color: white;
  }

  .btn-danger:hover:not(:disabled) {
    background: #DC2626;
  }

  .btn-primary:disabled,
  .btn-danger:disabled {
    opacity: 0.5;
    cursor: not-allowed;
  }

  .register-result {
    padding: 12px;
    background: var(--bg-secondary, #F8F9FA);
    border: 1px solid var(--border-light, #E0E0E0);
    border-radius: 6px;
    margin-bottom: 16px;
  }

  .data-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 4px 0;
    font-size: 13px;
  }

  .data-label {
    color: var(--text-secondary, #6B7280);
    font-weight: 500;
  }

  .data-value {
    color: var(--text-primary, #1F2937);
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    font-weight: 600;
  }

  .form-divider {
    height: 1px;
    background: var(--border-light, #E0E0E0);
    margin: 24px 0;
  }

  .error-message {
    padding: 12px;
    background: #FEF2F2;
    border: 1px solid #FCA5A5;
    border-radius: 4px;
    color: #991B1B;
    font-size: 13px;
    margin-top: 16px;
  }

  .placeholder-text {
    color: var(--text-secondary, #6B7280);
    font-size: 13px;
    margin-bottom: 8px;
  }

  .placeholder-hint {
    color: var(--text-tertiary, #9CA3AF);
    font-size: 12px;
    font-style: italic;
  }
</style>
