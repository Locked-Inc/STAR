import { useEffect, useLayoutEffect, useRef } from 'react';
import { useGamepad } from '../hooks/useGamepad';
import { useDashboardStore } from '../store/dashboardStore';
import { ControllerView } from './ControllerView';
import type { ControllerState } from '../proto/star/v1/controller';

const SEND_RATE_HZ = 50;
const SEND_INTERVAL_MS = 1000 / SEND_RATE_HZ;

interface TeleopPanelProps {
  sendControllerState: (state: ControllerState) => void;
}

export function TeleopPanel({ sendControllerState }: TeleopPanelProps) {
  const gamepadState = useGamepad();
  const connectionState = useDashboardStore((s) => s.connectionState);

  // Refs kept current via useLayoutEffect so interval closure always reads latest values.
  const sendRef = useRef(sendControllerState);
  const gamepadRef = useRef(gamepadState);

  // Update refs after each render (before the next paint) to avoid stale closures.
  useLayoutEffect(() => {
    sendRef.current = sendControllerState;
    gamepadRef.current = gamepadState;
  });

  useEffect(() => {
    const interval = setInterval(() => {
      const gp = gamepadRef.current;
      sendRef.current({
        linearVel: gp.linearVel,
        angularVel: gp.angularVel,
        timestamp: String(Date.now()),
        debug: false,
      });
    }, SEND_INTERVAL_MS);

    return () => clearInterval(interval);
  }, []); // interval never torn down; refs provide latest values

  return (
    <div
      style={{
        background: '#1a1d27',
        border: '1px solid #2a2e42',
        borderRadius: '6px',
        overflow: 'hidden',
      }}
    >
      <div
        style={{
          padding: '6px 10px',
          borderBottom: '1px solid #2a2e42',
          fontSize: '11px',
          fontWeight: 'bold',
          color: '#9ca3af',
          textTransform: 'uppercase',
          letterSpacing: '0.05em',
        }}
      >
        Teleop
      </div>
      <ControllerView
        gamepadState={gamepadState}
        connected={connectionState === 'connected'}
      />
    </div>
  );
}
