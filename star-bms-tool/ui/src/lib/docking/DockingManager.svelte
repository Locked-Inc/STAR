<script>
  import { dockingStore } from '../stores/docking.js';
  import { invoke } from '@tauri-apps/api/core';
  import DockZone from './DockZone.svelte';
  import DropZone from './DropZone.svelte';
  import { get } from 'svelte/store';
  import { onMount } from 'svelte';

  let { children } = $props();

  const zones = $derived($dockingStore.zones);
  const isDragging = $derived($dockingStore.dragState.isDragging && !!$dockingStore.dragState.panelId);
  let activeZone = $state(null);
  let leftWindowWhileDragging = $state(false);
  let lastPointer = $state({ x: 0, y: 0 });

  function resetDragState() {
    if (get(dockingStore).dragState.isDragging) {
      dockingStore.endDrag();
    }
    activeZone = null;
    leftWindowWhileDragging = false;
    document.body.classList.remove('dragging-panel');
  }

  onMount(() => {
    const handleDragEnd = () => resetDragState();
    const handleDrop = () => resetDragState();
    const handleKeyUp = (event) => {
      if (event.key === 'Escape') {
        resetDragState();
      }
    };

    document.addEventListener('pointermove', handlePointerMove);
    document.addEventListener('pointerup', handlePointerUp);
    document.addEventListener('mousemove', handlePointerMove);
    document.addEventListener('mouseup', handlePointerUp);
    window.addEventListener('dragend', handleDragEnd);
    window.addEventListener('drop', handleDrop);
    window.addEventListener('keyup', handleKeyUp);

    return () => {
      document.removeEventListener('pointermove', handlePointerMove);
      document.removeEventListener('pointerup', handlePointerUp);
      document.removeEventListener('mousemove', handlePointerMove);
      document.removeEventListener('mouseup', handlePointerUp);
      window.removeEventListener('dragend', handleDragEnd);
      window.removeEventListener('drop', handleDrop);
      window.removeEventListener('keyup', handleKeyUp);
    };
  });

  function resolveDropZone(x, y) {
    const width = window.innerWidth;
    const height = window.innerHeight;
    const topBounds = { x1: width * 0.2, x2: width * 0.8, y1: 20, y2: 140 };
    const bottomBounds = { x1: width * 0.2, x2: width * 0.8, y1: height - 140, y2: height - 20 };
    const leftBounds = { x1: 20, x2: 140, y1: height * 0.2, y2: height * 0.8 };
    const rightBounds = { x1: width - 140, x2: width - 20, y1: height * 0.2, y2: height * 0.8 };

    if (y >= topBounds.y1 && y <= topBounds.y2 && x >= topBounds.x1 && x <= topBounds.x2) {
      return 'top';
    }
    if (y >= bottomBounds.y1 && y <= bottomBounds.y2 && x >= bottomBounds.x1 && x <= bottomBounds.x2) {
      return 'bottom';
    }
    if (x >= leftBounds.x1 && x <= leftBounds.x2 && y >= leftBounds.y1 && y <= leftBounds.y2) {
      return 'left';
    }
    if (x >= rightBounds.x1 && x <= rightBounds.x2 && y >= rightBounds.y1 && y <= rightBounds.y2) {
      return 'right';
    }
    return null;
  }

  function handlePointerMove(event) {
    const state = get(dockingStore).dragState;
    if (!state.isDragging) {
      return;
    }
    lastPointer = { x: event.screenX, y: event.screenY };
    activeZone = resolveDropZone(event.clientX, event.clientY);
    const bounds = state.windowBounds || {
      x: window.screenX,
      y: window.screenY,
      width: window.outerWidth,
      height: window.outerHeight,
    };
    const outOfBounds = event.clientX < 0
      || event.clientY < 0
      || event.clientX > window.innerWidth
      || event.clientY > window.innerHeight
      || event.screenX < bounds.x
      || event.screenY < bounds.y
      || event.screenX > bounds.x + bounds.width
      || event.screenY > bounds.y + bounds.height;
    if (outOfBounds) {
      leftWindowWhileDragging = true;
    } else if (leftWindowWhileDragging) {
      leftWindowWhileDragging = false;
    }
  }

  async function floatActivePanel(forceOutside = false) {
    const state = get(dockingStore).dragState;
    if (!state.isDragging || !state.panelId || (!leftWindowWhileDragging && !forceOutside)) {
      return;
    }

    const panel = get(dockingStore).panels[state.panelId];
    if (panel && !panel.floating) {
      try {
        const windowLabel = await invoke('create_floating_panel', {
          panelId: state.panelId,
          title: panel.title || state.panelId,
          x: Math.round(lastPointer.x),
          y: Math.round(lastPointer.y),
        });
        dockingStore.floatPanel(state.panelId, windowLabel);
      } catch (error) {
        console.error(`Failed to float panel ${state.panelId}:`, error);
      }
    }

    resetDragState();
  }

  async function handlePointerUp(event) {
    const state = get(dockingStore).dragState;
    if (!state.isDragging) {
      activeZone = null;
      return;
    }

    if (state.panelId && activeZone) {
      dockingStore.markDrop();
      dockingStore.movePanel(state.panelId, activeZone, 'end');
    } else if (state.panelId && (leftWindowWhileDragging
        || event.clientX < 0
        || event.clientY < 0
        || event.clientX > window.innerWidth
        || event.clientY > window.innerHeight
        || event.screenX < (state.windowBounds?.x ?? window.screenX)
        || event.screenY < (state.windowBounds?.y ?? window.screenY)
        || event.screenX > (state.windowBounds?.x ?? window.screenX) + (state.windowBounds?.width ?? window.outerWidth)
        || event.screenY > (state.windowBounds?.y ?? window.screenY) + (state.windowBounds?.height ?? window.outerHeight))) {
      await floatActivePanel(true);
      return;
    }

    dockingStore.endDrag();
    activeZone = null;
    leftWindowWhileDragging = false;
    document.body.classList.remove('dragging-panel');
  }
</script>

<div class="docking-root">
  <!-- Drop zones overlay (shown when dragging) -->
  {#if isDragging}
    <div class="drop-zones-overlay">
      <DropZone zone="top" active={activeZone === 'top'} />
      <DropZone zone="bottom" active={activeZone === 'bottom'} />
      <DropZone zone="left" active={activeZone === 'left'} />
      <DropZone zone="right" active={activeZone === 'right'} />
    </div>
  {/if}

  <!-- Top zone -->
  {#if zones.top.visible}
    <DockZone zone="top" />
  {/if}

  <!-- Middle row: left + center + right -->
  <div class="docking-middle">
    <!-- Left zone -->
    {#if zones.left.visible}
      <DockZone zone="left" />
    {/if}

    <!-- Center (main app content) -->
    <div class="docking-center">
      {@render children()}
    </div>

    <!-- Right zone -->
    {#if zones.right.visible}
      <DockZone zone="right" />
    {/if}
  </div>

  <!-- Bottom zone -->
  {#if zones.bottom.visible}
    <DockZone zone="bottom" />
  {/if}
</div>

<style>
  .docking-root {
    display: flex;
    flex-direction: column;
    height: 100vh;
    width: 100vw;
    overflow: hidden;
    background: var(--bg-primary, #FFFFFF);
  }

  .docking-middle {
    display: flex;
    flex: 1;
    overflow: hidden;
    min-height: 0;
  }

  .docking-center {
    flex: 1;
    overflow: hidden;
    display: flex;
    flex-direction: column;
    min-width: 0;
    min-height: 0;
  }

  .drop-zones-overlay {
    position: fixed;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    pointer-events: auto;
    z-index: 999;
  }

  .drop-zones-overlay :global(.drop-zone) {
    pointer-events: auto;
  }
</style>
