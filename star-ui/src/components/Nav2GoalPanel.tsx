import { useState, useRef, useEffect } from 'react';
import { COLORS } from '../theme';

/**
 * Nav2 Goal Panel - send navigation goals to the ROS2 Nav2 stack.
 * Displays current goal status and allows setting X/Y/theta targets.
 */

const goalTimeoutMs = 5000;

export function Nav2GoalPanel() {
    const [goalX, setGoalX] = useState('0.0');
    const [goalY, setGoalY] = useState('0.0');
    const [goalTheta, setGoalTheta] = useState('0.0');
    const [status, setStatus] = useState<'idle' | 'navigating' | 'reached' | 'failed'>('idle');
    const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

    useEffect(() => {
        return () => {
            if (timerRef.current !== null) clearTimeout(timerRef.current);
        };
    }, []);

    const handleSendGoal = () => {
        // TODO: Send nav goal via ROS2 bridge / WebSocket
        setStatus('navigating');
        if (timerRef.current !== null) clearTimeout(timerRef.current);
        timerRef.current = setTimeout(() => {
            timerRef.current = null;
            setStatus('reached');
        }, goalTimeoutMs);
    };

    const handleCancel = () => {
        if (timerRef.current !== null) {
            clearTimeout(timerRef.current);
            timerRef.current = null;
        }
        setStatus('idle');
    };

    const statusConfig: Record<string, { label: string; color: string }> = {
        idle: { label: 'IDLE', color: COLORS.textMuted },
        navigating: { label: 'NAVIGATING', color: COLORS.primary },
        reached: { label: 'REACHED', color: COLORS.success },
        failed: { label: 'FAILED', color: COLORS.danger },
    };

    const cfg = statusConfig[status];

    const fields = [
        { id: 'nav2-x', label: 'X (m)', value: goalX, set: setGoalX },
        { id: 'nav2-y', label: 'Y (m)', value: goalY, set: setGoalY },
        { id: 'nav2-theta', label: 'theta (rad)', value: goalTheta, set: setGoalTheta },
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
                Nav2 Goal
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '10px' }}>
                {/* Status badge */}
                <div style={{
                    display: 'flex', alignItems: 'center', gap: '6px',
                    background: `${cfg.color}15`, border: `0.5px solid ${cfg.color}44`,
                    borderRadius: '6px', padding: '5px 10px', width: 'fit-content',
                    fontSize: '11px', fontWeight: 600, color: cfg.color,
                }}>
                    <div style={{
                        width: 6, height: 6, borderRadius: '50%', background: cfg.color,
                        animation: status === 'navigating' ? 'pulse-opacity 1s infinite' : 'none',
                    }} />
                    {cfg.label}
                </div>

                {/* Goal inputs */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px' }}>
                    {fields.map(f => (
                        <div key={f.id}>
                            <label
                                htmlFor={f.id}
                                style={{ fontSize: '9px', color: 'rgba(255,255,255,0.3)', textTransform: 'uppercase', display: 'block', marginBottom: '3px' }}
                            >
                                {f.label}
                            </label>
                            <input
                                id={f.id}
                                type="number" step="0.1" value={f.value}
                                onChange={e => f.set(e.target.value)}
                                disabled={status === 'navigating'}
                                style={{
                                    width: '100%', padding: '6px 8px', borderRadius: '4px',
                                    border: '0.5px solid rgba(255,255,255,0.12)', background: 'rgba(255,255,255,0.04)',
                                    color: '#fff', fontSize: '13px', fontFamily: 'monospace',
                                    outline: 'none', boxSizing: 'border-box',
                                    opacity: status === 'navigating' ? 0.5 : 1,
                                }}
                            />
                        </div>
                    ))}
                </div>

                {/* Action buttons */}
                <div style={{ display: 'flex', gap: '8px' }}>
                    <button
                        type="button"
                        onClick={handleSendGoal}
                        disabled={status === 'navigating'}
                        style={{
                            flex: 1, padding: '8px', border: 'none', borderRadius: '6px',
                            background: COLORS.success, color: '#fff',
                            fontSize: '11px', fontWeight: 600, cursor: 'pointer',
                            opacity: status === 'navigating' ? 0.5 : 1,
                            letterSpacing: '0.06em', textTransform: 'uppercase',
                        }}
                    >
                        Send Goal
                    </button>
                    <button
                        type="button"
                        onClick={handleCancel}
                        disabled={status !== 'navigating'}
                        style={{
                            flex: 1, padding: '8px', border: '0.5px solid rgba(255,255,255,0.15)',
                            borderRadius: '6px', background: status === 'navigating' ? 'rgba(255,69,58,0.15)' : 'rgba(255,255,255,0.04)',
                            color: status === 'navigating' ? COLORS.danger : 'rgba(255,255,255,0.4)',
                            fontSize: '11px', fontWeight: 600, cursor: 'pointer',
                            letterSpacing: '0.06em', textTransform: 'uppercase',
                        }}
                    >
                        Cancel
                    </button>
                </div>
            </div>
        </>
    );
}
