import { useState } from 'react'
import { useGateway } from '@/hooks/useGateway'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import { Button } from '@/components/ui/button'
import { Slider } from '@/components/ui/slider'
import { Progress } from '@/components/ui/progress'
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '@/components/ui/table'
import { MotorState } from '@/proto/star/v1/motor_control'
import { AlertTriangle, Zap } from 'lucide-react'
import { cn } from '@/lib/utils'

const MOTOR_LABELS = ['Front-Left', 'Front-Right', 'Back-Left', 'Back-Right']

function motorStateBadge(state: MotorState, faultFlags: number) {
  if (faultFlags !== 0) return <Badge variant="danger">FAULT</Badge>
  switch (state) {
    case MotorState.RUNNING: return <Badge variant="success">Running</Badge>
    case MotorState.IDLE: return <Badge variant="default">Idle</Badge>
    case MotorState.FAULT: return <Badge variant="danger">Fault</Badge>
    case MotorState.ESTOP: return <Badge variant="danger">E-Stop</Badge>
    default: return <Badge variant="default">Unknown</Badge>
  }
}

function faultDescription(flags: number): string[] {
  const faults: string[] = []
  if (flags & 0x01) faults.push('Overcurrent')
  if (flags & 0x02) faults.push('Overtemperature')
  if (flags & 0x04) faults.push('Encoder fault')
  return faults
}

interface MotorManualControl {
  motorId: number
  dutyCycle: number
}

export function MotorsPage() {
  const { client, snapshot } = useGateway()
  const { motors, encoders } = snapshot
  const [manualControls, setManualControls] = useState<Map<number, number>>(new Map())
  const [stopping, setStopping] = useState(false)
  const [sending, setSending] = useState<Set<number>>(new Set())

  async function handleEStop() {
    setStopping(true)
    try {
      await client.emergencyStop()
    } finally {
      setStopping(false)
    }
  }

  async function sendMotorPower(control: MotorManualControl) {
    setSending((prev) => new Set(prev).add(control.motorId))
    try {
      await client.setMotorPower(control.motorId, control.dutyCycle)
    } finally {
      setSending((prev) => {
        const next = new Set(prev)
        next.delete(control.motorId)
        return next
      })
    }
  }

  function getDutyCycle(motorId: number): number {
    return manualControls.get(motorId) ?? 0
  }

  function setDutyCycle(motorId: number, value: number) {
    setManualControls((prev) => new Map(prev).set(motorId, value))
  }

  const anyFault = motors.some((m) => m.faultFlags !== 0)

  return (
    <div className="space-y-6">
      {/* E-STOP */}
      <div className="flex items-center justify-between rounded-lg border border-red-700/40 bg-red-950/20 px-4 py-3">
        <div>
          <div className="font-semibold text-red-300">Emergency Stop</div>
          <div className="text-xs text-slate-400">Immediately zeros all motor commands</div>
        </div>
        <Button
          variant="destructive"
          size="lg"
          onClick={handleEStop}
          disabled={stopping}
        >
          {stopping ? 'Stopping…' : 'E-STOP'}
        </Button>
      </div>

      {anyFault && (
        <div className="flex items-center gap-3 rounded-lg border border-amber-700/40 bg-amber-950/20 p-3">
          <AlertTriangle size={16} className="text-amber-400" />
          <span className="text-sm text-amber-300">Motor faults detected — check individual motors below</span>
        </div>
      )}

      {/* Motor cards */}
      <div className="grid grid-cols-1 gap-4 lg:grid-cols-2">
        {motors.map((motor) => {
          const label = MOTOR_LABELS[motor.motorId] ?? `Motor ${motor.motorId}`
          const encoder = encoders.find((e) => e.motorId === motor.motorId)
          const dutyCycle = getDutyCycle(motor.motorId)
          const faults = faultDescription(motor.faultFlags)
          const pct = Math.abs(motor.dutyCyclePercent)

          return (
            <Card key={motor.motorId} className={cn(motor.faultFlags !== 0 && 'border-red-700/60')}>
              <CardHeader>
                <CardTitle className="flex items-center justify-between">
                  <span>{label}</span>
                  {motorStateBadge(motor.state, motor.faultFlags)}
                </CardTitle>
              </CardHeader>
              <CardContent className="space-y-4">
                {faults.length > 0 && (
                  <div className="rounded bg-red-900/30 p-2 text-xs text-red-300">
                    Faults: {faults.join(', ')}
                  </div>
                )}

                {/* Live metrics */}
                <div className="grid grid-cols-2 gap-3 text-sm">
                  <div>
                    <div className="text-xs text-slate-500">Velocity</div>
                    <div className="font-mono font-semibold text-white">
                      {motor.velocityMps.toFixed(3)} m/s
                    </div>
                    <div className="text-xs text-slate-500">
                      Target: {motor.targetVelocityMps.toFixed(3)} m/s
                    </div>
                  </div>
                  <div>
                    <div className="text-xs text-slate-500">Duty Cycle</div>
                    <div className="font-mono font-semibold text-white">
                      {motor.dutyCyclePercent.toFixed(1)}%
                    </div>
                  </div>
                  <div>
                    <div className="text-xs text-slate-500">Current</div>
                    <div className="font-mono text-amber-300">
                      {(motor.currentMa / 1000).toFixed(3)} A
                    </div>
                  </div>
                  <div>
                    <div className="text-xs text-slate-500">Temperature</div>
                    <div
                      className={cn(
                        'font-mono',
                        motor.temperatureCelsius > 60 ? 'text-red-400' : motor.temperatureCelsius > 45 ? 'text-amber-400' : 'text-white'
                      )}
                    >
                      {motor.temperatureCelsius.toFixed(1)}°C
                    </div>
                  </div>
                </div>

                {/* Duty cycle bar */}
                <div className="space-y-1">
                  <div className="flex justify-between text-xs text-slate-500">
                    <span>Duty Cycle</span>
                    <span>{pct.toFixed(1)}%</span>
                  </div>
                  <Progress
                    value={pct}
                    indicatorClassName={motor.dutyCyclePercent >= 0 ? 'bg-indigo-500' : 'bg-amber-500'}
                  />
                </div>

                {/* Encoder */}
                {encoder && (
                  <div className="rounded bg-slate-800/50 p-2 text-xs font-mono">
                    <div className="flex justify-between">
                      <span className="text-slate-400">Encoder ticks:</span>
                      <span className="text-slate-200">{encoder.ticks}</span>
                    </div>
                    <div className="flex justify-between mt-1">
                      <span className="text-slate-400">Encoder vel:</span>
                      <span className="text-slate-200">{encoder.velocityMps.toFixed(4)} m/s</span>
                    </div>
                  </div>
                )}

                {/* Manual power control */}
                <div className="space-y-2 border-t border-slate-700/50 pt-3">
                  <div className="flex items-center justify-between text-xs text-slate-400">
                    <span>Manual Power</span>
                    <span className="font-mono">{dutyCycle.toFixed(0)}%</span>
                  </div>
                  <Slider
                    min={-100}
                    max={100}
                    step={1}
                    value={[dutyCycle]}
                    onValueChange={([v]) => setDutyCycle(motor.motorId, v ?? 0)}
                  />
                  <div className="flex gap-2">
                    <Button
                      size="sm"
                      variant="outline"
                      className="flex-1"
                      onClick={() => sendMotorPower({ motorId: motor.motorId, dutyCycle })}
                      disabled={sending.has(motor.motorId)}
                    >
                      <Zap size={12} />
                      {sending.has(motor.motorId) ? 'Sending…' : 'Apply'}
                    </Button>
                    <Button
                      size="sm"
                      variant="ghost"
                      onClick={() => {
                        setDutyCycle(motor.motorId, 0)
                        void sendMotorPower({ motorId: motor.motorId, dutyCycle: 0 })
                      }}
                    >
                      Stop
                    </Button>
                  </div>
                </div>
              </CardContent>
            </Card>
          )
        })}
      </div>

      {/* Encoder summary table */}
      {encoders.length > 0 && (
        <Card>
          <CardHeader><CardTitle>Encoder Summary</CardTitle></CardHeader>
          <CardContent>
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>Motor</TableHead>
                  <TableHead>Ticks</TableHead>
                  <TableHead>Velocity</TableHead>
                  <TableHead>Rev (est)</TableHead>
                </TableRow>
              </TableHeader>
              <TableBody>
                {encoders.map((enc) => {
                  const ticks = parseInt(enc.ticks, 10) || 0
                  const revs = (ticks / 341).toFixed(2)
                  return (
                    <TableRow key={enc.motorId}>
                      <TableCell>{MOTOR_LABELS[enc.motorId] ?? `Motor ${enc.motorId}`}</TableCell>
                      <TableCell className="font-mono">{ticks.toLocaleString()}</TableCell>
                      <TableCell className="font-mono">{enc.velocityMps.toFixed(4)} m/s</TableCell>
                      <TableCell className="font-mono">{revs}</TableCell>
                    </TableRow>
                  )
                })}
              </TableBody>
            </Table>
          </CardContent>
        </Card>
      )}
    </div>
  )
}
