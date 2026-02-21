import type { GamepadState } from '../hooks/useGamepad';

interface ControllerViewProps {
  gamepadState: GamepadState;
  connected: boolean;
}

const COLORS = {
  statusConnectedBg: '#d4edda',
  statusConnectedText: '#155724',
  statusDisconnectedBg: '#f8d7da',
  statusDisconnectedText: '#721c24',
  warningBg: '#fff3cd',
  warningBorder: '#ffeeba',
  joystickTrack: '#1a1d27',
  joystickKnob: '#3b82f6',
  joystickBorder: '#2a2e42',
  labelText: '#aaa',
} as const;

const SIZES = {
  joystickDiameter: '100px',
  joystickKnobDiameter: '20px',
  joystickBorderWidth: '2px',
  joystickTransition: '0.05s',
  warningPadding: '10px',
  warningBorderRadius: '8px',
} as const;

// Joystick positioning
const JOYSTICK_CENTER_PERCENT = 50;  // center of track in percent
const JOYSTICK_MAX_OFFSET_PERCENT = 40;  // max displacement for full-scale input

function clampToUnit(v: number): number {
  return Math.max(-1, Math.min(1, v));
}

export function ControllerView({ gamepadState, connected }: ControllerViewProps) {
  return (
    <div style={{ padding: '12px', textAlign: 'center', fontFamily: 'sans-serif' }}>
      <div
        style={{
          marginBottom: '8px',
          padding: '4px 10px',
          borderRadius: '4px',
          backgroundColor: connected ? COLORS.statusConnectedBg : COLORS.statusDisconnectedBg,
          color: connected ? COLORS.statusConnectedText : COLORS.statusDisconnectedText,
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
            backgroundColor: COLORS.warningBg,
            padding: SIZES.warningPadding,
            borderRadius: SIZES.warningBorderRadius,
            border: `1px solid ${COLORS.warningBorder}`,
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
              <div style={{ fontSize: '0.75em', color: COLORS.labelText }}>Linear (V)</div>
              <div style={{ fontSize: '1.4em', fontWeight: 'bold' }}>
                {gamepadState.linearVel.toFixed(2)}
              </div>
            </div>
            <div>
              <div style={{ fontSize: '0.75em', color: COLORS.labelText }}>Angular (w)</div>
              <div style={{ fontSize: '1.4em', fontWeight: 'bold' }}>
                {gamepadState.angularVel.toFixed(2)}
              </div>
            </div>
          </div>

          <div style={{ marginTop: '12px' }}>
            <div
              style={{
                width: SIZES.joystickDiameter,
                height: SIZES.joystickDiameter,
                borderRadius: '50%',
                backgroundColor: COLORS.joystickTrack,
                margin: '0 auto',
                position: 'relative',
                border: `${SIZES.joystickBorderWidth} solid ${COLORS.joystickBorder}`,
              }}
            >
              <div
                style={{
                  width: SIZES.joystickKnobDiameter,
                  height: SIZES.joystickKnobDiameter,
                  borderRadius: '50%',
                  backgroundColor: COLORS.joystickKnob,
                  position: 'absolute',
                  top: `${JOYSTICK_CENTER_PERCENT - clampToUnit(gamepadState.linearVel) * JOYSTICK_MAX_OFFSET_PERCENT}%`,
                  left: `${JOYSTICK_CENTER_PERCENT + clampToUnit(gamepadState.angularVel) * JOYSTICK_MAX_OFFSET_PERCENT}%`,
                  transform: 'translate(-50%, -50%)',
                  transition: `top ${SIZES.joystickTransition}, left ${SIZES.joystickTransition}`,
                }}
              />
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
