import { memo } from 'react';
import { useDashboardStore } from '../store/dashboardStore';

const maxVelocity = 2.0; // Defines the bounds for the visual progress bar (m/s)

const motorConfig = [
  { label: 'FL', idx: 0 },
  { label: 'FR', idx: 1 },
  { label: 'BL', idx: 2 },
  { label: 'BR', idx: 3 },
];

interface MotorRowProps {
  label: string;
  value: number;
}

const MotorRow = memo(function MotorRow({ label, value }: MotorRowProps) {
  const clamped = Math.max(-maxVelocity, Math.min(maxVelocity, value));
  const percent = (Math.abs(clamped) / maxVelocity) * 100; // number 0-100
  const isForward = clamped >= 0;

  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
      <div style={{ width: '28px', fontSize: '12px', fontWeight: 600, color: 'var(--text-secondary)' }}>{label}</div>

      {/* Progress Bar Track */}
      <div style={{
        flex: 1,
        height: '10px',
        background: 'var(--track-bg)',
        borderRadius: '5px',
        border: '0.5px solid var(--track-border)',
        boxShadow: 'inset 0 2px 4px var(--track-shadow)',
        position: 'relative',
      }}>
        {/* Fill Bar */}
        <div style={{
          position: 'absolute',
          top: 0, bottom: 0,
          left: isForward ? '50%' : `${50 - (percent / 2)}%`,
          width: `${percent / 2}%`,
          background: isForward ? 'var(--accent-forward)' : 'var(--accent-reverse)',
          boxShadow: `0 0 12px ${isForward ? 'var(--accent-forward-glow)' : 'var(--accent-reverse-glow)'}`,
          borderRadius: '5px',
          transition: 'width 0.1s linear, left 0.1s linear'
        }} />

        {/* Center line marker */}
        <div style={{ position: 'absolute', left: '50%', top: '-2px', bottom: '-2px', width: '1px', background: 'var(--divider)', zIndex: 2 }} />
      </div>

      {/* Value Readout - Enforces minimum 13px tabular nums */}
      <div
        className="data-flash"
        key={`val-${label}-${value}`}
        style={{ width: '48px', textAlign: 'right', fontSize: '13px', fontVariantNumeric: 'tabular-nums', fontWeight: 400, color: 'var(--text-primary)' }}
      >
        {value.toFixed(2)}
      </div>
    </div>
  );
});

export function MotorPanel() {
  const motors = useDashboardStore((s) => s.motors);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div
        className="panel-header"
        style={{
          padding: '16px 24px 12px 24px',
          fontSize: '11px',
          fontWeight: 600,
          color: 'rgba(255, 255, 255, 0.5)',
          textTransform: 'uppercase',
          letterSpacing: '0.12em',
          userSelect: 'none',
        }}
      >
        Motor Velocities
      </div>
      <div className="panel-body" style={{ padding: '0 24px 20px 24px', display: 'flex', flexDirection: 'column', justifyContent: 'center' }}>
        {motorConfig.map((motor) => {
          const value = motors && motors[motor.idx] ? motors[motor.idx].velocityMps : 0;
          return <MotorRow key={motor.label} label={motor.label} value={value} />;
        })}
      </div>
    </div>
  );
}
