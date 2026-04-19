import { useEffect, useRef, useState, type MouseEvent as ReactMouseEvent } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { HeaderLink, Glyph, Chip, MetricTile, ControlButton, HealthRow } from '../components/dashboard/DashboardPrimitives';
import { ImuOrientationGraphic } from '../components/dashboard/ImuOrientationGraphic';
import { StarMapCanvas } from '../components/StarMapCanvas';
import {
  average,
  averageDefined,
  clamp,
  demoModeEnabled,
  formatHeading,
  formatNumber,
  formatTimestamp,
  formatUptime,
  connectionStateDisplayByState,
  levelLabel,
  modeLabel,
  parseTimestampUs,
  toneFromBoolean,
  toneFromLevel,
  toneFromScore,
  toneLabel,
  uiModeFromRobotMode,
  wheelCircumferenceMeters,
  secondsPerMinute
} from '../lib/dashboard';
import { useControllerBridge } from '../hooks/useControllerBridge';
import { useOperatorControls } from '../hooks/useOperatorControls';
import type { ControllerState } from '../proto/star/v1/controller';
import { useDashboardStore } from '../store/dashboardStore';
import type { AppRoute, Tone } from '../types/dashboard';

interface MainDashboardViewProps {
  navigate: (route: AppRoute) => void;
  sendControllerState: (state: ControllerState) => void;
  sendEStop: (reason: string) => void;
  sendEStopRelease: () => Promise<boolean>;
}

const healthScoreMax = 100;
const healthScoreMin = 0;
const wifiSignalFloorDbm = -100;
const wifiSignalRangeDb = 70;
const thermalBaselineCelsius = 32;
const thermalScaleFactor = 4;
const connectedSubsystemScore = 100;
const disconnectedSubsystemScore = 0;
const connectionScoreMap = {
  connected: 100,
  connecting: 70,
  reconnecting: 70,
  disconnected: 35,
} as const;

function deriveControllerTone({
  connectionState,
  eStopActive,
  gamepadConnected,
  uiMode,
}: {
  connectionState: keyof typeof connectionScoreMap;
  eStopActive: boolean;
  gamepadConnected: boolean;
  uiMode: 'autonomous' | 'manual';
}): Tone {
  if (eStopActive) {
    return 'danger';
  }

  if (uiMode === 'manual') {
    return gamepadConnected ? 'good' : 'warn';
  }

  if (connectionState === 'connected') {
    return 'good';
  }

  if (connectionState === 'disconnected') {
    return 'danger';
  }

  return 'warn';
}

function radiansToCompassDegrees(radians: number | null | undefined): number | null {
  if (radians == null || !Number.isFinite(radians)) {
    return null;
  }

  return (((radians * 180) / Math.PI) + 360) % 360;
}

function AlertsButton({
  count,
  onClick,
}: {
  count: number;
  onClick: () => void;
}) {
  return (
    <button className="alerts-button" type="button" onClick={onClick}>
      <Glyph accent={count > 0 ? 'warn' : 'neutral'} label="AL" />
      <span>Alerts</span>
      {count > 0 ? <span className="alerts-count">{count}</span> : null}
    </button>
  );
}

export function MainDashboardView({
  navigate,
  sendControllerState,
  sendEStop,
  sendEStopRelease,
}: MainDashboardViewProps) {
  const {
    alerts,
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
  const alertsRef = useRef<HTMLDivElement | null>(null);
  const connectionStateDisplay = connectionStateDisplayByState[connectionState];

  const reportedMode = uiModeFromRobotMode(systemStatus?.mode);
  const {
    autonomyRunning,
    coveragePercent,
    handleAutonomyStart,
    handleAutonomyStop,
    handleEStopToggle,
    handleModeToggle,
    missionProgressAvailable,
    modeChangeDisabled,
    statusCopy,
    statusTone,
    taskCompleted,
    uiMode,
  } = useOperatorControls({
    demoMode: demoModeEnabled,
    eStopActive,
    reportedMode,
    sendEStop,
    sendEStopRelease,
  });

  const controllerBridgeEnabled = uiMode === 'manual' && !eStopActive;
  const gamepadState = useControllerBridge(sendControllerState, controllerBridgeEnabled);

  useEffect(() => {
    if (!alertsOpen) {
      return;
    }

    function handleOutsideClick(event: globalThis.MouseEvent): void {
      if (alertsRef.current && !alertsRef.current.contains(event.target as Node)) {
        setAlertsOpen(false);
      }
    }

    function handleTouchOutside(event: TouchEvent): void {
      if (alertsRef.current && !alertsRef.current.contains(event.target as Node)) {
        setAlertsOpen(false);
      }
    }

    function handleKeyDown(event: KeyboardEvent): void {
      if (event.key === 'Escape') {
        setAlertsOpen(false);
      }
    }

    document.addEventListener('mousedown', handleOutsideClick);
    document.addEventListener('touchstart', handleTouchOutside);
    document.addEventListener('keydown', handleKeyDown);
    return () => {
      document.removeEventListener('mousedown', handleOutsideClick);
      document.removeEventListener('touchstart', handleTouchOutside);
      document.removeEventListener('keydown', handleKeyDown);
    };
  }, [alertsOpen]);

  const motorValues = motors ?? [];
  const motorVelocity = motorValues.length > 0 ? average(motorValues.map((motor) => motor.velocityMps)) : undefined;
  const odometryVelocity = odometry?.linearVelocityMps ?? motorVelocity;
  const lidarPointCount = lidarScan?.rangeM.filter((value) => value > 0).length ?? 0;
  const imu = telemetry?.imu;
  const headingRadians = imu?.yawRad ?? odometry?.thetaRad;
  const compassHeadingDegrees = radiansToCompassDegrees(headingRadians);
  const compassHeadingLabel = compassHeadingDegrees != null ? formatHeading(headingRadians) : '--';
  const wifiSignal = telemetry?.wifiSignalDbm;
  const cpuUsage = telemetry?.cpuUsagePercent;
  const temperature = telemetry?.temperatureCelsius;
  const motorLoad = telemetry?.motorLoadPercent;

  const wifiScore = wifiSignal != null
    ? clamp(((wifiSignal - wifiSignalFloorDbm) / wifiSignalRangeDb) * healthScoreMax, healthScoreMin, healthScoreMax)
    : undefined;
  const cpuScore = cpuUsage != null ? clamp(healthScoreMax - cpuUsage, healthScoreMin, healthScoreMax) : undefined;
  const tempScore = temperature != null
    ? clamp(healthScoreMax - Math.max(0, temperature - thermalBaselineCelsius) * thermalScaleFactor, healthScoreMin, healthScoreMax)
    : undefined;
  const connectivityScore = averageDefined([
    systemStatus ? (systemStatus.esp32Connected ? connectedSubsystemScore : disconnectedSubsystemScore) : undefined,
    systemStatus ? (systemStatus.lidarConnected ? connectedSubsystemScore : disconnectedSubsystemScore) : undefined,
    systemStatus ? (systemStatus.rosConnected ? connectedSubsystemScore : disconnectedSubsystemScore) : undefined,
    connectionScoreMap[connectionState],
  ]);
  const overallHealthScore = averageDefined([wifiScore, cpuScore, tempScore, connectivityScore]);
  const overallHealth = overallHealthScore != null ? Math.round(overallHealthScore) : undefined;

  const controllerTone = deriveControllerTone({
    connectionState,
    eStopActive,
    gamepadConnected: gamepadState.connected,
    uiMode,
  });
  const lidarTone = toneFromBoolean(systemStatus?.lidarConnected, lidarPointCount === 0 && connectionState === 'connected');
  const controllerMcuTone = toneFromBoolean(systemStatus?.esp32Connected, motorValues.length === 0 && connectionState === 'connected');
  const rosTone = toneFromBoolean(systemStatus?.rosConnected, dataIsStale || seqGapDetected);
  const alertPreview = alerts.slice(0, 6);

  const controlNote = eStopActive
    ? 'Robot is emergency stopped.'
    : reportedMode
      ? `Mode follows live robot telemetry: ${modeLabel(uiMode)}.`
      : uiMode === 'manual'
        ? gamepadState.connected
          ? 'Gamepad input is live and streaming to the controller.'
          : 'Connect a gamepad to enable manual control.'
        : missionProgressAvailable
          ? 'Autonomous progress is demo-only in this build.'
          : 'Autonomous progress is unavailable without a demo feed.';

  function handleAlertsToggle(event?: ReactMouseEvent<HTMLButtonElement>): void {
    event?.preventDefault();
    setAlertsOpen((current) => !current);
  }

  return (
    <main className="app-shell app-shell--dashboard">
      <div className="shell-container shell-container--dashboard">
        <header className="screen-header screen-header--dashboard">
          <div className="screen-header__identity">
            <Glyph accent="good" label="PS" />
            <div>
              <h1>Project Star</h1>
              <p>Autonomous Robot Control</p>
            </div>
          </div>

          <div className="screen-header__actions">
            {demoModeEnabled ? <Chip tone="warn">Demo Mode</Chip> : null}
            <Chip tone={controllerTone}>Controller: {toneLabel(controllerTone)}</Chip>
            <HeaderLink href="/ros" onNavigate={navigate} label="Debug" accent="accent" glyph="RX" />
            <Chip tone={connectionStateDisplay.tone}>{connectionStateDisplay.label}</Chip>
            {uiMode === 'manual' ? (
              <Chip tone={gamepadState.connected ? 'good' : 'warn'}>{gamepadState.connected ? 'Gamepad' : 'No Gamepad'}</Chip>
            ) : null}

            <div className="alerts-anchor" ref={alertsRef}>
              <AlertsButton count={alerts.length} onClick={handleAlertsToggle} />

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
                              <span>{formatTimestamp(parseTimestampUs(alert.timestampUs) ?? 0)}</span>
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

        <div className="dashboard-layout dashboard-layout--main">
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

              <div className="panel-card__body panel-card__body--map">
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
                      <div className="encoder-odometry-layout">
                        <div className="sensor-metrics sensor-metrics--encoders">
                          {['FL', 'FR', 'BL', 'BR'].map((label, index) => {
                            const motor = motorValues[index];
                            const rpm = motor
                              ? (motor.velocityMps / wheelCircumferenceMeters) * secondsPerMinute
                              : undefined;
                            return (
                              <MetricTile
                                key={label}
                                label={`Encoder ${label}`}
                                tone={eStopActive ? 'neutral' : motor ? 'good' : 'neutral'}
                                value={rpm != null ? `${formatNumber(rpm, 0)} RPM` : '--'}
                              />
                            );
                          })}
                        </div>
                        <div className="odometry-metric">
                          <MetricTile
                            label="Odometry"
                            tone="accent"
                            value={odometry ? `${formatNumber(odometry.xM, 2)} m` : '--'}
                            detail={odometry ? `Heading ${formatHeading(odometry.thetaRad)}` : 'Awaiting odometry'}
                          />
                        </div>
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
                        pitchDeg={imu ? (imu.pitchRad * 180) / Math.PI : 0}
                        rollDeg={imu ? (imu.rollRad * 180) / Math.PI : 0}
                      />

                      <div className="orientation-values">
                        <MetricTile label="Roll" tone="accent" value={`${formatNumber(imu ? (imu.rollRad * 180) / Math.PI : undefined, 1)} deg`} />
                        <MetricTile label="Pitch" tone="accent" value={`${formatNumber(imu ? (imu.pitchRad * 180) / Math.PI : undefined, 1)} deg`} />
                        <MetricTile label="Yaw" tone="accent" value={`${formatNumber(headingRadians != null ? (headingRadians * 180) / Math.PI : undefined, 1)} deg`} />
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
            <section className="panel-card panel-card--control">
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
                  disabled={modeChangeDisabled}
                />

                <p className="panel-note">{controlNote}</p>
              </div>
            </section>

            {uiMode === 'autonomous' ? (
              <section className="panel-card panel-card--mission">
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
                      disabled={eStopActive || autonomyRunning || !missionProgressAvailable}
                    />
                    <ControlButton
                      label="Stop"
                      tone="warn"
                      onClick={handleAutonomyStop}
                      disabled={eStopActive || !autonomyRunning}
                    />
                  </div>

                  {missionProgressAvailable ? (
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
                  ) : (
                    <div className="empty-state">Live mission progress is unavailable in this operator build.</div>
                  )}
                </div>
              </section>
            ) : (
              <section className="panel-card panel-card--mission">
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
                      <span className={`status-dot status-dot--${gamepadState.connected ? 'good' : 'warn'}`} />
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

            <section className="panel-card panel-card--health">
              <div className="panel-card__heading">
                <div className="panel-title">
                  <Glyph accent={toneFromScore(overallHealth)} label="SYS" />
                  <div>
                    <h2>System Health</h2>
                    <p>Critical stack readiness and controller telemetry.</p>
                  </div>
                </div>
                <div className="panel-card__meta">{overallHealth != null ? `${overallHealth}%` : '--'}</div>
              </div>

              <div className="panel-card__body panel-card__body--compact">
                <HealthRow label="LIDAR" tone={lidarTone} value={toneLabel(lidarTone)} />
                <HealthRow label="RX72N MCU" tone={controllerMcuTone} value={toneLabel(controllerMcuTone)} />
                <HealthRow label="ROS2 Stack" tone={rosTone} value={toneLabel(rosTone)} />
              </div>
            </section>

            <section className="panel-card panel-card--compass">
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
                    <div
                      className="compass-needle"
                      style={{
                        opacity: compassHeadingDegrees != null ? 1 : 0.2,
                        transform: compassHeadingDegrees != null
                          ? `translate(-50%, -100%) rotate(${compassHeadingDegrees}deg)`
                          : 'translate(-50%, -100%)',
                      }}
                    />
                    <div className="compass-center" />
                  </div>
                  <div className="compass-readout">
                    <span>Heading</span>
                    <strong>{compassHeadingLabel}</strong>
                  </div>
                </div>
              </div>
            </section>

            <section className="panel-card panel-card--telemetry">
              <div className="panel-card__heading">
                <div className="panel-title">
                  <Glyph accent="good" label="TEL" />
                  <div>
                    <h2>System Telemetry</h2>
                    <p>Network, compute, thermal, and runtime overview.</p>
                  </div>
                </div>
              </div>

              <div className="panel-card__body">
                <div className="metrics-grid">
                  <MetricTile
                    label="WiFi"
                    tone={toneFromScore(wifiScore)}
                    value={wifiSignal != null ? `${wifiSignal} dBm` : '--'}
                    detail={connectionStateDisplay.label}
                  />
                  <MetricTile
                    label="CPU"
                    tone={toneFromScore(cpuScore)}
                    value={cpuUsage != null ? `${cpuUsage.toFixed(0)}%` : '--'}
                    detail={temperature != null ? `${temperature.toFixed(1)} C` : 'No thermal data'}
                  />
                  <MetricTile
                    label="Motor Load"
                    tone={motorLoad != null ? toneFromScore(100 - Math.min(motorLoad, 100)) : 'neutral'}
                    value={motorLoad != null ? `${motorLoad.toFixed(0)}%` : '--'}
                  />
                  <MetricTile
                    label="Uptime"
                    tone="neutral"
                    value={formatUptime(systemStatus?.uptimeS)}
                    detail={systemStatus?.firmwareVersion || 'Firmware unavailable'}
                  />
                </div>
              </div>
            </section>
          </aside>
        </div>
      </div>
    </main>
  );
}
