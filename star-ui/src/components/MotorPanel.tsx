import { useDashboardStore } from '../store/dashboardStore';

const maxVelocity = 2.0; // Defines the bounds for the visual progress bar (m/s)

export function MotorPanel() {
  const motors = useDashboardStore((s) => s.motors);

  const renderMotorRow = (label: string, index: number) => {
    // Safely extract velocity, default to 0
    const value = motors && motors[index] ? motors[index].velocityMps : 0;

    // Calculate visual metrics based on a center-origin bar
    const clamped = Math.max(-maxVelocity, Math.min(maxVelocity, value));
    const percent = (Math.abs(clamped) / maxVelocity) * 100; // 0 to 100 string
    const isForward = clamped >= 0;

    return (
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '16px' }}>
        <div style={{ width: '28px', fontSize: '12px', fontWeight: 600, color: 'rgba(255,255,255,0.65)' }}>{label}</div>

        {/* Progress Bar Track */}
        <div style={{
          flex: 1,
          height: '10px',
          background: 'rgba(255,255,255,0.04)',
          borderRadius: '5px',
          border: '0.5px solid rgba(255,255,255,0.18)',
          boxShadow: 'inset 0 2px 4px rgba(0,0,0,0.5)',
          position: 'relative',
        }}>
          {/* Fill Bar */}
          <div style={{
            position: 'absolute',
            top: 0, bottom: 0,
            left: isForward ? '50%' : `${50 - (percent / 2)}%`,
            width: `${percent / 2}%`,
            background: isForward ? '#00e5ff' : '#FF9F0A',
            boxShadow: `0 0 12px ${isForward ? 'rgba(0,229,255,0.6)' : 'rgba(255,159,10,0.6)'}`,
            borderRadius: '5px',
            transition: 'width 0.1s linear, left 0.1s linear'
          }} />

          {/* Center line marker */}
          <div style={{ position: 'absolute', left: '50%', top: '-2px', bottom: '-2px', width: '1px', background: 'rgba(255,255,255,0.4)', zIndex: 2 }} />
        </div>

        {/* Value Readout - Enforces minimum 13px tabular nums */}
        <div
          className="data-flash"
          key={`val-${label}-${value}`}
          style={{ width: '48px', textAlign: 'right', fontSize: '13px', fontVariantNumeric: 'tabular-nums', fontWeight: 400, color: '#fff' }}
        >
          {value.toFixed(2)}
        </div>
      </div>
    );
  };

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
        {renderMotorRow('FL', 0)}
        {renderMotorRow('FR', 1)}
        {renderMotorRow('BL', 2)}
        {renderMotorRow('BR', 3)}
      </div>
    </div>
  );
}
