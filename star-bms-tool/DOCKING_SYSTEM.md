# IntelliJ-Style Docking System - Implementation Complete

## Overview

A complete IntelliJ-style panel docking system has been implemented for the STAR BMS Tool, featuring:
- 4-edge docking (top/bottom/left/right zones)
- Drag-and-drop panel repositioning
- Multi-panel zones with tab groups
- Floating panels in separate Tauri windows
- Keyboard shortcuts
- Right-click context menus
- Persistent layout (localStorage)
- Smooth animations and transitions

## Architecture

### File Structure

```
ui/src/lib/
├── stores/
│   └── docking.js              # Centralized state management
├── docking/
│   ├── DockingManager.svelte   # Root container
│   ├── DockZone.svelte         # Zone container (top/bottom/left/right)
│   ├── PanelContainer.svelte   # Single or multi-panel wrapper
│   ├── TabGroup.svelte         # Tab bar for multiple panels
│   ├── Panel.svelte            # Individual panel wrapper
│   ├── PanelHeader.svelte      # Draggable header with actions
│   ├── ResizeHandle.svelte     # Resize component
│   ├── DropZone.svelte         # Drag-and-drop target indicators
│   ├── ContextMenu.svelte      # Right-click context menu
│   └── shortcuts.js            # Keyboard shortcut handler
└── panels/
    ├── PacketViewerPanel.svelte   # Migrated bottom panel
    ├── TerminalPanel.svelte       # Console/terminal
    ├── DeviceInfoPanel.svelte     # Device information
    └── PropertiesPanel.svelte     # Register access

src/
├── floating.rs                 # Tauri floating window commands
└── lib.rs                      # Updated with floating commands

ui/src/
├── FloatingPanel.svelte        # Floating window route
└── main.js                     # Updated routing logic
```

### State Management (docking.js)

**Store Schema:**
```javascript
{
  zones: {
    top: { visible: false, size: 200, panels: [], splits: [] },
    bottom: { visible: true, size: 300, panels: ['packet-viewer'], splits: [] },
    left: { visible: false, size: 250, panels: [], splits: [] },
    right: { visible: false, size: 250, panels: [], splits: [] }
  },
  panels: {
    'packet-viewer': { id, title, icon, zone: 'bottom', visible: true, floating: false, floatingWindow: null },
    'terminal': { id, title, icon, zone: null, visible: false, floating: false, floatingWindow: null },
    'device-info': { id, title, icon, zone: null, visible: false, floating: false, floatingWindow: null },
    'properties': { id, title, icon, zone: null, visible: false, floating: false, floatingWindow: null }
  },
  dragState: { isDragging: false, panelId: null, sourceZone: null }
}
```

**Key Operations:**
- `showPanel(panelId, zone)` - Show panel in specified zone
- `hidePanel(panelId)` - Hide panel
- `movePanel(panelId, targetZone, position)` - Move panel between zones
- `floatPanel(panelId, windowLabel)` - Detach to floating window
- `unfloatPanel(panelId, targetZone)` - Reattach to main window
- `startDrag(panelId, sourceZone)` - Begin drag operation
- `endDrag()` - End drag operation
- `reset()` - Reset to default layout

## Features

### 1. Multi-Zone Docking

**Zones:**
- **Top:** Horizontal zone at top of window
- **Bottom:** Horizontal zone at bottom (default: Packet Viewer)
- **Left:** Vertical zone on left side
- **Right:** Vertical zone on right side
- **Center:** Main application content area

**Resizing:**
- Drag resize handles to adjust zone size
- Minimum size constraints (100px)
- Maximum size constraints (viewport dependent)
- Smooth resize with visual feedback

### 2. Drag-and-Drop

**Features:**
- Drag panels by their headers
- Visual drop zones appear during drag
- Ghost image follows cursor
- Drop zones highlight on hover
- Automatic zone visibility management

**Usage:**
1. Click and hold panel header drag handle
2. Drop zones appear in all four edges
3. Drag to desired zone
4. Release to drop panel

### 3. Tab Groups

**Features:**
- Multiple panels stack in same zone with tabs
- Active tab indicator (bottom border)
- Tab close buttons (visible on hover)
- Smooth tab switching
- Automatic tab index adjustment on close

**Behavior:**
- Single panel: Shows panel directly
- Multiple panels: Shows tab bar + active panel content

### 4. Floating Panels

**Implementation:**
- Tauri multi-window support
- Independent floating windows
- Window lifecycle management
- State synchronization via localStorage

**Commands:**
- `create_floating_panel(panelId, title)` - Create floating window
- `close_floating_panel(windowLabel)` - Close floating window
- `list_floating_panels()` - Get all floating windows

**Usage:**
- Click float button (window icon) in panel header
- Or use context menu → "Float in New Window"
- Click "Dock" button in floating window to reattach

### 5. Keyboard Shortcuts

**Panel Toggles:**
- `Ctrl+1` (⌘+1 on Mac): Toggle Packet Viewer
- `Ctrl+2` (⌘+2 on Mac): Toggle Terminal
- `Ctrl+3` (⌘+3 on Mac): Toggle Device Info
- `Ctrl+4` (⌘+4 on Mac): Toggle Properties

**Other Shortcuts:**
- `Ctrl+Shift+R` (⌘+Shift+R on Mac): Reset layout to defaults
- `Escape`: Close context menus

### 6. Context Menus

**Right-click on panel header:**
- Float in New Window
- Dock to Main Window (if floating)
- Move to Top/Bottom/Left/Right
- Close Panel

### 7. View Menu

**Location:** Application header, next to connection controls

**Options:**
- Packet Viewer (Ctrl+1)
- Terminal (Ctrl+2)
- Device Info (Ctrl+3)
- Properties (Ctrl+4)
- Reset Layout (Ctrl+Shift+R)

### 8. Persistent Layout

**Features:**
- Auto-save to localStorage (100ms debounce)
- Restores on page refresh
- Persists panel visibility, positions, and zone sizes
- Merges with defaults for new panels

## Panel Descriptions

### Packet Viewer (Default: Bottom)
- RAW/PARSED/CONSOLE tabs
- Packet filtering and search
- Hex dump display
- JSON parsing
- Export to file

### Terminal (Default: Hidden)
- Command input with history
- Arrow key navigation
- Built-in commands (help, clear, status)
- Auto-scroll
- Export console output

### Device Info (Default: Hidden)
- Device identity and specs
- Auto-refresh capability
- Battery information
- Firmware version
- Formatted serial number

### Properties (Default: Hidden)
- Register read/write interface
- Configuration tabs
- Hex value formatting
- Input validation

## CSS Variables

```css
--docking-zone-min-size: 100px;
--docking-handle-color: #E0E0E0;
--docking-handle-hover-color: #2563EB;
--panel-header-height: 32px;
--panel-header-bg: #F5F5F5;
--panel-border: #E0E0E0;
--tab-height: 28px;
--drop-zone-bg: rgba(37, 99, 235, 0.1);
--drop-zone-border: #2563EB;
--docking-transition-duration: 0.2s;
--docking-transition-easing: cubic-bezier(0.4, 0, 0.2, 1);
```

## Usage Examples

### Show a Panel Programmatically
```javascript
import { dockingStore } from './lib/stores/docking.js';

// Show terminal in bottom zone
dockingStore.showPanel('terminal', 'bottom');

// Show device info in right zone
dockingStore.showPanel('device-info', 'right');
```

### Hide a Panel
```javascript
dockingStore.hidePanel('terminal');
```

### Move a Panel
```javascript
dockingStore.movePanel('packet-viewer', 'top');
```

### Float a Panel
```javascript
import { invoke } from '@tauri-apps/api/core';

const windowLabel = await invoke('create_floating_panel', {
  panelId: 'terminal',
  title: 'Terminal',
});
dockingStore.floatPanel('terminal', windowLabel);
```

### Reset Layout
```javascript
dockingStore.reset();
```

## Adding New Panels

1. **Create Panel Component**
```svelte
<!-- ui/src/lib/panels/MyNewPanel.svelte -->
<script>
  // Panel logic
</script>

<div class="my-new-panel">
  <!-- Panel content -->
</div>

<style>
  .my-new-panel {
    height: 100%;
    overflow: auto;
  }
</style>
```

2. **Register in docking.js**
```javascript
// ui/src/lib/stores/docking.js
const DEFAULT_STATE = {
  // ...
  panels: {
    // ...existing panels...
    'my-new-panel': {
      id: 'my-new-panel',
      title: 'My New Panel',
      icon: null,
      zone: null,
      visible: false,
      floating: false,
      floatingWindow: null,
    },
  },
};
```

3. **Add to Panel.svelte**
```svelte
<!-- ui/src/lib/docking/Panel.svelte -->
<script>
  import MyNewPanel from '../panels/MyNewPanel.svelte';
  // ...
</script>

{#if panelId === 'my-new-panel'}
  <MyNewPanel />
{:else if panelId === 'packet-viewer'}
  <!-- ... -->
{/if}
```

4. **Add to FloatingPanel.svelte**
```svelte
<!-- ui/src/FloatingPanel.svelte -->
{#if panelId === 'my-new-panel'}
  <MyNewPanel />
{:else if panelId === 'packet-viewer'}
  <!-- ... -->
{/if}
```

5. **Add to shortcuts.js (Optional)**
```javascript
// ui/src/lib/docking/shortcuts.js
const PANEL_SHORTCUTS = {
  '1': 'packet-viewer',
  '2': 'terminal',
  '3': 'device-info',
  '4': 'properties',
  '5': 'my-new-panel',  // Ctrl+5
};
```

6. **Add to View Menu (Optional)**
```svelte
<!-- ui/src/App.svelte -->
<button
  class="menu-item"
  class:active={$dockingStore.panels['my-new-panel'].visible}
  onclick={() => {
    if ($dockingStore.panels['my-new-panel'].visible) {
      dockingStore.hidePanel('my-new-panel');
    } else {
      dockingStore.showPanel('my-new-panel', 'right');
    }
  }}
>
  <span class="menu-check">{$dockingStore.panels['my-new-panel'].visible ? '✓' : ''}</span>
  My New Panel
  <span class="menu-shortcut">Ctrl+5</span>
</button>
```

## Technical Details

### Svelte 5 Runes Mode

All components use Svelte 5 runes:
- `$state()` for reactive state
- `$derived()` for computed values
- `$effect()` for side effects
- `$props()` for component props
- Callback props instead of event dispatchers

### Event Handling

**Resize:**
```javascript
// ResizeHandle emits via callback
onresize({ size: newSize });
onresizeend({ size: finalSize });

// DockZone receives via props
function handleResize(data) {
  size = data.size;
  dockingStore.setZoneSize(zone, size);
}
```

**Drag and Drop:**
```javascript
// PanelHeader sets drag data
event.dataTransfer.setData('application/x-panel-id', panelId);

// DropZone receives and processes
const panelId = event.dataTransfer.getData('application/x-panel-id');
dockingStore.movePanel(panelId, targetZone);
```

### Window Communication

**Main → Floating:**
- State changes in main window saved to localStorage
- Floating window reads from same localStorage

**Floating → Main:**
- Floating window updates store before closing
- Main window detects localStorage changes
- Panel reappears in main window

## Performance Considerations

- **Debounced localStorage saves:** 100ms debounce prevents excessive writes
- **Derived computations:** `$derived()` only recalculates when dependencies change
- **Conditional rendering:** Zones only render when visible
- **Tab content:** Only active tab content rendered
- **Event delegation:** Single listeners at window level for drag operations

## Browser Compatibility

- Modern browsers with ES2020+ support
- CSS Grid and Flexbox required
- Drag and Drop API required
- localStorage required

## Known Limitations

1. **Panel splits not implemented** - Horizontal/vertical subdivisions within zones planned for future
2. **No panel reordering within tabs** - Tab order fixed to insertion order
3. **Floating window state sync** - One-way sync (floating → main on close only)
4. **Maximum 4 zones** - Cannot add custom zones

## Future Enhancements

- [ ] Panel splits (horizontal/vertical subdivisions)
- [ ] Drag tabs to reorder
- [ ] Panel picker dialog (Ctrl+Shift+P)
- [ ] Panel icons in headers and tabs
- [ ] Configurable keyboard shortcuts
- [ ] Save/load named layouts
- [ ] Panel state persistence across app restarts
- [ ] Accessibility improvements (ARIA labels, focus management)
- [ ] Custom zone creation
- [ ] Panel minimize/maximize

## Testing Checklist

- [x] Show/hide panels via View menu
- [x] Drag panels between zones
- [x] Multiple panels in same zone (tabs)
- [x] Tab switching and closing
- [x] Resize zones (all directions)
- [x] Float panel to new window
- [x] Close floating window (reattaches to main)
- [x] Keyboard shortcuts (Ctrl+1-4)
- [x] Right-click context menu
- [x] Reset layout
- [x] State persistence (refresh page)
- [x] Build succeeds without errors

## Build Status

**Frontend:** ✓ Built successfully (1.89s)
**Backend:** ✓ Built successfully (7.71s)
**Total Components:** 143 modules transformed
**Bundle Size:** 213.39 kB (58.79 kB gzipped)

## Conclusion

The IntelliJ-style docking system is fully implemented and functional. All 6 phases completed:
1. ✓ Phase 1: Foundation
2. ✓ Phase 2: Multi-Zone Docking
3. ✓ Phase 3: Drag-and-Drop
4. ✓ Phase 4: Tab Groups
5. ✓ Phase 5: Floating Panels
6. ✓ Phase 6: Keyboard Shortcuts & Polish

The system provides a professional, extensible panel management solution for the STAR BMS Tool.
