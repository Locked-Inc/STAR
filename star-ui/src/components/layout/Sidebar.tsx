import { NavLink } from 'react-router-dom'
import {
  LayoutDashboard,
  Gamepad2,
  Activity,
  Cog,
  Upload,
  Cpu,
} from 'lucide-react'
import { cn } from '@/lib/utils'
import { useGateway } from '@/hooks/useGateway'

const NAV_ITEMS = [
  { to: '/', icon: LayoutDashboard, label: 'Dashboard', exact: true },
  { to: '/controller', icon: Gamepad2, label: 'Controller' },
  { to: '/telemetry', icon: Activity, label: 'Telemetry' },
  { to: '/motors', icon: Cpu, label: 'Motors' },
  { to: '/configuration', icon: Cog, label: 'Configuration' },
  { to: '/firmware', icon: Upload, label: 'Firmware' },
]

export function Sidebar() {
  const { connected } = useGateway()

  return (
    <aside className="flex w-56 flex-col border-r border-slate-700/50 bg-slate-950/80">
      {/* Logo */}
      <div className="flex h-14 items-center gap-2.5 border-b border-slate-700/50 px-4">
        <div className="flex h-8 w-8 items-center justify-center rounded-lg bg-indigo-600">
          <span className="text-xs font-black text-white">ST</span>
        </div>
        <div>
          <div className="text-sm font-bold text-white">STAR</div>
          <div className="text-xs text-slate-500">Robot Control</div>
        </div>
      </div>

      {/* Navigation */}
      <nav className="flex flex-1 flex-col gap-0.5 p-2">
        {NAV_ITEMS.map(({ to, icon: Icon, label, exact }) => (
          <NavLink
            key={to}
            to={to}
            end={exact}
            className={({ isActive }) =>
              cn(
                'flex items-center gap-3 rounded-md px-3 py-2 text-sm font-medium transition-colors',
                isActive
                  ? 'bg-indigo-600/20 text-indigo-300'
                  : 'text-slate-400 hover:bg-slate-800 hover:text-slate-200'
              )
            }
          >
            <Icon size={16} />
            {label}
          </NavLink>
        ))}
      </nav>

      {/* Status indicator */}
      <div className="border-t border-slate-700/50 p-3">
        <div className="flex items-center gap-2 text-xs">
          <div
            className={cn(
              'h-2 w-2 rounded-full',
              connected ? 'bg-emerald-400 shadow-[0_0_6px_#34d399]' : 'bg-red-500'
            )}
          />
          <span className={connected ? 'text-emerald-400' : 'text-red-400'}>
            {connected ? 'Connected' : 'Disconnected'}
          </span>
        </div>
        <div className="mt-1 text-xs text-slate-600">
          {connected ? 'Mock data active' : 'Waiting…'}
        </div>
      </div>
    </aside>
  )
}
