import { writable, derived } from 'svelte/store';

const STORAGE_KEY = 'bms-tool-docking-layout';

// Default state
const DEFAULT_STATE = {
  zones: {
    top: {
      visible: false,
      size: 200, // px
      panels: [],
      splits: [],
    },
    bottom: {
      visible: true,
      size: 300,
      panels: ['packet-viewer'],
      splits: [],
    },
    left: {
      visible: false,
      size: 250,
      panels: [],
      splits: [],
    },
    right: {
      visible: false,
      size: 250,
      panels: [],
      splits: [],
    },
  },

  panels: {
    'packet-viewer': {
      id: 'packet-viewer',
      title: 'Packet Viewer',
      icon: null,
      zone: 'bottom',
      lastDockZone: 'bottom',
      visible: true,
      floating: false,
      floatingWindow: null,
    },
    'terminal': {
      id: 'terminal',
      title: 'Terminal',
      icon: null,
      zone: null,
      lastDockZone: 'bottom',
      visible: false,
      floating: false,
      floatingWindow: null,
    },
    'device-info': {
      id: 'device-info',
      title: 'Device Info',
      icon: null,
      zone: null,
      lastDockZone: 'right',
      visible: false,
      floating: false,
      floatingWindow: null,
    },
    'properties': {
      id: 'properties',
      title: 'Properties',
      icon: null,
      zone: null,
      lastDockZone: 'right',
      visible: false,
      floating: false,
      floatingWindow: null,
    },
  },

  dragState: {
    isDragging: false,
    panelId: null,
    sourceZone: null,
    didDrop: false,
    windowBounds: null,
  },
};

function mergeState(state) {
  return {
    ...DEFAULT_STATE,
    ...state,
    panels: { ...DEFAULT_STATE.panels, ...state.panels },
    zones: { ...DEFAULT_STATE.zones, ...state.zones },
    dragState: DEFAULT_STATE.dragState,
  };
}

// Load from localStorage
function loadState() {
  try {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved) {
      const parsed = JSON.parse(saved);
      // Merge with defaults to handle new panels added in updates
      return mergeState(parsed);
    }
  } catch (e) {
    console.error('Failed to load docking state:', e);
  }
  return DEFAULT_STATE;
}

// Create store
function createDockingStore() {
  const { subscribe, set, update } = writable(loadState());

  if (typeof window !== 'undefined') {
    window.addEventListener('storage', (event) => {
      if (event.key !== STORAGE_KEY || !event.newValue) {
        return;
      }

      try {
        const parsed = JSON.parse(event.newValue);
        set(mergeState(parsed));
      } catch (error) {
        console.error('Failed to sync docking state:', error);
      }
    });
  }

  // Auto-persist on every change (debounced)
  let saveTimeout;
  subscribe((state) => {
    clearTimeout(saveTimeout);
    saveTimeout = setTimeout(() => {
      try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
      } catch (e) {
        console.error('Failed to save docking state:', e);
      }
    }, 100); // Debounce 100ms
  });

  return {
    subscribe,

    // Zone operations
    setZoneSize: (zone, size) => update(s => {
      s.zones[zone].size = size;
      return s;
    }),

    toggleZoneVisibility: (zone) => update(s => {
      s.zones[zone].visible = !s.zones[zone].visible;
      return s;
    }),

    showZone: (zone) => update(s => {
      s.zones[zone].visible = true;
      return s;
    }),

    hideZone: (zone) => update(s => {
      s.zones[zone].visible = false;
      return s;
    }),

    // Panel operations
    showPanel: (panelId, zone = 'bottom') => update(s => {
      const panel = s.panels[panelId];
      if (!panel) {
        console.warn(`Panel ${panelId} not found`);
        return s;
      }

      const updatedZones = { ...s.zones };

      if (panel.zone && updatedZones[panel.zone]) {
        const updatedPanels = updatedZones[panel.zone].panels.filter(id => id !== panelId);
        updatedZones[panel.zone] = {
          ...updatedZones[panel.zone],
          panels: updatedPanels,
          visible: updatedPanels.length > 0 ? updatedZones[panel.zone].visible : false,
        };
      }

      const targetPanelList = Array.from(new Set([
        ...updatedZones[zone]?.panels || [],
        panelId,
      ]));
      updatedZones[zone] = {
        ...updatedZones[zone],
        panels: targetPanelList,
        visible: true,
      };

      return {
        ...s,
        zones: updatedZones,
        panels: {
          ...s.panels,
          [panelId]: {
            ...panel,
            zone,
            visible: true,
            floating: false,
            lastDockZone: zone,
          },
        },
      };
    }),

    hidePanel: (panelId) => update(s => {
      const panel = s.panels[panelId];
      if (!panel) {
        console.warn(`Panel ${panelId} not found`);
        return s;
      }

      const priorZone = panel.zone || panel.lastDockZone || null;
      const updatedZones = {};
      Object.entries(s.zones).forEach(([zoneKey, zoneState]) => {
        const filtered = zoneState.panels.filter(id => id !== panelId);
        updatedZones[zoneKey] = {
          ...zoneState,
          panels: filtered,
          visible: filtered.length > 0 ? zoneState.visible : false,
        };
      });

      return {
        ...s,
        zones: updatedZones,
        panels: {
          ...s.panels,
          [panelId]: {
            ...panel,
            visible: false,
            zone: null,
            floating: false,
            floatingWindow: null,
            lastDockZone: priorZone || panel.lastDockZone,
          },
        },
      };
    }),

    movePanel: (panelId, targetZone, position = 'end') => update(s => {
      const panel = s.panels[panelId];
      if (!panel) {
        console.warn(`Panel ${panelId} not found`);
        return s;
      }

      const updatedZones = { ...s.zones };

      if (panel.zone && updatedZones[panel.zone]) {
        const filtered = updatedZones[panel.zone].panels.filter(id => id !== panelId);
        updatedZones[panel.zone] = {
          ...updatedZones[panel.zone],
          panels: filtered,
          visible: filtered.length > 0 ? updatedZones[panel.zone].visible : false,
        };
      }

      const targetPanels = Array.from(new Set([
        ...(updatedZones[targetZone]?.panels || []),
      ]));
      if (position === 'start') {
        targetPanels.unshift(panelId);
      } else if (!targetPanels.includes(panelId)) {
        targetPanels.push(panelId);
      }

      updatedZones[targetZone] = {
        ...updatedZones[targetZone],
        panels: targetPanels,
        visible: true,
      };

      return {
        ...s,
        zones: updatedZones,
        panels: {
          ...s.panels,
          [panelId]: {
            ...panel,
            zone: targetZone,
            visible: true,
            lastDockZone: targetZone,
          },
        },
      };
    }),

    // Drag state
    startDrag: (panelId, sourceZone, windowBounds = null) => update(s => {
      return {
        ...s,
        dragState: {
          isDragging: true,
          panelId,
          sourceZone,
          didDrop: false,
          windowBounds,
        },
      };
    }),

    markDrop: () => update(s => {
      if (!s.dragState.isDragging) {
        return s;
      }
      return {
        ...s,
        dragState: {
          ...s.dragState,
          didDrop: true,
        },
      };
    }),

    endDrag: () => update(s => {
      return {
        ...s,
        dragState: {
          isDragging: false,
          panelId: null,
          sourceZone: null,
          didDrop: false,
          windowBounds: null,
        },
      };
    }),

    // Floating windows (Phase 5)
    floatPanel: (panelId, windowLabel) => update(s => {
      const panel = s.panels[panelId];
      if (!panel) {
        console.warn(`Panel ${panelId} not found`);
        return s;
      }

      const updatedZones = {};
      Object.entries(s.zones).forEach(([zoneKey, zoneState]) => {
        const filtered = zoneState.panels.filter(id => id !== panelId);
        updatedZones[zoneKey] = {
          ...zoneState,
          panels: filtered,
          visible: filtered.length > 0 ? zoneState.visible : false,
        };
      });

      return {
        ...s,
        zones: updatedZones,
        panels: {
          ...s.panels,
          [panelId]: {
            ...panel,
            lastDockZone: panel.zone || panel.lastDockZone || 'bottom',
            floating: true,
            floatingWindow: windowLabel,
            visible: false,
            zone: null,
          },
        },
      };
    }),

    unfloatPanel: (panelId, targetZone) => update(s => {
      const panel = s.panels[panelId];
      if (!panel) {
        console.warn(`Panel ${panelId} not found`);
        return s;
      }

      const resolvedZone = targetZone || panel.lastDockZone || 'bottom';
      const updatedZones = { ...s.zones };
      const existing = Array.from(new Set([
        ...(updatedZones[resolvedZone]?.panels || []),
        panelId,
      ]));

      updatedZones[resolvedZone] = {
        ...updatedZones[resolvedZone],
        panels: existing,
        visible: true,
      };

      return {
        ...s,
        zones: updatedZones,
        panels: {
          ...s.panels,
          [panelId]: {
            ...panel,
            floating: false,
            floatingWindow: null,
            visible: true,
            zone: resolvedZone,
          },
        },
      };
    }),

    // Reset to defaults
    reset: () => {
      set({ ...DEFAULT_STATE });
    },
  };
}

export const dockingStore = createDockingStore();

// Derived stores for convenience
export const visiblePanels = derived(
  dockingStore,
  $docking => Object.values($docking.panels).filter(p => p.visible && !p.floating)
);

export const isDragging = derived(
  dockingStore,
  $docking => $docking.dragState.isDragging
);

export const draggedPanel = derived(
  dockingStore,
  $docking => $docking.dragState.panelId
);
