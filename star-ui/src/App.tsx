import { useSTARConnection } from './hooks/useSTARConnection';
import { StatusBar } from './components/StatusBar';
import { TeleopPanel } from './components/TeleopPanel';
import { MotorPanel } from './components/MotorPanel';
import { BatteryPanel } from './components/BatteryPanel';
import { OdometryPanel } from './components/OdometryPanel';
import { LidarPanel } from './components/LidarPanel';
import { AlertsPanel } from './components/AlertsPanel';
import { PacketAnalyzer } from './components/PacketAnalyzer';

const WS_URL = `ws://${window.location.hostname}:8080/ws`;

function App() {
  const { sendControllerState, sendEStop } = useSTARConnection(WS_URL);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <StatusBar sendEStop={sendEStop} />

      <div
        style={{
          flex: 1,
          overflow: 'auto',
          padding: '8px',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fit, minmax(280px, 1fr))',
          gridAutoRows: 'min-content',
          gap: '8px',
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
