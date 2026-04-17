import { AlertLevel } from '../proto/star/v1/ui';
import { RobotMode } from '../proto/star/v1/telemetry';
import type { RosTopicDefinition, Tone, UiMode } from '../types/dashboard';

export const controllerSendIntervalMs = 20;
export const packetPollIntervalMs = 1000;
export const recentPacketWindowMs = 8_000;
export const coverageTickIntervalMs = 450;
export const coverageStepPercent = 1.5;
export const traceHistoryLength = 28;

const validAlertTimestampFloorMs = Date.parse('2020-01-01T00:00:00Z');

export const demoModeEnabled = import.meta.env.VITE_DEMO_MODE === 'true';

export const rosTopicDefinitions: RosTopicDefinition[] = [
  {
    packetType: 'lidar',
    topic: '/scan',
    type: 'sensor_msgs/LaserScan',
    description: 'Observed LIDAR scan traffic from the gateway.',
  },
  {
    packetType: 'odometry',
    topic: '/odom',
    type: 'nav_msgs/Odometry',
    description: 'Observed wheel odometry updates from the gateway.',
  },
  {
    packetType: 'controller',
    topic: '/cmd_vel',
    type: 'geometry_msgs/Twist',
    description: 'Observed controller velocity commands.',
  },
  {
    packetType: 'telemetry',
    topic: '/imu/data',
    type: 'sensor_msgs/Imu',
    description: 'Observed IMU telemetry updates from the gateway.',
  },
  {
    packetType: 'alert',
    topic: '/diagnostics',
    type: 'diagnostic_msgs/DiagnosticArray',
    description: 'Observed diagnostic alerts and warnings.',
  },
];

export function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

export function average(values: number[]): number {
  if (values.length === 0) {
    return 0;
  }

  return values.reduce((sum, value) => sum + value, 0) / values.length;
}

export function averageDefined(values: Array<number | undefined>): number | undefined {
  const present = values.filter((value): value is number => value != null && Number.isFinite(value));
  if (present.length === 0) {
    return undefined;
  }

  return average(present);
}

export function formatNumber(value: number | undefined, digits = 1): string {
  if (value == null || Number.isNaN(value)) {
    return '--';
  }

  return value.toFixed(digits);
}

export function formatHeading(radians: number | undefined): string {
  if (radians == null || Number.isNaN(radians)) {
    return '--';
  }

  const degrees = ((radians * 180) / Math.PI + 360) % 360;
  return `${degrees.toFixed(1)}°`;
}

export function formatUptime(seconds: string | undefined): string {
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

export function formatTimestamp(tsMs: number): string {
  if (!Number.isFinite(tsMs) || tsMs <= 0) {
    return '--:--:--';
  }

  return new Date(tsMs).toLocaleTimeString([], {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

export function parseTimestampUs(timestampUs: string | undefined): number | null {
  const raw = Number(timestampUs ?? 0);
  const ms = raw / 1000;
  if (!Number.isFinite(ms) || ms < validAlertTimestampFloorMs) {
    return null;
  }

  return ms;
}

export function toneFromLevel(level: AlertLevel): Tone {
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

export function toneFromScore(score: number | undefined): Tone {
  if (score == null || Number.isNaN(score)) {
    return 'neutral';
  }
  if (score >= 85) {
    return 'good';
  }
  if (score >= 60) {
    return 'warn';
  }
  return 'danger';
}

export function toneFromBoolean(active: boolean | undefined, warning = false): Tone {
  if (active == null) {
    return 'neutral';
  }
  if (active && !warning) {
    return 'good';
  }
  if (warning) {
    return 'warn';
  }
  return active ? 'good' : 'danger';
}

export function toneLabel(tone: Tone): string {
  switch (tone) {
    case 'good':
      return 'OK';
    case 'warn':
      return 'Warning';
    case 'danger':
      return 'Error';
    case 'accent':
      return 'Active';
    default:
      return 'Unavailable';
  }
}

export function levelLabel(level: AlertLevel): string {
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

export function modeLabel(mode: UiMode): string {
  return mode === 'manual' ? 'Manual' : 'Autonomous';
}

export function uiModeFromRobotMode(mode: RobotMode | undefined): UiMode | null {
  switch (mode) {
    case RobotMode.MANUAL:
      return 'manual';
    case RobotMode.AUTONOMOUS:
    case RobotMode.MAPPING:
      return 'autonomous';
    default:
      return null;
  }
}

export function pushTraceSample(values: number[], nextValue: number): number[] {
  return [...values, nextValue].slice(-traceHistoryLength);
}
