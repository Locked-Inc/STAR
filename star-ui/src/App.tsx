import { useSTARConnection } from './hooks/useSTARConnection';
import { useAppRoute } from './hooks/useAppRoute';
import { MainDashboardView } from './views/MainDashboardView';
import { RosDebugView } from './views/RosDebugView';

const defaultWsPort = 8080;
const rawWsPort = import.meta.env.VITE_WS_PORT;
const parsedWsPort = rawWsPort === undefined ? defaultWsPort : Number(rawWsPort);
if (!Number.isInteger(parsedWsPort) || parsedWsPort <= 0 || parsedWsPort > 65535) {
  console.warn(`Invalid VITE_WS_PORT="${rawWsPort}", falling back to ${defaultWsPort}`);
}
const wsPort = Number.isInteger(parsedWsPort) && parsedWsPort > 0 && parsedWsPort <= 65535
  ? parsedWsPort
  : defaultWsPort;
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