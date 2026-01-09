<script>
  import { dockingStore } from '../stores/docking.js';
  import ResizeHandle from './ResizeHandle.svelte';
  import PanelContainer from './PanelContainer.svelte';

  let { zone = 'bottom' } = $props(); // 'top' | 'bottom' | 'left' | 'right'

  const zoneState = $derived($dockingStore.zones[zone]);
  const panels = $derived(
    (zoneState?.panels || []).filter((panelId) => {
      const panel = $dockingStore.panels[panelId];
      return panel && panel.visible && !panel.floating;
    })
  );
  const visible = $derived(zoneState?.visible || false);
  const activePanelId = $derived($dockingStore.dragState.panelId);
  let size = $state(300);

  // Update size when store changes (e.g., from localStorage restore)
  $effect(() => {
    if (zoneState) {
      size = zoneState.size;
    }
  });

  const isHorizontal = $derived(zone === 'top' || zone === 'bottom');
  const isVertical = $derived(zone === 'left' || zone === 'right');

  // Resize handle orientation
  const handleOrientation = $derived(isHorizontal ? 'horizontal' : 'vertical');

  // Resize handle position
  const handlePosition = $derived(
    zone === 'top' ? 'bottom' :
    zone === 'bottom' ? 'top' :
    zone === 'left' ? 'right' :
    'left'
  );

  function handleResize(data) {
    size = data.size;
    dockingStore.setZoneSize(zone, size);
  }

  function handleResizeEnd(data) {
    // Already updated in handleResize, just ensure it's persisted
    dockingStore.setZoneSize(zone, data.size);
  }

  function handleDrop(event) {
    event.preventDefault();

    const panelId = activePanelId
      || event.dataTransfer.getData('application/x-panel-id')
      || event.dataTransfer.getData('text/plain');
    if (!panelId) {
      return;
    }

    dockingStore.markDrop();
    dockingStore.movePanel(panelId, zone, 'end');
    dockingStore.endDrag();
  }

  function handleDragOver(event) {
    event.preventDefault();
    event.dataTransfer.dropEffect = 'move';
  }
</script>

{#if visible}
  <div
    class="dock-zone dock-zone-{zone}"
    style:width={isVertical ? `${size}px` : 'auto'}
    style:height={isHorizontal ? `${size}px` : 'auto'}
    data-zone={zone}
    role="region"
    aria-label={`Dock zone ${zone}`}
    ondragover={handleDragOver}
    ondrop={handleDrop}
  >
    {#if handlePosition === 'top'}
      <ResizeHandle
        orientation={handleOrientation}
        handlePosition={handlePosition}
        currentSize={size}
        onresize={handleResize}
        onresizeend={handleResizeEnd}
      />
    {/if}

    {#if handlePosition === 'left'}
      <ResizeHandle
        orientation={handleOrientation}
        handlePosition={handlePosition}
        currentSize={size}
        onresize={handleResize}
        onresizeend={handleResizeEnd}
      />
    {/if}

    <div class="dock-zone-content">
      <PanelContainer panelIds={panels} />
    </div>

    {#if handlePosition === 'right'}
      <ResizeHandle
        orientation={handleOrientation}
        handlePosition={handlePosition}
        currentSize={size}
        onresize={handleResize}
        onresizeend={handleResizeEnd}
      />
    {/if}

    {#if handlePosition === 'bottom'}
      <ResizeHandle
        orientation={handleOrientation}
        handlePosition={handlePosition}
        currentSize={size}
        onresize={handleResize}
        onresizeend={handleResizeEnd}
      />
    {/if}
  </div>
{/if}

<style>
  .dock-zone {
    position: relative;
    background: var(--bg-secondary, #F5F5F5);
    border: 1px solid var(--panel-border, #E0E0E0);
    overflow: hidden;
    display: flex;
  }

  .dock-zone-top {
    border-bottom: 1px solid var(--panel-border, #E0E0E0);
    flex-direction: column;
  }

  .dock-zone-bottom {
    border-top: 1px solid var(--panel-border, #E0E0E0);
    flex-direction: column;
  }

  .dock-zone-left {
    border-right: 1px solid var(--panel-border, #E0E0E0);
    flex-direction: row;
  }

  .dock-zone-right {
    border-left: 1px solid var(--panel-border, #E0E0E0);
    flex-direction: row;
  }

  .dock-zone-content {
    flex: 1;
    overflow: hidden;
    min-width: 0;
    min-height: 0;
  }
</style>
