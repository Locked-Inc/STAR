export const COLORS = {
  // Semantic aliases - accent colors stay the same in both themes
  accent: '#0A84FF',
  warning: '#FF9F0A',
  primary: '#0A84FF',
  success: '#30D158',
  danger: '#FF453A',

  // Statuses
  connected: '#30D158',
  connecting: '#FF9F0A',
  reconnecting: '#FF9F0A',
  disconnected: '#FF453A',

  // E-Stop
  estopActive: '#8E0000',
  estopDefault: '#FF453A',

  // Theme-aware - these reference CSS custom properties
  // Use them for inline style strings, not CSSProperties
  textPrimary: 'var(--color-text)',
  textMuted: 'var(--color-text-muted)',
  textDim: 'var(--color-text-dim)',
  panelBg: 'var(--panel-bg)',
  border: 'var(--panel-header-border)',
  bodyBg: 'var(--color-body-bg)',
} as const;
