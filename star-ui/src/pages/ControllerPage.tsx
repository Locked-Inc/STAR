import { useState } from 'react'
import { useGamepad } from '@/hooks/useGamepad'
import { useControllerConnection } from '@/hooks/useControllerConnection'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Badge } from '@/components/ui/badge'
import { Switch } from '@/components/ui/switch'
import { Label } from '@/components/ui/label'
import { Button } from '@/components/ui/button'
import { Gamepad2, Wifi, AlertTriangle } from 'lucide-react'
import { cn, clamp } from '@/lib/utils'

function JoystickViz({ x, y }: { x: number; y: number }) {
  const dotX = 50 + clamp(x, -1, 1) * 40
  const dotY = 50 - clamp(y, -1, 1) * 40

  return (
    <div className="relative mx-auto h-36 w-36 rounded-full border-2 border-slate-600 bg-slate-800">
      {/* Crosshair */}
      <div className="absolute left-1/2 top-0 h-full w-px -translate-x-1/2 bg-slate-700" />
      <div className="absolute top-1/2 left-0 h-px w-full -translate-y-1/2 bg-slate-700" />
      {/* Dead-zone circle */}
      <div className="absolute left-1/2 top-1/2 h-6 w-6 -translate-x-1/2 -translate-y-1/2 rounded-full border border-dashed border-slate-600" />
      {/* Dot */}
      <div
        className="absolute h-5 w-5 -translate-x-1/2 -translate-y-1/2 rounded-full bg-indigo-400 shadow-[0_0_8px_#818cf8] transition-all duration-50"
        style={{ left: `${dotX}%`, top: `${dotY}%` }}
      />
    </div>
  )
}

function VelocityBar({
  label,
  value,
  max,
  unit,
  horizontal = false,
}: {
  label: string
  value: number
  max: number
  unit: string
  horizontal?: boolean
}) {
  const pct = Math.abs(value / max) * 100
  const positive = value >= 0

  return (
    <div className="flex flex-col items-center gap-1">
      <span className="text-xs uppercase tracking-wider text-slate-400">{label}</span>
      <span className="font-mono text-lg font-semibold text-white">
        {value.toFixed(3)} <span className="text-xs text-slate-400">{unit}</span>
      </span>
      {horizontal ? (
        <div className="relative h-3 w-40 overflow-hidden rounded-full bg-slate-700">
          <div
            className={cn(
              'absolute top-0 h-full rounded-full transition-all',
              positive ? 'bg-blue-500 left-1/2' : 'bg-amber-500 right-1/2'
            )}
            style={{ width: `${pct / 2}%` }}
          />
        </div>
      ) : (
        <div className="relative h-28 w-3 overflow-hidden rounded-full bg-slate-700">
          <div
            className={cn(
              'absolute left-0 w-full rounded-full transition-all',
              positive ? 'bg-indigo-500 bottom-1/2' : 'bg-red-500 top-1/2'
            )}
            style={{ height: `${pct / 2}%` }}
          />
        </div>
      )}
    </div>
  )
}

export function ControllerPage() {
  const [debugMode, setDebugMode] = useState(false)
  const gamepadState = useGamepad()
  const { gatewayConnected } = useControllerConnection(gamepadState, debugMode)

  return (
    <div className="mx-auto max-w-3xl space-y-6">
      {/* Status bar */}
      <div className="flex items-center gap-3 rounded-lg border border-slate-700/50 bg-slate-900/60 p-3">
        <div className="flex items-center gap-2">
          <Wifi size={14} />
          <span className="text-sm text-slate-400">Gateway:</span>
          <Badge variant={gatewayConnected ? 'success' : 'danger'}>
            {gatewayConnected ? 'Connected' : 'Disconnected'}
          </Badge>
        </div>
        <div className="flex items-center gap-2">
          <Gamepad2 size={14} />
          <span className="text-sm text-slate-400">Gamepad:</span>
          <Badge variant={gamepadState.connected ? 'success' : 'warning'}>
            {gamepadState.connected ? 'Active' : 'Not detected'}
          </Badge>
        </div>

        <div className="ml-auto flex items-center gap-2">
          <Switch
            id="debug-mode"
            checked={debugMode}
            onCheckedChange={setDebugMode}
          />
          <Label htmlFor="debug-mode">Debug logs</Label>
        </div>
      </div>

      {!gamepadState.connected ? (
        <Card>
          <CardContent className="py-12 text-center">
            <AlertTriangle size={48} className="mx-auto mb-4 text-amber-400" />
            <h2 className="text-lg font-semibold text-white">Gamepad Not Detected</h2>
            <p className="mt-2 text-sm text-slate-400">
              Connect a gamepad and press any button to activate.
            </p>
            <p className="mt-1 text-xs text-slate-500">
              Target: Retroid Pocket 2S · Enable "Gamepad Mode" in Android settings
            </p>
          </CardContent>
        </Card>
      ) : (
        <div className="grid grid-cols-1 gap-6 sm:grid-cols-2">
          {/* Joystick visualization */}
          <Card>
            <CardHeader><CardTitle>Left Stick</CardTitle></CardHeader>
            <CardContent className="flex flex-col items-center gap-4">
              <JoystickViz x={gamepadState.angularVel} y={gamepadState.linearVel} />
              <div className="text-xs text-slate-500">
                X: {gamepadState.angularVel.toFixed(3)} · Y: {gamepadState.linearVel.toFixed(3)}
              </div>
            </CardContent>
          </Card>

          {/* Velocity outputs */}
          <Card>
            <CardHeader><CardTitle>Velocity Command</CardTitle></CardHeader>
            <CardContent className="flex items-center justify-center gap-8 py-4">
              <VelocityBar
                label="Linear"
                value={gamepadState.linearVel}
                max={1}
                unit="m/s"
              />
              <VelocityBar
                label="Angular"
                value={gamepadState.angularVel}
                max={1}
                unit="rad/s"
                horizontal
              />
            </CardContent>
          </Card>

          {/* Differential drive visualization */}
          <Card className="sm:col-span-2">
            <CardHeader><CardTitle>Differential Drive Preview</CardTitle></CardHeader>
            <CardContent>
              <div className="flex items-center justify-around gap-4">
                {(['Left', 'Right'] as const).map((side) => {
                  const halfBase = 0.075
                  const v =
                    side === 'Left'
                      ? gamepadState.linearVel - gamepadState.angularVel * halfBase
                      : gamepadState.linearVel + gamepadState.angularVel * halfBase
                  const pct = clamp(Math.abs(v) * 50, 0, 100)
                  const forward = v >= 0
                  return (
                    <div key={side} className="flex flex-col items-center gap-2">
                      <span className="text-xs uppercase tracking-wider text-slate-400">{side}</span>
                      <div className="relative h-24 w-6 overflow-hidden rounded-full bg-slate-700">
                        <div
                          className={cn(
                            'absolute left-0 w-full rounded-full transition-all',
                            forward ? 'bg-indigo-500 bottom-1/2' : 'bg-red-500 top-1/2'
                          )}
                          style={{ height: `${pct / 2}%` }}
                        />
                      </div>
                      <span className="font-mono text-xs text-white">{v.toFixed(3)} m/s</span>
                    </div>
                  )
                })}

                <div className="text-center">
                  <div className="text-sm font-medium text-slate-400">Track Width</div>
                  <div className="font-mono text-lg text-white">150 mm</div>
                </div>
              </div>
            </CardContent>
          </Card>

          {/* E-stop */}
          <Card className="sm:col-span-2 border-red-900/40">
            <CardContent className="py-4">
              <div className="flex items-center justify-between">
                <div>
                  <div className="font-semibold text-white">Emergency Stop</div>
                  <div className="text-xs text-slate-400">
                    Immediately zeros all velocity commands to the gateway
                  </div>
                </div>
                <Button variant="destructive" size="lg">
                  E-STOP
                </Button>
              </div>
            </CardContent>
          </Card>
        </div>
      )}
    </div>
  )
}
