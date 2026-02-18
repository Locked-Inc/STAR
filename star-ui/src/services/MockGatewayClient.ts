/**
 * Simulated gateway client with realistic, slowly-varying robot state.
 * Used in development or when the gateway is unreachable.
 */

import type { GatewayClient, RobotSnapshot } from './GatewayClient'
import type { TelemetryData, SystemStatus } from '@/proto/star/v1/telemetry'
import type { BatteryState } from '@/proto/star/v1/battery_management'
import type { MotorStatus } from '@/proto/star/v1/motor_control'
import type { SystemConfiguration, MotorPidConfiguration } from '@/proto/star/v1/configuration'
import type { FirmwareInfo, FirmwareUpdateProgress } from '@/proto/star/v1/firmware_update'
import { ConnectionStatus, RobotMode, GpsFix } from '@/proto/star/v1/telemetry'
import { FirmwareUpdateState } from '@/proto/star/v1/firmware_update'
import { MotorState } from '@/proto/star/v1/motor_control'
import { BatteryStateEnum } from '@/proto/star/v1/battery_management'

type Subscriber = (snapshot: RobotSnapshot) => void

const MOTOR_COUNT = 4

function rand(min: number, max: number): number {
  return min + Math.random() * (max - min)
}

function lerp(a: number, b: number, t: number): number {
  return a + (b - a) * t
}

const DEFAULT_PID_CONFIG = {
  kp: 1.2,
  ki: 0.8,
  kd: 0.05,
  outputMinPercent: -100,
  outputMaxPercent: 100,
  integralMin: -50,
  integralMax: 50,
}

export class MockGatewayClient implements GatewayClient {
  private readonly subscribers: Set<Subscriber> = new Set()
  private intervalId: ReturnType<typeof setInterval> | null = null
  private tick = 0

  private pitch = 0
  private roll = 0
  private yaw = 0
  private cpu = 35
  private wifi = -65
  private battPct = 87
  private motorVelocities: number[] = [0.0, 0.0, 0.0, 0.0]
  private motorTargets: number[] = [0.3, 0.3, 0.3, 0.3]
  private bootTime = Date.now()

  connect(): void {
    if (this.intervalId) return
    this.intervalId = setInterval(() => this.step(), 200)
  }

  disconnect(): void {
    if (this.intervalId) {
      clearInterval(this.intervalId)
      this.intervalId = null
    }
  }

  subscribe(cb: Subscriber): () => void {
    this.subscribers.add(cb)
    return () => this.subscribers.delete(cb)
  }

  private step(): void {
    this.tick++
    const t = 0.1

    this.pitch = lerp(this.pitch, rand(-0.1, 0.1), t) + rand(-0.002, 0.002)
    this.roll = lerp(this.roll, rand(-0.05, 0.05), t) + rand(-0.001, 0.001)
    this.yaw = (this.yaw + rand(-0.005, 0.005)) % (2 * Math.PI)
    this.cpu = Math.max(5, Math.min(95, this.cpu + rand(-2, 2)))
    this.wifi = Math.max(-90, Math.min(-40, this.wifi + rand(-1, 1)))
    this.battPct = Math.max(0, this.battPct - 0.001)

    for (let i = 0; i < MOTOR_COUNT; i++) {
      this.motorVelocities[i] = lerp(
        this.motorVelocities[i],
        this.motorTargets[i] + rand(-0.02, 0.02),
        0.3
      )
    }

    const snapshot = this.buildSnapshot()
    this.subscribers.forEach((cb) => cb(snapshot))
  }

  private buildSnapshot(): RobotSnapshot {
    const telemetry: TelemetryData = {
      imu: {
        pitchRad: this.pitch,
        rollRad: this.roll,
        yawRad: this.yaw,
        accelXMps2: rand(-0.05, 0.05),
        accelYMps2: rand(-0.02, 0.02),
        accelZMps2: 9.81 + rand(-0.02, 0.02),
        gyroXRadPerS: rand(-0.01, 0.01),
        gyroYRadPerS: rand(-0.01, 0.01),
        gyroZRadPerS: rand(-0.005, 0.005),
      },
      gps: {
        latitudeDeg: 30.628,
        longitudeDeg: -96.334,
        altitudeM: 103.5,
        accuracyM: 1.2,
        satellites: 9,
        fixType: GpsFix.GPS_FIX_3D,
      },
      batteryPercent: this.battPct,
      wifiSignalDbm: Math.round(this.wifi),
      cpuUsagePercent: this.cpu,
      temperatureCelsius: 28.5 + rand(-0.5, 0.5),
      motorLoadPercent: Math.abs(this.motorVelocities[0]) * 50,
      timestamp: undefined,
    }

    const systemStatus: SystemStatus = {
      connectionStatus: ConnectionStatus.CONNECTED,
      mode: RobotMode.MANUAL,
      esp32Connected: false,
      lidarConnected: true,
      rosConnected: true,
      firmwareVersion: '2.1.0',
      uptimeS: String(Math.round((Date.now() - this.bootTime) / 1000)),
      freeHeapBytes: Math.round(rand(180000, 220000)),
    }

    const cellMvs = [3710, 3720, 3700, 3680, 3715, 3705].map((v) => Math.round(v + rand(-8, 8)))
    const minCellMv = Math.min(...cellMvs)
    const maxCellMv = Math.max(...cellMvs)
    const packMv = cellMvs.reduce((a, b) => a + b, 0)

    const battery: BatteryState = {
      cells: {
        cellMv: cellMvs,
        validCells: 6,
        packMv,
        minCellMv,
        maxCellMv,
        deltaMv: maxCellMv - minCellMv,
      },
      temperatures: {
        tempDeciCelsius: [295, 298, 301].map((v) => Math.round(v + rand(-5, 5))),
        validSensors: 3,
        avgTempDeciCelsius: Math.round(298 + rand(-3, 3)),
        minTempDeciCelsius: 293,
        maxTempDeciCelsius: 303,
      },
      current: {
        currentMa: Math.round(-1200 - rand(0, 100)),
        avgCurrentMa: Math.round(-1150 + rand(-50, 50)),
        voltageMv: packMv,
        powerMw: Math.round(packMv * 1.2),
      },
      soc: {
        remainingCapacityMah: Math.round(this.battPct * 25),
        fullCapacityMah: 2500,
        designCapacityMah: 2600,
        relativeSocPercent: Math.round(this.battPct * 10) / 10,
        absoluteSocPercent: Math.round(this.battPct * 9.5) / 10,
        cycleCount: 42,
      },
      status: {
        batteryStatusRegister: 0x0000,
        safetyStatusRegister: 0x0000,
        operationStatusRegister: 0x0003,
        alarmWarningRegister: 0x0000,
        charging: false,
        discharging: true,
        fullyCharged: false,
        fullyDischarged: false,
        faultActive: false,
        safetyFaults: undefined,
        state: BatteryStateEnum.DISCHARGING,
      },
      timestampUs: String(Date.now() * 1000),
    }

    const motors: MotorStatus[] = Array.from({ length: MOTOR_COUNT }, (_, i) => ({
      motorId: i,
      dutyCyclePercent: this.motorVelocities[i] * 50,
      velocityMps: this.motorVelocities[i],
      targetVelocityMps: this.motorTargets[i],
      temperatureCelsius: 35 + rand(-2, 2),
      currentMa: Math.abs(this.motorVelocities[i]) * 800 + rand(-50, 50),
      faultFlags: 0,
      state: MotorState.RUNNING,
    }))

    return {
      telemetry,
      systemStatus,
      battery,
      motors,
      encoders: motors.map((_, i) => ({
        motorId: i,
        ticks: String(Math.round(this.motorVelocities[i] * 341 * (this.tick * 0.2))),
        velocityMps: this.motorVelocities[i],
        timestampUs: String(Date.now() * 1000),
      })),
      lastUpdated: new Date(),
    }
  }

  // ---- Motor control -------------------------------------------------------

  async emergencyStop(): Promise<void> {
    this.motorTargets = [0, 0, 0, 0]
    await new Promise((r) => setTimeout(r, 50))
  }

  async setMotorPower(motorId: number, dutyCycle: number): Promise<void> {
    if (motorId >= 0 && motorId < MOTOR_COUNT) {
      this.motorTargets[motorId] = dutyCycle / 50
    }
    await new Promise((r) => setTimeout(r, 30))
  }

  // ---- Configuration -------------------------------------------------------

  async getConfiguration(): Promise<SystemConfiguration> {
    await new Promise((r) => setTimeout(r, 100))
    return {
      motorConfigs: Array.from({ length: MOTOR_COUNT }, (_, i) => ({
        motorId: i,
        pidConfig: { ...DEFAULT_PID_CONFIG },
      })),
      safetyThresholds: {
        overcurrentThresholdMa: 3000,
        thermalShutdownDeciCelsius: 700,
        cellImbalanceThresholdMv: 100,
      },
      timingConfig: {
        motorControlPeriodMs: 10,
        telemetryPeriodMs: 100,
        communicationTimeoutMs: 500,
        bmsPollPeriodMs: 1000,
      },
      configVersion: 1,
      configCrc: 0,
    }
  }

  async setConfiguration(cfg: SystemConfiguration): Promise<void> {
    void cfg
    await new Promise((r) => setTimeout(r, 150))
  }

  async getMotorPidConfig(motorId: number): Promise<MotorPidConfiguration> {
    await new Promise((r) => setTimeout(r, 80))
    return {
      motorId,
      pidConfig: { ...DEFAULT_PID_CONFIG },
    }
  }

  async setMotorPidConfig(_motorId: number, _pid: MotorPidConfiguration): Promise<void> {
    await new Promise((r) => setTimeout(r, 100))
  }

  async resetConfiguration(): Promise<void> {
    await new Promise((r) => setTimeout(r, 200))
  }

  // ---- Firmware ------------------------------------------------------------

  async getFirmwareInfo(): Promise<FirmwareInfo> {
    await new Promise((r) => setTimeout(r, 100))
    return {
      version: '2.1.0',
      gitHash: 'a3f8c12',
      buildDate: '2026-02-15T10:30:00Z',
      size: 524288,
      crc32: 0xdeadbeef,
      idfVersion: 'v5.1.0',
      chipTarget: 'rx72n',
    }
  }

  async beginFirmwareUpdate(_size: number, _crc32: number): Promise<void> {
    await new Promise((r) => setTimeout(r, 200))
  }

  async uploadFirmwareChunk(_data: Uint8Array, _offset: number): Promise<void> {
    await new Promise((r) => setTimeout(r, 50))
  }

  async finalizeFirmwareUpdate(): Promise<void> {
    await new Promise((r) => setTimeout(r, 300))
  }

  async abortFirmwareUpdate(): Promise<void> {
    await new Promise((r) => setTimeout(r, 100))
  }

  async getFirmwareUpdateProgress(): Promise<FirmwareUpdateProgress> {
    await new Promise((r) => setTimeout(r, 50))
    return {
      totalSize: 0,
      bytesReceived: 0,
      progressPercent: 0,
      runningCrc32: 0,
      state: FirmwareUpdateState.IDLE,
      estimatedSecondsRemaining: 0,
      transferSpeedBps: 0,
      lastSequence: 0,
    }
  }

  async reboot(_delayMs: number): Promise<void> {
    await new Promise((r) => setTimeout(r, 200))
  }

  async rollback(): Promise<void> {
    await new Promise((r) => setTimeout(r, 200))
  }

  async markValid(): Promise<void> {
    await new Promise((r) => setTimeout(r, 100))
  }

  // ---- Battery -------------------------------------------------------------

  async enableBalancing(_cells: number[]): Promise<void> {
    await new Promise((r) => setTimeout(r, 100))
  }

  async disableBalancing(): Promise<void> {
    await new Promise((r) => setTimeout(r, 100))
  }
}
