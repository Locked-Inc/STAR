import { useDashboardStore } from '../store/dashboardStore';
import { COLORS } from '../theme';

/**
 * IMU Attitude Indicator — displays roll / pitch / yaw plus acceleration & gyro.
 * Safety-critical: helps operators detect tip-over or abnormal orientation.
 */
export function ImuPanel() {
    const telemetry = useDashboardStore(s => s.telemetry);
    const imu = telemetry?.imu;

    const fmt = (v: number | undefined) => v != null ? v.toFixed(2) : '--';
    const deg = (rad: number | undefined) => rad != null ? ((rad * 180) / Math.PI).toFixed(1) + ' deg' : '--';

    // Attitude disc parameters
    const pitch = imu?.pitchRad ?? 0;
    const roll = imu?.rollRad ?? 0;
    const yaw = imu?.yawRad ?? 0;

    return (
        <>
            <div
                className="panel-header"
                style={{
                    padding: '16px 20px 8px 20px',
                    fontSize: '11px',
                    fontWeight: 600,
                    color: 'rgba(255,255,255,0.5)',
                    textTransform: 'uppercase' as const,
                    letterSpacing: '0.12em',
                    userSelect: 'none' as const,
                }}
            >
                IMU / Attitude
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '16px' }}>
                {/* Attitude ball / horizon indicator */}
                <div style={{ display: 'flex', justifyContent: 'center' }}>
                    <svg viewBox="-60 -60 120 120" width="110" height="110" aria-labelledby="imu-attitude-title" role="img">
                        <title id="imu-attitude-title">Attitude indicator showing pitch and roll</title>
                        {/* Outer ring */}
                        <circle cx="0" cy="0" r="55" fill="none" stroke="rgba(255,255,255,0.15)" strokeWidth="1" />
                        {/* Horizon - tilts with roll, shifts with pitch */}
                        <g transform={`rotate(${(-roll * 180) / Math.PI})`}>
                            <line x1="-50" y1={pitch * 30} x2="50" y2={pitch * 30}
                                stroke={COLORS.success} strokeWidth="2" />
                            {/* Sky / ground fill */}
                            <rect x="-50" y={pitch * 30} width="100" height="60"
                                fill="rgba(65,105,225,0.15)" rx="2" />
                            <rect x="-50" y={pitch * 30 - 60} width="100" height="60"
                                fill="rgba(139,90,43,0.12)" rx="2" />
                        </g>
                        {/* Center crosshair (fixed) */}
                        <line x1="-10" y1="0" x2="10" y2="0" stroke={COLORS.primary} strokeWidth="2" />
                        <line x1="0" y1="-6" x2="0" y2="6" stroke={COLORS.primary} strokeWidth="2" />
                        {/* Heading tick at top */}
                        <g transform={`rotate(${(-yaw * 180) / Math.PI})`}>
                            <line x1="0" y1="-55" x2="0" y2="-48" stroke={COLORS.warning} strokeWidth="2" />
                        </g>
                    </svg>
                </div>

                {/* Orientation values */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', textAlign: 'center' }}>
                    {[
                        { label: 'Roll', value: deg(imu?.rollRad), color: COLORS.primary },
                        { label: 'Pitch', value: deg(imu?.pitchRad), color: COLORS.success },
                        { label: 'Yaw', value: deg(imu?.yawRad), color: COLORS.warning },
                    ].map(v => (
                        <div key={v.label}>
                            <div style={{ fontSize: '10px', color: 'rgba(255,255,255,0.4)', textTransform: 'uppercase', letterSpacing: '0.08em' }}>{v.label}</div>
                            <div style={{ fontSize: '18px', fontWeight: 600, color: v.color, fontVariantNumeric: 'tabular-nums' }}>{v.value}</div>
                        </div>
                    ))}
                </div>

                {/* Acceleration */}
                <div>
                    <div style={{ fontSize: '10px', color: 'rgba(255,255,255,0.35)', textTransform: 'uppercase', letterSpacing: '0.08em', marginBottom: '6px' }}>Acceleration (m/s^2)</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px', fontSize: '13px', fontVariantNumeric: 'tabular-nums' }}>
                        <span style={{ color: 'rgba(255,255,255,0.7)' }}>X: {fmt(imu?.accelXMps2)}</span>
                        <span style={{ color: 'rgba(255,255,255,0.7)' }}>Y: {fmt(imu?.accelYMps2)}</span>
                        <span style={{ color: 'rgba(255,255,255,0.7)' }}>Z: {fmt(imu?.accelZMps2)}</span>
                    </div>
                </div>

                {/* Gyro */}
                <div>
                    <div style={{ fontSize: '10px', color: 'rgba(255,255,255,0.35)', textTransform: 'uppercase', letterSpacing: '0.08em', marginBottom: '6px' }}>Gyroscope (rad/s)</div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '4px', fontSize: '13px', fontVariantNumeric: 'tabular-nums' }}>
                        <span style={{ color: 'rgba(255,255,255,0.7)' }}>X: {fmt(imu?.gyroXRadPerS)}</span>
                        <span style={{ color: 'rgba(255,255,255,0.7)' }}>Y: {fmt(imu?.gyroYRadPerS)}</span>
                        <span style={{ color: 'rgba(255,255,255,0.7)' }}>Z: {fmt(imu?.gyroZRadPerS)}</span>
                    </div>
                </div>
            </div>
        </>
    );
}
