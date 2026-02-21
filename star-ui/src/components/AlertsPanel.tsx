import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import { AlertLevel } from '../proto/star/v1/ui';
import type { Alert } from '../proto/star/v1/ui';

const LEVEL_COLORS: Record<number, string> = {
  [AlertLevel.UNKNOWN]: '#6b7280',
  [AlertLevel.INFO]: '#3b82f6',
  [AlertLevel.WARN]: '#f59e0b',
  [AlertLevel.ERROR]: '#ef4444',
  [AlertLevel.CRITICAL]: '#dc2626',
};

const LEVEL_NAMES: Record<number, string> = {
  [AlertLevel.UNKNOWN]: 'UNK',
  [AlertLevel.INFO]: 'INFO',
  [AlertLevel.WARN]: 'WARN',
  [AlertLevel.ERROR]: 'ERR',
  [AlertLevel.CRITICAL]: 'CRIT',
};

const US_PER_MS = 1000n;
const MS_PER_SEC = 1000n;
const MS_PER_MIN = 60n * MS_PER_SEC;

function formatAge(timestampUs: string): string {
  try {
    const timestampMs = BigInt(timestampUs) / US_PER_MS;
    let ageMs = BigInt(Date.now()) - timestampMs;
    if (ageMs < 0n) ageMs = 0n;  // clock skew guard
    if (ageMs < MS_PER_SEC) return `${Number(ageMs)}ms`;
    if (ageMs < MS_PER_MIN) return `${(Number(ageMs) / Number(MS_PER_SEC)).toFixed(0)}s`;
    return `${(Number(ageMs) / Number(MS_PER_MIN)).toFixed(0)}m`;
  } catch (err) {
    console.debug('formatAge: invalid timestampUs', timestampUs, err);
    return '--';
  }
}

function AlertRow({ alert }: { alert: Alert }) {
  const color = LEVEL_COLORS[alert.level] ?? '#6b7280';
  const levelName = LEVEL_NAMES[alert.level] ?? '???';

  return (
    <div
      style={{
        display: 'flex',
        gap: '8px',
        padding: '4px 6px',
        borderBottom: '1px solid #1e2335',
        fontSize: '11px',
        fontFamily: 'monospace',
        alignItems: 'flex-start',
      }}
    >
      <span style={{ color, minWidth: '32px', fontWeight: 'bold' }}>{levelName}</span>
      <span style={{ color: '#9ca3af', minWidth: '70px' }}>{alert.source}</span>
      <span style={{ flex: 1, color: '#e2e8f0', wordBreak: 'break-all' }}>{alert.message}</span>
      <span style={{ color: '#6b7280', minWidth: '30px', textAlign: 'right' }}>
        {formatAge(alert.timestampUs)}
      </span>
    </div>
  );
}

export function AlertsPanel() {
  const alerts = useDashboardStore(useShallow((s) => s.alerts));

  return (
    <div
      style={{
        background: '#1a1d27',
        border: '1px solid #2a2e42',
        borderRadius: '6px',
        overflow: 'hidden',
      }}
    >
      <div
        style={{
          padding: '6px 10px',
          borderBottom: '1px solid #2a2e42',
          fontSize: '11px',
          fontWeight: 'bold',
          color: '#9ca3af',
          textTransform: 'uppercase',
          letterSpacing: '0.05em',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}
      >
        <span>Alerts</span>
        <span style={{ color: '#6b7280', fontWeight: 'normal' }}>{alerts.length}</span>
      </div>
      <div style={{ maxHeight: '180px', overflowY: 'auto' }}>
        {alerts.length === 0 ? (
          <div style={{ padding: '10px', color: '#6b7280', fontSize: '12px' }}>
            No alerts
          </div>
        ) : (
          alerts.map((alert) => (
            <AlertRow key={`${alert.timestampUs}-${alert.source}-${alert.code}`} alert={alert} />
          ))
        )}
      </div>
    </div>
  );
}
