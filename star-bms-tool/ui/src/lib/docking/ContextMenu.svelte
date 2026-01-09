<script>
  import { invoke } from '@tauri-apps/api/core';
  import { dockingStore } from '../stores/docking.js';

  let { panelId = '', x = 0, y = 0, onClose = () => {} } = $props();

  const panel = $derived($dockingStore.panels[panelId]);
  const isFloating = $derived(panel?.floating || false);

  function handleClose() {
    console.log(`[CONTEXT] Close panel: ${panelId}`);
    dockingStore.hidePanel(panelId);
    onClose();
  }

  async function handleFloat() {
    console.log(`[CONTEXT] Float panel: ${panelId}`);
    try {
      const windowLabel = await invoke('create_floating_panel', {
        panelId: panelId,
        title: panel?.title || 'Panel',
      });
      dockingStore.floatPanel(panelId, windowLabel);
      onClose();
    } catch (error) {
      console.error(`Failed to float panel ${panelId}:`, error);
    }
  }

  async function handleUnfloat() {
    console.log(`[CONTEXT] Unfloat panel: ${panelId}`);
    try {
      if (panel?.floatingWindow) {
        await invoke('close_floating_panel', {
          windowLabel: panel.floatingWindow,
        });
      }
      dockingStore.unfloatPanel(panelId, 'bottom');
      onClose();
    } catch (error) {
      console.error(`Failed to unfloat panel ${panelId}:`, error);
    }
  }

  function handleMoveToZone(zone) {
    console.log(`[CONTEXT] Move panel ${panelId} to ${zone}`);
    dockingStore.movePanel(panelId, zone);
    onClose();
  }

  // Close menu on Escape key
  function handleKeyDown(event) {
    if (event.key === 'Escape') {
      onClose();
    }
  }

  // Close menu when clicking outside
  function handleClickOutside(event) {
    if (!event.target.closest('.context-menu')) {
      onClose();
    }
  }
</script>

<svelte:window onkeydown={handleKeyDown} onclick={handleClickOutside} />

<div
  class="context-menu"
  style="left: {x}px; top: {y}px;"
  role="menu"
  aria-label="Panel context menu"
>
  <div class="context-menu-section">
    <div class="context-menu-label">{panel?.title || 'Panel'}</div>
  </div>

  <div class="context-menu-divider"></div>

  <div class="context-menu-section">
    {#if !isFloating}
      <button class="context-menu-item" onclick={handleFloat}>
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <rect x="3" y="3" width="18" height="18" rx="2" ry="2"/>
          <path d="M9 3v18"/>
        </svg>
        Float in New Window
      </button>
    {:else}
      <button class="context-menu-item" onclick={handleUnfloat}>
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>
        </svg>
        Dock to Main Window
      </button>
    {/if}
  </div>

  {#if !isFloating}
    <div class="context-menu-divider"></div>

    <div class="context-menu-section">
      <div class="context-menu-label">Move to</div>
      <button
        class="context-menu-item"
        class:disabled={panel?.zone === 'top'}
        onclick={() => handleMoveToZone('top')}
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M12 19V5M5 12l7-7 7 7"/>
        </svg>
        Top
      </button>
      <button
        class="context-menu-item"
        class:disabled={panel?.zone === 'bottom'}
        onclick={() => handleMoveToZone('bottom')}
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M12 5v14M5 12l7 7 7-7"/>
        </svg>
        Bottom
      </button>
      <button
        class="context-menu-item"
        class:disabled={panel?.zone === 'left'}
        onclick={() => handleMoveToZone('left')}
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M19 12H5M12 5l-7 7 7 7"/>
        </svg>
        Left
      </button>
      <button
        class="context-menu-item"
        class:disabled={panel?.zone === 'right'}
        onclick={() => handleMoveToZone('right')}
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M5 12h14M12 5l7 7-7 7"/>
        </svg>
        Right
      </button>
    </div>
  {/if}

  <div class="context-menu-divider"></div>

  <div class="context-menu-section">
    <button class="context-menu-item danger" onclick={handleClose}>
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M18 6L6 18M6 6l12 12"/>
      </svg>
      Close Panel
    </button>
  </div>
</div>

<style>
  .context-menu {
    position: fixed;
    min-width: 200px;
    background: var(--bg-primary, #FFFFFF);
    border: 1px solid var(--panel-border, #E0E0E0);
    border-radius: 6px;
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
    z-index: 9999;
    padding: 4px;
  }

  .context-menu-section {
    padding: 4px 0;
  }

  .context-menu-label {
    padding: 6px 12px;
    font-size: 11px;
    font-weight: 600;
    color: var(--text-tertiary, #9CA3AF);
    text-transform: uppercase;
    letter-spacing: 0.5px;
  }

  .context-menu-divider {
    height: 1px;
    background: var(--panel-border, #E0E0E0);
    margin: 4px 8px;
  }

  .context-menu-item {
    display: flex;
    align-items: center;
    gap: 10px;
    width: 100%;
    padding: 8px 12px;
    border: none;
    background: transparent;
    color: var(--text-primary, #1F2937);
    font-size: 13px;
    text-align: left;
    cursor: pointer;
    border-radius: 4px;
    transition: all 0.15s ease;
  }

  .context-menu-item:hover:not(.disabled) {
    background: var(--bg-tertiary, #E5E7EB);
  }

  .context-menu-item.disabled {
    opacity: 0.4;
    cursor: not-allowed;
  }

  .context-menu-item.danger {
    color: var(--color-error, #DC2626);
  }

  .context-menu-item.danger:hover {
    background: var(--color-error-bg, #FEE2E2);
  }

  .context-menu-item svg {
    flex-shrink: 0;
  }
</style>
