import { useState, useEffect, useRef } from 'react'
import { useGateway } from '@/hooks/useGateway'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  Legend,
} from 'recharts'
import { Activity, Navigation2, Thermometer, Cpu, Wifi, Compass } from 'lucide-react'

const MAX_HISTORY = 60

interface TelemetrySample {
  t: number
  pitch: number
  roll: number
  yaw: number
  cpu: number
  wifi: number
  temp: number
  motorLoad: number
}

function AngleGauge({ label, value, maxDeg = 180 }: { label: string; value: number; maxDeg?: number }) {
  const deg = value * (180 / Math.PI)
  const angle = (deg / maxDeg) * 90
  const color = Math.abs(deg) > maxDeg * 0.7 ? '#ef4444' : Math.abs(deg) > maxDeg * 0.4 ? '#f59e0b' : '#6366f1'

  return (
    <div className="flex flex-col items-center gap-2">
      <span className="text-xs uppercase tracking-wider text-slate-400">{label}</span>
      <div className="relative h-20 w-20">
        <svg viewBox="0 0 80 80" className="h-full w-full -rotate-90">
          <circle cx="40" cy="40" r="32" fill="none" stroke="#1e293b" strokeWidth="8" />
          <circle
            cx="40"
            cy="40"
            r="32"
            fill="none"
            stroke={color}
            strokeWidth="8"
            strokeDasharray={`${Math.abs(angle) * 2.01} 201`}
            strokeLinecap="round"
            style={{ transition: 'stroke-dasharray 0.15s ease' }}
          />
        </svg>
        <div className="absolute inset-0 flex flex-col items-center justify-center">
          <span className="font-mono text-sm font-bold text-white">{deg.toFixed(1)}°</span>
        </div>
      </div>
    </div>
  )
}

function MetricCard({ icon: Icon, label, value, unit, color = 'text-white' }: {
  icon: React.ElementType
  label: string
  value: string | number
  unit: string
  color?: string
}) {
  return (
    <div className="flex items-center gap-3 rounded-lg bg-slate-800/50 p-3">
      <Icon size={18} className="shrink-0 text-slate-400" />
      <div>
        <div className="text-xs text-slate-500">{label}</div>
        <div className={`font-mono font-semibold ${color}`}>
          {typeof value === 'number' ? value.toFixed(3) : value}{' '}
          <span className="text-xs font-normal text-slate-400">{unit}</span>
        </div>
      </div>
    </div>
  )
}

export function TelemetryPage() {
  const { snapshot } = useGateway()
  const t = snapshot.telemetry
  const historyRef = useRef<TelemetrySample[]>([])
  const [history, setHistory] = useState<TelemetrySample[]>([])

  useEffect(() => {
    if (!t) return
    const sample: TelemetrySample = {
      t: Date.now(),
      pitch: (t.imu?.pitchRad ?? 0) * (180 / Math.PI),
      roll: (t.imu?.rollRad ?? 0) * (180 / Math.PI),
      yaw: (t.imu?.yawRad ?? 0) * (180 / Math.PI),
      cpu: t.cpuUsagePercent,
      wifi: t.wifiSignalDbm,
      temp: t.temperatureCelsius,
      motorLoad: t.motorLoadPercent,
    }
    historyRef.current = [...historyRef.current.slice(-MAX_HISTORY + 1), sample]
    setHistory([...historyRef.current])
  }, [t])

  const chartData = history.map((s, i) => ({ i, ...s }))

  return (
    <div className="space-y-6">
      {/* IMU Orientation */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Compass size={14} /> IMU Orientation
          </CardTitle>
        </CardHeader>
        <CardContent>
          {t?.imu ? (
            <div className="grid grid-cols-3 gap-6">
              <AngleGauge label="Pitch" value={t.imu.pitchRad} maxDeg={90} />
              <AngleGauge label="Roll" value={t.imu.rollRad} maxDeg={180} />
              <AngleGauge label="Yaw" value={t.imu.yawRad} maxDeg={180} />
            </div>
          ) : (
            <div className="py-8 text-center text-slate-500">No IMU data</div>
          )}
        </CardContent>
      </Card>

      {/* IMU Acceleration & Gyro */}
      {t?.imu && (
        <div className="grid grid-cols-2 gap-4 lg:grid-cols-3">
          <Card>
            <CardHeader><CardTitle>Acceleration</CardTitle></CardHeader>
            <CardContent className="space-y-2">
              <MetricCard icon={Activity} label="X-axis" value={t.imu.accelXMps2} unit="m/s²" color="text-red-300" />
              <MetricCard icon={Activity} label="Y-axis" value={t.imu.accelYMps2} unit="m/s²" color="text-green-300" />
              <MetricCard icon={Activity} label="Z-axis" value={t.imu.accelZMps2} unit="m/s²" color="text-blue-300" />
            </CardContent>
          </Card>

          <Card>
            <CardHeader><CardTitle>Angular Velocity</CardTitle></CardHeader>
            <CardContent className="space-y-2">
              <MetricCard icon={Activity} label="Gyro X (roll)" value={t.imu.gyroXRadPerS} unit="rad/s" color="text-red-300" />
              <MetricCard icon={Activity} label="Gyro Y (pitch)" value={t.imu.gyroYRadPerS} unit="rad/s" color="text-green-300" />
              <MetricCard icon={Activity} label="Gyro Z (yaw)" value={t.imu.gyroZRadPerS} unit="rad/s" color="text-blue-300" />
            </CardContent>
          </Card>

          <Card className="lg:col-span-1 col-span-2">
            <CardHeader><CardTitle>System Metrics</CardTitle></CardHeader>
            <CardContent className="space-y-2">
              <MetricCard icon={Cpu} label="CPU Usage" value={t.cpuUsagePercent.toFixed(1)} unit="%" color="text-violet-300" />
              <MetricCard icon={Thermometer} label="Temperature" value={t.temperatureCelsius} unit="°C" color="text-amber-300" />
              <MetricCard icon={Wifi} label="WiFi Signal" value={t.wifiSignalDbm} unit="dBm" color="text-blue-300" />
            </CardContent>
          </Card>
        </div>
      )}

      {/* GPS */}
      {t?.gps && (
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Navigation2 size={14} /> GPS
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="grid grid-cols-2 gap-4 lg:grid-cols-4">
              {[
                { label: 'Latitude', value: `${t.gps.latitudeDeg.toFixed(6)}°`, unit: 'N' },
                { label: 'Longitude', value: `${t.gps.longitudeDeg.toFixed(6)}°`, unit: 'E' },
                { label: 'Altitude', value: `${t.gps.altitudeM.toFixed(1)}`, unit: 'm' },
                { label: 'Accuracy', value: `±${t.gps.accuracyM.toFixed(1)}`, unit: 'm' },
              ].map(({ label, value, unit }) => (
                <div key={label} className="space-y-1">
                  <div className="text-xs uppercase tracking-wider text-slate-500">{label}</div>
                  <div className="font-mono text-sm font-semibold text-white">
                    {value} <span className="text-xs text-slate-400">{unit}</span>
                  </div>
                </div>
              ))}
            </div>
            <div className="mt-3 flex items-center gap-3">
              <Badge variant="success">{t.gps.satellites} satellites</Badge>
              <Badge variant="info">Fix {t.gps.satellites > 4 ? '3D' : '2D'}</Badge>
            </div>
          </CardContent>
        </Card>
      )}

      {/* Orientation history chart */}
      {history.length > 2 && (
        <Card>
          <CardHeader><CardTitle>Orientation History (°)</CardTitle></CardHeader>
          <CardContent>
            <ResponsiveContainer width="100%" height={220}>
              <LineChart data={chartData}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                <XAxis dataKey="i" hide />
                <YAxis domain={[-90, 90]} tick={{ fill: '#64748b', fontSize: 11 }} />
                <Tooltip
                  contentStyle={{ background: '#1e293b', border: '1px solid #334155', fontSize: 12 }}
                  labelFormatter={() => ''}
                />
                <Legend wrapperStyle={{ fontSize: 12 }} />
                <Line type="monotone" dataKey="pitch" stroke="#6366f1" dot={false} strokeWidth={1.5} name="Pitch" />
                <Line type="monotone" dataKey="roll" stroke="#22c55e" dot={false} strokeWidth={1.5} name="Roll" />
                <Line type="monotone" dataKey="yaw" stroke="#f59e0b" dot={false} strokeWidth={1.5} name="Yaw" />
              </LineChart>
            </ResponsiveContainer>
          </CardContent>
        </Card>
      )}

      {/* CPU/WiFi chart */}
      {history.length > 2 && (
        <Card>
          <CardHeader><CardTitle>System Metrics History</CardTitle></CardHeader>
          <CardContent>
            <ResponsiveContainer width="100%" height={200}>
              <LineChart data={chartData}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                <XAxis dataKey="i" hide />
                <YAxis yAxisId="cpu" domain={[0, 100]} tick={{ fill: '#64748b', fontSize: 11 }} />
                <YAxis yAxisId="wifi" orientation="right" domain={[-100, -30]} tick={{ fill: '#64748b', fontSize: 11 }} />
                <Tooltip
                  contentStyle={{ background: '#1e293b', border: '1px solid #334155', fontSize: 12 }}
                  labelFormatter={() => ''}
                />
                <Legend wrapperStyle={{ fontSize: 12 }} />
                <Line yAxisId="cpu" type="monotone" dataKey="cpu" stroke="#8b5cf6" dot={false} strokeWidth={1.5} name="CPU %" />
                <Line yAxisId="wifi" type="monotone" dataKey="wifi" stroke="#3b82f6" dot={false} strokeWidth={1.5} name="WiFi dBm" />
              </LineChart>
            </ResponsiveContainer>
          </CardContent>
        </Card>
      )}
    </div>
  )
}
