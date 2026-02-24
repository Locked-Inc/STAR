import type { CSSProperties } from 'react';
import type { GamepadState } from '../hooks/useGamepad';
import type { ConnectionState } from '../store/dashboardStore';
import { COLORS } from '../theme';

interface ControllerViewProps {
  gamepadState: GamepadState;
  connectionState: ConnectionState;
}

const joystickSize = 120;
const dotSize = 16;
const joystickRadius = (joystickSize - dotSize) / 2;

const dotColors: Record<ConnectionState, string> = {
  connected: COLORS.connected,
  connecting: COLORS.warning,
  reconnecting: COLORS.warning,
  disconnected: COLORS.disconnected,
};

function clampToUnit(v: number): number {
  return Math.max(-1, Math.min(1, v));
}

export function ControllerView({ gamepadState, connectionState }: ControllerViewProps) {
  const isAnimated = connectionState === 'reconnecting' || connectionState === 'connecting';
  const dotColor = dotColors[connectionState] ?? COLORS.disconnected;

  const joystickX = clampToUnit(gamepadState.angularVel);
  const joystickY = -clampToUnit(gamepadState.linearVel); // Negative because -1 linear (reverse) should move down, 1 should move up

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', flex: 1 }}>

      {/* Liquid Glass Gateway Status Pill */}
      <div
        style={{
          background: 'rgba(255, 255, 255, 0.04)',
          backdropFilter: 'blur(12px) saturate(180%)',
          WebkitBackdropFilter: 'blur(12px) saturate(180%)',
          border: '0.5px solid rgba(255, 255, 255, 0.15)',
          borderRadius: '16px',
          padding: '4px 12px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          fontSize: '11px',
          fontWeight: 600,
          color: 'rgba(255,255,255,0.7)',
          letterSpacing: '0.05em',
          textTransform: 'uppercase',
          marginBottom: '16px',
        }}
      >
        <span
          style={{
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            backgroundColor: dotColor,
            border: '0.5px solid rgba(255,255,255,0.2)',
            animation: isAnimated ? 'pulse-opacity 1s infinite' : 'none',
          } as CSSProperties}
        />
        {connectionState === 'connected' ? 'Gateway' : connectionState}
      </div>

      {!gamepadState.connected ? (
        <div
          style={{
            backgroundColor: 'rgba(255, 159, 10, 0.15)', // System Orange tinted
            border: '0.5px solid rgba(255, 159, 10, 0.4)',
            backdropFilter: 'blur(16px)',
            borderRadius: '16px',
            padding: '16px',
            display: 'flex',
            alignItems: 'center',
            gap: '12px',
            marginTop: '8px',
          }}
        >
          <svg aria-hidden="true" style={{ color: '#FF9F0A' }} width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <line x1="6" y1="12" x2="10" y2="12"></line>
            <line x1="8" y1="10" x2="8" y2="14"></line>
            <line x1="15" y1="13.01" x2="15.01" y2="12.99"></line>
            <line x1="18" y1="11.01" x2="18.01" y2="10.99"></line>
            <path d="M17.32 5H6.68a4 4 0 0 0-3.978 3.59c-.006.052-.01.101-.017.152C2.604 9.416 2 14.456 2 16a3 3 0 0 0 3 3c1 0 1.5-.5 2-1l1.414-1.414A2 2 0 0 1 9.828 16h4.344a2 2 0 0 1 1.414.586L17 18c.5.5 1 1 2 1a3 3 0 0 0 3-3c0-1.545-.604-6.584-.685-7.258-.007-.05-.011-.1-.017-.151A4 4 0 0 0 17.32 5z"></path>
          </svg>
          <div style={{ textAlign: 'left' }}>
            <div style={{ fontSize: '12px', fontWeight: 600, color: '#FF9F0A', letterSpacing: '0.05em', textTransform: 'uppercase' }}>Gamepad Disconnected</div>
            <div style={{ fontSize: '11px', color: 'rgba(255,255,255,0.6)', marginTop: '2px' }}>Press any button to connect</div>
          </div>
        </div>
      ) : (
        <div style={{ width: '100%', display: 'flex', flexDirection: 'column', alignItems: 'center' }}>

          <div style={{ display: 'flex', justifyContent: 'space-around', width: '100%', marginBottom: '24px' }}>
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
              <div style={{ fontSize: '12px', color: 'rgba(255,255,255,0.65)' }}>Linear</div>
              <div className="data-flash" key={`lin-${gamepadState.linearVel}`} style={{ fontSize: '22px', fontWeight: 300, color: '#FFFFFF', fontVariantNumeric: 'tabular-nums' }}>
                {gamepadState.linearVel.toFixed(2)}
              </div>
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center' }}>
              <div style={{ fontSize: '12px', color: 'rgba(255,255,255,0.65)' }}>Angular</div>
              <div className="data-flash" key={`ang-${gamepadState.angularVel}`} style={{ fontSize: '22px', fontWeight: 300, color: '#FFFFFF', fontVariantNumeric: 'tabular-nums' }}>
                {gamepadState.angularVel.toFixed(2)}
              </div>
            </div>
          </div>

          {/* Virtual Joystick Target */}
          <div
            style={{
              position: 'relative',
              width: `${joystickSize}px`,
              height: `${joystickSize}px`,
              borderRadius: '50%',
              background: 'rgba(255,255,255,0.02)',
              border: '0.5px solid rgba(255,255,255,0.1)',
              boxShadow: 'inset 0 4px 12px rgba(0,0,0,0.5)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}
          >
            {/* Crosshair */}
            <div style={{ position: 'absolute', width: '100%', height: '1px', background: 'rgba(255,255,255,0.05)' }} />
            <div style={{ position: 'absolute', width: '1px', height: '100%', background: 'rgba(255,255,255,0.05)' }} />

            {/* The Dot */}
            <div
              style={{
                position: 'absolute',
                width: `${dotSize}px`,
                height: `${dotSize}px`,
                borderRadius: '50%',
                background: '#0a84ff',
                boxShadow: '0 0 12px rgba(10, 132, 255, 0.6), inset 0 2px 4px rgba(255,255,255,0.3)',
                transition: 'transform 0.05s linear',
                transform: `translate(${joystickX * joystickRadius}px, ${joystickY * joystickRadius}px)`
              }}
            />
          </div>

        </div>
      )}
    </div>
  );
}
