import type { GamepadState } from '../hooks/useGamepad';

interface ControllerViewProps {
  gamepadState: GamepadState;
  connected: boolean;
}

export function ControllerView({ gamepadState, connected }: ControllerViewProps) {
  return (
    <div style={{ padding: '12px', textAlign: 'center', fontFamily: 'sans-serif' }}>
      <div
        style={{
          marginBottom: '8px',
          padding: '4px 10px',
          borderRadius: '4px',
          backgroundColor: connected ? '#d4edda' : '#f8d7da',
          color: connected ? '#155724' : '#721c24',
          fontSize: '0.85em',
          fontWeight: 'bold',
          display: 'inline-block',
        }}
      >
        Gateway: {connected ? 'CONNECTED' : 'DISCONNECTED'}
      </div>

      {!gamepadState.connected ? (
        <div
          style={{
            backgroundColor: '#fff3cd',
            padding: '10px',
            borderRadius: '8px',
            border: '1px solid #ffeeba',
            fontSize: '0.85em',
          }}
        >
          <strong>Gamepad Disconnected</strong>
          <br />
          Connect gamepad and press any button.
        </div>
      ) : (
        <div>
          <div style={{ display: 'flex', justifyContent: 'center', gap: '30px', marginTop: '10px' }}>
            <div>
              <div style={{ fontSize: '0.75em', color: '#aaa' }}>Linear (V)</div>
              <div style={{ fontSize: '1.4em', fontWeight: 'bold' }}>
                {gamepadState.linearVel.toFixed(2)}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '0.75em', color: '#aaa' }}>Angular (w)</div>
              <div style={{ fontSize: '1.4em', fontWeight: 'bold' }}>
                {gamepadState.angularVel.toFixed(2)}
              </div>
            </div>
          </div>

          <div style={{ marginTop: '12px' }}>
            <div
              style={{
                width: '100px',
                height: '100px',
                borderRadius: '50%',
                backgroundColor: '#1a1d27',
                margin: '0 auto',
                position: 'relative',
                border: '2px solid #2a2e42',
              }}
            >
              <div
                style={{
                  width: '20px',
                  height: '20px',
                  borderRadius: '50%',
                  backgroundColor: '#3b82f6',
                  position: 'absolute',
                  top: `${50 - gamepadState.linearVel * 40}%`,
                  left: `${50 + gamepadState.angularVel * 40}%`,
                  transform: 'translate(-50%, -50%)',
                  transition: 'top 0.05s, left 0.05s',
                }}
              />
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
