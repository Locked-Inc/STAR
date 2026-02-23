import type { CSSProperties } from 'react';
import { useState } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import type { ConnectionState } from '../store/dashboardStore';
import { COLORS } from '../theme';
import { RobotMode } from '../proto/star/v1/telemetry';

interface StatusBarProps {
  sendEStop: (reason: string) => void;
  onResetLayout: () => void;
  applyPreset: (presetName: 'DEFAULT' | 'FOCUS' | 'DATA') => void;
}

const ESTOP_SOURCE_USER_UI = 'user_ui_button';

const DOT_COLORS: Record<ConnectionState, string> = {
  connected: COLORS.connected,
  connecting: COLORS.warning,
  reconnecting: COLORS.warning,
  disconnected: COLORS.disconnected,
};

export function StatusBar({ sendEStop, onResetLayout, applyPreset }: StatusBarProps) {
  const { connectionState, dataIsStale, eStopActive, systemStatus } = useDashboardStore(
    useShallow((s) => ({
      connectionState: s.connectionState,
      dataIsStale: s.dataIsStale,
      eStopActive: s.eStopActive,
      systemStatus: s.systemStatus,
    }))
  );

  const dotColor = DOT_COLORS[connectionState] ?? COLORS.disconnected;

  function handleEStop(): void {
    sendEStop(ESTOP_SOURCE_USER_UI);
    useDashboardStore.getState().triggerEStop();
  }

  const styles = `
    @keyframes pulse-opacity {
      0% { opacity: 0.4; }
      50% { opacity: 1; }
      100% { opacity: 0.4; }
    }
    .status-pill {
      background: rgba(255, 255, 255, 0.04);
      backdrop-filter: blur(24px) saturate(180%);
      -webkit-backdrop-filter: blur(24px) saturate(180%);
      border: 0.5px solid rgba(255, 255, 255, 0.18);
      border-radius: 16px;
      padding: 6px 14px;
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.12em;
      color: rgba(255,255,255,0.8);
      text-transform: uppercase;
      box-shadow: 0 4px 16px rgba(0, 0, 0, 0.3);
    }
    .stale-badge {
      background: rgba(255,160,0,0.15);
      border: 0.5px solid rgba(255,160,0,0.4);
      color: #FF9F0A;
    }
    .estop-btn {
      background: rgba(220,38,38,0.9);
      backdrop-filter: blur(12px);
      border: 0.5px solid rgba(255,100,100,0.5);
      border-radius: 12px;
      padding: 8px 24px;
      color: #fff;
      font-weight: 700;
      font-size: 13px;
      letter-spacing: 0.1em;
      text-transform: uppercase;
      cursor: pointer;
      transition: all 0.2s cubic-bezier(0.25, 1, 0.5, 1);
      box-shadow: 0 4px 12px rgba(220, 38, 38, 0.3);
    }
    .estop-btn:hover:not(:disabled) {
      transform: scale(1.04);
      box-shadow: 0 8px 24px rgba(220, 38, 38, 0.5), inset 0 0 12px rgba(255,100,100,0.6);
    }
    .estop-btn:active:not(:disabled) {
      transform: scale(0.97);
      background: rgba(255,0,0,1);
      transition: all 0.08s ease-out;
    }
    .estop-btn:disabled {
      background: #8E0000;
      border-color: #ff453a44;
      color: #ff453a88;
      cursor: not-allowed;
      transform: translateY(2px);
      box-shadow: inset 0 4px 8px rgba(0,0,0,0.6);
    }
    .reset-btn {
      background: rgba(255, 255, 255, 0.04);
      backdrop-filter: blur(24px) saturate(180%);
      border: 0.5px solid rgba(255, 255, 255, 0.18);
      border-radius: 12px;
      padding: 8px;
      color: rgba(255,255,255,0.7);
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s;
    }
    .reset-btn:hover {
      background: rgba(255,255,255,0.1);
      color: #fff;
      transform: scale(1.05);
    }
    .reset-btn:active {
      transform: scale(0.95);
    }
  `;

  const [presetOpen, setPresetOpen] = useState(false);

  const isAnimated = connectionState === 'reconnecting' || connectionState === 'connecting';

  return (
    <>
      <style>{styles}</style>
      <div
        style={{
          position: 'absolute',
          top: 0,
          left: 0,
          right: 0,
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          padding: '24px 32px',
          zIndex: 1000, // Stay above draggable windows
          pointerEvents: 'none', // Let clicks pass through empty areas
        }}
      >
        <div style={{ display: 'flex', gap: '12px', pointerEvents: 'auto' }}>
          {/* Connection Pill */}
          <div className="status-pill">
            <span
              style={{
                width: '8px',
                height: '8px',
                borderRadius: '50%',
                backgroundColor: dotColor,
                display: 'inline-block',
                animation: isAnimated ? 'pulse-opacity 1s infinite' : 'none',
              } as CSSProperties}
            />
            {connectionState}
          </div>

          {/* Stale Badge */}
          {dataIsStale && (
            <div className="status-pill stale-badge">
              STALE
            </div>
          )}

          {/* Robot Mode Badge */}
          {systemStatus && (() => {
            const modeConfig: Record<number, { label: string; color: string }> = {
              [RobotMode.IDLE]: { label: 'IDLE', color: COLORS.textMuted },
              [RobotMode.MANUAL]: { label: 'MANUAL', color: COLORS.accent },
              [RobotMode.AUTONOMOUS]: { label: 'AUTO', color: COLORS.success },
              [RobotMode.MAPPING]: { label: 'MAPPING', color: COLORS.warning },
              [RobotMode.EMERGENCY_STOP]: { label: 'E-STOP', color: COLORS.danger },
            };
            const cfg = modeConfig[systemStatus.mode] ?? { label: 'UNKNOWN', color: COLORS.textMuted };
            return (
              <div className="status-pill" style={{
                background: `${cfg.color}15`,
                border: `0.5px solid ${cfg.color}44`,
                color: cfg.color,
              }}>
                {cfg.label}
              </div>
            );
          })()}
        </div>

        <div style={{ flex: 1 }} />

        {/* Right side controls */}
        <div style={{ display: 'flex', gap: '16px', alignItems: 'center', pointerEvents: 'auto' }}>
          <div style={{ position: 'relative' }}>
            <button
              type="button"
              className="reset-btn"
              onClick={() => setPresetOpen(!presetOpen)}
              title="Layout Presets"
              style={{ padding: '8px 16px', gap: '8px', fontSize: '12px', fontWeight: 600, letterSpacing: '0.05em', backgroundImage: presetOpen ? 'linear-gradient(180deg, rgba(255,255,255,0.1), rgba(255,255,255,0.05))' : '' }}
            >
              <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect>
                <line x1="3" y1="9" x2="21" y2="9"></line>
                <line x1="9" y1="21" x2="9" y2="9"></line>
              </svg>
              LAYOUTS
            </button>

            {presetOpen && (
              <div style={{
                position: 'absolute',
                top: '100%',
                right: 0,
                marginTop: '12px',
                background: 'rgba(20, 20, 20, 0.85)',
                backdropFilter: 'blur(24px) saturate(180%)',
                WebkitBackdropFilter: 'blur(24px) saturate(180%)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                borderRadius: '16px',
                padding: '8px',
                display: 'flex',
                flexDirection: 'column',
                gap: '8px',
                boxShadow: '0 8px 32px rgba(0, 0, 0, 0.5)',
                minWidth: '200px'
              }}>
                {(['DEFAULT', 'FOCUS', 'DATA'] as const).map(preset => (
                  <button
                    key={preset}
                    className="reset-btn"
                    style={{ justifyContent: 'flex-start', padding: '10px 12px', background: 'transparent', border: 'none', borderRadius: '8px' }}
                    onClick={() => {
                      applyPreset(preset);
                      setPresetOpen(false);
                    }}
                  >
                    <span style={{ fontSize: '13px', fontWeight: 500 }}>{preset === 'DEFAULT' ? 'Default Grid' : preset === 'FOCUS' ? 'Focus Mode' : 'Data Mode'}</span>
                  </button>
                ))}
                <div style={{ height: '1px', background: 'rgba(255,255,255,0.1)', margin: '4px 0' }} />
                <button
                  className="reset-btn"
                  style={{ justifyContent: 'flex-start', padding: '10px 12px', background: 'transparent', border: 'none', borderRadius: '8px', color: 'rgba(255,255,255,0.5)' }}
                  onClick={() => {
                    onResetLayout();
                    setPresetOpen(false);
                  }}
                >
                  <span style={{ fontSize: '12px' }}>Reset to Default</span>
                </button>
              </div>
            )}
          </div>

          <button
            type="button"
            className="estop-btn"
            onClick={handleEStop}
            disabled={eStopActive}
            aria-label={eStopActive ? 'Emergency stop active' : 'Activate emergency stop'}
          >
            {eStopActive ? 'E-STOP ACTIVE' : 'E-STOP'}
          </button>
        </div>
      </div>
    </>
  );
}
