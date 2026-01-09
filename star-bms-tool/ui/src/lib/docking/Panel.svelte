<script>
  import { dockingStore } from '../stores/docking.js';
  import PanelHeader from './PanelHeader.svelte';
  import PacketViewerPanel from '../panels/PacketViewerPanel.svelte';
  import TerminalPanel from '../panels/TerminalPanel.svelte';
  import DeviceInfoPanel from '../panels/DeviceInfoPanel.svelte';
  import PropertiesPanel from '../panels/PropertiesPanel.svelte';

  let { panelId = '' } = $props();

  const panel = $derived($dockingStore.panels[panelId]);
</script>

{#if panel && !panel.floating}
  <div class="panel" data-panel-id={panelId}>
    <PanelHeader panelId={panelId} title={panel.title} />
    <div class="panel-body">
      {#if panelId === 'packet-viewer'}
        <PacketViewerPanel />
      {:else if panelId === 'terminal'}
        <TerminalPanel />
      {:else if panelId === 'device-info'}
        <DeviceInfoPanel />
      {:else if panelId === 'properties'}
        <PropertiesPanel />
      {:else}
        <div class="placeholder-panel">
          <h2>Unknown Panel</h2>
          <p>Panel "{panelId}" not found</p>
        </div>
      {/if}
    </div>
  </div>
{/if}

<style>
  .panel {
    display: flex;
    flex-direction: column;
    height: 100%;
    width: 100%;
    overflow: hidden;
    background: var(--bg-primary, #FFFFFF);
  }

  .panel-body {
    flex: 1;
    overflow: hidden;
    display: flex;
    flex-direction: column;
  }

  .placeholder-panel {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: var(--text-tertiary, #999);
    padding: 24px;
    text-align: center;
  }

  .placeholder-panel h2 {
    margin: 0 0 8px 0;
    font-size: 18px;
    font-weight: 600;
  }

  .placeholder-panel p {
    margin: 0;
    font-size: 14px;
  }
</style>
