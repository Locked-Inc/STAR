import { useEffect, useRef, useCallback } from 'react';
import { useDashboardStore } from '../store/dashboardStore';
import { COLORS } from '../theme';

const maxPoints = 120; // ~2 minutes at 1 Hz
const graphW = 280;
const graphH = 60;

interface Series {
    label: string;
    color: string;
    getValue: () => number | undefined;
}

const SERIES: Series[] = [
    { label: 'FL Motor', color: COLORS.primary, getValue: () => useDashboardStore.getState().motors?.[0]?.velocityMps },
    { label: 'FR Motor', color: COLORS.success, getValue: () => useDashboardStore.getState().motors?.[1]?.velocityMps },
    { label: 'CPU %', color: COLORS.warning, getValue: () => useDashboardStore.getState().telemetry?.cpuUsagePercent },
    { label: 'Temp C', color: COLORS.danger, getValue: () => useDashboardStore.getState().telemetry?.temperatureCelsius },
];

/**
 * Time-Series Graphs Panel - buffers last N telemetry values and renders
 * sparkline SVG charts in real-time.
 */
export function TimeSeriesPanel() {
    const buffers = useRef<Map<string, number[]>>(new Map());
    const canvasRefs = useRef<Map<string, SVGSVGElement | null>>(new Map());

    useEffect(() => {
        if (buffers.current.size !== 0) {
            return;
        }

        for (const s of SERIES) {
            buffers.current.set(s.label, []);
        }
    }, []);

    const sample = useCallback(() => {
        for (const s of SERIES) {
            const val = s.getValue();
            const buf = buffers.current.get(s.label)!;
            if (val != null) {
                buf.push(val);
                if (buf.length > maxPoints) buf.shift();
            }
        }
    }, []);

    // Draw sparklines
    const draw = useCallback(() => {
        for (const s of SERIES) {
            const svg = canvasRefs.current.get(s.label);
            const buf = buffers.current.get(s.label)!;
            if (!svg || buf.length < 2) continue;

            const min = Math.min(...buf);
            const max = Math.max(...buf);
            const range = max - min || 1;

            const points = buf.map((v, i) => {
                const x = (i / (maxPoints - 1)) * graphW;
                const y = graphH - ((v - min) / range) * (graphH - 4) - 2;
                return `${x},${y}`;
            }).join(' ');

            // Update the polyline
            const polyline = svg.querySelector('polyline');
            if (polyline) {
                polyline.setAttribute('points', points);
            }

            // Update value label
            const text = svg.querySelector('text');
            if (text) {
                text.textContent = buf[buf.length - 1].toFixed(2);
            }
        }
    }, []);

    useEffect(() => {
        const interval = setInterval(() => {
            sample();
            draw();
        }, 1000); // 1 Hz sampling

        return () => {
            clearInterval(interval);
        };
    }, [sample, draw]);

    return (
        <>
            <div
                className="panel-header"
                style={{
                    padding: '16px 20px 8px 20px',
                    fontSize: '11px', fontWeight: 600,
                    color: COLORS.textMuted,
                    textTransform: 'uppercase' as const,
                    letterSpacing: '0.12em',
                    userSelect: 'none' as const,
                }}
            >
                Time Series
            </div>

            <div className="panel-body" style={{ padding: '8px 16px 16px 16px', display: 'flex', flexDirection: 'column', gap: '6px' }}>
                {SERIES.map((s, index) => {
                    const sparklineTitleId = `sparkline-title-${index}-${s.label.toLowerCase().replace(/[^a-z0-9]+/g, '-')}`;
                    return (
                    <div key={s.label} style={{
                        background: COLORS.panelBg, borderRadius: '6px',
                        padding: '8px 12px', display: 'flex', alignItems: 'center', gap: '10px',
                    }}>
                        <div style={{ width: '70px', flexShrink: 0 }}>
                            <div style={{ fontSize: '9px', color: COLORS.textDim, textTransform: 'uppercase', letterSpacing: '0.06em' }}>
                                {s.label}
                            </div>
                        </div>
                        <svg
                            ref={el => { canvasRefs.current.set(s.label, el); }}
                            viewBox={`0 0 ${graphW} ${graphH}`}
                            style={{ flex: 1, height: '40px' }}
                            preserveAspectRatio="none"
                            role="img"
                            aria-labelledby={sparklineTitleId}
                        >
                            <title id={sparklineTitleId}>{`${s.label} sparkline`}</title>
                            <polyline
                                points=""
                                fill="none"
                                stroke={s.color}
                                strokeWidth="1.5"
                                strokeLinejoin="round"
                                vectorEffect="non-scaling-stroke"
                            />
                            <text
                                x={graphW - 2} y="12"
                                textAnchor="end" fill={s.color}
                                fontSize="11" fontFamily="monospace" fontWeight="600"
                            >
                                --
                            </text>
                        </svg>
                    </div>
                    );
                })}
            </div>
        </>
    );
}
