import type { ReactNode } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import { AlertLevel } from '../proto/star/v1/ui';
import type { Alert } from '../proto/star/v1/ui';

const LEVEL_COLORS: Record<number, string> = {
  [AlertLevel.UNKNOWN]: '#6b7280', // Gray
  [AlertLevel.INFO]: '#0a84ff',     // System Blue
  [AlertLevel.WARN]: '#ff9f0a',     // System Orange
  [AlertLevel.ERROR]: '#ff453a',    // System Red
  [AlertLevel.CRITICAL]: '#ff375f', // System Pink/Deep Red
};

const ICONS: Record<number, ReactNode> = {
  [AlertLevel.UNKNOWN]: <span style={{ opacity: 0.5 }}>?</span>,
  [AlertLevel.INFO]: (
    <svg aria-hidden="true" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
      <circle cx="12" cy="12" r="10"></circle><line x1="12" y1="16" x2="12" y2="12"></line><line x1="12" y1="8" x2="12.01" y2="8"></line>
    </svg>
  ),
  [AlertLevel.WARN]: (
    <svg aria-hidden="true" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
      <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path><line x1="12" y1="9" x2="12" y2="13"></line><line x1="12" y1="17" x2="12.01" y2="17"></line>
    </svg>
  ),
  [AlertLevel.ERROR]: (
    <svg aria-hidden="true" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
      <circle cx="12" cy="12" r="10"></circle><line x1="15" y1="9" x2="9" y2="15"></line><line x1="9" y1="9" x2="15" y2="15"></line>
    </svg>
  ),
  [AlertLevel.CRITICAL]: (
    <svg aria-hidden="true" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
      <polygon points="7.86 2 16.14 2 22 7.86 22 16.14 16.14 22 7.86 22 2 16.14 2 7.86 7.86 2"></polygon><line x1="12" y1="8" x2="12" y2="12"></line><line x1="12" y1="16" x2="12.01" y2="16"></line>
    </svg>
  ),
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
    return '--';
  }
}

function AlertRow({ alert }: { alert: Alert }) {
  const color = LEVEL_COLORS[alert.level] ?? '#6b7280';
  const icon = ICONS[alert.level] ?? ICONS[AlertLevel.UNKNOWN];

  return (
    <div
      style={{
        display: 'flex',
        gap: '12px',
        padding: '12px 20px',
        borderBottom: '0.5px solid rgba(255, 255, 255, 0.05)',
        alignItems: 'flex-start',
        background: `linear-gradient(90deg, ${color}0D 0%, transparent 100%)`,
      }}
    >
      <div style={{ color, marginTop: '2px', filter: `drop-shadow(0 0 4px ${color}66)` }}>
        {icon}
      </div>
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '4px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline' }}>
          <span style={{ color: '#fff', fontSize: '13px', fontWeight: 500, letterSpacing: '0.02em' }}>
            {alert.source}
          </span>
          <span style={{ color: 'rgba(255,255,255,0.4)', fontSize: '11px', fontVariantNumeric: 'tabular-nums' }}>
            {formatAge(alert.timestampUs)}
          </span>
        </div>
        <span style={{ color: 'rgba(255,255,255,0.7)', fontSize: '12px', lineHeight: 1.4, wordBreak: 'break-word' }}>
          {alert.message}
        </span>
      </div>
    </div>
  );
}

export function AlertsPanel() {
  const alerts = useDashboardStore(useShallow((s) => s.alerts));

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div
        className="panel-header"
        style={{
          padding: '16px 24px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          userSelect: 'none',
          borderBottom: '0.5px solid rgba(255,255,255,0.05)',
        }}
      >
        <span style={{
          fontSize: '11px',
          fontWeight: 600,
          color: 'rgba(255, 255, 255, 0.5)',
          textTransform: 'uppercase',
          letterSpacing: '0.12em',
        }}>
          Alerts
        </span>

        {/* Liquid Glass Pill for Count */}
        <div style={{
          background: alerts.length > 0 ? 'rgba(255, 69, 58, 0.15)' : 'rgba(255, 255, 255, 0.08)',
          border: `0.5px solid ${alerts.length > 0 ? 'rgba(255, 69, 58, 0.4)' : 'rgba(255, 255, 255, 0.15)'}`,
          backdropFilter: 'blur(12px)',
          borderRadius: '12px',
          padding: '2px 8px',
          fontSize: '11px',
          fontWeight: 700,
          color: alerts.length > 0 ? '#ff453a' : 'rgba(255,255,255,0.6)',
          fontVariantNumeric: 'tabular-nums',
        }}>
          {alerts.length}
        </div>
      </div>

      <div className="panel-body">
        {alerts.length === 0 ? (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px', alignItems: 'center', justifyContent: 'center', height: '100%', opacity: 0.5 }}>
            <svg aria-hidden="true" width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5">
              <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path>
              <polyline points="22 4 12 14.01 9 11.01"></polyline>
            </svg>
            <span style={{ fontSize: '13px', color: '#fff' }}>All systems nominal</span>
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
