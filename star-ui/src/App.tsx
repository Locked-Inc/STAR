import { useSTARConnection } from './hooks/useSTARConnection';
import { useAppRoute } from './hooks/useAppRoute';
import { MainDashboardView } from './views/MainDashboardView';
import { RosDebugView } from './views/RosDebugView';

const defaultWsPort = 8080;
const minTcpPort = 1;
const maxTcpPort = 65535;
const rawWsPort = import.meta.env.VITE_WS_PORT;
const parsedWsPort = rawWsPort === undefined ? undefined : Number.parseInt(rawWsPort, 10);
const wsPortIsValid =
  parsedWsPort !== undefined &&
  Number.isFinite(parsedWsPort) &&
  parsedWsPort >= minTcpPort &&
  parsedWsPort <= maxTcpPort;
if (rawWsPort !== undefined && !wsPortIsValid) {
  console.warn(`Invalid VITE_WS_PORT="${rawWsPort}", falling back to ${defaultWsPort}`);
}
const wsPort = wsPortIsValid ? parsedWsPort : defaultWsPort;
const wsProtocol = window.location.protocol === 'https:' ? 'wss' : 'ws';
const wsUrl = `${wsProtocol}://${window.location.hostname}:${wsPort}/ws`;

function App() {
  const { route, navigate } = useAppRoute();
  const { sendControllerState, sendEStop, sendEStopRelease } = useSTARConnection(wsUrl);

  return route === '/ros' ? (
    <RosDebugView navigate={navigate} />
  ) : (
    <MainDashboardView
      navigate={navigate}
      sendControllerState={sendControllerState}
      sendEStop={sendEStop}
      sendEStopRelease={sendEStopRelease}
    />
  );
}

export default App;
