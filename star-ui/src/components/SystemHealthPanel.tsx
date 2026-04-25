import { useDashboardStore } from '../store/dashboardStore';
import { useShallow } from 'zustand/react/shallow';
import { COLORS } from '../theme';

/**
 * System Health Panel - displays uptime, free heap, firmware version,
 * CPU usage, temperature, and subsystem connection states.
 */

function fmtUptime(secs?: number | string): string {
    if (secs == null) return '--';
    const s = Number(secs);
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = Math.floor(s % 60);
    return `${h}h ${m}m ${sec}s`;
}

function fmtBytes(bytes?: number): string {
    if (bytes == null) return '--';
    if (bytes > 1024 * 1024) return (bytes / 1024 / 1024).toFixed(1) + ' MB';
    if (bytes > 1024) return (bytes / 1024).toFixed(1) + ' KB';
    return bytes + ' B';
}

function conn(connected?: boolean): { label: string; color: string } {
    return connected
        ? { label: 'ONLINE', color: COLORS.success }
        : { label: 'OFFLINE', color: COLORS.danger };
}

export function SystemHealthPanel() {
    const { systemStatus, telemetry } = useDashboardStore(
        useShallow((s) => ({ systemStatus: s.systemStatus, telemetry: s.telemetry }))
    );

    const subsystems = [
        { name: 'RX72N', ...conn(systemStatus?.rx72NConnected) },
        { name: 'LiDAR', ...conn(systemStatus?.lidarConnected) },
        { name: 'ROS2', ...conn(systemStatus?.rosConnected) },
    ];

    return (
        <>
            <div
                className="panel-header"
                style={{
                    padding: '16px 20px 8px 20px',
                    fontSize: '11px',
                    fontWeight: 600,
                    color: 'var(--text-50)',
                    textTransform: 'uppercase' as const,
                    letterSpacing: '0.12em',
                    userSelect: 'none' as const,
                }}
            >
                System Health
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '14px' }}>
                {/* Subsystem connection badges */}
                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    {subsystems.map(sub => (
                        <div key={sub.name} style={{
                            display: 'flex', alignItems: 'center', gap: '6px',
                            background: 'var(--panel-bg)', borderRadius: '6px', padding: '5px 10px',
                            fontSize: '11px', fontWeight: 500,
                        }}>
                            <div style={{ width: 6, height: 6, borderRadius: '50%', background: sub.color }} />
                            <span style={{ color: 'var(--text-60)' }}>{sub.name}</span>
                            <span style={{ color: sub.color, fontWeight: 600 }}>{sub.label}</span>
                        </div>
                    ))}
                </div>

                {/* Metrics grid */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                    <MetricCard label="Uptime" value={fmtUptime(systemStatus?.uptimeS)} />
                    <MetricCard label="Free Heap" value={fmtBytes(systemStatus?.freeHeapBytes)} />
                    <MetricCard label="CPU" value={telemetry?.cpuUsagePercent != null ? telemetry.cpuUsagePercent.toFixed(1) + '%' : '--'} />
                    <MetricCard label="Temperature" value={telemetry?.temperatureCelsius != null ? telemetry.temperatureCelsius.toFixed(1) + ' deg C' : '--'} />
                </div>

                {/* Firmware version */}
                <div style={{
                    background: 'var(--panel-bg)', borderRadius: '6px', padding: '10px 14px',
                    fontSize: '12px', display: 'flex', justifyContent: 'space-between',
                }}>
                    <span style={{ color: 'var(--text-40)' }}>Firmware</span>
                    <span style={{ color: COLORS.primary, fontFamily: 'monospace', fontWeight: 600 }}>
                        {systemStatus?.firmwareVersion || '--'}
                    </span>
                </div>

                {/* WiFi signal */}
                {telemetry?.wifiSignalDbm != null && (
                    <div style={{
                        background: 'var(--panel-bg)', borderRadius: '6px', padding: '10px 14px',
                        fontSize: '12px', display: 'flex', justifyContent: 'space-between',
                    }}>
                        <span style={{ color: 'var(--text-40)' }}>WiFi Signal</span>
                        <span style={{ color: telemetry.wifiSignalDbm > -60 ? COLORS.success : telemetry.wifiSignalDbm > -80 ? COLORS.warning : COLORS.danger, fontWeight: 600 }}>
                            {telemetry.wifiSignalDbm} dBm
                        </span>
                    </div>
                )}
            </div>
        </>
    );
}

function MetricCard({ label, value }: { label: string; value: string }) {
    return (
        <div style={{
            background: 'var(--panel-bg)', borderRadius: '6px',
            padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: '4px',
        }}>
            <span style={{ fontSize: '10px', color: 'var(--text-35)', textTransform: 'uppercase', letterSpacing: '0.08em' }}>{label}</span>
            <span style={{ fontSize: '16px', fontWeight: 600, color: 'var(--text-85)', fontVariantNumeric: 'tabular-nums' }}>{value}</span>
        </div>
    );
}
