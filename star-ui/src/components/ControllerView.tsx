import React, { useState } from 'react';
import { useGamepad } from '../hooks/useGamepad';
import { useControllerConnection } from '../hooks/useControllerConnection';

export const ControllerView: React.FC = () => {
  const [debugMode, setDebugMode] = useState(false);
  const gamepadState = useGamepad();
  const { gatewayConnected } = useControllerConnection(gamepadState, debugMode);

  return (
    <div style={{ padding: '20px', textAlign: 'center', fontFamily: 'sans-serif' }}>
      <h1>STAR Robot Controller</h1>

      <div style={{ marginBottom: '20px', display: 'flex', justifyContent: 'center', alignItems: 'center', gap: '20px' }}>
        <span style={{ 
          padding: '5px 10px', 
          borderRadius: '4px', 
          backgroundColor: gatewayConnected ? '#d4edda' : '#f8d7da',
          color: gatewayConnected ? '#155724' : '#721c24',
          fontSize: '0.9em',
          fontWeight: 'bold'
        }}>
          Gateway: {gatewayConnected ? 'CONNECTED' : 'DISCONNECTED'}
        </span>

        <label style={{ display: 'flex', alignItems: 'center', cursor: 'pointer', fontSize: '0.9em' }}>
          <input 
            type="checkbox" 
            checked={debugMode} 
            onChange={(e) => setDebugMode(e.target.checked)}
            style={{ marginRight: '5px' }}
          />
          Debug Mode (Verbose Logs)
        </label>
      </div>
      
      {!gamepadState.connected ? (
        <div style={{ backgroundColor: '#fff3cd', padding: '20px', borderRadius: '8px', border: '1px solid #ffeeba' }}>
          <h2>Gamepad Disconnected</h2>
          <p>Please connect a gamepad and <strong>press any button</strong> to activate.</p>
        </div>
      ) : (
        <div style={{ backgroundColor: '#d4edda', padding: '20px', borderRadius: '8px', border: '1px solid #c3e6cb' }}>
          <h2 style={{ color: '#155724' }}>Gamepad Connected</h2>
          
          <div style={{ display: 'flex', justifyContent: 'center', gap: '50px', marginTop: '30px' }}>
            <div>
              <h3>Linear (V)</h3>
              <div style={{ fontSize: '2em', fontWeight: 'bold' }}>{gamepadState.linearVel.toFixed(2)}</div>
              <div style={{ width: '20px', height: '100px', backgroundColor: '#eee', margin: '10px auto', position: 'relative' }}>
                <div style={{ 
                  width: '100%', 
                  height: `${Math.abs(gamepadState.linearVel) * 50}%`, 
                  backgroundColor: gamepadState.linearVel >= 0 ? 'green' : 'red',
                  position: 'absolute',
                  bottom: gamepadState.linearVel >= 0 ? '50%' : 'auto',
                  top: gamepadState.linearVel < 0 ? '50%' : 'auto'
                }} />
              </div>
            </div>

            <div>
              <h3>Angular (ω)</h3>
              <div style={{ fontSize: '2em', fontWeight: 'bold' }}>{gamepadState.angularVel.toFixed(2)}</div>
              <div style={{ width: '100px', height: '20px', backgroundColor: '#eee', margin: '50px auto', position: 'relative' }}>
                <div style={{ 
                  height: '100%', 
                  width: `${Math.abs(gamepadState.angularVel) * 50}%`, 
                  backgroundColor: 'blue',
                  position: 'absolute',
                  left: gamepadState.angularVel >= 0 ? '50%' : 'auto',
                  right: gamepadState.angularVel < 0 ? '50%' : 'auto'
                }} />
              </div>
            </div>
          </div>

          <div style={{ marginTop: '40px' }}>
            <div style={{ 
              width: '150px', 
              height: '150px', 
              borderRadius: '50%', 
              backgroundColor: '#eee', 
              margin: '0 auto', 
              position: 'relative',
              border: '2px solid #ccc'
            }}>
              <div style={{ 
                width: '30px', 
                height: '30px', 
                borderRadius: '50%', 
                backgroundColor: '#007bff', 
                position: 'absolute',
                top: `${50 - (gamepadState.linearVel * 40)}%`,
                left: `${50 + (gamepadState.angularVel * 40)}%`,
                transform: 'translate(-50%, -50%)',
                transition: 'top 0.05s, left 0.05s'
              }} />
            </div>
            <p>Left Stick Visualization</p>
          </div>
        </div>
      )}

      <div style={{ marginTop: '50px', fontSize: '0.8em', color: '#666' }}>
        <p>Target Device: Retroid Pocket 2S</p>
        <p>Ensure "Gamepad Mode" is enabled in Android settings.</p>
      </div>
    </div>
  );
};
