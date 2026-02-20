import type { TelemetryData, SystemStatus } from '@/proto/star/v1/telemetry'
import type { MotorStatus, EncoderData } from '@/proto/star/v1/motor_control'
import type {
  SystemConfiguration,
  MotorPidConfiguration,
} from '@/proto/star/v1/configuration'
import type { FirmwareInfo, FirmwareUpdateProgress } from '@/proto/star/v1/firmware_update'

/** Snapshot of all observable robot state. */
export interface RobotSnapshot {
  telemetry: TelemetryData | null
  systemStatus: SystemStatus | null
  motors: MotorStatus[]
  encoders: EncoderData[]
  lastUpdated: Date | null
}

/** Config read/write operations. */
export interface GatewayClient {
  /** Start polling / streaming. */
  connect(): void
  /** Stop all activity. */
  disconnect(): void
  /** Subscribe to state snapshots. */
  subscribe(cb: (snapshot: RobotSnapshot) => void): () => void

  // Motor control
  emergencyStop(): Promise<void>
  setMotorPower(motorId: number, dutyCycle: number): Promise<void>

  // Configuration
  getConfiguration(): Promise<SystemConfiguration>
  setConfiguration(cfg: SystemConfiguration): Promise<void>
  getMotorPidConfig(motorId: number): Promise<MotorPidConfiguration>
  setMotorPidConfig(motorId: number, pid: MotorPidConfiguration): Promise<void>
  resetConfiguration(): Promise<void>

  // Firmware
  getFirmwareInfo(): Promise<FirmwareInfo>
  beginFirmwareUpdate(size: number, crc32: number): Promise<void>
  uploadFirmwareChunk(data: Uint8Array, offset: number): Promise<void>
  finalizeFirmwareUpdate(): Promise<void>
  abortFirmwareUpdate(): Promise<void>
  getFirmwareUpdateProgress(): Promise<FirmwareUpdateProgress>
  reboot(delayMs: number): Promise<void>
  rollback(): Promise<void>
  markValid(): Promise<void>

}
