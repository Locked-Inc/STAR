import { useEffect, useRef } from 'react';
import { useDashboardStore } from '../store/dashboardStore';
import { COLORS } from '../theme';
import { AlertLevel } from '../proto/star/v1/ui';

/**
 * Diagnostic Log Console - scrolling log of all alerts and system events.
 * Reads from the alerts array in dashboardStore.
 */
export function DiagLogPanel() {
    const alerts = useDashboardStore(s => s.alerts);
    const scrollRef = useRef<HTMLDivElement>(null);
    const latestTimestampUs = alerts.length > 0 ? alerts[0].timestampUs : undefined;

    // Auto-scroll to top on new alerts (newest first)
    useEffect(() => {
        if (scrollRef.current) {
            scrollRef.current.scrollTop = 0;
        }
    }, [latestTimestampUs]);

    const levelConfig: Record<number, { label: string; color: string }> = {
        [AlertLevel.INFO]: { label: 'INFO', color: COLORS.primary },
        [AlertLevel.WARN]: { label: 'WARN', color: COLORS.warning },
        [AlertLevel.ERROR]: { label: 'ERROR', color: COLORS.danger },
        [AlertLevel.CRITICAL]: { label: 'CRIT', color: '#FF2D55' },
    };

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
                    display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                }}
            >
                <span>Diagnostic Log</span>
                <span style={{
                    fontSize: '10px', color: 'rgba(255,255,255,0.3)',
                    fontWeight: 400, letterSpacing: 'normal', textTransform: 'none',
                }}>
                    {alerts.length} entries
                </span>
            </div>

            <div
                ref={scrollRef}
                className="panel-body"
                style={{
                    padding: '4px 12px 12px 12px',
                    fontFamily: 'monospace', fontSize: '11px',
                    lineHeight: '1.6',
                }}
            >
                {alerts.length === 0 && (
                    <div style={{ textAlign: 'center', color: 'rgba(255,255,255,0.2)', padding: '20px 0' }}>
                        No diagnostic entries
                    </div>
                )}
                {alerts.map((alert) => {
                    const cfg = levelConfig[alert.level] ?? { label: '???', color: COLORS.textMuted };
                    const ts = alert.timestampUs
                        ? new Date(Number(alert.timestampUs) / 1000).toLocaleTimeString('en-US', { hour12: false, fractionalSecondDigits: 3 })
                        : '??:??:??.???';
                    // Stable composite key: timestamp + source + code uniquely identifies an
                    // alert event without depending on array index, which shifts when the
                    // ring-buffer wraps and would cause React to re-render unrelated rows.
                    const stableKey = `${alert.timestampUs}-${alert.source}-${alert.code}`;
                    return (
                        <div key={stableKey} style={{
                            display: 'flex', gap: '8px', padding: '2px 4px',
                            borderBottom: '0.5px solid rgba(255,255,255,0.04)',
                        }}>
                            <span style={{ color: 'rgba(255,255,255,0.25)', flexShrink: 0 }}>{ts}</span>
                            <span style={{
                                color: cfg.color, fontWeight: 600, flexShrink: 0, width: '40px',
                            }}>
                                {cfg.label}
                            </span>
                            <span style={{ color: 'rgba(255,255,255,0.5)', flexShrink: 0 }}>
                                [{alert.source ?? '--'}]
                            </span>
                            <span style={{ color: 'rgba(255,255,255,0.7)' }}>{alert.message}</span>
                        </div>
                    );
                })}
            </div>
        </>
    );
}
