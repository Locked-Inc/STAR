import { create } from 'zustand';
import { subscribeWithSelector } from 'zustand/middleware';
import type { STAREnvelope, Alert, LidarScan, OdometryData } from '../proto/star/v1/ui';
import { AlertLevel } from '../proto/star/v1/ui';
import type { TelemetryData, SystemStatus } from '../proto/star/v1/telemetry';
import type { MotorStatus } from '../proto/star/v1/motor_control';

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'reconnecting';

const ALERTS_MAX = 200;

interface DashboardState {
  // Connection
  connectionState: ConnectionState;
  lastSeq: number;
  seqGapDetected: boolean;
  dataIsStale: boolean;

  // Robot data -- individual slices
  telemetry: TelemetryData | null;
  motors: MotorStatus[] | null;   // length 4: FL, FR, BL, BR
  odometry: OdometryData | null;
  lidarScan: LidarScan | null;
  systemStatus: SystemStatus | null;
  eStopActive: boolean;

  // Alerts -- capped array; oldest dropped at ALERTS_MAX
  alerts: Alert[];

  // Actions
  setConnectionState: (s: ConnectionState) => void;
  updateFromEnvelope: (env: STAREnvelope) => void;
  markStale: () => void;
  addAlert: (alert: Alert) => void;
  triggerEStop: () => void;
  releaseEStop: () => void;
}

export const useDashboardStore = create<DashboardState>()(
  subscribeWithSelector((set, get) => ({
    connectionState: 'disconnected',
    lastSeq: 0,
    seqGapDetected: false,
    dataIsStale: false,
    telemetry: null,
    motors: null,
    odometry: null,
    lidarScan: null,
    systemStatus: null,
    eStopActive: false,
    alerts: [],

    setConnectionState: (s) => {
      if (s === 'connected') {
        set({ connectionState: s, seqGapDetected: false, lastSeq: 0 });
      } else {
        set({ connectionState: s });
      }
    },

    // One set() per case -- ensures only the relevant selector fires.
    // Never merge multiple fields into one set() call across cases.
    updateFromEnvelope: (env) => {
      // Detect sequence gaps and advance lastSeq on gateway-originated messages
      if (Number(env.seq) > 0) {
        const seq = Number(env.seq);
        const prev = get().lastSeq;
        if (seq > prev + 1) {
          set({ seqGapDetected: true, lastSeq: seq });
          get().addAlert({
            level: AlertLevel.WARN,
            code: 'SEQ_GAP',
            message: `Sequence gap: ${prev} -> ${seq}`,
            source: 'ws',
            timestampUs: String(Date.now() * 1000),
          });
        } else if (seq > prev) {
          set({ lastSeq: seq });
        }
        // seq <= prev: duplicate or out-of-order, ignore
      }
      const p = env.payload;
      switch (p.oneofKind) {
        case 'telemetry':
          set({ telemetry: p.telemetry, dataIsStale: false });
          break;
        case 'motors':
          set({ motors: p.motors.motors });
          break;
        case 'odometry':
          set({ odometry: p.odometry });
          break;
        case 'lidar':
          set({ lidarScan: p.lidar });
          break;
        case 'system':
          set({ systemStatus: p.system });
          break;
        case 'alert':
          get().addAlert(p.alert);
          break;
        default:
          break;
      }
    },

    markStale: () => set({ dataIsStale: true }),

    addAlert: (alert) => set((state) => ({
      alerts: [alert, ...state.alerts].slice(0, ALERTS_MAX),
    })),

    triggerEStop: () => set({ eStopActive: true }),

    releaseEStop: () => set({ eStopActive: false }),
  }))
);
