import { useSTARConnection } from './hooks/useSTARConnection';
import { useAppRoute } from './hooks/useAppRoute';
import { MainDashboardView } from './views/MainDashboardView';
import { RosDebugView } from './views/RosDebugView';

const wsPort = Number(import.meta.env.VITE_WS_PORT) || 8080;
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
