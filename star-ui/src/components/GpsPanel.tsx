import { useDashboardStore } from '../store/dashboardStore';
import { COLORS } from '../theme';

/**
 * GPS Position Panel - shows lat/lon, altitude, satellite count,
 * fix quality, and accuracy. Reads from telemetry.gps.
 */

const fixLabels: Record<number, { label: string; color: string }> = {
    0: { label: 'UNKNOWN', color: COLORS.textMuted },
    1: { label: 'NO FIX', color: COLORS.danger },
    2: { label: '2D FIX', color: COLORS.warning },
    3: { label: '3D FIX', color: COLORS.success },
    4: { label: 'DGPS', color: COLORS.success },
    5: { label: 'RTK FLOAT', color: COLORS.primary },
    6: { label: 'RTK FIXED', color: COLORS.primary },
};

export function GpsPanel() {
    const telemetry = useDashboardStore(s => s.telemetry);
    const gps = telemetry?.gps;

    const fix = fixLabels[gps?.fixType ?? 0] ?? fixLabels[0];

    return (
        <>
            <div
                className="panel-header"
                style={{
                    padding: '16px 20px 8px 20px',
                    fontSize: '11px', fontWeight: 600,
                    color: 'rgba(255,255,255,0.5)',
                    textTransform: 'uppercase' as const,
                    letterSpacing: '0.12em',
                    userSelect: 'none' as const,
                }}
            >
                GPS / Position
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {/* Fix quality badge */}
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <div style={{
                        display: 'flex', alignItems: 'center', gap: '6px',
                        background: `${fix.color}15`, border: `0.5px solid ${fix.color}44`,
                        borderRadius: '6px', padding: '5px 10px',
                        fontSize: '11px', fontWeight: 600, color: fix.color,
                    }}>
                        <div style={{ width: 6, height: 6, borderRadius: '50%', background: fix.color }} />
                        {fix.label}
                    </div>
                    {gps && (
                        <span style={{ fontSize: '11px', color: 'rgba(255,255,255,0.4)' }}>
                            {gps.satellites} sats | +/-{gps.accuracyM?.toFixed(1) ?? '--'}m
                        </span>
                    )}
                </div>

                {/* Coordinates */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
                    <CoordCard label="Latitude" value={gps?.latitudeDeg} suffix=" deg" precision={6} />
                    <CoordCard label="Longitude" value={gps?.longitudeDeg} suffix=" deg" precision={6} />
                    <CoordCard label="Altitude" value={gps?.altitudeM} suffix=" m" precision={1} />
                    <CoordCard label="Accuracy" value={gps?.accuracyM} suffix=" m" precision={1} />
                </div>

                {/* Mini coordinate display - only show when fix is 2D or better */}
                {gps?.fixType != null && gps.fixType >= 2 && (
                    <div style={{
                        background: 'rgba(255,255,255,0.03)', borderRadius: '6px',
                        padding: '10px 14px', fontSize: '11px',
                        fontFamily: 'monospace', color: 'rgba(255,255,255,0.5)',
                        textAlign: 'center',
                    }}>
                        {gps.latitudeDeg.toFixed(6)}, {gps.longitudeDeg.toFixed(6)}
                    </div>
                )}
            </div>
        </>
    );
}

function CoordCard({ label, value, suffix, precision }: {
    label: string; value?: number; suffix: string; precision: number;
}) {
    return (
        <div style={{
            background: 'rgba(255,255,255,0.03)', borderRadius: '6px',
            padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: '3px',
        }}>
            <span style={{ fontSize: '10px', color: 'rgba(255,255,255,0.35)', textTransform: 'uppercase', letterSpacing: '0.08em' }}>
                {label}
            </span>
            <span style={{ fontSize: '15px', fontWeight: 600, color: 'rgba(255,255,255,0.85)', fontVariantNumeric: 'tabular-nums', fontFamily: 'monospace' }}>
                {value != null ? value.toFixed(precision) + suffix : '--'}
            </span>
        </div>
    );
}
