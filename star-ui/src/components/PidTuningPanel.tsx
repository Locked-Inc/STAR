import { useState } from 'react';
import { COLORS } from '../theme';

/**
 * PID Tuning Panel - UI for reading/writing PID gains for each motor.
 *
 * Status: PREVIEW / NOT IMPLEMENTED. Although ConfigurationService.SetMotorPidConfig
 * exists in the gateway, the UI's WebSocket connection currently carries only
 * STAREnvelope (telemetry / controller / estop) -- there is no gRPC-Web or REST
 * channel from the browser to the gateway PID RPC yet. Inputs are interactive
 * for visual testing, but the Apply button is disabled and a banner is shown
 * so an operator does not believe new gains were saved.
 */

export function PidTuningPanel() {
    const [motorIdx, setMotorIdx] = useState(0);
    const motorNames = ['FL', 'FR', 'BL', 'BR'];

    const [kp, setKp] = useState('1.0');
    const [ki, setKi] = useState('0.1');
    const [kd, setKd] = useState('0.05');

    const pidParams = [
        { id: 'pid-kp', label: 'Kp (Proportional)', value: kp, set: setKp },
        { id: 'pid-ki', label: 'Ki (Integral)', value: ki, set: setKi },
        { id: 'pid-kd', label: 'Kd (Derivative)', value: kd, set: setKd },
    ];

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
                PID Tuning
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {/* Not-implemented banner: warns operators that no gains will be sent. */}
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
                    Not implemented yet: browser has no channel to the gateway
                    SetMotorPidConfig RPC. Inputs are visual-only; Apply is
                    disabled until the bridge is wired.
                </div>

                {/* Motor selector */}
                <div style={{ display: 'flex', gap: '4px' }}>
                    {motorNames.map((name, idx) => (
                        <button
                            key={name}
                            type="button"
                            onClick={() => setMotorIdx(idx)}
                            style={{
                                flex: 1, padding: '6px 0', border: 'none', borderRadius: '6px',
                                fontSize: '11px', fontWeight: 600, cursor: 'pointer',
                                background: motorIdx === idx ? `${COLORS.primary}25` : COLORS.panelBg,
                                color: motorIdx === idx ? COLORS.primary : COLORS.textMuted,
                                transition: 'all 0.15s',
                            }}
                        >
                            {name}
                        </button>
                    ))}
                </div>

                {/* PID Inputs */}
                {pidParams.map(param => (
                    <div key={param.id}>
                        <label
                            htmlFor={param.id}
                            style={{ fontSize: '10px', color: COLORS.textDim, textTransform: 'uppercase', letterSpacing: '0.06em', display: 'block', marginBottom: '4px' }}
                        >
                            {param.label}
                        </label>
                        <input
                            id={param.id}
                            type="number"
                            step="0.01"
                            value={param.value}
                            onChange={e => param.set(e.target.value)}
                            style={{
                                width: '100%', padding: '8px 12px', borderRadius: '6px',
                                border: `0.5px solid ${COLORS.border}`, background: COLORS.panelBg,
                                color: COLORS.textPrimary, fontSize: '14px', fontFamily: 'monospace',
                                outline: 'none', boxSizing: 'border-box',
                            }}
                        />
                    </div>
                ))}

                {/* Apply is disabled until a real RPC channel exists. No mock-success path. */}
                <button
                    type="button"
                    disabled
                    aria-disabled="true"
                    title="Not connected to gateway"
                    style={{
                        padding: '10px', border: '0.5px solid rgba(255,255,255,0.15)',
                        borderRadius: '8px', background: 'rgba(255,255,255,0.04)',
                        color: 'rgba(255,255,255,0.4)',
                        fontSize: '12px', fontWeight: 600,
                        cursor: 'not-allowed',
                        letterSpacing: '0.08em', textTransform: 'uppercase',
                    }}
                >
                    Apply to Motor {motorNames[motorIdx]} (not connected)
                </button>
            </div>
        </>
    );
}
