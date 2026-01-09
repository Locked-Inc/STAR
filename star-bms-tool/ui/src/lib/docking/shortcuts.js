// Keyboard shortcut handling for docking system

import { dockingStore } from '../stores/docking.js';

// Panel shortcuts mapping
const PANEL_SHORTCUTS = {
  '1': 'packet-viewer',
  '2': 'terminal',
  '3': 'device-info',
  '4': 'properties',
};

/**
 * Handle keyboard shortcuts for docking system
 * @param {KeyboardEvent} event - The keyboard event
 * @returns {boolean} - True if shortcut was handled, false otherwise
 */
export function handleKeyboardShortcut(event) {
  // Ctrl/Cmd + Number keys (1-4): Toggle panels
  if ((event.ctrlKey || event.metaKey) && !event.shiftKey && !event.altKey) {
    const panelId = PANEL_SHORTCUTS[event.key];
    if (panelId) {
      event.preventDefault();
      togglePanel(panelId);
      return true;
    }
  }

  // Ctrl/Cmd + Shift + P: Panel picker (future feature)
  if ((event.ctrlKey || event.metaKey) && event.shiftKey && event.key === 'P') {
    event.preventDefault();
    console.log('[SHORTCUTS] Panel picker not yet implemented');
    return true;
  }

  // Ctrl/Cmd + Shift + R: Reset layout
  if ((event.ctrlKey || event.metaKey) && event.shiftKey && event.key === 'R') {
    event.preventDefault();
    resetLayout();
    return true;
  }

  // Escape: Close active floating windows or focused panel
  if (event.key === 'Escape') {
    // Let individual components handle Escape for now
    return false;
  }

  return false;
}

/**
 * Toggle panel visibility
 * @param {string} panelId - The panel ID to toggle
 */
function togglePanel(panelId) {
  const state = dockingStore;
  let currentState;

  const unsubscribe = state.subscribe((s) => {
    currentState = s;
  });
  unsubscribe();

  const panel = currentState.panels[panelId];
  if (!panel) {
    console.warn(`[SHORTCUTS] Panel ${panelId} not found`);
    return;
  }

  if (panel.visible && !panel.floating) {
    // Hide panel if visible in main window
    console.log(`[SHORTCUTS] Hiding panel: ${panelId}`);
    dockingStore.hidePanel(panelId);
  } else if (!panel.visible) {
    // Show panel if hidden
    const defaultZone = panel.zone || 'bottom';
    console.log(`[SHORTCUTS] Showing panel: ${panelId} in ${defaultZone}`);
    dockingStore.showPanel(panelId, defaultZone);
  } else if (panel.floating) {
    // If floating, bring window to front (future feature)
    console.log(`[SHORTCUTS] Panel ${panelId} is floating`);
  }
}

/**
 * Reset layout to defaults
 */
function resetLayout() {
  console.log('[SHORTCUTS] Resetting layout to defaults');
  if (confirm('Reset panel layout to defaults? This will close all panels except Packet Viewer.')) {
    dockingStore.reset();
  }
}

/**
 * Get keyboard shortcut display text for a panel
 * @param {string} panelId - The panel ID
 * @returns {string} - The shortcut display text (e.g., "Ctrl+1")
 */
export function getShortcutText(panelId) {
  const isMac = navigator.platform.toUpperCase().indexOf('MAC') >= 0;
  const modifier = isMac ? '⌘' : 'Ctrl';

  const key = Object.keys(PANEL_SHORTCUTS).find(k => PANEL_SHORTCUTS[k] === panelId);
  if (key) {
    return `${modifier}+${key}`;
  }

  return '';
}
