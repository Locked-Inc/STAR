import { useDashboardStore } from '../store/dashboardStore';

interface StatusBarProps {
  sendEStop: (reason: string) => void;
}

const DOT_COLORS: Record<string, string> = {
  connected: '#22c55e',
  connecting: '#eab308',
  reconnecting: '#f97316',
  disconnected: '#ef4444',
};

export function StatusBar({ sendEStop }: StatusBarProps) {
  const connectionState = useDashboardStore((s) => s.connectionState);
  const dataIsStale = useDashboardStore((s) => s.dataIsStale);
  const eStopActive = useDashboardStore((s) => s.eStopActive);

  const dotColor = DOT_COLORS[connectionState] ?? '#ef4444';

  function handleEStop(): void {
    sendEStop('user_ui_button');
    useDashboardStore.getState().triggerEStop();
  }

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        padding: '6px 12px',
        background: '#1a1d27',
        borderBottom: '1px solid #2a2e42',
        fontSize: '12px',
      }}
    >
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
        <span style={{ color: '#f97316', fontWeight: 'bold' }}>STALE</span>
      )}

      <span style={{ flex: 1 }} />

      <button
        onClick={handleEStop}
        disabled={eStopActive}
        style={{
          padding: '4px 12px',
          background: eStopActive ? '#7f1d1d' : '#dc2626',
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
