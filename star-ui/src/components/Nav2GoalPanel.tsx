import { useState } from 'react';
import { COLORS } from '../theme';

/**
 * Nav2 Goal Panel - send navigation goals to the ROS2 Nav2 stack.
 * Displays current goal status and allows setting X/Y/theta targets.
 *
 * Status: PREVIEW / NOT IMPLEMENTED. The gateway does not yet expose a
 * Nav2 goal RPC and STAREnvelope has no nav-goal payload. Inputs remain
 * interactive for visual layout testing, but the Send button is disabled
 * and a banner is shown so an operator does not believe a goal was sent.
 */

export function Nav2GoalPanel() {
    const [goalX, setGoalX] = useState('0.0');
    const [goalY, setGoalY] = useState('0.0');
    const [goalTheta, setGoalTheta] = useState('0.0');

    // Status is fixed to PREVIEW until the gateway exposes a Nav2 goal RPC.
    const cfg = { label: 'PREVIEW', color: COLORS.warning };

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
                {/* Not-implemented banner: warns operators that no goal will be sent. */}
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
                    Not implemented yet: gateway has no Nav2 goal RPC. Inputs are
                    visual-only; Send is disabled until the bridge is wired.
                </div>

                {/* Status badge */}
                <div style={{
                    display: 'flex', alignItems: 'center', gap: '6px',
                    background: `${cfg.color}15`, border: `0.5px solid ${cfg.color}44`,
                    borderRadius: '6px', padding: '5px 10px', width: 'fit-content',
                    fontSize: '11px', fontWeight: 600, color: cfg.color,
                }}>
                    <div style={{
                        width: 6, height: 6, borderRadius: '50%', background: cfg.color,
                    }} />
                    {cfg.label}
                </div>

                {/* Goal inputs - interactive for visual testing; nothing leaves the browser. */}
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
                                style={{
                                    width: '100%', padding: '6px 8px', borderRadius: '4px',
                                    border: '0.5px solid rgba(255,255,255,0.12)', background: 'rgba(255,255,255,0.04)',
                                    color: '#fff', fontSize: '13px', fontFamily: 'monospace',
                                    outline: 'none', boxSizing: 'border-box',
                                }}
                            />
                        </div>
                    ))}
                </div>

                {/* Send Goal is disabled until a real RPC exists. No mock-success path. */}
                <button
                    type="button"
                    disabled
                    aria-disabled="true"
                    title="Not connected to gateway"
                    style={{
                        padding: '8px', border: '0.5px solid rgba(255,255,255,0.15)',
                        borderRadius: '6px', background: 'rgba(255,255,255,0.04)',
                        color: 'rgba(255,255,255,0.4)',
                        fontSize: '11px', fontWeight: 600, cursor: 'not-allowed',
                        letterSpacing: '0.06em', textTransform: 'uppercase',
                    }}
                >
                    Send Goal (not connected)
                </button>
            </div>
        </>
    );
}
