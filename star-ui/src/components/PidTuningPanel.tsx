import { useState } from 'react';
import { COLORS } from '../theme';

/**
 * PID Tuning Panel — UI for reading/writing PID gains for each motor.
 * Maps to ConfigurationService.GetMotorPidConfig / SetMotorPidConfig.
 * Currently uses local state; will send gRPC when gateway wired.
 */
export function PidTuningPanel() {
    const [motorIdx, setMotorIdx] = useState(0);
    const motorNames = ['FL', 'FR', 'BL', 'BR'];

    const [kp, setKp] = useState('1.0');
    const [ki, setKi] = useState('0.1');
    const [kd, setKd] = useState('0.05');
    const [saved, setSaved] = useState(false);

    const handleSend = () => {
        // TODO: Call ConfigurationService.SetMotorPidConfig via gRPC/WebSocket
        setSaved(true);
        setTimeout(() => setSaved(false), 2000);
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
                }}
            >
                PID Tuning
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {/* Motor selector */}
                <div style={{ display: 'flex', gap: '4px' }}>
                    {motorNames.map((name, idx) => (
                        <button
                            key={name}
                            onClick={() => setMotorIdx(idx)}
                            style={{
                                flex: 1, padding: '6px 0', border: 'none', borderRadius: '6px',
                                fontSize: '11px', fontWeight: 600, cursor: 'pointer',
                                background: motorIdx === idx ? `${COLORS.primary}25` : 'rgba(255,255,255,0.04)',
                                color: motorIdx === idx ? COLORS.primary : 'rgba(255,255,255,0.5)',
                                transition: 'all 0.15s',
                            }}
                        >
                            {name}
                        </button>
                    ))}
                </div>

                {/* PID Inputs */}
                {[
                    { label: 'Kp (Proportional)', value: kp, set: setKp },
                    { label: 'Ki (Integral)', value: ki, set: setKi },
                    { label: 'Kd (Derivative)', value: kd, set: setKd },
                ].map(param => (
                    <div key={param.label}>
                        <label style={{ fontSize: '10px', color: 'rgba(255,255,255,0.35)', textTransform: 'uppercase', letterSpacing: '0.06em', display: 'block', marginBottom: '4px' }}>
                            {param.label}
                        </label>
                        <input
                            type="number"
                            step="0.01"
                            value={param.value}
                            onChange={e => param.set(e.target.value)}
                            style={{
                                width: '100%', padding: '8px 12px', borderRadius: '6px',
                                border: '0.5px solid rgba(255,255,255,0.15)', background: 'rgba(255,255,255,0.04)',
                                color: '#fff', fontSize: '14px', fontFamily: 'monospace',
                                outline: 'none', boxSizing: 'border-box',
                            }}
                        />
                    </div>
                ))}

                {/* Apply button */}
                <button
                    onClick={handleSend}
                    style={{
                        padding: '10px', border: 'none', borderRadius: '8px',
                        background: saved ? COLORS.success : COLORS.primary,
                        color: '#fff', fontSize: '12px', fontWeight: 600,
                        cursor: 'pointer', transition: 'all 0.2s',
                        letterSpacing: '0.08em', textTransform: 'uppercase',
                    }}
                >
                    {saved ? '✓ Saved' : `Apply to Motor ${motorNames[motorIdx]}`}
                </button>
            </div>
        </>
    );
}
