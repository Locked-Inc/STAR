import type { CSSProperties } from 'react';

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

export const panelBorderRadius = '24px';
const PANEL_PADDING_VERTICAL = '16px';
const PANEL_PADDING_HORIZONTAL = '20px';
const PANEL_HEADER_FONT_SIZE = '12px';
const PANEL_HEADER_LETTER_SPACING = '0.1em';

export const PANEL_CONTAINER_STYLE: CSSProperties = {
  padding: 0,
  display: 'flex',
  flexDirection: 'column',
};

export const PANEL_HEADER_STYLE: CSSProperties = {
  padding: `${PANEL_PADDING_VERTICAL} ${PANEL_PADDING_HORIZONTAL}`,
  borderBottom: '1px solid var(--panel-header-border)',
  fontSize: PANEL_HEADER_FONT_SIZE,
  fontWeight: 600,
  color: 'var(--color-text-muted)',
  textTransform: 'uppercase',
  letterSpacing: PANEL_HEADER_LETTER_SPACING,
  background: 'var(--panel-header-bg)',
};
