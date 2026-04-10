import { useEffect, useMemo, useRef, useState, type MouseEvent as ReactMouseEvent, type ReactNode } from 'react';
import { DebugTraceChart } from './components/DebugTraceChart';
import { useShallow } from 'zustand/react/shallow';
import { StarMapCanvas } from './components/StarMapCanvas';
import { useGamepad } from './hooks/useGamepad';
import { useSTARConnection } from './hooks/useSTARConnection';
import type { ControllerState } from './proto/star/v1/controller';
import { RobotMode } from './proto/star/v1/telemetry';
import { AlertLevel } from './proto/star/v1/ui';
import { getRecentPackets, type PacketRecord } from './services/GatewayService';
import { useDashboardStore } from './store/dashboardStore';

const WS_PORT = Number(import.meta.env.VITE_WS_PORT) || 8080;
const WS_PROTOCOL = window.location.protocol === 'https:' ? 'wss' : 'ws';
const WS_URL = `${WS_PROTOCOL}://${window.location.hostname}:${WS_PORT}/ws`;

type AppRoute = '/' | '/ros';
type UiMode = 'autonomous' | 'manual';
type Tone = 'neutral' | 'good' | 'warn' | 'danger' | 'accent';

const controllerSendIntervalMs = 20;
const packetPollIntervalMs = 1000;
const coverageTickIntervalMs = 450;
const coverageStepPercent = 1.5;
const recentPacketWindowMs = 8_000;

const packetTypeToTopic: Record<string, string> = {
  lidar: '/scan',
  odometry: '/odom',
  controller: '/cmd_vel',
  telemetry: '/imu/data',
  battery: '/battery_state',
  system: '/tf',
  alert: '/diagnostics',
};

const staticTopicRows = [
  { topic: '/scan', type: 'sensor_msgs/LaserScan', description: 'LIDAR point cloud data', rate: '10 Hz' },
  { topic: '/odom', type: 'nav_msgs/Odometry', description: 'Wheel encoder odometry', rate: '50 Hz' },
  { topic: '/cmd_vel', type: 'geometry_msgs/Twist', description: 'Velocity commands', rate: '50 Hz' },
  { topic: '/imu/data', type: 'sensor_msgs/Imu', description: 'Inertial measurement data', rate: '100 Hz' },
  { topic: '/map', type: 'nav_msgs/OccupancyGrid', description: 'SLAM occupancy grid', rate: '1 Hz' },
  { topic: '/tf', type: 'tf2_msgs/TFMessage', description: 'Transform tree', rate: '200 Hz' },
];

function normalizeRoute(pathname: string): AppRoute {
  return pathname === '/ros' ? '/ros' : '/';
}

function useAppRoute(): { route: AppRoute; navigate: (route: AppRoute) => void } {
  const [route, setRoute] = useState<AppRoute>(() => normalizeRoute(window.location.pathname));

  useEffect(() => {
    const handlePopState = () => setRoute(normalizeRoute(window.location.pathname));
    window.addEventListener('popstate', handlePopState);
    return () => window.removeEventListener('popstate', handlePopState);
  }, []);

  function navigate(nextRoute: AppRoute): void {
    if (normalizeRoute(window.location.pathname) !== nextRoute) {
      window.history.pushState({}, '', nextRoute);
    }
    setRoute(nextRoute);
  }

  return { route, navigate };
}

function usePacketFeed(): PacketRecord[] {
  const [packets, setPackets] = useState<PacketRecord[]>(() => getRecentPackets());

  useEffect(() => {
    const interval = window.setInterval(() => {
      setPackets(getRecentPackets());
    }, packetPollIntervalMs);

    return () => window.clearInterval(interval);
  }, []);

  return packets;
}

function useControllerBridge(sendControllerState: (state: ControllerState) => void, enabled: boolean) {
  const gamepadState = useGamepad();
  const sendRef = useRef(sendControllerState);
  const gamepadRef = useRef(gamepadState);

  useEffect(() => {
    sendRef.current = sendControllerState;
  }, [sendControllerState]);

  useEffect(() => {
    gamepadRef.current = gamepadState;
  }, [gamepadState]);

  useEffect(() => {
    if (!enabled) {
      sendRef.current({
        linearVel: 0,
        angularVel: 0,
        timestamp: String(Date.now()),
        debug: false,
      });
      return;
    }

    const interval = window.setInterval(() => {
      const controllerState = gamepadRef.current;
      sendRef.current({
        linearVel: controllerState.linearVel,
        angularVel: controllerState.angularVel,
        timestamp: String(Date.now()),
        debug: false,
      });
    }, controllerSendIntervalMs);

    return () => window.clearInterval(interval);
  }, [enabled]);

  return gamepadState;
}

function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

function average(values: number[]): number {
  if (values.length === 0) {
    return 0;
  }
  return values.reduce((sum, value) => sum + value, 0) / values.length;
}

function formatNumber(value: number | undefined, digits = 1): string {
  if (value == null || Number.isNaN(value)) {
    return '--';
  }
  return value.toFixed(digits);
}

function formatPercent(value: number | undefined): string {
  if (value == null || Number.isNaN(value)) {
    return '--';
  }
  return `${Math.round(value)}%`;
}

function formatVoltage(millivolts: number | undefined): string {
  if (millivolts == null || Number.isNaN(millivolts)) {
    return '--';
  }
  return `${(millivolts / 1000).toFixed(1)} V`;
}

function formatCurrent(milliamps: number | undefined): string {
  if (milliamps == null || Number.isNaN(milliamps)) {
    return '--';
  }
  return `${(milliamps / 1000).toFixed(1)} A`;
}

function formatHeading(radians: number | undefined): string {
  if (radians == null || Number.isNaN(radians)) {
    return '--';
  }
  const degrees = ((radians * 180) / Math.PI + 360) % 360;
  return `${degrees.toFixed(1)}°`;
}

function formatUptime(seconds: string | undefined): string {
  const total = Number(seconds ?? 0);
  if (!Number.isFinite(total) || total <= 0) {
    return '--';
  }

  const hours = Math.floor(total / 3600);
  const minutes = Math.floor((total % 3600) / 60);
  const secs = Math.floor(total % 60);
  return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${secs
    .toString()
    .padStart(2, '0')}`;
}

function formatPacketTimestamp(tsMs: number): string {
  return new Date(tsMs).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

function formatAlertTimestamp(timestampUs: string, fallbackOffset: number): string {
  const raw = Number(timestampUs);
  const ms = raw / 1000;
  const floor = Date.parse('2020-01-01T00:00:00Z');
  const fallback = new Date(Date.now() - fallbackOffset * 1000);

  if (!Number.isFinite(ms) || ms < floor) {
    return fallback.toLocaleTimeString([], {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
    });
  }

  return new Date(ms).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

function toneFromLevel(level: AlertLevel): Tone {
  switch (level) {
    case AlertLevel.ERROR:
    case AlertLevel.CRITICAL:
      return 'danger';
    case AlertLevel.WARN:
      return 'warn';
    case AlertLevel.INFO:
      return 'accent';
    default:
      return 'neutral';
  }
}

function toneFromScore(score: number): Tone {
  if (score >= 85) {
    return 'good';
  }
  if (score >= 60) {
    return 'warn';
  }
  return 'danger';
}

function toneFromBoolean(active: boolean, warning = false): Tone {
  if (active && !warning) {
    return 'good';
  }
  if (warning) {
    return 'warn';
  }
  return active ? 'good' : 'danger';
}

function levelLabel(level: AlertLevel): string {
  switch (level) {
    case AlertLevel.INFO:
      return 'INFO';
    case AlertLevel.WARN:
      return 'WARN';
    case AlertLevel.ERROR:
      return 'ERROR';
    case AlertLevel.CRITICAL:
      return 'CRITICAL';
    default:
      return 'UNKNOWN';
  }
}

function modeLabel(mode: UiMode): string {
  return mode === 'manual' ? 'Manual' : 'Autonomous';
}

interface GlyphProps {
  accent: Tone;
  label: string;
}

function Glyph({ accent, label }: GlyphProps) {
  return (
    <div className={`star-glyph star-glyph--${accent}`} aria-hidden="true">
      {label}
    </div>
  );
}

interface StatusDotProps {
  tone: Tone;
}

function StatusDot({ tone }: StatusDotProps) {
  return <span className={`status-dot status-dot--${tone}`} />;
}

interface ChipProps {
  tone?: Tone;
  children: ReactNode;
}

function Chip({ tone = 'neutral', children }: ChipProps) {
  return <span className={`ui-chip ui-chip--${tone}`}>{children}</span>;
}

interface MetricTileProps {
  label: string;
  value: string;
  tone?: Tone;
  detail?: string;
}

function MetricTile({ label, value, tone = 'neutral', detail }: MetricTileProps) {
  return (
    <div className="metric-tile">
      <span className="metric-label">{label}</span>
      <span className={`metric-value metric-value--${tone}`}>{value}</span>
      {detail ? <span className="metric-detail">{detail}</span> : null}
    </div>
  );
}

interface HealthRowProps {
  label: string;
  tone: Tone;
  value: string;
}

function HealthRow({ label, tone, value }: HealthRowProps) {
  return (
    <div className="health-row">
      <div className="health-row__label">
        <StatusDot tone={tone} />
        <span>{label}</span>
      </div>
      <span className={`health-row__value health-row__value--${tone}`}>{value}</span>
    </div>
  );
}

interface HeaderLinkProps {
  href: AppRoute;
  onNavigate: (route: AppRoute) => void;
  label: string;
  accent: Tone;
  glyph: string;
}

function HeaderLink({ href, onNavigate, label, accent, glyph }: HeaderLinkProps) {
  function handleClick(event: ReactMouseEvent<HTMLAnchorElement>): void {
    event.preventDefault();
    onNavigate(href);
  }

  return (
    <a className="header-link" href={href} onClick={handleClick}>
      <Glyph accent={accent} label={glyph} />
      <span>{label}</span>
    </a>
  );
}

interface ControlButtonProps {
  label: string;
  tone: Tone;
  onClick: () => void | Promise<void>;
  disabled?: boolean;
}

function ControlButton({ label, tone, onClick, disabled }: ControlButtonProps) {
  return (
    <button
      className={`control-button control-button--${tone}`}
      disabled={disabled}
      type="button"
      onClick={() => {
        void onClick();
      }}
    >
      {label}
    </button>
  );
}

interface LogLine {
  key: string;
  tsMs: number;
  tone: Tone;
  source: string;
  message: string;
  emphasis: string;
}

interface EStopResumeSnapshot {
  coveragePercent: number;
  taskCompleted: boolean;
  uiMode: UiMode;
  wasAutonomyRunning: boolean;
}

interface TraceHistory {
  battery: number[];
  cpu: number[];
  lidar: number[];
  velocity: number[];
}

const traceHistoryLength = 28;

function pushTraceSample(values: number[], nextValue: number): number[] {
  return [...values, nextValue].slice(-traceHistoryLength);
}

function ImuOrientationGraphic({
  pitchDeg,
  rollDeg,
}: {
  pitchDeg: number;
  rollDeg: number;
}) {
  const visualRoll = clamp(rollDeg * 0.65, -22, 22);
  const visualPitch = clamp(pitchDeg * 0.42, -7, 7);
  const deckY = 50 + visualPitch;
  const arrowY = -30 - visualPitch * 0.35;

  return (
    <div className="imu-orientation">
      <svg aria-hidden="true" className="imu-orientation__svg" viewBox="0 0 100 100">
        <defs>
          <linearGradient id="imu-front" x1="0" x2="1">
            <stop offset="0%" stopColor="#3b82f6" />
            <stop offset="100%" stopColor="#2563eb" />
          </linearGradient>
        </defs>

        <circle cx="50" cy="50" fill="none" r="40" stroke="#1e293b" strokeWidth="1" />
        <ellipse cx="50" cy="75" fill="#334155" opacity="0.3" rx="35" ry="10" />

        <g transform={`translate(50 ${deckY}) rotate(${visualRoll})`}>
          <rect fill="url(#imu-front)" height="30" rx="2" stroke="#60a5fa" strokeWidth="1" width="40" x="-20" y="-15" />
          <path d="M-20,-15 L-10,-25 L30,-25 L20,-15 Z" fill="#60a5fa" stroke="#93c5fd" strokeWidth="1" />
          <path d="M20,-15 L30,-25 L30,5 L20,15 Z" fill="#2563eb" stroke="#3b82f6" strokeWidth="1" />
          <path d={`M5,-20 L15,-20 L10,${arrowY} Z`} fill="#ef4444" />
        </g>

        <line stroke="#1e293b" strokeDasharray="3 3" strokeWidth="1" x1="10" x2="90" y1="50" y2="50" />
        <line stroke="#1e293b" strokeDasharray="3 3" strokeWidth="1" x1="50" x2="50" y1="10" y2="90" />
      </svg>
      <span className="imu-orientation__label">Visual Orientation</span>
    </div>
  );
}

function MainScreen({
  navigate,
  sendControllerState,
  sendEStop,
  sendEStopRelease,
}: {
  navigate: (route: AppRoute) => void;
  sendControllerState: (state: ControllerState) => void;
  sendEStop: (reason: string) => void;
  sendEStopRelease: () => Promise<boolean>;
}) {
  const {
    alerts,
    battery,
    connectionState,
    dataIsStale,
    eStopActive,
    lidarScan,
    motors,
    odometry,
    seqGapDetected,
    systemStatus,
    telemetry,
  } = useDashboardStore(
    useShallow((state) => ({
      alerts: state.alerts,
      battery: state.battery,
      connectionState: state.connectionState,
      dataIsStale: state.dataIsStale,
      eStopActive: state.eStopActive,
      lidarScan: state.lidarScan,
      motors: state.motors,
      odometry: state.odometry,
      seqGapDetected: state.seqGapDetected,
      systemStatus: state.systemStatus,
      telemetry: state.telemetry,
    })),
  );

  const [alertsOpen, setAlertsOpen] = useState(false);
  const [uiMode, setUiMode] = useState<UiMode>('autonomous');
  const [autonomyRunning, setAutonomyRunning] = useState(false);
  const [coveragePercent, setCoveragePercent] = useState(0);
  const [taskCompleted, setTaskCompleted] = useState(false);
  const alertsRef = useRef<HTMLDivElement | null>(null);
  const estopResumeRef = useRef<EStopResumeSnapshot | null>(null);

  const controllerBridgeEnabled = uiMode === 'manual' && !eStopActive;
  const gamepadState = useControllerBridge(sendControllerState, controllerBridgeEnabled);

  useEffect(() => {
    if (!systemStatus) {
      return;
    }

    if (systemStatus.mode === RobotMode.MANUAL) {
      setUiMode('manual');
    }

    if (systemStatus.mode === RobotMode.AUTONOMOUS || systemStatus.mode === RobotMode.MAPPING) {
      setUiMode('autonomous');
    }
  }, [systemStatus]);

  useEffect(() => {
    if (!alertsOpen) {
      return;
    }

    function handleOutsideClick(event: globalThis.MouseEvent): void {
      if (alertsRef.current && !alertsRef.current.contains(event.target as Node)) {
        setAlertsOpen(false);
      }
    }

    document.addEventListener('mousedown', handleOutsideClick);
    return () => document.removeEventListener('mousedown', handleOutsideClick);
  }, [alertsOpen]);

  useEffect(() => {
    if (eStopActive) {
      setAutonomyRunning(false);
    }
  }, [eStopActive]);

  useEffect(() => {
    if (uiMode !== 'autonomous') {
      setAutonomyRunning(false);
    }
  }, [uiMode]);

  useEffect(() => {
    if (uiMode !== 'autonomous' || !autonomyRunning || eStopActive) {
      return;
    }

    const interval = window.setInterval(() => {
      setCoveragePercent((currentCoverage) => {
        const nextCoverage = Math.min(100, currentCoverage + coverageStepPercent);
        if (nextCoverage >= 100) {
          setTaskCompleted(true);
          setAutonomyRunning(false);
        }
        return nextCoverage;
      });
    }, coverageTickIntervalMs);

    return () => window.clearInterval(interval);
  }, [autonomyRunning, eStopActive, uiMode]);

  const motorValues = motors ?? [];
  const motorVelocity = average(motorValues.map((motor) => motor.velocityMps));
  const odometryVelocity = odometry?.linearVelocityMps ?? motorVelocity;
  const lidarPointCount = lidarScan?.rangeM.filter((value) => value > 0).length ?? 0;
  const batteryPercent = battery?.soc?.relativeSocPercent ?? telemetry?.batteryPercent;
  const batteryVoltage = battery?.current?.voltageMv ?? battery?.cells?.packMv;
  const batteryCurrent = battery?.current?.currentMa;
  const batteryCycles = battery?.soc?.cycleCount;
  const imu = telemetry?.imu;
  const headingDegrees = imu?.yawRad ?? odometry?.thetaRad ?? 0;
  const wifiSignal = telemetry?.wifiSignalDbm;
  const cpuUsage = telemetry?.cpuUsagePercent;
  const temperature = telemetry?.temperatureCelsius;
  const motorLoad = telemetry?.motorLoadPercent;
  const minCell = battery?.cells?.minCellMv;
  const maxCell = battery?.cells?.maxCellMv;
  const cellDelta = battery?.cells?.deltaMv;

  const wifiScore = wifiSignal == null ? 70 : clamp(((wifiSignal + 100) / 70) * 100, 0, 100);
  const cpuScore = cpuUsage == null ? 78 : clamp(100 - cpuUsage, 0, 100);
  const tempScore = temperature == null ? 82 : clamp(100 - Math.max(0, temperature - 32) * 4, 0, 100);
  const powerScore = batteryPercent == null ? 84 : clamp(batteryPercent, 0, 100);
  const connectivityScore = average([
    systemStatus?.esp32Connected === false ? 0 : 100,
    systemStatus?.lidarConnected === false ? 0 : 100,
    systemStatus?.rosConnected === false ? 0 : 100,
    connectionState === 'connected' ? 100 : connectionState === 'connecting' || connectionState === 'reconnecting' ? 70 : 35,
  ]);
  const overallHealth = Math.round(average([wifiScore, cpuScore, tempScore, powerScore, connectivityScore]));

  const controllerTone: Tone =
    eStopActive || connectionState === 'disconnected'
      ? 'danger'
      : gamepadState.connected || uiMode === 'autonomous'
        ? 'good'
        : 'warn';

  const lidarTone = toneFromBoolean(Boolean(systemStatus?.lidarConnected), lidarPointCount === 0 && connectionState === 'connected');
  const controllerMcuTone = toneFromBoolean(Boolean(systemStatus?.esp32Connected), motorValues.length === 0 && connectionState === 'connected');
  const rosTone = toneFromBoolean(Boolean(systemStatus?.rosConnected), dataIsStale || seqGapDetected);

  const statusCopy = eStopActive
    ? 'Emergency Stopped'
    : taskCompleted
      ? 'Completed'
      : autonomyRunning
        ? 'Running'
        : 'Standby';

  const statusTone: Tone = eStopActive ? 'danger' : taskCompleted ? 'good' : autonomyRunning ? 'accent' : 'neutral';

  async function handleEStopToggle(): Promise<void> {
    if (eStopActive) {
      const released = await sendEStopRelease();
      const store = useDashboardStore.getState();
      const snapshot = estopResumeRef.current;

      store.releaseEStop();

      if (snapshot) {
        setUiMode(snapshot.uiMode);
        setCoveragePercent(snapshot.coveragePercent);
        setTaskCompleted(snapshot.taskCompleted);
        setAutonomyRunning(snapshot.uiMode === 'autonomous' && snapshot.wasAutonomyRunning && !snapshot.taskCompleted);
      }

      if (!released) {
        store.addAlert({
          code: 'ESTOP_RESUME_LOCAL',
          level: AlertLevel.WARN,
          message: 'Resume applied locally; backend release confirmation was not received.',
          source: 'ui',
          timestampUs: String(Date.now() * 1000),
        });
      }
      return;
    }

    estopResumeRef.current = {
      coveragePercent,
      taskCompleted,
      uiMode,
      wasAutonomyRunning: autonomyRunning,
    };
    setAutonomyRunning(false);
    sendEStop('user_ui_button');
    useDashboardStore.getState().triggerEStop();
  }

  function handleModeToggle(): void {
    setUiMode((currentMode) => {
      const nextMode: UiMode = currentMode === 'autonomous' ? 'manual' : 'autonomous';
      if (nextMode === 'manual') {
        setAutonomyRunning(false);
      } else {
        setTaskCompleted(false);
        setCoveragePercent(0);
      }
      return nextMode;
    });
  }

  function handleAutonomyStart(): void {
    setTaskCompleted(false);
    setCoveragePercent(0);
    setAutonomyRunning(true);
  }

  function handleAutonomyStop(): void {
    setAutonomyRunning(false);
  }

  const alertPreview = alerts.slice(0, 6);

  return (
    <main className="app-shell">
      <div className="shell-container">
        <header className="screen-header">
          <div className="screen-header__identity">
            <Glyph accent="good" label="PS" />
            <div>
              <h1>Project Star</h1>
              <p>Autonomous Robot Control</p>
            </div>
          </div>

          <div className="screen-header__actions">
            <Chip tone={controllerTone}>Controller: {controllerTone === 'good' ? 'OK' : controllerTone === 'warn' ? 'Warning' : 'Error'}</Chip>
            <HeaderLink href="/ros" onNavigate={navigate} label="Debug" accent="accent" glyph="RX" />
            <Chip tone={connectionState === 'connected' ? 'good' : connectionState === 'disconnected' ? 'danger' : 'warn'}>
              {connectionState === 'connected' ? 'Connected' : connectionState === 'reconnecting' ? 'Reconnecting' : connectionState}
            </Chip>
            {uiMode === 'manual' ? (
              <Chip tone={gamepadState.connected ? 'good' : 'warn'}>{gamepadState.connected ? 'Gamepad' : 'No Gamepad'}</Chip>
            ) : null}

            <div className="alerts-anchor" ref={alertsRef}>
              <button
                className="alerts-button"
                type="button"
                onClick={() => setAlertsOpen((current) => !current)}
              >
                <Glyph accent={alertPreview.length > 0 ? 'warn' : 'neutral'} label="AL" />
                <span>Alerts</span>
                {alertPreview.length > 0 ? <span className="alerts-count">{alertPreview.length}</span> : null}
              </button>

              {alertsOpen ? (
                <div className="alerts-popover">
                  <div className="alerts-popover__header">
                    <div>
                      <h2>System Alerts</h2>
                      <p>Latest diagnostic events and transport warnings.</p>
                    </div>
                    <button className="subtle-button" type="button" onClick={() => setAlertsOpen(false)}>
                      Close
                    </button>
                  </div>

                  <div className="alerts-list">
                    {alertPreview.length === 0 ? (
                      <div className="empty-state">No alerts at this time.</div>
                    ) : (
                      alertPreview.map((alert, index) => (
                        <div key={`${alert.code}-${alert.timestampUs}-${index}`} className="alert-entry">
                          <div className={`alert-entry__marker alert-entry__marker--${toneFromLevel(alert.level)}`} />
                          <div className="alert-entry__body">
                            <div className="alert-entry__meta">
                              <span>{levelLabel(alert.level)}</span>
                              <span>{alert.source || 'system'}</span>
                              <span>{formatAlertTimestamp(alert.timestampUs, index)}</span>
                            </div>
                            <p>{alert.message}</p>
                          </div>
                        </div>
                      ))
                    )}
                  </div>
                </div>
              ) : null}
            </div>
          </div>
        </header>

        <div className="dashboard-layout">
          <section className="dashboard-main">
            <div className="panel-card panel-card--map">
              <div className="panel-toolbar">
                <div className="panel-toolbar__left">
                  <div className="panel-toolbar__cluster">
                    <span className="panel-toolbar__label">Mode</span>
                    <Chip tone={uiMode === 'autonomous' ? 'good' : 'accent'}>{modeLabel(uiMode)}</Chip>
                  </div>
                  <div className="panel-toolbar__cluster">
                    <span className="panel-toolbar__label">Status</span>
                    <Chip tone={statusTone}>{statusCopy}</Chip>
                  </div>
                  <div className="panel-toolbar__cluster">
                    <span className="panel-toolbar__label">Velocity</span>
                    <span className="panel-toolbar__value">{eStopActive ? '0.00 m/s' : `${formatNumber(odometryVelocity, 2)} m/s`}</span>
                  </div>
                  <div className="panel-toolbar__cluster">
                    <span className="panel-toolbar__label">Position</span>
                    <span className="panel-toolbar__value panel-toolbar__value--mono">
                      ({formatNumber(odometry?.xM, 2)}, {formatNumber(odometry?.yM, 2)})
                    </span>
                  </div>
                </div>
                <div className="panel-toolbar__right">
                  <Chip tone={dataIsStale ? 'warn' : 'neutral'}>{dataIsStale ? 'Telemetry Stale' : 'Live Feed'}</Chip>
                  {seqGapDetected ? <Chip tone="warn">Sequence Gap</Chip> : null}
                </div>
              </div>

              <div className="panel-card__body">
                <div className="panel-card__heading">
                  <div className="panel-title">
                    <Glyph accent="accent" label="MAP" />
                    <div>
                      <h2>SLAM Map</h2>
                      <p>Live localization, scan overlays, and trajectory history.</p>
                    </div>
                  </div>
                  <div className="panel-card__meta">LIDAR Points: {lidarPointCount}</div>
                </div>

                <StarMapCanvas lidarScan={lidarScan} odometry={odometry} />

                <div className="legend-row">
                  <div className="legend-item">
                    <span className="legend-swatch legend-swatch--robot" />
                    <span>Robot</span>
                  </div>
                  <div className="legend-item">
                    <span className="legend-swatch legend-swatch--scan" />
                    <span>Active Scan</span>
                  </div>
                  <div className="legend-item">
                    <span className="legend-swatch legend-swatch--obstacle" />
                    <span>Obstacles</span>
                  </div>
                  <div className="legend-item">
                    <span className="legend-swatch legend-swatch--trajectory" />
                    <span>Trajectory</span>
                  </div>
                  <div className="legend-item">
                    <span className="legend-swatch legend-swatch--grid" />
                    <span>Grid Space</span>
                  </div>
                </div>

                <div className="subpanel-grid">
                  <section className="subpanel-card">
                    <div className="subpanel-card__header">
                      <h3>Encoders &amp; Odometry</h3>
                      <p>Wheel RPM estimates and global pose tracking.</p>
                    </div>
                    <div className="subpanel-card__content">
                      <div className="sensor-metrics sensor-metrics--five">
                        {['FL', 'FR', 'RL', 'RR'].map((label, index) => {
                          const motor = motorValues[index];
                          const rpm = ((motor?.velocityMps ?? 0) / 0.2) * 60;
                          return (
                            <MetricTile
                              key={label}
                              label={`Encoder ${label}`}
                              tone={eStopActive ? 'neutral' : 'good'}
                              value={`${formatNumber(rpm, 0)} RPM`}
                            />
                          );
                        })}
                        <MetricTile label="Odometry" tone="accent" value={`${formatNumber(odometry?.xM ?? 0, 2)} m`} detail={`Heading ${formatHeading(odometry?.thetaRad)}`} />
                      </div>
                    </div>
                  </section>

                  <section className="subpanel-card">
                    <div className="subpanel-card__header">
                      <h3>Orientation (IMU)</h3>
                      <p>Roll, pitch, yaw, and inertial acceleration.</p>
                    </div>
                    <div className="subpanel-card__content subpanel-card__content--split">
                      <ImuOrientationGraphic
                        pitchDeg={((imu?.pitchRad ?? 0) * 180) / Math.PI}
                        rollDeg={((imu?.rollRad ?? 0) * 180) / Math.PI}
                      />

                      <div className="orientation-values">
                        <MetricTile label="Roll" tone="accent" value={`${formatNumber(((imu?.rollRad ?? 0) * 180) / Math.PI, 1)}°`} />
                        <MetricTile label="Pitch" tone="accent" value={`${formatNumber(((imu?.pitchRad ?? 0) * 180) / Math.PI, 1)}°`} />
                        <MetricTile label="Yaw" tone="accent" value={`${formatNumber(((imu?.yawRad ?? odometry?.thetaRad ?? 0) * 180) / Math.PI, 1)}°`} />
                        <MetricTile
                          label="Accel X/Y/Z"
                          tone="good"
                          value={`${formatNumber(imu?.accelXMps2, 1)}, ${formatNumber(imu?.accelYMps2, 1)}, ${formatNumber(imu?.accelZMps2, 1)}`}
                        />
                        <MetricTile
                          label="Gyro X/Y/Z"
                          tone="neutral"
                          value={`${formatNumber(imu?.gyroXRadPerS, 1)}, ${formatNumber(imu?.gyroYRadPerS, 1)}, ${formatNumber(imu?.gyroZRadPerS, 1)}`}
                        />
                      </div>
                    </div>
                  </section>
                </div>
              </div>
            </div>
          </section>

          <aside className="dashboard-sidebar">
            <section className="panel-card">
              <div className="panel-card__heading">
                <div className="panel-title">
                  <Glyph accent="warn" label="CTL" />
                  <div>
                    <h2>Control Mode</h2>
                    <p>Primary mode switching and emergency actions.</p>
                  </div>
                </div>
              </div>

              <div className="panel-card__body panel-card__body--compact">
                <ControlButton
                  label={eStopActive ? 'RESUME' : 'EMERGENCY STOP'}
                  tone={eStopActive ? 'warn' : 'danger'}
                  onClick={handleEStopToggle}
                />

                <ControlButton
                  label={uiMode === 'autonomous' ? 'Autonomous Mode' : 'Manual Mode'}
                  tone={uiMode === 'autonomous' ? 'good' : 'accent'}
                  onClick={handleModeToggle}
                  disabled={eStopActive}
                />

                <p className="panel-note">
                  {eStopActive
                    ? 'Robot is emergency stopped.'
                    : uiMode === 'autonomous'
                      ? taskCompleted
                        ? 'Task completed successfully.'
                        : 'Robot is staged for autonomous operation.'
                      : gamepadState.connected
                        ? 'Gamepad input is live and streaming to the controller.'
                        : 'Connect a gamepad to enable manual control.'}
                </p>
              </div>
            </section>

            {uiMode === 'autonomous' ? (
              <section className="panel-card">
                <div className="panel-card__heading">
                  <div className="panel-title">
                    <Glyph accent="good" label="AUTO" />
                    <div>
                      <h2>Autonomous Controls</h2>
                      <p>Mission execution and progress tracking.</p>
                    </div>
                  </div>
                </div>

                <div className="panel-card__body panel-card__body--compact">
                  <div className="button-pair">
                    <ControlButton
                      label="Start"
                      tone="good"
                      onClick={handleAutonomyStart}
                      disabled={eStopActive || autonomyRunning}
                    />
                    <ControlButton
                      label="Stop"
                      tone="warn"
                      onClick={handleAutonomyStop}
                      disabled={eStopActive || !autonomyRunning}
                    />
                  </div>

                  <div className="progress-panel">
                    <div className="progress-panel__meta">
                      <span>Coverage Progress</span>
                      <span className={taskCompleted ? 'is-complete' : undefined}>
                        {taskCompleted ? 'Completed ' : ''}
                        {coveragePercent.toFixed(0)}%
                      </span>
                    </div>
                    <div className="progress-bar">
                      <div
                        className={`progress-bar__fill ${taskCompleted ? 'progress-bar__fill--complete' : ''}`}
                        style={{ width: `${coveragePercent}%` }}
                      />
                    </div>
                  </div>
                </div>
              </section>
            ) : (
              <section className="panel-card">
                <div className="panel-card__heading">
                  <div className="panel-title">
                    <Glyph accent="accent" label="PAD" />
                    <div>
                      <h2>Manual Controls</h2>
                      <p>Live gamepad steering and velocity preview.</p>
                    </div>
                  </div>
                </div>

                <div className="panel-card__body panel-card__body--compact">
                  <div className="manual-status">
                    <div className="manual-status__pill">
                      <StatusDot tone={gamepadState.connected ? 'good' : 'warn'} />
                      <span>{gamepadState.connected ? 'Gamepad Connected' : 'No Gamepad Detected'}</span>
                    </div>
                    <div className="manual-status__metrics">
                      <MetricTile label="Linear" tone="accent" value={formatNumber(gamepadState.linearVel, 2)} />
                      <MetricTile label="Angular" tone="accent" value={formatNumber(gamepadState.angularVel, 2)} />
                    </div>
                  </div>
                </div>
              </section>
            )}

            <section className="panel-card">
              <div className="panel-card__heading">
                <div className="panel-title">
                  <Glyph accent={toneFromScore(overallHealth)} label="SYS" />
                  <div>
                    <h2>System Health</h2>
                    <p>Critical stack readiness and controller telemetry.</p>
                  </div>
                </div>
                <div className="panel-card__meta">{overallHealth}%</div>
              </div>

              <div className="panel-card__body panel-card__body--compact">
                <HealthRow label="LIDAR" tone={lidarTone} value={lidarTone === 'good' ? 'OK' : lidarTone === 'warn' ? 'Warning' : 'Error'} />
                <HealthRow label="RX72N MCU" tone={controllerMcuTone} value={controllerMcuTone === 'good' ? 'OK' : controllerMcuTone === 'warn' ? 'Warning' : 'Error'} />
                <HealthRow label="ROS2 Stack" tone={rosTone} value={rosTone === 'good' ? 'OK' : rosTone === 'warn' ? 'Warning' : 'Error'} />
              </div>
            </section>

            <section className="panel-card">
              <div className="panel-card__heading">
                <div className="panel-title">
                  <Glyph accent="accent" label="CMP" />
                  <div>
                    <h2>Compass</h2>
                    <p>Heading reference from odometry and IMU yaw.</p>
                  </div>
                </div>
              </div>

              <div className="panel-card__body panel-card__body--compact">
                <div className="compass-card">
                  <div className="compass-ring">
                    <span className="compass-label compass-label--north">N</span>
                    <span className="compass-label compass-label--south">S</span>
                    <span className="compass-label compass-label--east">E</span>
                    <span className="compass-label compass-label--west">W</span>
                    <div className="compass-needle" style={{ transform: `translate(-50%, -100%) rotate(${((headingDegrees * 180) / Math.PI + 360) % 360}deg)` }} />
                    <div className="compass-center" />
                  </div>
                  <div className="compass-readout">
                    <span>Heading</span>
                    <strong>{formatHeading(headingDegrees)}</strong>
                  </div>
                </div>
              </div>
            </section>

            <section className="panel-card">
              <div className="panel-card__heading">
                <div className="panel-title">
                  <Glyph accent="good" label="POW" />
                  <div>
                    <h2>Power &amp; Telemetry</h2>
                    <p>Battery, network, compute, and thermal overview.</p>
                  </div>
                </div>
              </div>

              <div className="panel-card__body">
                <div className="metrics-grid">
                  <MetricTile label="Battery" tone={toneFromScore(powerScore)} value={formatPercent(batteryPercent)} detail={formatVoltage(batteryVoltage)} />
                  <MetricTile label="Current" tone="neutral" value={formatCurrent(batteryCurrent)} detail={batteryCycles != null ? `${batteryCycles} cycles` : 'No cycle data'} />
                  <MetricTile label="WiFi" tone={toneFromScore(wifiScore)} value={wifiSignal != null ? `${wifiSignal} dBm` : '--'} detail={connectionState} />
                  <MetricTile label="CPU" tone={toneFromScore(cpuScore)} value={cpuUsage != null ? `${cpuUsage.toFixed(0)}%` : '--'} detail={temperature != null ? `${temperature.toFixed(1)} C` : 'No thermal data'} />
                  <MetricTile label="Motor Load" tone={toneFromScore(100 - Math.min(motorLoad ?? 35, 100))} value={motorLoad != null ? `${motorLoad.toFixed(0)}%` : '--'} />
                  <MetricTile label="Uptime" tone="neutral" value={formatUptime(systemStatus?.uptimeS)} detail={systemStatus?.firmwareVersion || 'Firmware unavailable'} />
                </div>

                <div className="cell-balance">
                  <div className="cell-balance__header">
                    <h3>Cell Balance</h3>
                    <p>Pack spread and minimum / maximum cell voltage.</p>
                  </div>
                  <div className="cell-balance__grid">
                    <MetricTile label="Min Cell" tone="neutral" value={formatVoltage(minCell)} />
                    <MetricTile label="Max Cell" tone="neutral" value={formatVoltage(maxCell)} />
                    <MetricTile label="Delta" tone={cellDelta != null && cellDelta > 80 ? 'warn' : 'good'} value={cellDelta != null ? `${cellDelta} mV` : '--'} />
                  </div>
                </div>
              </div>
            </section>
          </aside>
        </div>
      </div>
    </main>
  );
}

function RosScreen({
  navigate,
}: {
  navigate: (route: AppRoute) => void;
}) {
  const { alerts, battery, connectionState, lidarScan, odometry, systemStatus, telemetry } = useDashboardStore(
    useShallow((state) => ({
      alerts: state.alerts,
      battery: state.battery,
      connectionState: state.connectionState,
      lidarScan: state.lidarScan,
      odometry: state.odometry,
      systemStatus: state.systemStatus,
      telemetry: state.telemetry,
    })),
  );

  const packets = usePacketFeed();
  const now = Date.now();
  const lidarPointCount = lidarScan?.rangeM.filter((value) => value > 0).length ?? 0;
  const velocitySample = Math.abs(odometry?.linearVelocityMps ?? 0);
  const cpuSample = clamp(telemetry?.cpuUsagePercent ?? 0, 0, 100);
  const batterySample = clamp(battery?.soc?.relativeSocPercent ?? telemetry?.batteryPercent ?? 0, 0, 100);
  const lidarSample = lidarPointCount;
  const latestTraceSamplesRef = useRef({
    battery: batterySample,
    cpu: cpuSample,
    lidar: lidarSample,
    velocity: velocitySample,
  });
  const [traceHistory, setTraceHistory] = useState<TraceHistory>(() => ({
    battery: Array.from({ length: traceHistoryLength }, () => batterySample),
    cpu: Array.from({ length: traceHistoryLength }, () => cpuSample),
    lidar: Array.from({ length: traceHistoryLength }, () => lidarSample),
    velocity: Array.from({ length: traceHistoryLength }, () => velocitySample),
  }));

  useEffect(() => {
    latestTraceSamplesRef.current = {
      battery: batterySample,
      cpu: cpuSample,
      lidar: lidarSample,
      velocity: velocitySample,
    };
  }, [batterySample, cpuSample, lidarSample, velocitySample]);

  useEffect(() => {
    const interval = window.setInterval(() => {
      setTraceHistory((currentHistory) => ({
        battery: pushTraceSample(currentHistory.battery, latestTraceSamplesRef.current.battery),
        cpu: pushTraceSample(currentHistory.cpu, latestTraceSamplesRef.current.cpu),
        lidar: pushTraceSample(currentHistory.lidar, latestTraceSamplesRef.current.lidar),
        velocity: pushTraceSample(currentHistory.velocity, latestTraceSamplesRef.current.velocity),
      }));
    }, 1000);

    return () => window.clearInterval(interval);
  }, []);

  const packetTopics = useMemo(() => {
    const seen = new Map<string, PacketRecord>();
    packets.forEach((packet) => {
      const topic = packetTypeToTopic[packet.type];
      if (topic && !seen.has(topic)) {
        seen.set(topic, packet);
      }
    });
    return seen;
  }, [packets]);

  const activeTopics = useMemo(
    () =>
      staticTopicRows.map((topicRow) => {
        const packet = packetTopics.get(topicRow.topic);
        const isLive = packet ? now - packet.tsMs < recentPacketWindowMs : false;

        let detail = `${topicRow.rate} • ${topicRow.description}`;
        if (topicRow.topic === '/scan' && lidarPointCount > 0) {
          detail = `${lidarPointCount} samples • ${topicRow.description}`;
        }
        if (topicRow.topic === '/odom' && odometry) {
          detail = `${formatNumber(odometry.linearVelocityMps, 2)} m/s • ${topicRow.description}`;
        }
        if (topicRow.topic === '/imu/data' && telemetry?.imu) {
          detail = `${formatHeading(telemetry.imu.yawRad)} • ${topicRow.description}`;
        }
        if (topicRow.topic === '/cmd_vel' && packet) {
          detail = `${packet.preview} • ${topicRow.description}`;
        }
        if (topicRow.topic === '/map') {
          detail = lidarPointCount > 0 ? `Live scan fusion • ${topicRow.description}` : detail;
        }
        if (topicRow.topic === '/tf' && systemStatus?.rosConnected) {
          detail = `Bridge connected • ${topicRow.description}`;
        }

        return {
          ...topicRow,
          isLive:
            topicRow.topic === '/map'
              ? lidarPointCount > 0 || Boolean(odometry)
              : topicRow.topic === '/tf'
                ? Boolean(systemStatus?.rosConnected)
                : isLive,
          detail,
        };
      }),
    [lidarPointCount, now, odometry, packetTopics, systemStatus, telemetry],
  );

  const rosNodes = useMemo(
    () => [
      {
        name: '/slam_toolbox',
        description: 'SLAM mapping and localization',
        tone: lidarPointCount > 0 && Boolean(odometry) ? 'good' : 'warn',
      },
      {
        name: '/rplidar_node',
        description: 'LIDAR sensor driver',
        tone: systemStatus?.lidarConnected ? 'good' : 'danger',
      },
      {
        name: '/motor_controller',
        description: 'RX72N motor control interface',
        tone: systemStatus?.esp32Connected ? 'good' : 'danger',
      },
      {
        name: '/imu_filter',
        description: 'IMU data filtering and fusion',
        tone: telemetry?.imu ? 'good' : 'warn',
      },
      {
        name: '/nav2_controller',
        description: 'Navigation2 path planning',
        tone: systemStatus?.rosConnected ? 'good' : 'warn',
      },
      {
        name: '/robot_state_publisher',
        description: 'Robot kinematic model',
        tone: connectionState === 'connected' ? 'good' : 'warn',
      },
    ],
    [connectionState, lidarPointCount, odometry, systemStatus, telemetry],
  );

  const parameterCards = useMemo(
    () => [
      { label: 'max_velocity', value: `${formatNumber(Math.max(Math.abs(odometry?.linearVelocityMps ?? 0), 3), 1)} m/s` },
      { label: 'lidar_range', value: '12.0 m' },
      { label: 'scan_frequency', value: lidarPointCount > 0 ? '10 Hz' : '--' },
      { label: 'map_resolution', value: '0.05 m/cell' },
      { label: 'wheel_diameter', value: '0.20 m' },
      { label: 'imu_sample_rate', value: telemetry?.imu ? '100 Hz' : '--' },
      { label: 'battery_cells', value: battery?.cells?.validCells ? `${battery.cells.validCells}S` : '--' },
      { label: 'controller_frequency', value: packets.some((packet) => packet.type === 'controller') ? '50 Hz' : '--' },
      { label: 'firmware_version', value: systemStatus?.firmwareVersion || '--' },
    ],
    [battery, lidarPointCount, odometry, packets, systemStatus, telemetry],
  );

  const logLines = useMemo<LogLine[]>(() => {
    const packetLogs = packets.slice(0, 14).map((packet) => ({
      key: `packet-${packet.tsMs}-${packet.seq}-${packet.type}`,
      tsMs: packet.tsMs,
      tone:
        packet.type === 'alert'
          ? 'warn'
          : packet.direction === 'tx'
            ? 'accent'
            : 'good',
      source: packet.direction === 'tx' ? 'TX' : 'RX',
      message: packet.preview || packet.type,
      emphasis: packet.type,
    }));

    const alertLogs = alerts.slice(0, 10).map((alert, index) => ({
      key: `alert-${alert.code}-${alert.timestampUs}-${index}`,
      tsMs: Date.now() - index * 750,
      tone: toneFromLevel(alert.level),
      source: levelLabel(alert.level),
      message: `${alert.source || 'system'}: ${alert.message}`,
      emphasis: alert.code || 'alert',
    }));

    return [...packetLogs, ...alertLogs]
      .sort((left, right) => right.tsMs - left.tsMs)
      .slice(0, 20);
  }, [alerts, packets]);

  return (
    <main className="app-shell">
      <div className="shell-container shell-container--ros">
        <header className="screen-header screen-header--ros">
          <div className="screen-header__identity">
            <HeaderLink href="/" onNavigate={navigate} label="Back to Control" accent="neutral" glyph="BK" />
            <div className="screen-header__identity-group">
              <Glyph accent="accent" label="ROS" />
              <div>
                <h1>ROS2 Debug Console</h1>
                <p>Project Star - System Diagnostics</p>
              </div>
            </div>
          </div>
        </header>

        <div className="ros-grid">
          <section className="panel-card">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="good" label="TPC" />
                <div>
                  <h2>Active ROS2 Topics</h2>
                  <p>Runtime subscriptions, message types, and throughput.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              <div className="list-stack">
                {activeTopics.map((topic) => (
                  <div className="list-card" key={topic.topic}>
                    <div className="list-card__header">
                      <span className={`mono accent-text accent-text--${topic.isLive ? 'good' : 'warn'}`}>{topic.topic}</span>
                      <span className="list-card__caption">{topic.type}</span>
                    </div>
                    <div className="list-card__footer">{topic.detail}</div>
                  </div>
                ))}
              </div>
            </div>
          </section>

          <section className="panel-card">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="warn" label="NDS" />
                <div>
                  <h2>ROS2 Nodes</h2>
                  <p>Core runtime services and health indicators.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              <div className="list-stack">
                {rosNodes.map((node) => (
                  <div className="list-card" key={node.name}>
                    <div className="list-card__header">
                      <span className="mono accent-text accent-text--warn">{node.name}</span>
                      <StatusDot tone={node.tone as Tone} />
                    </div>
                    <div className="list-card__footer">{node.description}</div>
                  </div>
                ))}
              </div>
            </div>
          </section>

          <section className="panel-card panel-card--wide">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="good" label="TRC" />
                <div>
                  <h2>Telemetry Traces</h2>
                  <p>Live diagnostic graphs integrated into the ROS debug workspace.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              <div className="trace-grid">
                <DebugTraceChart
                  accent="accent"
                  formatValue={(value) => `${value.toFixed(2)} m/s`}
                  max={3}
                  samples={traceHistory.velocity}
                  title="Velocity"
                />
                <DebugTraceChart
                  accent="good"
                  formatValue={(value) => `${Math.round(value)} pts`}
                  samples={traceHistory.lidar}
                  title="LIDAR Density"
                />
                <DebugTraceChart
                  accent="warn"
                  formatValue={(value) => `${Math.round(value)}%`}
                  max={100}
                  samples={traceHistory.cpu}
                  title="CPU Load"
                />
                <DebugTraceChart
                  accent="good"
                  formatValue={(value) => `${Math.round(value)}%`}
                  max={100}
                  samples={traceHistory.battery}
                  title="Battery SOC"
                />
              </div>
            </div>
          </section>

          <section className="panel-card panel-card--wide">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="accent" label="LOG" />
                <div>
                  <h2>System Logs</h2>
                  <p>Transport previews, alerts, and gateway activity.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              <div className="log-terminal">
                {logLines.map((line) => (
                  <div className="log-terminal__line" key={line.key}>
                    <span className="log-terminal__timestamp">[{formatPacketTimestamp(line.tsMs)}]</span>{' '}
                    <span className={`log-terminal__source log-terminal__source--${line.tone}`}>[{line.source}]</span>{' '}
                    <span className="log-terminal__emphasis">{line.emphasis}</span>{' '}
                    <span>{line.message}</span>
                  </div>
                ))}
              </div>
            </div>
          </section>

          <section className="panel-card panel-card--wide">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="accent" label="CFG" />
                <div>
                  <h2>Active Parameters</h2>
                  <p>Live operating settings and diagnostics constants.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              <div className="metrics-grid metrics-grid--three">
                {parameterCards.map((parameter) => (
                  <MetricTile key={parameter.label} label={parameter.label} tone="good" value={parameter.value} />
                ))}
              </div>
            </div>
          </section>
        </div>
      </div>
    </main>
  );
}

function App() {
  const { route, navigate } = useAppRoute();
  const { sendControllerState, sendEStop, sendEStopRelease } = useSTARConnection(WS_URL);

  return route === '/ros' ? (
    <RosScreen navigate={navigate} />
  ) : (
    <MainScreen
      navigate={navigate}
      sendControllerState={sendControllerState}
      sendEStop={sendEStop}
      sendEStopRelease={sendEStopRelease}
    />
  );
}

export default App;
