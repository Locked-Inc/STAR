import type { CSSProperties } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import type { ConnectionState } from '../store/dashboardStore';
import { COLORS } from '../theme';

interface StatusBarProps {
  sendEStop: (reason: string) => void;
}

const ESTOP_SOURCE_USER_UI = 'user_ui_button';

const DOT_COLORS: Record<ConnectionState, string> = {
  connected: COLORS.connected,
  connecting: COLORS.connecting,
  reconnecting: COLORS.reconnecting,
  disconnected: COLORS.disconnected,
};

const STATUS_BAR_STYLE: CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  gap: '12px',
  padding: '6px 12px',
  background: COLORS.panelBg,
  borderBottom: `1px solid ${COLORS.border}`,
  fontSize: '12px',
};

export function StatusBar({ sendEStop }: StatusBarProps) {
  const { connectionState, dataIsStale, eStopActive } = useDashboardStore(
    useShallow((s) => ({
      connectionState: s.connectionState,
      dataIsStale: s.dataIsStale,
      eStopActive: s.eStopActive,
    }))
  );

  const dotColor = DOT_COLORS[connectionState] ?? COLORS.disconnected;

  function handleEStop(): void {
    sendEStop(ESTOP_SOURCE_USER_UI);
    useDashboardStore.getState().triggerEStop();
  }

  return (
    <div style={STATUS_BAR_STYLE}>
      <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
        <span
          style={{
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            backgroundColor: dotColor,
            display: 'inline-block',
          }}
        />
        {connectionState}
      </span>

      {dataIsStale && (
        <span style={{ color: COLORS.warning, fontWeight: 'bold' }}>STALE</span>
      )}

      <span style={{ flex: 1 }} />

      <button
        type="button"
        onClick={handleEStop}
        disabled={eStopActive}
        aria-label={eStopActive ? 'Emergency stop active' : 'Activate emergency stop'}
        style={{
          padding: '4px 12px',
          background: eStopActive ? COLORS.estopActive : COLORS.estopDefault,
          color: '#fff',
          border: 'none',
          borderRadius: '4px',
          cursor: eStopActive ? 'not-allowed' : 'pointer',
          fontWeight: 'bold',
          fontSize: '12px',
        }}
      >
        {eStopActive ? 'E-STOP ACTIVE' : 'E-STOP'}
      </button>
    </div>
  );
}
