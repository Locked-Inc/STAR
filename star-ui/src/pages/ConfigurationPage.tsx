import { useState, useEffect } from 'react'
import { useGateway } from '@/hooks/useGateway'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { Badge } from '@/components/ui/badge'
import type { SystemConfiguration, MotorPidConfiguration } from '@/proto/star/v1/configuration'
import { CheckCircle, RotateCcw, Save, AlertCircle } from 'lucide-react'

const MOTOR_LABELS = ['Front-Left', 'Front-Right', 'Back-Left', 'Back-Right']

function NumberField({
  label,
  value,
  onChange,
  min,
  max,
  step = 0.001,
  unit,
}: {
  label: string
  value: number
  onChange: (v: number) => void
  min?: number
  max?: number
  step?: number
  unit?: string
}) {
  return (
    <div className="space-y-1">
      <Label>{label}{unit ? ` (${unit})` : ''}</Label>
      <Input
        type="number"
        value={value}
        min={min}
        max={max}
        step={step}
        onChange={(e) => onChange(parseFloat(e.target.value) || 0)}
      />
    </div>
  )
}

function PidEditor({
  motorId,
  config,
  onChange,
  onSave,
  saving,
}: {
  motorId: number
  config: MotorPidConfiguration
  onChange: (pid: MotorPidConfiguration) => void
  onSave: () => Promise<void>
  saving: boolean
}) {
  const pid = config.pidConfig ?? { kp: 0, ki: 0, kd: 0, outputMinPercent: -100, outputMaxPercent: 100, integralMin: -50, integralMax: 50 }

  function updatePid(key: keyof typeof pid, value: number) {
    onChange({ ...config, pidConfig: { ...pid, [key]: value } })
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center justify-between">
          <span>{MOTOR_LABELS[motorId] ?? `Motor ${motorId}`}</span>
          <Badge variant="default">ID {motorId}</Badge>
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-4">
        <div className="grid grid-cols-3 gap-4">
          <NumberField label="Kp" value={pid.kp} onChange={(v) => updatePid('kp', v)} min={0} max={100} step={0.01} />
          <NumberField label="Ki" value={pid.ki} onChange={(v) => updatePid('ki', v)} min={0} max={100} step={0.01} />
          <NumberField label="Kd" value={pid.kd} onChange={(v) => updatePid('kd', v)} min={0} max={100} step={0.001} />
        </div>
        <div className="grid grid-cols-2 gap-4">
          <NumberField
            label="Output Min"
            value={pid.outputMinPercent}
            onChange={(v) => updatePid('outputMinPercent', v)}
            min={-100}
            max={0}
            step={1}
            unit="%"
          />
          <NumberField
            label="Output Max"
            value={pid.outputMaxPercent}
            onChange={(v) => updatePid('outputMaxPercent', v)}
            min={0}
            max={100}
            step={1}
            unit="%"
          />
        </div>
        <Button className="w-full" onClick={onSave} disabled={saving}>
          <Save size={14} />
          {saving ? 'Saving…' : 'Save PID Config'}
        </Button>
      </CardContent>
    </Card>
  )
}

type SaveState = 'idle' | 'saving' | 'success' | 'error'

export function ConfigurationPage() {
  const { client } = useGateway()
  const [config, setConfig] = useState<SystemConfiguration | null>(null)
  const [pidConfigs, setPidConfigs] = useState<MotorPidConfiguration[]>([])
  const [loading, setLoading] = useState(true)
  const [saveState, setSaveState] = useState<SaveState>('idle')
  const [errorMsg, setErrorMsg] = useState('')
  const [pidSaving, setPidSaving] = useState<Set<number>>(new Set())

  useEffect(() => {
    void loadConfig()
  }, [])

  async function loadConfig() {
    setLoading(true)
    try {
      const cfg = await client.getConfiguration()
      setConfig(cfg)
      setPidConfigs(cfg.motorConfigs ?? [])
    } finally {
      setLoading(false)
    }
  }

  async function saveConfig() {
    if (!config) return
    setSaveState('saving')
    try {
      await client.setConfiguration(config)
      setSaveState('success')
      setTimeout(() => setSaveState('idle'), 2000)
    } catch (e) {
      setErrorMsg(e instanceof Error ? e.message : 'Unknown error')
      setSaveState('error')
    }
  }

  async function resetConfig() {
    setSaveState('saving')
    try {
      await client.resetConfiguration()
      await loadConfig()
      setSaveState('success')
      setTimeout(() => setSaveState('idle'), 2000)
    } catch (e) {
      setErrorMsg(e instanceof Error ? e.message : 'Unknown error')
      setSaveState('error')
    }
  }

  async function savePid(motorId: number) {
    const pidConfig = pidConfigs.find((p) => p.motorId === motorId)
    if (!pidConfig) return
    setPidSaving((prev) => new Set(prev).add(motorId))
    try {
      await client.setMotorPidConfig(motorId, pidConfig)
    } finally {
      setPidSaving((prev) => {
        const next = new Set(prev)
        next.delete(motorId)
        return next
      })
    }
  }

  if (loading) {
    return (
      <div className="flex h-64 items-center justify-center text-slate-500">
        Loading configuration…
      </div>
    )
  }

  const safetyThresholds = config?.safetyThresholds
  const timingConfig = config?.timingConfig

  return (
    <div className="space-y-6">
      {/* Save/reset toolbar */}
      <div className="flex items-center gap-3">
        <Button onClick={saveConfig} disabled={saveState === 'saving'}>
          <Save size={14} />
          {saveState === 'saving' ? 'Saving…' : 'Save All'}
        </Button>
        <Button variant="outline" onClick={resetConfig} disabled={saveState === 'saving'}>
          <RotateCcw size={14} />
          Reset to Defaults
        </Button>
        {saveState === 'success' && (
          <span className="flex items-center gap-1.5 text-sm text-emerald-400">
            <CheckCircle size={14} /> Saved
          </span>
        )}
        {saveState === 'error' && (
          <span className="flex items-center gap-1.5 text-sm text-red-400">
            <AlertCircle size={14} /> {errorMsg}
          </span>
        )}
      </div>

      <Tabs defaultValue="pid">
        <TabsList>
          <TabsTrigger value="pid">PID Gains</TabsTrigger>
          <TabsTrigger value="safety">Safety Thresholds</TabsTrigger>
          <TabsTrigger value="timing">Timing</TabsTrigger>
        </TabsList>

        {/* PID Configuration */}
        <TabsContent value="pid">
          <div className="grid grid-cols-1 gap-4 lg:grid-cols-2">
            {pidConfigs.map((pc) => (
              <PidEditor
                key={pc.motorId}
                motorId={pc.motorId}
                config={pc}
                onChange={(updated) => {
                  setPidConfigs((prev) => prev.map((p) => (p.motorId === updated.motorId ? updated : p)))
                }}
                onSave={() => savePid(pc.motorId)}
                saving={pidSaving.has(pc.motorId)}
              />
            ))}
            {pidConfigs.length === 0 && (
              <div className="col-span-2 py-8 text-center text-slate-500">
                No motor PID configurations available
              </div>
            )}
          </div>
        </TabsContent>

        {/* Safety thresholds */}
        <TabsContent value="safety">
          <Card>
            <CardHeader><CardTitle>Safety Thresholds</CardTitle></CardHeader>
            <CardContent className="grid grid-cols-2 gap-4 lg:grid-cols-3">
              {safetyThresholds ? (
                <>
                  <NumberField
                    label="Overcurrent Threshold"
                    value={safetyThresholds.overcurrentThresholdMa}
                    onChange={(v) =>
                      setConfig((c) =>
                        c ? { ...c, safetyThresholds: { ...safetyThresholds, overcurrentThresholdMa: v } } : c
                      )
                    }
                    min={500}
                    max={20000}
                    step={100}
                    unit="mA"
                  />
                  <NumberField
                    label="Thermal Shutdown"
                    value={safetyThresholds.thermalShutdownDeciCelsius}
                    onChange={(v) =>
                      setConfig((c) =>
                        c ? { ...c, safetyThresholds: { ...safetyThresholds, thermalShutdownDeciCelsius: v } } : c
                      )
                    }
                    min={400}
                    max={900}
                    step={10}
                    unit="deci-°C"
                  />
                  <NumberField
                    label="Cell Imbalance"
                    value={safetyThresholds.cellImbalanceThresholdMv}
                    onChange={(v) =>
                      setConfig((c) =>
                        c ? { ...c, safetyThresholds: { ...safetyThresholds, cellImbalanceThresholdMv: v } } : c
                      )
                    }
                    min={20}
                    max={500}
                    step={10}
                    unit="mV"
                  />
                </>
              ) : (
                <div className="col-span-3 text-slate-500">No safety threshold data</div>
              )}
            </CardContent>
          </Card>
        </TabsContent>

        {/* Timing */}
        <TabsContent value="timing">
          <Card>
            <CardHeader><CardTitle>Timing Configuration</CardTitle></CardHeader>
            <CardContent className="grid grid-cols-2 gap-4 lg:grid-cols-3">
              {timingConfig ? (
                <>
                  <NumberField
                    label="Motor Control Period"
                    value={timingConfig.motorControlPeriodMs}
                    onChange={(v) =>
                      setConfig((c) => c ? { ...c, timingConfig: { ...timingConfig, motorControlPeriodMs: v } } : c)
                    }
                    min={1}
                    max={100}
                    step={1}
                    unit="ms"
                  />
                  <NumberField
                    label="Telemetry Period"
                    value={timingConfig.telemetryPeriodMs}
                    onChange={(v) =>
                      setConfig((c) => c ? { ...c, timingConfig: { ...timingConfig, telemetryPeriodMs: v } } : c)
                    }
                    min={10}
                    max={1000}
                    step={10}
                    unit="ms"
                  />
                  <NumberField
                    label="Comm Timeout"
                    value={timingConfig.communicationTimeoutMs}
                    onChange={(v) =>
                      setConfig((c) => c ? { ...c, timingConfig: { ...timingConfig, communicationTimeoutMs: v } } : c)
                    }
                    min={100}
                    max={5000}
                    step={100}
                    unit="ms"
                  />
                  <NumberField
                    label="BMS Poll Period"
                    value={timingConfig.bmsPollPeriodMs}
                    onChange={(v) =>
                      setConfig((c) => c ? { ...c, timingConfig: { ...timingConfig, bmsPollPeriodMs: v } } : c)
                    }
                    min={100}
                    max={10000}
                    step={100}
                    unit="ms"
                  />
                </>
              ) : (
                <div className="col-span-3 text-slate-500">No timing configuration data</div>
              )}
            </CardContent>
          </Card>
        </TabsContent>
      </Tabs>
    </div>
  )
}
