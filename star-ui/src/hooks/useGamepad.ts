import { useState, useEffect, useCallback, useRef } from 'react';

export interface GamepadState {
  connected: boolean;
  linearVel: number; // Y-axis (clamped -1 to 1)
  angularVel: number; // X-axis (clamped -1 to 1)
}

export const useGamepad = () => {
  const [state, setState] = useState<GamepadState>({
    connected: false,
    linearVel: 0,
    angularVel: 0,
  });

  const requestRef = useRef<number>(0);

  const updateGamepadState = useCallback(() => {
    const gamepads = navigator.getGamepads();
    // Use the first available gamepad
    const gamepad = gamepads[0] || gamepads.find(gp => gp !== null);

    if (gamepad) {
      // Arcade Drive Mapping:
      // Linear (Forward/Backward): Left Stick Y-axis (axis 1 in standard mapping)
      // Angular (Steering): Left Stick X-axis (axis 0 in standard mapping)
      // Note: Y-axis is often inverted in browser API (up is -1)
      const rawLinear = -gamepad.axes[1]; 
      const rawAngular = gamepad.axes[0];

      // Deadzone handling (0.1)
      const linear = Math.abs(rawLinear) > 0.1 ? rawLinear : 0;
      const angular = Math.abs(rawAngular) > 0.1 ? rawAngular : 0;

      setState({
        connected: true,
        linearVel: linear,
        angularVel: angular,
      });
    } else {
      setState(prev => prev.connected ? { connected: false, linearVel: 0, angularVel: 0 } : prev);
    }

    requestRef.current = requestAnimationFrame(updateGamepadState);
  }, []);

  useEffect(() => {
    const handleConnect = () => {
      console.log('Gamepad connected');
      requestRef.current = requestAnimationFrame(updateGamepadState);
    };

    const handleDisconnect = () => {
      console.log('Gamepad disconnected');
      // If no gamepads left, the loop will handle setting connected: false
    };

    window.addEventListener('gamepadconnected', handleConnect);
    window.addEventListener('gamepaddisconnected', handleDisconnect);

    // Start polling immediately if a gamepad is already there
    requestRef.current = requestAnimationFrame(updateGamepadState);

    return () => {
      window.removeEventListener('gamepadconnected', handleConnect);
      window.removeEventListener('gamepaddisconnected', handleDisconnect);
      cancelAnimationFrame(requestRef.current);
    };
  }, [updateGamepadState]);

  return state;
};
