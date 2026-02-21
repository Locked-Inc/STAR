import { create } from 'zustand';
import { subscribeWithSelector } from 'zustand/middleware';
import type { STAREnvelope, Alert, LidarScan, OdometryData } from '../proto/star/v1/ui';
import type { TelemetryData, SystemStatus } from '../proto/star/v1/telemetry';
import type { MotorStatus } from '../proto/star/v1/motor_control';
import type { BatteryState } from '../proto/star/v1/battery_management';

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'reconnecting';

interface DashboardState {
  // Connection
  connectionState: ConnectionState;
  lastSeq: number;
  seqGapDetected: boolean;
  dataIsStale: boolean;

  // Robot data -- individual slices
  telemetry: TelemetryData | null;
  motors: MotorStatus[] | null;   // length 4: FL, FR, BL, BR
  battery: BatteryState | null;
  odometry: OdometryData | null;
  lidarScan: LidarScan | null;    // LidarPanel uses .subscribe() + useRef
  systemStatus: SystemStatus | null;
  eStopActive: boolean;

  // Alerts -- capped array; oldest dropped at 200
  alerts: Alert[];

  // Actions
  setConnectionState: (s: ConnectionState) => void;
  updateFromEnvelope: (env: STAREnvelope) => void;
  markStale: () => void;
  addAlert: (alert: Alert) => void;
  triggerEStop: () => void;
}

export const useDashboardStore = create<DashboardState>()(
  subscribeWithSelector((set, get) => ({
    connectionState: 'disconnected',
    lastSeq: 0,
    seqGapDetected: false,
    dataIsStale: false,
    telemetry: null,
    motors: null,
    battery: null,
    odometry: null,
    lidarScan: null,
    systemStatus: null,
    eStopActive: false,
    alerts: [],

    setConnectionState: (s) => set({ connectionState: s }),

    // One set() per case -- ensures only the relevant selector fires.
    // Never merge multiple fields into one set() call across cases.
    updateFromEnvelope: (env) => {
      // Always advance lastSeq on gateway-originated messages
      if (Number(env.seq) > 0) {
        set({ lastSeq: Number(env.seq) });
      }
      const p = env.payload;
      switch (p.oneofKind) {
        case 'telemetry':
          set({ telemetry: p.telemetry, dataIsStale: false });
          break;
        case 'motors':
          set({ motors: p.motors.motors });
          break;
        case 'battery':
          set({ battery: p.battery });
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
      alerts: [alert, ...state.alerts].slice(0, 200),
    })),

    triggerEStop: () => set({ eStopActive: true }),
  }))
);
