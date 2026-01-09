<script>
  import PacketViewerPanel from './lib/panels/PacketViewerPanel.svelte';
  import TerminalPanel from './lib/panels/TerminalPanel.svelte';
  import DeviceInfoPanel from './lib/panels/DeviceInfoPanel.svelte';
  import PropertiesPanel from './lib/panels/PropertiesPanel.svelte';
  import { dockingStore } from './lib/stores/docking.js';
  import { get } from 'svelte/store';
  import { emit } from '@tauri-apps/api/event';
  import { invoke } from '@tauri-apps/api/core';
  import { getCurrentWindow } from '@tauri-apps/api/window';

  // Get panel ID from URL hash
  const hash = window.location.hash;
  const match = hash.match(/#\/floating\/(.+)/);
  const panelId = match ? match[1] : null;

  const panel = $derived($dockingStore.panels[panelId]);
  const title = $derived(panel?.title || 'Panel');
  let closing = false;

  async function closeWindow(action = null) {
    try {
      const currentWindow = getCurrentWindow();
      const windowLabel = currentWindow.label || `floating-${panelId}`;
      try {
        if (action && panelId) {
          await invoke('close_floating_panel_with_action', {
            windowLabel,
            panelId,
            action,
          });
        } else {
          await invoke('close_floating_panel', { windowLabel });
        }
      } catch (error) {
        await currentWindow.close();
      }
    } catch (error) {
      console.error('Failed to close floating window:', error);
    }
  }

  async function handleDock() {
    if (closing) {
      return;
    }
    closing = true;

    console.log(`[FLOATING] Docking floating window for: ${panelId}`);

    if (panelId) {
      dockingStore.unfloatPanel(panelId, panel?.lastDockZone || 'bottom');
      try {
        await emit('docking:unfloat-panel', { panelId, targetZone: panel?.lastDockZone || 'bottom' });
      } catch (error) {
        console.error('Failed to emit dock event:', error);
      }
      try {
        localStorage.setItem('bms-tool-docking-layout', JSON.stringify(get(dockingStore)));
      } catch (error) {
        console.error('Failed to persist docking layout:', error);
      }
    }

    await closeWindow();
  }

  async function handleWindowClose() {
    if (closing) {
      return;
    }
    closing = true;

    console.log(`[FLOATING] Closing floating window for: ${panelId}`);

    if (panelId) {
      dockingStore.hidePanel(panelId);
      try {
        await emit('docking:hide-panel', { panelId });
      } catch (error) {
        console.error('Failed to emit hide event:', error);
      }
      try {
        localStorage.setItem('bms-tool-docking-layout', JSON.stringify(get(dockingStore)));
      } catch (error) {
        console.error('Failed to persist docking layout:', error);
      }
    }

    await closeWindow('hide');
  }

  // Listen for window close event
  import { onMount } from 'svelte';
  onMount(() => {
    const currentWindow = getCurrentWindow();

    // Clean up on window close
    const unlisten = currentWindow.listen('tauri://close-requested', async (event) => {
      if (event?.preventDefault) {
        event.preventDefault();
      }
      await handleWindowClose();
    });

    return () => {
      unlisten.then(fn => fn());
    };
  });
</script>

<div class="floating-panel-root">
  <div class="floating-panel-header">
    <span class="floating-panel-title">{title}</span>
    <button
      class="floating-panel-dock-btn"
      onclick={handleDock}
      title="Dock panel back to main window"
      aria-label="Dock panel back to main window"
    >
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>
      </svg>
      Dock
    </button>
  </div>

  <div class="floating-panel-content">
    {#if panelId === 'packet-viewer'}
      <PacketViewerPanel />
    {:else if panelId === 'terminal'}
      <TerminalPanel />
    {:else if panelId === 'device-info'}
      <DeviceInfoPanel />
    {:else if panelId === 'properties'}
      <PropertiesPanel />
    {:else}
      <div class="error-panel">
        <h2>Unknown Panel</h2>
        <p>Panel "{panelId}" not found</p>
      </div>
    {/if}
  </div>
</div>

<style>
  .floating-panel-root {
    display: flex;
    flex-direction: column;
    height: 100vh;
    width: 100vw;
    overflow: hidden;
    background: var(--bg-primary, #FFFFFF);
  }

  .floating-panel-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 8px 12px;
    background: var(--panel-header-bg, #F5F5F5);
    border-bottom: 1px solid var(--panel-border, #E0E0E0);
    flex-shrink: 0;
  }

  .floating-panel-title {
    font-size: 14px;
    font-weight: 600;
    color: var(--text-primary, #1F2937);
  }

  .floating-panel-dock-btn {
    display: flex;
    align-items: center;
    gap: 6px;
    padding: 4px 10px;
    font-size: 12px;
    font-weight: 500;
    border: 1px solid var(--panel-border, #E0E0E0);
    background: var(--bg-primary, #FFFFFF);
    border-radius: 4px;
    color: var(--text-secondary, #6B7280);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .floating-panel-dock-btn:hover {
    background: var(--bg-tertiary, #E5E7EB);
    border-color: var(--color-accent-500, #2563EB);
    color: var(--color-accent-600, #1D4ED8);
  }

  .floating-panel-content {
    flex: 1;
    overflow: hidden;
    display: flex;
    flex-direction: column;
  }

  .error-panel {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: var(--text-tertiary, #999);
    padding: 24px;
    text-align: center;
  }

  .error-panel h2 {
    margin: 0 0 8px 0;
    font-size: 18px;
    font-weight: 600;
  }

  .error-panel p {
    margin: 0;
    font-size: 14px;
  }
</style>
