<script>
  import { dockingStore } from '../stores/docking.js';
  import { invoke } from '@tauri-apps/api/core';
  import ContextMenu from './ContextMenu.svelte';

  let { panelId = '', title = '', draggable = true } = $props();

  const panel = $derived($dockingStore.panels[panelId]);
  // Context menu state
  let showContextMenu = $state(false);
  let contextMenuX = $state(0);
  let contextMenuY = $state(0);

  function startDrag(event) {
    if (!draggable) return;
    if (event.button !== 0) return;
    if (event.target && event.target.closest('.header-action-btn')) {
      return;
    }

    event.preventDefault();
    if (event.currentTarget?.setPointerCapture) {
      event.currentTarget.setPointerCapture(event.pointerId);
    }
    console.log(`[DRAG] Starting drag for panel: ${panelId}`);

    const sourceZone = panel?.zone;
    const windowBounds = {
      x: window.screenX,
      y: window.screenY,
      width: window.outerWidth,
      height: window.outerHeight,
    };
    dockingStore.startDrag(panelId, sourceZone, windowBounds);
    document.body.classList.add('dragging-panel');
  }

  function handleClose() {
    console.log(`[PANEL] Closing panel: ${panelId}`);
    dockingStore.hidePanel(panelId);
  }

  async function handleFloat() {
    console.log(`[PANEL] Floating panel: ${panelId}`);
    try {
      const windowLabel = await invoke('create_floating_panel', {
        panelId: panelId,
        title: title,
      });
      dockingStore.floatPanel(panelId, windowLabel);
    } catch (error) {
      console.error(`Failed to float panel ${panelId}:`, error);
    }
  }

  function handleContextMenu(event) {
    event.preventDefault();
    contextMenuX = event.clientX;
    contextMenuY = event.clientY;
    showContextMenu = true;
  }

  function closeContextMenu() {
    showContextMenu = false;
  }
</script>

<div
  class="panel-header"
  class:draggable
  draggable={false}
  onpointerdown={startDrag}
  onmousedown={startDrag}
  oncontextmenu={handleContextMenu}
  role="toolbar"
  aria-label={title}
  tabindex="0"
>
  <div class="panel-header-left">
    {#if draggable}
      <div class="drag-handle" title="Drag to reposition">
        <svg width="12" height="12" viewBox="0 0 24 24" fill="currentColor">
          <circle cx="5" cy="5" r="2"/>
          <circle cx="12" cy="5" r="2"/>
          <circle cx="19" cy="5" r="2"/>
          <circle cx="5" cy="12" r="2"/>
          <circle cx="12" cy="12" r="2"/>
          <circle cx="19" cy="12" r="2"/>
          <circle cx="5" cy="19" r="2"/>
          <circle cx="12" cy="19" r="2"/>
          <circle cx="19" cy="19" r="2"/>
        </svg>
      </div>
    {/if}
    <span class="panel-title">{title}</span>
  </div>

  <div class="panel-header-right">
    <button
      type="button"
      class="header-action-btn"
      onclick={handleFloat}
      title="Float panel in new window"
      aria-label="Float panel in new window"
    >
      <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <rect x="3" y="3" width="18" height="18" rx="2" ry="2"/>
        <path d="M9 3v18"/>
      </svg>
    </button>
    <button
      type="button"
      class="header-action-btn"
      onclick={handleClose}
      title="Close panel"
      aria-label="Close panel"
    >
      <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <path d="M18 6L6 18M6 6l12 12"/>
      </svg>
    </button>
  </div>
</div>

{#if showContextMenu}
  <ContextMenu
    panelId={panelId}
    x={contextMenuX}
    y={contextMenuY}
    onClose={closeContextMenu}
  />
{/if}

<style>
  .panel-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 6px 12px;
    background: var(--panel-header-bg, #F5F5F5);
    border-bottom: 1px solid var(--panel-border, #E0E0E0);
    user-select: none;
    flex-shrink: 0;
  }

  .panel-header * {
    user-select: none;
  }

  .panel-header.draggable {
    cursor: grab;
  }

  .panel-header.draggable:active {
    cursor: grabbing;
  }

  .panel-header-left {
    display: flex;
    align-items: center;
    gap: 8px;
    flex: 1;
    min-width: 0;
  }

  .drag-handle {
    display: flex;
    align-items: center;
    justify-content: center;
    color: var(--text-tertiary, #9CA3AF);
    flex-shrink: 0;
    opacity: 0.6;
    transition: opacity 0.15s ease;
  }

  .panel-header:hover .drag-handle {
    opacity: 1;
  }

  .panel-title {
    font-size: 13px;
    font-weight: 600;
    color: var(--text-primary, #1F2937);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .panel-header-right {
    display: flex;
    align-items: center;
    gap: 4px;
    flex-shrink: 0;
  }

  .header-action-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 20px;
    height: 20px;
    padding: 0;
    border: none;
    background: transparent;
    border-radius: 3px;
    color: var(--text-tertiary, #9CA3AF);
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .header-action-btn:hover {
    background: var(--bg-tertiary, #E5E7EB);
    color: var(--text-primary, #1F2937);
  }

  .header-action-btn:active {
    background: var(--bg-quaternary, #D1D5DB);
  }
</style>
