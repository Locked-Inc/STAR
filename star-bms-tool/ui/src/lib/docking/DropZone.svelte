<script>
  let { zone = '', active = false } = $props();
</script>

<div
  class="drop-zone drop-zone-{zone}"
  class:hovering={active}
  role="region"
  aria-label="Drop zone for {zone}"
>
  <div class="drop-zone-content">
    <div class="drop-zone-icon">
      {#if zone === 'top'}
        <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M12 19V5M5 12l7-7 7 7"/>
        </svg>
      {:else if zone === 'bottom'}
        <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M12 5v14M5 12l7 7 7-7"/>
        </svg>
      {:else if zone === 'left'}
        <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M19 12H5M12 5l-7 7 7 7"/>
        </svg>
      {:else if zone === 'right'}
        <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M5 12h14M12 5l7 7-7 7"/>
        </svg>
      {:else}
        <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <rect x="3" y="3" width="18" height="18" rx="2"/>
        </svg>
      {/if}
    </div>
    <div class="drop-zone-text">
      Drop here
    </div>
  </div>
</div>

<style>
  .drop-zone {
    position: absolute;
    background: var(--drop-zone-bg, rgba(37, 99, 235, 0.1));
    border: 2px dashed var(--drop-zone-border, #2563EB);
    border-radius: 8px;
    display: flex;
    align-items: center;
    justify-content: center;
    pointer-events: auto;
    z-index: 1000;
    transition: transform 0.12s ease, opacity 0.12s ease, background 0.12s ease, border-color 0.12s ease;
    will-change: transform, opacity;
    opacity: 0.8;
  }

  .drop-zone.hovering {
    background: var(--drop-zone-hover-bg, rgba(37, 99, 235, 0.2));
    border-color: var(--color-accent-600, #1D4ED8);
    opacity: 1;
    transform: scale(1.02);
  }

  .drop-zone-top {
    top: 20px;
    left: 20%;
    right: 20%;
    height: 120px;
  }

  .drop-zone-bottom {
    bottom: 20px;
    left: 20%;
    right: 20%;
    height: 120px;
  }

  .drop-zone-left {
    top: 20%;
    bottom: 20%;
    left: 20px;
    width: 120px;
  }

  .drop-zone-right {
    top: 20%;
    bottom: 20%;
    right: 20px;
    width: 120px;
  }

  .drop-zone-center {
    top: 30%;
    bottom: 30%;
    left: 30%;
    right: 30%;
  }

  .drop-zone-content {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    pointer-events: none;
  }

  .drop-zone-icon {
    color: var(--color-accent-500, #2563EB);
  }

  .drop-zone-text {
    font-size: 14px;
    font-weight: 600;
    color: var(--color-accent-600, #1D4ED8);
  }
</style>
