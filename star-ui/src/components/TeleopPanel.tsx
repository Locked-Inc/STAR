import { useEffect, useLayoutEffect, useRef } from 'react';
import { useGamepad } from '../hooks/useGamepad';
import { useDashboardStore } from '../store/dashboardStore';
import { ControllerView } from './ControllerView';
import type { ControllerState } from '../proto/star/v1/controller';

const sendRateHz = 50;
const sendIntervalMs = 1000 / sendRateHz;

interface TeleopPanelProps {
  sendControllerState: (state: ControllerState) => void;
}

export function TeleopPanel({ sendControllerState }: TeleopPanelProps) {
  const gamepadState = useGamepad();
  const connectionState = useDashboardStore((s) => s.connectionState);

  const sendRef = useRef(sendControllerState);
  const gamepadRef = useRef(gamepadState);

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
        timestampMs: String(Date.now()),
        debug: false,
      });
    }, sendIntervalMs);

    return () => clearInterval(interval);
  }, []);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/*
        The drag-handle class here makes the entire header the target
        for moving the react-grid-layout panel.
      */}
      <div
        className="panel-header"
        style={{
          padding: '16px 20px 8px 20px',
          fontSize: '11px',
          fontWeight: 600,
          color: 'rgba(255, 255, 255, 0.5)',
          textTransform: 'uppercase',
          letterSpacing: '0.12em',
          userSelect: 'none',
        }}
      >
        Movement
      </div>

      <div style={{ padding: '0 20px 20px 20px', flex: 1, display: 'flex', flexDirection: 'column' }}>
        <ControllerView
          gamepadState={gamepadState}
          connectionState={connectionState}
        />
      </div>
    </div>
  );
}
