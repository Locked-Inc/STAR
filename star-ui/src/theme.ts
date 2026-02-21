import type { CSSProperties } from 'react';

export const COLORS = {
  panelBg: '#1a1d27',
  border: '#2a2e42',
  bodyBg: '#0f1117',
  textMuted: '#9ca3af',
  textDim: '#6b7280',
  textPrimary: '#e2e8f0',
  accent: '#3b82f6',
  warning: '#f97316',
  connected: '#22c55e',
  connecting: '#eab308',
  reconnecting: '#f97316',
  disconnected: '#ef4444',
  estopActive: '#7f1d1d',
  estopDefault: '#dc2626',
} as const;

const PANEL_BORDER_RADIUS = '6px';
const PANEL_PADDING_VERTICAL = '6px';
const PANEL_PADDING_HORIZONTAL = '10px';
const PANEL_HEADER_FONT_SIZE = '11px';
const PANEL_HEADER_LETTER_SPACING = '0.05em';

export const PANEL_CONTAINER_STYLE: CSSProperties = {
  background: COLORS.panelBg,
  border: `1px solid ${COLORS.border}`,
  borderRadius: PANEL_BORDER_RADIUS,
  overflow: 'hidden',
};

export const PANEL_HEADER_STYLE: CSSProperties = {
  padding: `${PANEL_PADDING_VERTICAL} ${PANEL_PADDING_HORIZONTAL}`,
  borderBottom: `1px solid ${COLORS.border}`,
  fontSize: PANEL_HEADER_FONT_SIZE,
  fontWeight: 'bold',
  color: COLORS.textMuted,
  textTransform: 'uppercase',
  letterSpacing: PANEL_HEADER_LETTER_SPACING,
};
