import { useEffect, useRef } from 'react';
import type { ControllerState } from '../proto/star/v1/controller';
import { controllerSendIntervalMs } from '../lib/dashboard';
import { useGamepad } from './useGamepad';

export function useControllerBridge(sendControllerState: (state: ControllerState) => void, enabled: boolean) {
  const gamepadState = useGamepad();
  const sendRef = useRef(sendControllerState);
  const gamepadRef = useRef(gamepadState);

  useEffect(() => {
    sendRef.current = sendControllerState;
  }, [sendControllerState]);

  useEffect(() => {
    gamepadRef.current = gamepadState;
  }, [gamepadState]);

  useEffect(() => {
    if (!enabled) {
      sendRef.current({
        linearVel: 0,
        angularVel: 0,
        timestamp: String(Date.now()),
        debug: false,
      });
      return;
    }

    const interval = window.setInterval(() => {
      const controllerState = gamepadRef.current;
      sendRef.current({
        linearVel: controllerState.linearVel,
        angularVel: controllerState.angularVel,
        timestamp: String(Date.now()),
        debug: false,
      });
    }, controllerSendIntervalMs);

    return () => window.clearInterval(interval);
  }, [enabled]);

  return gamepadState;
}
