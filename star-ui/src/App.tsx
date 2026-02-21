import { useSTARConnection } from './hooks/useSTARConnection';
import { StatusBar } from './components/StatusBar';
import { TeleopPanel } from './components/TeleopPanel';
import { MotorPanel } from './components/MotorPanel';
import { BatteryPanel } from './components/BatteryPanel';
import { OdometryPanel } from './components/OdometryPanel';
import { LidarPanel } from './components/LidarPanel';
import { AlertsPanel } from './components/AlertsPanel';
import { PacketAnalyzer } from './components/PacketAnalyzer';

const WS_PORT: number = Number(import.meta.env.VITE_WS_PORT) || 8080;
const WS_PROTOCOL = window.location.protocol === 'https:' ? 'wss' : 'ws';
const WS_URL = `${WS_PROTOCOL}://${window.location.hostname}:${WS_PORT}/ws`;

const LAYOUT_PADDING = 8;
const LAYOUT_GAP = 8;
const MIN_CARD_WIDTH = 280;

function App() {
  const { sendControllerState, sendEStop } = useSTARConnection(WS_URL);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <StatusBar sendEStop={sendEStop} />

      <div
        style={{
          flex: 1,
          overflow: 'auto',
          padding: LAYOUT_PADDING,
          display: 'grid',
          gridTemplateColumns: `repeat(auto-fit, minmax(${MIN_CARD_WIDTH}px, 1fr))`,
          gridAutoRows: 'min-content',
          gap: LAYOUT_GAP,
          alignContent: 'start',
        }}
      >
        <TeleopPanel sendControllerState={sendControllerState} />
        <MotorPanel />
        <BatteryPanel />
        <OdometryPanel />
        <LidarPanel />
        <AlertsPanel />
        <div style={{ gridColumn: '1 / -1' }}>
          <PacketAnalyzer />
        </div>
      </div>
    </div>
  );
}

export default App;
