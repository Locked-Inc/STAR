import { useState, useRef, useEffect } from 'react';
import { COLORS } from '../theme';

/**
 * PID Tuning Panel - UI for reading/writing PID gains for each motor.
 * Maps to ConfigurationService.GetMotorPidConfig / SetMotorPidConfig.
 * Currently uses local state; will send gRPC when gateway wired.
 */

const savedResetMs = 2000;

export function PidTuningPanel() {
    const [motorIdx, setMotorIdx] = useState(0);
    const motorNames = ['FL', 'FR', 'BL', 'BR'];

    const [kp, setKp] = useState('1.0');
    const [ki, setKi] = useState('0.1');
    const [kd, setKd] = useState('0.05');
    const [saved, setSaved] = useState(false);
    const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

    useEffect(() => {
        return () => {
            if (timerRef.current !== null) clearTimeout(timerRef.current);
        };
    }, []);

    const handleSend = () => {
        // TODO: Call ConfigurationService.SetMotorPidConfig via gRPC/WebSocket
        if (timerRef.current !== null) clearTimeout(timerRef.current);
        setSaved(true);
        timerRef.current = setTimeout(() => {
            timerRef.current = null;
            setSaved(false);
        }, savedResetMs);
    };

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

                {/* Apply button */}
                <button
                    type="button"
                    onClick={handleSend}
                    style={{
                        padding: '10px', border: 'none', borderRadius: '8px',
                        background: saved ? COLORS.success : COLORS.primary,
                        color: '#fff', fontSize: '12px', fontWeight: 600,
                        cursor: 'pointer', transition: 'all 0.2s',
                        letterSpacing: '0.08em', textTransform: 'uppercase',
                    }}
                >
                    {saved ? '[OK] Saved' : `Apply to Motor ${motorNames[motorIdx]}`}
                </button>
            </div>
        </>
    );
}
