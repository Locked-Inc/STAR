import type { CSSProperties } from 'react';

export const COLORS = {
  // We rely more on transparent glass now, but keep logical colors for charts/text
  panelBg: 'rgba(255, 255, 255, 0.04)',
  border: 'rgba(255, 255, 255, 0.1)',
  bodyBg: '#000000',
  textMuted: '#8E8E93', // System Gray
  textDim: '#AEAEB2',   // System Gray 2
  textPrimary: '#FFFFFF',

  // Accents (Apple System Colors)
  accent: '#0A84FF', // System Blue
  warning: '#FF9F0A', // System Orange

  // Semantic aliases for new panels
  primary: '#0A84FF',    // = accent (System Blue)
  success: '#30D158',    // = connected (System Green)
  danger: '#FF453A',     // = disconnected (System Red)

  // Statuses
  connected: '#30D158', // System Green
  connecting: '#FF9F0A', // System Orange
  reconnecting: '#FF9F0A',
  disconnected: '#FF453A', // System Red

  // E-Stop
  estopActive: '#8E0000', // Darker red for active state
  estopDefault: '#FF453A', // System Red
} as const;

export const PANEL_BORDER_RADIUS = '24px';
const PANEL_PADDING_VERTICAL = '16px';
const PANEL_PADDING_HORIZONTAL = '20px';
const PANEL_HEADER_FONT_SIZE = '12px';
const PANEL_HEADER_LETTER_SPACING = '0.1em';

export const PANEL_CONTAINER_STYLE: CSSProperties = {
  // The actual glass effect is handled by the className="glass-panel" in index.css
  // but we keep padding and basic structure here.
  padding: 0,
  display: 'flex',
  flexDirection: 'column',
};

export const PANEL_HEADER_STYLE: CSSProperties = {
  padding: `${PANEL_PADDING_VERTICAL} ${PANEL_PADDING_HORIZONTAL}`,
  borderBottom: `1px solid ${COLORS.border}`,
  fontSize: PANEL_HEADER_FONT_SIZE,
  fontWeight: 600,
  color: COLORS.textMuted,
  textTransform: 'uppercase',
  letterSpacing: PANEL_HEADER_LETTER_SPACING,
  background: 'rgba(0, 0, 0, 0.2)', // Slight darkening for header separation
};
