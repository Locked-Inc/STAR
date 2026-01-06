import { useEffect, useRef, useState } from 'react';
import { ControllerService } from '../services/ControllerService';
import type { GamepadState } from './useGamepad';

export const useControllerConnection = (gamepadState: GamepadState, debug: boolean = false) => {
  const [connected, setConnected] = useState(false);
  const serviceRef = useRef<ControllerService | null>(null);

  useEffect(() => {
    // Target the gateway on port 8080 (assuming local network/RPi)
    // In production, this might be a configurable URL
    const gatewayUrl = `ws://${window.location.hostname}:8080/ws/controller`;
    
    const service = new ControllerService(gatewayUrl, (status) => {
      setConnected(status);
    });

    service.connect();
    serviceRef.current = service;

    return () => {
      service.disconnect();
    };
  }, []);

  useEffect(() => {
    if (!connected || !serviceRef.current) return;

    if (!gamepadState.connected) {
      // Gamepad disconnected - send stop command for safety
      serviceRef.current.sendState(0, 0, debug);
      return;
    }

    serviceRef.current.sendState(gamepadState.linearVel, gamepadState.angularVel, debug);
    
  }, [connected, gamepadState, debug]);

  return { gatewayConnected: connected };
};
