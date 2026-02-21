import type { STAREnvelope } from '../proto/star/v1/ui';
import { AlertLevel } from '../proto/star/v1/ui';

export interface PacketRecord {
  seq: number;
  type: string;
  direction: 'rx' | 'tx';
  sizeBytes: number;
  tsMs: number;
  preview: string;
}

const RING_SIZE = 100;
const ringBuffer: (PacketRecord | null)[] = new Array(RING_SIZE).fill(null);
let writeIdx = 0;

export function recordPacket(
  env: STAREnvelope | null,
  direction: 'rx' | 'tx',
  sizeBytes: number,
  errorType?: string,
): void {
  ringBuffer[writeIdx] = {
    seq: env ? Number(env.seq) : 0,
    type: errorType ?? env?.payload.oneofKind ?? 'unknown',
    direction,
    sizeBytes,
    tsMs: Date.now(),
    preview: env ? formatPreview(env) : (errorType ?? ''),
  };
  writeIdx = (writeIdx + 1) % RING_SIZE;
}

export function getRecentPackets(): PacketRecord[] {
  const result: PacketRecord[] = [];
  for (let i = 0; i < RING_SIZE; i++) {
    const idx = (writeIdx + i) % RING_SIZE;
    const record = ringBuffer[idx];
    if (record !== null) {
      result.push(record);
    }
  }
  // Sort chronologically descending (most recent first)
  return result.reverse();
}

function formatPreview(env: STAREnvelope): string {
  const p = env.payload;
  switch (p.oneofKind) {
    case 'telemetry':
      return `cpu=${p.telemetry.cpuUsagePercent.toFixed(1)}% temp=${p.telemetry.temperatureCelsius.toFixed(1)}C`;
    case 'motors':
      return p.motors.motors
        .map((m, i) => `${(['FL', 'FR', 'BL', 'BR'] as const)[i]}:${m.velocityMps.toFixed(2)}`)
        .join(' ');
    case 'battery': {
      const mv = p.battery.cells?.packMv;
      return `V=${mv != null ? (mv / 1000).toFixed(1) : '?'}v`;
    }
    case 'odometry':
      return `x=${p.odometry.xM.toFixed(2)} y=${p.odometry.yM.toFixed(2)} th=${p.odometry.thetaRad.toFixed(2)}`;
    case 'lidar':
      return `${p.lidar.angleRad.length} pts`;
    case 'alert':
      return `[${AlertLevel[p.alert.level]}] ${p.alert.source}: ${p.alert.message}`;
    case 'system':
      return `lidar=${p.system.lidarConnected} ros=${p.system.rosConnected}`;
    case 'controller':
      return `lin=${p.controller.linearVel.toFixed(2)} ang=${p.controller.angularVel.toFixed(2)} [TX]`;
    case 'estop':
      return `reason=${p.estop.reason} [TX]`;
    default:
      return '';
  }
}
