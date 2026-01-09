<script>
  let {
    orientation = 'horizontal', // 'horizontal' | 'vertical'
    handlePosition = 'top', // 'top' | 'bottom' | 'left' | 'right'
    minSize = 100,
    maxSize = null, // null = use window size * maxPercent
    maxPercent = 0.8,
    currentSize = 300,
    onresize = () => {},
    onresizeend = () => {}
  } = $props();

  let isDragging = $state(false);
  let startPos = $state(0);
  let startSize = $state(0);

  const isHorizontal = $derived(orientation === 'horizontal');
  const isVertical = $derived(orientation === 'vertical');

  function handleMouseDown(event) {
    isDragging = true;
    startPos = isHorizontal ? event.clientY : event.clientX;
    startSize = currentSize;
    event.preventDefault();
  }

  function handleMouseMove(event) {
    if (!isDragging) return;

    const currentPos = isHorizontal ? event.clientY : event.clientX;
    const delta = (() => {
      if (isHorizontal) {
        return handlePosition === 'bottom'
          ? currentPos - startPos  // Drag down increases height for top zone
          : startPos - currentPos; // Drag up increases height for bottom zone
      }
      return handlePosition === 'left'
        ? startPos - currentPos    // Drag left increases width for right zone
        : currentPos - startPos;   // Drag right increases width for left zone
    })();

    const maxAllowed = maxSize !== null
      ? maxSize
      : (isHorizontal ? window.innerHeight : window.innerWidth) * maxPercent;

    const newSize = Math.max(minSize, Math.min(startSize + delta, maxAllowed));

    onresize({ size: newSize });
  }

  function handleMouseUp() {
    if (isDragging) {
      isDragging = false;
      onresizeend({ size: currentSize });
    }
  }

  function handleKeyDown(event) {
    if (event.key === 'Escape' && isDragging) {
      isDragging = false;
      onresize({ size: startSize }); // Reset to start size
    }
  }
</script>

<svelte:window
  onmousemove={handleMouseMove}
  onmouseup={handleMouseUp}
  onkeydown={handleKeyDown}
/>

<!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
<!-- svelte-ignore a11y_no_noninteractive_tabindex -->
<div
  class="resize-handle resize-handle-{orientation}"
  class:dragging={isDragging}
  onmousedown={handleMouseDown}
  onkeydown={handleKeyDown}
  role="separator"
  tabindex="0"
  aria-label="Resize panel"
  aria-orientation={orientation}
  aria-valuenow={currentSize}
  aria-valuemin={minSize}
  aria-valuemax={maxSize || Math.floor((isHorizontal ? window.innerHeight : window.innerWidth) * maxPercent)}
>
  <div class="resize-handle-grip"></div>
</div>

<style>
  .resize-handle {
    background: var(--docking-handle-color, #E0E0E0);
    transition: background 0.15s ease;
    position: relative;
    flex-shrink: 0;
    user-select: none;
    border: none;
    padding: 0;
  }

  .resize-handle-horizontal {
    height: 8px;
    width: 100%;
    cursor: ns-resize;
  }

  .resize-handle-vertical {
    width: 8px;
    height: 100%;
    cursor: ew-resize;
  }

  .resize-handle::before {
    content: '';
    position: absolute;
    top: -6px;
    bottom: -6px;
    left: -6px;
    right: -6px;
  }

  .resize-handle:hover,
  .resize-handle:focus,
  .resize-handle.dragging {
    background: var(--docking-handle-hover-color, #2563EB);
  }

  .resize-handle:focus {
    outline: 2px solid var(--color-accent-500, #2563EB);
    outline-offset: 2px;
  }

  .resize-handle-grip {
    position: absolute;
    background: currentColor;
    opacity: 0.3;
  }

  .resize-handle-horizontal .resize-handle-grip {
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 40px;
    height: 2px;
  }

  .resize-handle-vertical .resize-handle-grip {
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 2px;
    height: 40px;
  }

  .resize-handle.dragging {
    cursor: ns-resize;
  }

  .resize-handle-vertical.dragging {
    cursor: ew-resize;
  }
</style>
