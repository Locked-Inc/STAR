import { useEffect, useLayoutEffect, useRef } from 'react';
import { useGamepad } from '../hooks/useGamepad';
import { useDashboardStore } from '../store/dashboardStore';
import { ControllerView } from './ControllerView';
import type { ControllerState } from '../proto/star/v1/controller';
import { PANEL_CONTAINER_STYLE, PANEL_HEADER_STYLE } from '../theme';

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

  // Intentionally no deps: runs after every render to keep refs current,
  // allowing the interval closure to always read the latest callbacks.
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

    // interval is cleared on unmount via clearInterval(interval); refs keep latest values
    return () => clearInterval(interval);
  }, []);

  return (
    <div style={PANEL_CONTAINER_STYLE}>
      <div style={PANEL_HEADER_STYLE}>
        Teleop
      </div>
      <ControllerView
        gamepadState={gamepadState}
        connected={connectionState === 'connected'}
      />
    </div>
  );
}
