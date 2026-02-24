import { useState, useRef, useEffect } from 'react';
import { COLORS } from '../theme';

/**
 * Firmware Update Panel - OTA update UI matching FirmwareUpdateService proto.
 * Supports: BeginUpdate, WriteChunk, FinalizeUpdate, AbortUpdate, Reboot, Rollback.
 * Currently placeholder - will integrate with gateway when available.
 */

const uploadPollMs = 200;

export function FirmwarePanel() {
    const [stage, setStage] = useState<'idle' | 'uploading' | 'finalizing' | 'done'>('idle');
    const [progress, setProgress] = useState(0);
    const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

    useEffect(() => {
        return () => {
            if (intervalRef.current !== null) clearInterval(intervalRef.current);
        };
    }, []);

    const simulateUpload = () => {
        if (intervalRef.current !== null) clearInterval(intervalRef.current);
        setStage('uploading');
        setProgress(0);
        intervalRef.current = setInterval(() => {
            setProgress(p => {
                if (p >= 100) {
                    clearInterval(intervalRef.current!);
                    intervalRef.current = null;
                    setStage('done');
                    return 100;
                }
                return p + 5;
            });
        }, uploadPollMs);
    };

    const handleReset = () => {
        if (intervalRef.current !== null) {
            clearInterval(intervalRef.current);
            intervalRef.current = null;
        }
        setStage('idle');
        setProgress(0);
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
                Firmware OTA
            </div>

            <div className="panel-body" style={{ padding: '8px 20px 20px 20px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {/* Current version */}
                <div style={{
                    background: 'rgba(255,255,255,0.03)', borderRadius: '6px', padding: '10px 14px',
                    fontSize: '12px', display: 'flex', justifyContent: 'space-between',
                }}>
                    <span style={{ color: 'rgba(255,255,255,0.4)' }}>Current</span>
                    <span style={{ color: COLORS.primary, fontFamily: 'monospace', fontWeight: 600 }}>--</span>
                </div>

                {/* Progress bar */}
                {stage !== 'idle' && (
                    <div>
                        <div style={{
                            height: '6px', borderRadius: '3px',
                            background: 'rgba(255,255,255,0.08)',
                            overflow: 'hidden',
                        }}>
                            <div style={{
                                height: '100%', borderRadius: '3px',
                                background: stage === 'done' ? COLORS.success : COLORS.primary,
                                width: `${progress}%`,
                                transition: 'width 0.2s ease-out',
                            }} />
                        </div>
                        <div style={{ fontSize: '10px', color: 'rgba(255,255,255,0.4)', marginTop: '4px', textAlign: 'center' }}>
                            {stage === 'done' ? 'Update complete' : `Uploading... ${progress}%`}
                        </div>
                    </div>
                )}

                {/* Action buttons */}
                <div style={{ display: 'flex', gap: '8px' }}>
                    <button
                        type="button"
                        onClick={simulateUpload}
                        disabled={stage === 'uploading'}
                        style={{
                            flex: 1, padding: '8px', border: 'none', borderRadius: '6px',
                            background: COLORS.primary, color: '#fff',
                            fontSize: '11px', fontWeight: 600, cursor: 'pointer',
                            opacity: stage === 'uploading' ? 0.5 : 1,
                            letterSpacing: '0.06em', textTransform: 'uppercase',
                        }}
                    >
                        Upload
                    </button>
                    <button
                        type="button"
                        onClick={handleReset}
                        style={{
                            flex: 1, padding: '8px', border: '0.5px solid rgba(255,255,255,0.15)',
                            borderRadius: '6px', background: 'rgba(255,255,255,0.04)',
                            color: 'rgba(255,255,255,0.6)', fontSize: '11px', fontWeight: 600,
                            cursor: 'pointer', letterSpacing: '0.06em', textTransform: 'uppercase',
                        }}
                    >
                        Rollback
                    </button>
                </div>

                {/* Reboot */}
                <button
                    type="button"
                    onClick={handleReset}
                    style={{
                        padding: '8px', border: '0.5px solid rgba(255,160,0,0.3)',
                        borderRadius: '6px', background: 'rgba(255,160,0,0.08)',
                        color: COLORS.warning, fontSize: '11px', fontWeight: 600,
                        cursor: 'pointer', letterSpacing: '0.06em', textTransform: 'uppercase',
                    }}
                >
                    Reboot MCU
                </button>
            </div>
        </>
    );
}
