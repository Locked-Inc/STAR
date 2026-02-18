import { useState } from 'react'
import { useGateway } from '@/hooks/useGateway'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Progress } from '@/components/ui/progress'
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '@/components/ui/table'
import { BatteryFull, Thermometer, Zap, AlertTriangle, Scale } from 'lucide-react'
import { BatteryStateEnum } from '@/proto/star/v1/battery_management'
import { cn } from '@/lib/utils'

const CELL_NAMES = ['C1', 'C2', 'C3', 'C4', 'C5', 'C6', 'C7', 'C8']

function stateBadge(state: BatteryStateEnum) {
  switch (state) {
    case BatteryStateEnum.CHARGING: return <Badge variant="info">Charging</Badge>
    case BatteryStateEnum.DISCHARGING: return <Badge variant="warning">Discharging</Badge>
    case BatteryStateEnum.FULL: return <Badge variant="success">Full</Badge>
    case BatteryStateEnum.IDLE: return <Badge variant="default">Idle</Badge>
    default: return <Badge variant="default">Unknown</Badge>
  }
}

function cellColor(mv: number, minMv: number, maxMv: number): string {
  const range = maxMv - minMv
  if (range === 0) return 'bg-slate-600'
  const delta = mv - minMv
  const ratio = delta / range
  if (ratio < 0.15) return 'bg-red-500'
  if (ratio > 0.85) return 'bg-emerald-500'
  return 'bg-indigo-500'
}

export function BatteryPage() {
  const { client, snapshot } = useGateway()
  const battery = snapshot.battery
  const [balancing, setBalancing] = useState(false)

  if (!battery) {
    return (
      <div className="flex h-64 items-center justify-center text-slate-500">
        Waiting for battery data…
      </div>
    )
  }

  const { cells, temperatures, current, soc, status } = battery
  const socPct = soc?.relativeSocPercent ?? 0
  const packV = (current?.voltageMv ?? 0) / 1000
  const currentA = (current?.currentMa ?? 0) / 1000
  const powerW = packV * Math.abs(currentA)

  const faultList = status?.safetyFaults
    ? (Object.entries(status.safetyFaults).filter(([, v]) => v === true) as [string, boolean][]).map(([k]) => k)
    : []

  const minCell = cells ? Math.min(...cells.cellMv) : 0
  const maxCell = cells ? Math.max(...cells.cellMv) : 0
  const deltaCell = maxCell - minCell

  async function handleBalancing() {
    setBalancing(true)
    try {
      if (balancing) {
        await client.disableBalancing()
      } else {
        await client.enableBalancing(cells?.cellMv.map((_, i) => i) ?? [])
      }
    } finally {
      setBalancing(false)
    }
  }

  return (
    <div className="space-y-6">
      {/* Faults banner */}
      {faultList.length > 0 && (
        <div className="flex items-start gap-3 rounded-lg border border-red-700/60 bg-red-900/20 p-4">
          <AlertTriangle size={18} className="shrink-0 text-red-400 mt-0.5" />
          <div>
            <div className="font-semibold text-red-300">Safety Faults Detected</div>
            <div className="mt-1 flex flex-wrap gap-2">
              {faultList.map((f) => (
                <Badge key={f} variant="danger">{f.replace(/([A-Z])/g, ' $1').trim()}</Badge>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* Overview row */}
      <div className="grid grid-cols-2 gap-4 lg:grid-cols-4">
        {/* SoC */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <BatteryFull size={14} /> State of Charge
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-3">
            <div className="text-4xl font-mono font-bold text-white">
              {socPct.toFixed(1)}
              <span className="text-lg font-normal text-slate-400">%</span>
            </div>
            <Progress
              value={socPct}
              indicatorClassName={socPct > 50 ? 'bg-emerald-500' : socPct > 20 ? 'bg-amber-500' : 'bg-red-500'}
            />
            {soc && (
              <div className="text-xs text-slate-400">
                {soc.remainingCapacityMah} / {soc.fullCapacityMah} mAh · {soc.cycleCount} cycles
              </div>
            )}
          </CardContent>
        </Card>

        {/* Voltage */}
        <Card>
          <CardHeader><CardTitle>Pack Voltage</CardTitle></CardHeader>
          <CardContent>
            <div className="text-3xl font-mono font-bold text-white">
              {packV.toFixed(3)}
              <span className="text-sm font-normal text-slate-400"> V</span>
            </div>
            <div className="mt-2 space-y-1 text-xs text-slate-400">
              <div>Min cell: <span className="font-mono text-slate-200">{minCell} mV</span></div>
              <div>Max cell: <span className="font-mono text-slate-200">{maxCell} mV</span></div>
              <div>
                Δ delta:{' '}
                <span className={cn('font-mono', deltaCell > 50 ? 'text-amber-400' : 'text-slate-200')}>
                  {deltaCell} mV
                </span>
              </div>
            </div>
          </CardContent>
        </Card>

        {/* Current */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Zap size={14} /> Current
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="text-3xl font-mono font-bold text-white">
              {Math.abs(currentA).toFixed(3)}
              <span className="text-sm font-normal text-slate-400"> A</span>
            </div>
            <div className="mt-1 text-xs text-slate-400">
              {currentA < 0 ? 'Discharging' : 'Charging'}
            </div>
            <div className="mt-2 text-sm">
              <span className="text-slate-400">Power: </span>
              <span className="font-mono font-semibold text-amber-300">{powerW.toFixed(2)} W</span>
            </div>
            {status && stateBadge(status.state)}
          </CardContent>
        </Card>

        {/* Temperature */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Thermometer size={14} /> Temperature
            </CardTitle>
          </CardHeader>
          <CardContent>
            {temperatures && temperatures.tempDeciCelsius.length > 0 ? (
              <div className="space-y-2">
                {temperatures.tempDeciCelsius.slice(0, temperatures.validSensors).map((v, i) => {
                  const celsius = v / 10
                  return (
                    <div key={i} className="flex items-center justify-between">
                      <span className="text-xs text-slate-400">Sensor {i + 1}</span>
                      <span
                        className={cn(
                          'font-mono text-sm font-semibold',
                          celsius > 45 ? 'text-red-400' : celsius > 35 ? 'text-amber-400' : 'text-white'
                        )}
                      >
                        {celsius.toFixed(1)}°C
                      </span>
                    </div>
                  )
                })}
                <div className="border-t border-slate-700 pt-2 text-xs text-slate-400">
                  Avg: <span className="font-mono text-white">{(temperatures.avgTempDeciCelsius / 10).toFixed(1)}°C</span>
                </div>
              </div>
            ) : (
              <div className="text-slate-500">No temp data</div>
            )}
          </CardContent>
        </Card>
      </div>

      {/* Cell voltages */}
      {cells && cells.cellMv.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center justify-between">
              <span className="flex items-center gap-2">
                <Scale size={14} /> Cell Voltages
              </span>
              <div className="flex items-center gap-2">
                {balancing && <Badge variant="info">Balancing Active</Badge>}
                <Button
                  size="sm"
                  variant="outline"
                  onClick={handleBalancing}
                  disabled={balancing}
                >
                  {balancing ? 'Stop Balancing' : 'Start Balancing'}
                </Button>
              </div>
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="space-y-3">
              {/* Bar chart */}
              <div className="flex items-end justify-around gap-2 h-24">
                {cells.cellMv.slice(0, cells.validCells).map((mv, i) => {
                  const height = ((mv - 3000) / 1200) * 100
                  return (
                    <div key={i} className="flex flex-1 flex-col items-center gap-1">
                      <span className="text-xs font-mono text-slate-400">{mv}</span>
                      <div className="w-full flex items-end h-16 rounded overflow-hidden bg-slate-800">
                        <div
                          className={cn('w-full rounded transition-all', cellColor(mv, minCell, maxCell))}
                          style={{ height: `${Math.max(5, height)}%` }}
                        />
                      </div>
                      <span className="text-xs text-slate-500">{CELL_NAMES[i]}</span>
                    </div>
                  )
                })}
              </div>

              {/* Table */}
              <Table>
                <TableHeader>
                  <TableRow>
                    <TableHead>Cell</TableHead>
                    <TableHead>Voltage</TableHead>
                    <TableHead>vs Min</TableHead>
                    <TableHead>Status</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {cells.cellMv.slice(0, cells.validCells).map((mv, i) => (
                    <TableRow key={i}>
                      <TableCell className="font-mono">{CELL_NAMES[i]}</TableCell>
                      <TableCell className="font-mono">{mv} mV</TableCell>
                      <TableCell className="font-mono text-slate-400">+{mv - minCell} mV</TableCell>
                      <TableCell>
                        <Badge
                          variant={
                            mv < 3200 ? 'danger' : mv < 3400 ? 'warning' : 'success'
                          }
                        >
                          {mv < 3200 ? 'Low' : mv < 3400 ? 'OK' : 'Good'}
                        </Badge>
                      </TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </div>
          </CardContent>
        </Card>
      )}

      {/* FET & status */}
      {status && (
        <Card>
          <CardHeader><CardTitle>Hardware Status</CardTitle></CardHeader>
          <CardContent>
            <div className="grid grid-cols-2 gap-3 sm:grid-cols-4">
              {[
                { label: 'Charging', active: status.charging },
                { label: 'Discharging', active: status.discharging },
                { label: 'Fully Charged', active: status.fullyCharged },
                { label: 'Fault Active', active: status.faultActive },
              ].map(({ label, active }) => (
                <div key={label} className="flex flex-col gap-1 rounded-md bg-slate-800/50 p-3">
                  <span className="text-xs text-slate-400">{label}</span>
                  <Badge variant={active ? 'success' : 'default'}>{active ? 'ON' : 'OFF'}</Badge>
                </div>
              ))}
            </div>
            <div className="mt-4 grid grid-cols-3 gap-3 text-xs font-mono text-slate-400">
              <div>Status reg: <span className="text-slate-200">0x{status.batteryStatusRegister.toString(16).toUpperCase().padStart(4, '0')}</span></div>
              <div>Safety reg: <span className="text-slate-200">0x{status.safetyStatusRegister.toString(16).toUpperCase().padStart(4, '0')}</span></div>
              <div>Op status: <span className="text-slate-200">0x{status.operationStatusRegister.toString(16).toUpperCase().padStart(4, '0')}</span></div>
            </div>
          </CardContent>
        </Card>
      )}
    </div>
  )
}
