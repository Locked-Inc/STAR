import { COLORS } from '../theme';

/**
 * Transport Diagnostics Panel - placeholder for SPI/USB transport health.
 *
 * Status: PREVIEW / NOT IMPLEMENTED. Will display packet loss, latency, error
 * rates from TransportDiagnostics proto (defined in star.v1.WireMessage), but
 * the gateway does not yet stream these over the UI WebSocket (STAREnvelope
 * has no transport-diagnostics payload). The values shown below are static
 * placeholders to validate layout, not live readings.
 */
export function TransportDiagPanel() {
    // Placeholder rows: values are static and do NOT reflect live transport state.
    const transports = [
        { name: 'SPI', active: true, healthy: true, loss: 0, latency: '--', sent: 0, errors: 0 },
        { name: 'USB CDC', active: false, healthy: true, loss: 0, latency: '--', sent: 0, errors: 0 },
    ];

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
                Transport
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '10px' }}>
                {/* Not-implemented banner: warns operators that values are static. */}
                <div
                    role="status"
                    style={{
                        background: `${COLORS.warning}18`,
                        border: `0.5px solid ${COLORS.warning}66`,
                        borderRadius: '6px',
                        padding: '8px 10px',
                        fontSize: '11px',
                        lineHeight: 1.4,
                        color: COLORS.warning,
                    }}
                >
                    Not implemented yet: gateway does not stream
                    TransportDiagnostics over the UI WebSocket. The rows below
                    are static placeholders, not live transport health.
                </div>

                {transports.map(t => (
                    <div key={t.name} style={{
                        background: 'rgba(255,255,255,0.03)', borderRadius: '6px',
                        padding: '10px 14px',
                    }}>
                        {/* Header row */}
                        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                                <div style={{
                                    width: 6, height: 6, borderRadius: '50%',
                                    background: t.healthy ? COLORS.success : COLORS.danger,
                                }} />
                                <span style={{ fontSize: '12px', fontWeight: 600, color: 'rgba(255,255,255,0.8)' }}>
                                    {t.name}
                                </span>
                            </div>
                            {t.active && (
                                <span style={{
                                    fontSize: '9px', fontWeight: 600, color: COLORS.primary,
                                    background: `${COLORS.primary}15`, border: `0.5px solid ${COLORS.primary}33`,
                                    borderRadius: '4px', padding: '2px 6px',
                                    textTransform: 'uppercase', letterSpacing: '0.08em',
                                }}>
                                    ACTIVE
                                </span>
                            )}
                        </div>

                        {/* Metrics */}
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '6px', fontSize: '10px' }}>
                            <div>
                                <div style={{ color: 'rgba(255,255,255,0.3)', textTransform: 'uppercase', letterSpacing: '0.06em' }}>Loss</div>
                                <div style={{ fontWeight: 600, color: t.loss > 0.01 ? COLORS.danger : 'rgba(255,255,255,0.7)', fontVariantNumeric: 'tabular-nums' }}>
                                    {(t.loss * 100).toFixed(1)}%
                                </div>
                            </div>
                            <div>
                                <div style={{ color: 'rgba(255,255,255,0.3)', textTransform: 'uppercase', letterSpacing: '0.06em' }}>Latency</div>
                                <div style={{ fontWeight: 600, color: 'rgba(255,255,255,0.7)', fontVariantNumeric: 'tabular-nums' }}>{t.latency}</div>
                            </div>
                            <div>
                                <div style={{ color: 'rgba(255,255,255,0.3)', textTransform: 'uppercase', letterSpacing: '0.06em' }}>Errors</div>
                                <div style={{ fontWeight: 600, color: t.errors > 0 ? COLORS.danger : 'rgba(255,255,255,0.7)', fontVariantNumeric: 'tabular-nums' }}>
                                    {t.errors}
                                </div>
                            </div>
                        </div>
                    </div>
                ))}

                <div style={{
                    fontSize: '10px', color: 'rgba(255,255,255,0.25)',
                    textAlign: 'center', fontStyle: 'italic',
                }}>
                    Waiting for gateway diagnostics stream
                </div>
            </div>
        </>
    );
}
