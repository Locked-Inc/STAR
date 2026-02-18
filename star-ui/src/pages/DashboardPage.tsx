import { useGateway } from '@/hooks/useGateway'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import { Progress } from '@/components/ui/progress'
import {
  Battery,
  Wifi,
  Cpu,
  Thermometer,
  Navigation,
  Radio,
  Zap,
  Clock,
  MemoryStick,
} from 'lucide-react'
import { ConnectionStatus, RobotMode } from '@/proto/star/v1/telemetry'
import { cn } from '@/lib/utils'

function formatUptime(seconds: string): string {
  const s = parseInt(seconds, 10) || 0
  const h = Math.floor(s / 3600)
  const m = Math.floor((s % 3600) / 60)
  const sec = s % 60
  if (h > 0) return `${h}h ${m}m ${sec}s`
  if (m > 0) return `${m}m ${sec}s`
  return `${sec}s`
}

function batteryColor(pct: number): string {
  if (pct > 50) return 'bg-emerald-500'
  if (pct > 20) return 'bg-amber-500'
  return 'bg-red-500'
}

function wifiLabel(dbm: number): string {
  if (dbm > -55) return 'Excellent'
  if (dbm > -67) return 'Good'
  if (dbm > -75) return 'Fair'
  return 'Poor'
}

function wifiColor(dbm: number): 'success' | 'warning' | 'danger' {
  if (dbm > -67) return 'success'
  if (dbm > -75) return 'warning'
  return 'danger'
}

function cpuColor(pct: number): string {
  if (pct < 50) return 'bg-indigo-500'
  if (pct < 80) return 'bg-amber-500'
  return 'bg-red-500'
}

function modeLabel(mode: RobotMode): string {
  switch (mode) {
    case RobotMode.MANUAL: return 'MANUAL'
    case RobotMode.AUTONOMOUS: return 'AUTO'
    case RobotMode.MAPPING: return 'MAPPING'
    case RobotMode.IDLE: return 'IDLE'
    default: return 'UNKNOWN'
  }
}

function modeBadgeVariant(mode: RobotMode): 'success' | 'warning' | 'info' | 'default' {
  switch (mode) {
    case RobotMode.MANUAL: return 'success'
    case RobotMode.AUTONOMOUS: return 'info'
    case RobotMode.MAPPING: return 'warning'
    default: return 'default'
  }
}

export function DashboardPage() {
  const { snapshot } = useGateway()
  const { telemetry: t, systemStatus: s, motors, battery } = snapshot

  const isConnected = s?.connectionStatus === ConnectionStatus.CONNECTED
  const avgMotorVel =
    motors.length > 0
      ? motors.reduce((sum, m) => sum + Math.abs(m.velocityMps), 0) / motors.length
      : 0
  const socPct = battery?.soc?.relativeSocPercent ?? t?.batteryPercent ?? 0

  return (
    <div className="space-y-6">
      {/* System status row */}
      <div className="grid grid-cols-2 gap-4 lg:grid-cols-4">
        {/* Connection */}
        <Card>
          <CardHeader><CardTitle>Gateway</CardTitle></CardHeader>
          <CardContent>
            <div className={cn('text-2xl font-bold', isConnected ? 'text-emerald-400' : 'text-red-400')}>
              {isConnected ? 'Online' : 'Offline'}
            </div>
            {s && (
              <div className="mt-2 flex flex-wrap gap-1">
                {s.lidarConnected && <Badge variant="success">LiDAR</Badge>}
                {s.rosConnected && <Badge variant="success">ROS2</Badge>}
                {s.esp32Connected && <Badge variant="success">RX72N</Badge>}
                {!s.lidarConnected && <Badge variant="warning">No LiDAR</Badge>}
                {!s.rosConnected && <Badge variant="warning">No ROS</Badge>}
              </div>
            )}
          </CardContent>
        </Card>

        {/* Mode */}
        <Card>
          <CardHeader><CardTitle>Mode</CardTitle></CardHeader>
          <CardContent>
            {s ? (
              <>
                <Badge variant={modeBadgeVariant(s.mode)} className="text-sm px-3 py-1">
                  {modeLabel(s.mode)}
                </Badge>
                <div className="mt-2 flex items-center gap-1.5 text-xs text-slate-500">
                  <Clock size={12} />
                  {formatUptime(s.uptimeS)}
                </div>
              </>
            ) : (
              <div className="text-slate-500">—</div>
            )}
          </CardContent>
        </Card>

        {/* Firmware */}
        <Card>
          <CardHeader><CardTitle>Firmware</CardTitle></CardHeader>
          <CardContent>
            <div className="font-mono text-lg font-semibold text-white">
              {s?.firmwareVersion ?? '—'}
            </div>
            {s && (
              <div className="mt-1 flex items-center gap-1.5 text-xs text-slate-500">
                <MemoryStick size={12} />
                {Math.round(s.freeHeapBytes / 1024)} KB free
              </div>
            )}
          </CardContent>
        </Card>

        {/* Average motor speed */}
        <Card>
          <CardHeader><CardTitle>Avg Speed</CardTitle></CardHeader>
          <CardContent>
            <div className="font-mono text-2xl font-bold text-white">
              {avgMotorVel.toFixed(3)}
              <span className="ml-1 text-sm font-normal text-slate-400">m/s</span>
            </div>
            <div className="mt-1 text-xs text-slate-500">
              {motors.filter((m) => m.velocityMps !== 0).length} / {motors.length} motors active
            </div>
          </CardContent>
        </Card>
      </div>

      {/* Metrics row */}
      <div className="grid grid-cols-1 gap-4 lg:grid-cols-3">
        {/* Battery */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Battery size={14} /> Battery
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-3">
            <div className="flex items-end justify-between">
              <span className="font-mono text-3xl font-bold text-white">
                {socPct.toFixed(1)}
                <span className="ml-1 text-base font-normal text-slate-400">%</span>
              </span>
              <Badge variant={socPct > 50 ? 'success' : socPct > 20 ? 'warning' : 'danger'}>
                {socPct > 80 ? 'Full' : socPct > 50 ? 'Good' : socPct > 20 ? 'Low' : 'Critical'}
              </Badge>
            </div>
            <Progress value={socPct} indicatorClassName={batteryColor(socPct)} />
            {battery?.current && (
              <div className="grid grid-cols-2 gap-2 text-xs text-slate-400">
                <span>
                  Current:{' '}
                  <span className="text-white font-mono">
                    {(Math.abs(battery.current.currentMa) / 1000).toFixed(2)} A
                  </span>
                </span>
                <span>
                  Voltage:{' '}
                  <span className="text-white font-mono">
                    {(battery.current.voltageMv / 1000).toFixed(2)} V
                  </span>
                </span>
              </div>
            )}
          </CardContent>
        </Card>

        {/* Wireless */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Wifi size={14} /> Wireless
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-3">
            {t ? (
              <>
                <div className="flex items-end justify-between">
                  <span className="font-mono text-3xl font-bold text-white">
                    {t.wifiSignalDbm}
                    <span className="ml-1 text-base font-normal text-slate-400">dBm</span>
                  </span>
                  <Badge variant={wifiColor(t.wifiSignalDbm)}>{wifiLabel(t.wifiSignalDbm)}</Badge>
                </div>
                <Progress
                  value={Math.max(0, 100 + t.wifiSignalDbm + 30)}
                  indicatorClassName="bg-blue-500"
                />
                {t.gps && (
                  <div className="flex items-center gap-1.5 text-xs text-slate-400">
                    <Navigation size={11} />
                    <span>
                      {t.gps.satellites} sats · {t.gps.latitudeDeg.toFixed(4)}°N{' '}
                      {Math.abs(t.gps.longitudeDeg).toFixed(4)}°W
                    </span>
                  </div>
                )}
              </>
            ) : (
              <div className="text-slate-500">No data</div>
            )}
          </CardContent>
        </Card>

        {/* Compute */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Cpu size={14} /> Compute
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-3">
            {t ? (
              <>
                <div className="flex items-end justify-between">
                  <span className="font-mono text-3xl font-bold text-white">
                    {t.cpuUsagePercent.toFixed(0)}
                    <span className="ml-1 text-base font-normal text-slate-400">%</span>
                  </span>
                  <Badge
                    variant={t.cpuUsagePercent < 50 ? 'success' : t.cpuUsagePercent < 80 ? 'warning' : 'danger'}
                  >
                    CPU
                  </Badge>
                </div>
                <Progress value={t.cpuUsagePercent} indicatorClassName={cpuColor(t.cpuUsagePercent)} />
                <div className="grid grid-cols-2 gap-2 text-xs text-slate-400">
                  <span className="flex items-center gap-1">
                    <Thermometer size={11} />
                    {t.temperatureCelsius.toFixed(1)}°C
                  </span>
                  <span className="flex items-center gap-1">
                    <Zap size={11} />
                    {t.motorLoadPercent.toFixed(0)}% motor load
                  </span>
                </div>
              </>
            ) : (
              <div className="text-slate-500">No data</div>
            )}
          </CardContent>
        </Card>
      </div>

      {/* Motor overview */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Radio size={14} /> Motor Overview
          </CardTitle>
        </CardHeader>
        <CardContent>
          {motors.length === 0 ? (
            <div className="py-4 text-center text-slate-500">No motor data</div>
          ) : (
            <div className="grid grid-cols-2 gap-4 lg:grid-cols-4">
              {motors.map((m, i) => {
                const LABELS = ['FL', 'FR', 'BL', 'BR']
                const label = LABELS[i] ?? `M${i}`
                const pct = Math.abs(m.dutyCyclePercent)
                const forward = m.velocityMps >= 0
                return (
                  <div key={m.motorId} className="space-y-2">
                    <div className="flex items-center justify-between text-xs">
                      <span className="font-semibold text-slate-300">{label}</span>
                      <Badge
                        variant={m.faultFlags !== 0 ? 'danger' : m.velocityMps !== 0 ? 'success' : 'default'}
                      >
                        {m.faultFlags !== 0 ? 'FAULT' : m.velocityMps !== 0 ? 'RUN' : 'IDLE'}
                      </Badge>
                    </div>
                    <Progress
                      value={pct}
                      indicatorClassName={forward ? 'bg-indigo-500' : 'bg-amber-500'}
                    />
                    <div className="text-xs text-slate-500">
                      {m.velocityMps.toFixed(3)} m/s · {m.dutyCyclePercent.toFixed(1)}%
                    </div>
                  </div>
                )
              })}
            </div>
          )}
        </CardContent>
      </Card>

      {/* IMU quick view */}
      {t?.imu && (
        <Card>
          <CardHeader>
            <CardTitle>IMU Orientation</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="grid grid-cols-3 gap-4">
              {[
                { label: 'Pitch', value: t.imu.pitchRad * (180 / Math.PI) },
                { label: 'Roll', value: t.imu.rollRad * (180 / Math.PI) },
                { label: 'Yaw', value: t.imu.yawRad * (180 / Math.PI) },
              ].map(({ label, value }) => (
                <div key={label} className="text-center">
                  <div className="text-xs uppercase tracking-wider text-slate-500">{label}</div>
                  <div className="font-mono text-2xl font-bold text-white">
                    {value.toFixed(1)}
                    <span className="text-sm font-normal text-slate-400">°</span>
                  </div>
                </div>
              ))}
            </div>
          </CardContent>
        </Card>
      )}
    </div>
  )
}
