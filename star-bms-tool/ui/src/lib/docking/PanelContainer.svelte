<script>
  import Panel from './Panel.svelte';
  import TabGroup from './TabGroup.svelte';

  let { panelIds = [] } = $props(); // Array of panel IDs

  const hasPanels = $derived(panelIds.length > 0);
  const hasMultiplePanels = $derived(panelIds.length > 1);
</script>

<div class="panel-container">
  {#if !hasPanels}
    <!-- Empty state -->
    <div class="empty-panel-container">
      <p>No panels in this zone</p>
    </div>
  {:else if hasMultiplePanels}
    <!-- Multiple panels: show tabs -->
    <TabGroup panelIds={panelIds} />
  {:else}
    <!-- Single panel -->
    <Panel panelId={panelIds[0]} />
  {/if}
</div>

<style>
  .panel-container {
    display: flex;
    flex-direction: column;
    height: 100%;
    width: 100%;
    overflow: hidden;
  }

  .empty-panel-container {
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: var(--text-tertiary, #999);
    font-size: 14px;
    font-style: italic;
  }

  .empty-panel-container p {
    margin: 0;
  }
</style>
