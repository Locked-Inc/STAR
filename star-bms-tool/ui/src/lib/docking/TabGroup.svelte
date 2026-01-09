<script>
  import { dockingStore } from '../stores/docking.js';
  import Panel from './Panel.svelte';

  let { panelIds = [] } = $props(); // ['packet-viewer', 'terminal']

  let activeTabIndex = $state(0);
  const activePanelId = $derived(panelIds[activeTabIndex] || null);

  // Update active tab if current panel is removed
  $effect(() => {
    if (activeTabIndex >= panelIds.length && panelIds.length > 0) {
      activeTabIndex = panelIds.length - 1;
    }
  });

  function handleTabClick(index) {
    activeTabIndex = index;
  }

  function handleTabClose(event, panelId, index) {
    event.stopPropagation(); // Prevent tab selection
    console.log(`[TAB] Closing tab: ${panelId}`);
    dockingStore.hidePanel(panelId);

    // Adjust active tab index if needed
    if (activeTabIndex === index && panelIds.length > 1) {
      if (index > 0) {
        activeTabIndex = index - 1;
      }
      // If index === 0, activeTabIndex stays 0 (next tab becomes active)
    }
  }

  function getPanelTitle(panelId) {
    return $dockingStore.panels[panelId]?.title || panelId;
  }
</script>

<div class="tab-group">
  <div class="tab-bar" role="tablist">
    {#each panelIds as panelId, index}
      <div
        class="tab"
        class:active={index === activeTabIndex}
        role="tab"
        tabindex="0"
        aria-selected={index === activeTabIndex}
        aria-controls="panel-{panelId}"
        onclick={() => handleTabClick(index)}
        onkeydown={(e) => { if (e.key === 'Enter' || e.key === ' ') handleTabClick(index); }}
      >
        <span class="tab-title">{getPanelTitle(panelId)}</span>
        <button
          type="button"
          class="tab-close"
          onclick={(e) => handleTabClose(e, panelId, index)}
          title="Close {getPanelTitle(panelId)}"
          aria-label="Close {getPanelTitle(panelId)}"
        >
          <svg width="10" height="10" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
            <path d="M18 6L6 18M6 6l12 12"/>
          </svg>
        </button>
      </div>
    {/each}
  </div>

  <div class="tab-content" role="tabpanel" id="panel-{activePanelId}">
    {#if activePanelId}
      <Panel panelId={activePanelId} />
    {/if}
  </div>
</div>

<style>
  .tab-group {
    display: flex;
    flex-direction: column;
    height: 100%;
    width: 100%;
    overflow: hidden;
  }

  .tab-bar {
    display: flex;
    align-items: center;
    background: var(--bg-tertiary, #E5E7EB);
    border-bottom: 1px solid var(--panel-border, #E0E0E0);
    flex-shrink: 0;
    overflow-x: auto;
    overflow-y: hidden;
  }

  .tab-bar::-webkit-scrollbar {
    height: 4px;
  }

  .tab-bar::-webkit-scrollbar-track {
    background: transparent;
  }

  .tab-bar::-webkit-scrollbar-thumb {
    background: var(--bg-quaternary, #D1D5DB);
    border-radius: 2px;
  }

  .tab {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 6px 12px;
    font-size: 13px;
    font-weight: 500;
    border: none;
    border-right: 1px solid var(--panel-border, #E0E0E0);
    background: transparent;
    color: var(--text-secondary, #6B7280);
    cursor: pointer;
    transition: all 0.15s ease;
    white-space: nowrap;
    position: relative;
  }

  .tab:hover {
    background: var(--bg-quaternary, #D1D5DB);
    color: var(--text-primary, #1F2937);
  }

  .tab.active {
    background: var(--bg-primary, #FFFFFF);
    color: var(--text-primary, #1F2937);
    font-weight: 600;
  }

  .tab.active::after {
    content: '';
    position: absolute;
    bottom: 0;
    left: 0;
    right: 0;
    height: 2px;
    background: var(--color-accent-500, #2563EB);
  }

  .tab-title {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .tab-close {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 16px;
    height: 16px;
    padding: 0;
    border: none;
    background: transparent;
    border-radius: 3px;
    color: var(--text-tertiary, #9CA3AF);
    cursor: pointer;
    transition: all 0.15s ease;
    opacity: 0;
  }

  .tab:hover .tab-close,
  .tab.active .tab-close {
    opacity: 1;
  }

  .tab-close:hover {
    background: var(--bg-quaternary, #D1D5DB);
    color: var(--text-primary, #1F2937);
  }

  .tab-content {
    flex: 1;
    overflow: hidden;
    min-height: 0;
  }
</style>
