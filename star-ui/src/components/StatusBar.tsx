import type { CSSProperties } from 'react';
import { useState, useEffect } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useDashboardStore } from '../store/dashboardStore';
import type { ConnectionState } from '../store/dashboardStore';
import { COLORS } from '../theme';
import { RobotMode } from '../proto/star/v1/telemetry';
import { VIEWS } from '../store/useWindowStore';
import type { ViewName } from '../store/useWindowStore';

interface StatusBarProps {
  sendEStop: (reason: string) => void;
  sendEStopRelease: () => void;
  onResetLayout: () => void;
  activeView: ViewName;
  setView: (view: ViewName) => void;
}

const ESTOP_SOURCE_USER_UI = 'user_ui_button';

const DOT_COLORS: Record<ConnectionState, string> = {
  connected: COLORS.connected,
  connecting: COLORS.warning,
  reconnecting: COLORS.warning,
  disconnected: COLORS.disconnected,
};

export function StatusBar({ sendEStop, sendEStopRelease, onResetLayout, activeView, setView }: StatusBarProps) {
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

  function handleResume(): void {
    sendEStopRelease();
    useDashboardStore.getState().releaseEStop();
  }

  // ── Theme toggle ──
  const [theme, setTheme] = useState<'dark' | 'light'>(() => {
    if (typeof window !== 'undefined') {
      return (localStorage.getItem('star-theme') as 'dark' | 'light') || 'dark';
    }
    return 'dark';
  });

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('star-theme', theme);
  }, [theme]);

  function toggleTheme(): void {
    setTheme(prev => prev === 'dark' ? 'light' : 'dark');
  }

  const styles = `
    @keyframes pulse-opacity {
      0% { opacity: 0.4; }
      50% { opacity: 1; }
      100% { opacity: 0.4; }
    }
    .status-pill {
      background: var(--pill-bg);
      backdrop-filter: blur(24px) saturate(180%);
      -webkit-backdrop-filter: blur(24px) saturate(180%);
      border: 0.5px solid var(--pill-border);
      border-radius: 16px;
      padding: 6px 14px;
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.12em;
      color: var(--pill-text);
      text-transform: uppercase;
      box-shadow: var(--pill-shadow);
      transition: background 0.3s, border-color 0.3s, color 0.3s;
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
      background: var(--btn-bg);
      backdrop-filter: blur(24px) saturate(180%);
      border: 0.5px solid var(--btn-border);
      border-radius: 12px;
      padding: 8px;
      color: var(--btn-text);
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s;
    }
    .reset-btn:hover {
      background: var(--btn-hover-bg);
      color: var(--btn-hover-text);
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
                background: 'var(--dropdown-bg)',
                backdropFilter: 'blur(24px) saturate(180%)',
                WebkitBackdropFilter: 'blur(24px) saturate(180%)',
                border: '1px solid var(--dropdown-border)',
                borderRadius: '16px',
                padding: '8px',
                display: 'flex',
                flexDirection: 'column',
                gap: '8px',
                boxShadow: 'var(--dropdown-shadow)',
                minWidth: '200px'
              }}>
                {(Object.keys(VIEWS) as ViewName[]).map(viewName => {
                  const v = VIEWS[viewName];
                  const isActive = viewName === activeView;
                  return (
                    <button
                      key={viewName}
                      type="button"
                      className="reset-btn"
                      style={{
                        justifyContent: 'flex-start',
                        padding: '10px 12px',
                        background: isActive ? 'rgba(255,255,255,0.08)' : 'transparent',
                        border: isActive ? '0.5px solid rgba(255,255,255,0.15)' : 'none',
                        borderRadius: '8px',
                        display: 'flex',
                        gap: '10px',
                        alignItems: 'center',
                      }}
                      onClick={() => {
                        setView(viewName);
                        setPresetOpen(false);
                      }}
                    >
                      <span style={{ fontSize: '16px' }}>{v.icon}</span>
                      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-start', gap: '2px' }}>
                        <span style={{ fontSize: '13px', fontWeight: 600, color: isActive ? '#fff' : 'rgba(255,255,255,0.7)' }}>{v.label}</span>
                        <span style={{ fontSize: '10px', color: 'rgba(255,255,255,0.35)' }}>{v.desc}</span>
                      </div>
                    </button>
                  );
                })}
                <div style={{ height: '1px', background: 'rgba(255,255,255,0.1)', margin: '4px 0' }} />
                <button
                  type="button"
                  className="reset-btn"
                  style={{ justifyContent: 'flex-start', padding: '10px 12px', background: 'transparent', border: 'none', borderRadius: '8px', color: 'var(--color-text-dim)' }}
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

          {/* Theme toggle */}
          <button
            type="button"
            className="reset-btn"
            onClick={toggleTheme}
            title={theme === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'}
            aria-label="Toggle theme"
            style={{ padding: '8px 12px', fontSize: '16px' }}
          >
            {theme === 'dark' ? '☀️' : '🌙'}
          </button>

          <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
            {eStopActive && (
              <button
                type="button"
                onClick={handleResume}
                aria-label="Resume from emergency stop"
                style={{
                  background: 'rgba(48,209,88,0.85)',
                  backdropFilter: 'blur(12px)',
                  border: '0.5px solid rgba(48,209,88,0.5)',
                  borderRadius: '12px',
                  padding: '8px 20px',
                  color: '#fff',
                  fontWeight: 700,
                  fontSize: '13px',
                  letterSpacing: '0.1em',
                  textTransform: 'uppercase' as const,
                  cursor: 'pointer',
                  transition: 'all 0.2s cubic-bezier(0.25, 1, 0.5, 1)',
                  boxShadow: '0 4px 12px rgba(48,209,88,0.3)',
                }}
              >
                RESUME
              </button>
            )}
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
      </div>
    </>
  );
}
