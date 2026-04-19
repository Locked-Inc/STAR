import { useMemo } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { DebugTraceChart } from '../components/DebugTraceChart';
import { Chip, Glyph, HeaderLink, MetricTile, StatusDot } from '../components/dashboard/DashboardPrimitives';
import {
  clamp,
  demoModeEnabled,
  formatHeading,
  formatNumber,
  formatTimestamp,
  formatUptime,
  connectionStateDisplayByState,
  levelLabel,
  parseTimestampUs,
  recentPacketWindowMs,
  rosTopicDefinitions,
  toneFromLevel,
} from '../lib/dashboard';
import { usePacketFeed } from '../hooks/usePacketFeed';
import { useTraceHistory } from '../hooks/useTraceHistory';
import { useDashboardStore } from '../store/dashboardStore';
import type { AppRoute, LogLine } from '../types/dashboard';

const demoRosNodes = [
  { name: '/slam_toolbox', description: 'SLAM mapping and localization', tone: 'good' as const },
  { name: '/rplidar_node', description: 'LIDAR sensor driver', tone: 'good' as const },
  { name: '/motor_controller', description: 'RX72N motor control interface', tone: 'good' as const },
  { name: '/imu_filter', description: 'IMU data filtering and fusion', tone: 'warn' as const },
  { name: '/nav2_controller', description: 'Navigation2 path planning', tone: 'good' as const },
  { name: '/robot_state_publisher', description: 'Robot kinematic model', tone: 'good' as const },
];

const maxTerminalLogLines = 20;
const minimumRateSampleCount = 2;

function formatObservedRate(rateHz: number | undefined): string {
  if (rateHz == null || !Number.isFinite(rateHz) || rateHz <= 0) {
    return '--';
  }

  return `${formatNumber(rateHz, rateHz >= 10 ? 0 : 1)} Hz`;
}

function calculateObservedRateHz(sampleTimestampsMs: number[]): number | undefined {
  if (sampleTimestampsMs.length < minimumRateSampleCount) {
    return undefined;
  }

  const intervalsMs: number[] = [];
  for (let index = 1; index < sampleTimestampsMs.length; index += 1) {
    const intervalMs = Math.abs(sampleTimestampsMs[index] - sampleTimestampsMs[index - 1]);
    if (intervalMs > 0) {
      intervalsMs.push(intervalMs);
    }
  }

  if (intervalsMs.length === 0) {
    return undefined;
  }

  const averageIntervalMs = intervalsMs.reduce((sum, intervalMs) => sum + intervalMs, 0) / intervalsMs.length;
  if (averageIntervalMs <= 0) {
    return undefined;
  }

  return 1000 / averageIntervalMs;
}

interface RosDebugViewProps {
  navigate: (route: AppRoute) => void;
}

export function RosDebugView({ navigate }: RosDebugViewProps) {
  const { alerts, connectionState, lidarScan, odometry, systemStatus, telemetry } = useDashboardStore(
    useShallow((state) => ({
      alerts: state.alerts,
      connectionState: state.connectionState,
      lidarScan: state.lidarScan,
      odometry: state.odometry,
      systemStatus: state.systemStatus,
      telemetry: state.telemetry,
    })),
  );

  const { packets, sampledAtMs } = usePacketFeed();
  const lidarPointCount = lidarScan?.rangeM.filter((value) => value > 0).length ?? 0;
  const traceHistory = useTraceHistory({
    cpu: clamp(telemetry?.cpuUsagePercent ?? 0, 0, 100),
    lidar: lidarPointCount,
    temperature: clamp(telemetry?.temperatureCelsius ?? 0, 0, 100),
    velocity: Math.abs(odometry?.linearVelocityMps ?? 0),
  });

  const packetTopics = useMemo(() => {
    const seen = new Map<string, (typeof packets)[number]>();
    packets.forEach((packet) => {
      const definition = rosTopicDefinitions.find((candidate) => candidate.packetType === packet.type);
      if (definition && !seen.has(definition.topic)) {
        seen.set(definition.topic, packet);
      }
    });
    return seen;
  }, [packets]);

  const observedRatesByPacketType = useMemo(() => {
    const timestampsByPacketType = new Map<string, number[]>();

    packets.forEach((packet) => {
      if (sampledAtMs - packet.tsMs >= recentPacketWindowMs) {
        return;
      }

      const packetTimestampsMs = timestampsByPacketType.get(packet.type);
      if (packetTimestampsMs) {
        packetTimestampsMs.push(packet.tsMs);
        return;
      }

      timestampsByPacketType.set(packet.type, [packet.tsMs]);
    });

    return new Map(
      Array.from(timestampsByPacketType.entries(), ([packetType, sampleTimestampsMs]) => [
        packetType,
        calculateObservedRateHz(sampleTimestampsMs),
      ]),
    );
  }, [packets, sampledAtMs]);

  const connectionStateDisplay = connectionStateDisplayByState[connectionState];

  const parameterCards = useMemo(
    () => [
      {
        label: 'scan_frequency',
        value: formatObservedRate(observedRatesByPacketType.get('lidar')),
      },
      {
        label: 'controller_frequency',
        value: formatObservedRate(observedRatesByPacketType.get('controller')),
      },
    ],
    [observedRatesByPacketType],
  );

  const referenceParameters = useMemo(
    () => [
      {
        label: 'map_resolution',
        value: '0.05 m/cell',
      },
      {
        label: 'wheel_diameter',
        value: '0.20 m',
      },
    ],
    [],
  );

  const activeTopics = useMemo(
    () =>
      rosTopicDefinitions.map((topicRow) => {
        const packet = packetTopics.get(topicRow.topic);
        const observedRateHz = observedRatesByPacketType.get(topicRow.packetType);
        const isLive = packet ? sampledAtMs > 0 && sampledAtMs - packet.tsMs < recentPacketWindowMs : false;

        let detail = packet?.preview ? packet.preview : 'No recent traffic observed.';
        if (topicRow.topic === '/scan' && lidarPointCount > 0) {
          detail = `${lidarPointCount} scan points observed`;
        }
        if (topicRow.topic === '/odom' && odometry) {
          detail = `${formatNumber(odometry.linearVelocityMps, 2)} m/s`;
        }
        if (topicRow.topic === '/imu/data' && telemetry?.imu) {
          detail = formatHeading(telemetry.imu.yawRad);
        }

        return {
          ...topicRow,
          detail,
          isLive,
          rateLabel: formatObservedRate(observedRateHz),
        };
      }),
    [lidarPointCount, observedRatesByPacketType, odometry, packetTopics, sampledAtMs, telemetry],
  );

  const logLines = useMemo<LogLine[]>(() => {
    const fallbackAlertTimestampMs = sampledAtMs > 0 ? sampledAtMs : 0;

    const packetLogs: LogLine[] = packets
      .filter((packet) => packet.type !== 'battery')
      .map((packet): LogLine => ({
        key: `packet-${packet.tsMs}-${packet.seq}-${packet.type}`,
        tsMs: packet.tsMs,
        tone: packet.type === 'alert' ? 'warn' : packet.direction === 'tx' ? 'accent' : 'good',
        source: packet.direction === 'tx' ? 'TX' : 'RX',
        message: packet.preview || packet.type,
        emphasis: packet.type,
      }));

    const alertLogs: LogLine[] = alerts.map((alert, index): LogLine => {
      const parsedTimestampMs = parseTimestampUs(alert.timestampUs);

      return {
        key: `alert-${alert.code}-${alert.timestampUs}-${index}`,
        tsMs: parsedTimestampMs ?? fallbackAlertTimestampMs,
        tone: toneFromLevel(alert.level),
        source: levelLabel(alert.level),
        message: `${alert.source || 'system'}: ${alert.message}`,
        emphasis: alert.code || 'alert',
      };
    });

    return [...packetLogs, ...alertLogs].sort((left, right) => right.tsMs - left.tsMs).slice(0, maxTerminalLogLines);
  }, [alerts, packets, sampledAtMs]);

  return (
    <main className="app-shell app-shell--ros-view">
      <div className="shell-container shell-container--ros shell-container--ros-view">
        <header className="screen-header screen-header--ros screen-header--ros-view">
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

          <div className="screen-header__actions">
            {demoModeEnabled ? <Chip tone="warn">Demo Mode</Chip> : null}
            <Chip tone={connectionStateDisplay.tone}>{connectionStateDisplay.label}</Chip>
          </div>
        </header>

        <div className="ros-grid ros-grid--viewport">
          <section className="panel-card panel-card--ros-topics">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="good" label="TPC" />
                <div>
                  <h2>Active ROS2 Topics</h2>
                  <p>Observed gateway traffic mapped to operator-facing ROS channels.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              <div className="list-stack">
                {activeTopics.map((topic) => (
                  <div className="list-card" key={topic.topic}>
                    <div className="list-card__header">
                      <span className={`mono accent-text accent-text--${topic.isLive ? 'good' : 'warn'}`}>{topic.topic}</span>
                      <span className="list-card__caption">
                        {topic.type} | observed {topic.rateLabel}
                      </span>
                    </div>
                    <div className="list-card__footer">
                      {topic.description} <span className="mono">{topic.detail}</span>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </section>

          <section className="panel-card panel-card--ros-nodes">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="warn" label="NDS" />
                <div>
                  <h2>ROS2 Nodes</h2>
                  <p>Node graph visibility from the current gateway build.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              {demoModeEnabled ? (
                <div className="list-stack">
                  {demoRosNodes.map((node) => (
                    <div className="list-card" key={node.name}>
                      <div className="list-card__header">
                        <span className={`mono accent-text accent-text--${node.tone}`}>{node.name}</span>
                        <StatusDot tone={node.tone} />
                      </div>
                      <div className="list-card__footer">{node.description}</div>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="empty-state">Live ROS2 node graph data is unavailable from the current gateway.</div>
              )}
            </div>
          </section>

          <section className="panel-card panel-card--wide panel-card--ros-traces">
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
                  min={0}
                  samples={traceHistory.velocity}
                  title="Velocity"
                />
                <DebugTraceChart
                  accent="good"
                  formatValue={(value) => `${Math.round(value)} pts`}
                  min={0}
                  samples={traceHistory.lidar}
                  title="LIDAR Density"
                />
                <DebugTraceChart
                  accent="warn"
                  formatValue={(value) => `${Math.round(value)}%`}
                  max={100}
                  min={0}
                  samples={traceHistory.cpu}
                  title="CPU Load"
                />
                <DebugTraceChart
                  accent="danger"
                  formatValue={(value) => `${value.toFixed(1)} C`}
                  max={100}
                  min={0}
                  samples={traceHistory.temperature}
                  title="Temperature"
                />
              </div>
            </div>
          </section>

          <section className="panel-card panel-card--wide panel-card--ros-params">
            <div className="panel-card__heading">
              <div className="panel-title">
                <Glyph accent="accent" label="CFG" />
                <div>
                  <h2>Active Parameters</h2>
                  <p>Parameter visibility from the current debug transport.</p>
                </div>
              </div>
            </div>

            <div className="panel-card__body">
              {demoModeEnabled ? (
                <>
                  <h3>Live Parameters</h3>
                  <div className="metrics-grid metrics-grid--three">
                    {parameterCards.map((parameter) => (
                      <MetricTile key={parameter.label} label={parameter.label} tone="accent" value={parameter.value} />
                    ))}
                  </div>
                  <h3>Reference / Demo Parameters</h3>
                  <div className="metrics-grid metrics-grid--three">
                    {referenceParameters.map((parameter) => (
                      <MetricTile key={parameter.label} label={parameter.label} tone="warn" value={parameter.value} />
                    ))}
                  </div>
                </>
              ) : (
                <div className="empty-state">
                  Live parameter values are unavailable from the current gateway. Firmware {systemStatus?.firmwareVersion || '--'} | Uptime{' '}
                  {formatUptime(systemStatus?.uptimeS)}.
                </div>
              )}
            </div>
          </section>

          <section className="panel-card panel-card--wide panel-card--ros-logs">
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
                    <span className="log-terminal__timestamp">[{formatTimestamp(line.tsMs)}]</span>{' '}
                    <span className={`log-terminal__source log-terminal__source--${line.tone}`}>[{line.source}]</span>{' '}
                    <span className="log-terminal__emphasis">{line.emphasis}</span>{' '}
                    <span>{line.message}</span>
                  </div>
                ))}
              </div>
            </div>
          </section>
        </div>
      </div>
    </main>
  );
}
