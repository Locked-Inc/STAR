import { useGateway } from '@/hooks/useGateway'
import { Badge } from '@/components/ui/badge'
import { Wifi, Battery, Cpu } from 'lucide-react'

export function Header({ title }: { title: string }) {
  const { snapshot } = useGateway()
  const t = snapshot.telemetry

  return (
    <header className="flex h-14 items-center justify-between border-b border-slate-700/50 bg-slate-950/60 px-6">
      <h1 className="text-base font-semibold text-white">{title}</h1>

      <div className="flex items-center gap-4 text-xs text-slate-400">
        {t && (
          <>
            <span className="flex items-center gap-1.5">
              <Battery size={14} className="text-emerald-400" />
              {t.batteryPercent.toFixed(0)}%
            </span>
            <span className="flex items-center gap-1.5">
              <Wifi size={14} className="text-blue-400" />
              {t.wifiSignalDbm} dBm
            </span>
            <span className="flex items-center gap-1.5">
              <Cpu size={14} className="text-violet-400" />
              {t.cpuUsagePercent.toFixed(0)}%
            </span>
          </>
        )}
        <Badge variant="info" className="font-mono text-xs">
          MOCK
        </Badge>
      </div>
    </header>
  )
}
