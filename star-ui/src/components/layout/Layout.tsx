import { Outlet, useLocation } from 'react-router-dom'
import { Sidebar } from './Sidebar'
import { Header } from './Header'

const ROUTE_TITLES: Record<string, string> = {
  '/': 'Dashboard',
  '/controller': 'Controller',
  '/telemetry': 'Telemetry',
  '/battery': 'Battery Management',
  '/motors': 'Motor Control',
  '/configuration': 'Configuration',
  '/firmware': 'Firmware',
}

export function Layout() {
  const { pathname } = useLocation()
  const title = ROUTE_TITLES[pathname] ?? 'STAR'

  return (
    <div className="flex h-full bg-slate-950">
      <Sidebar />
      <div className="flex flex-1 flex-col overflow-hidden">
        <Header title={title} />
        <main className="flex-1 overflow-auto p-6">
          <Outlet />
        </main>
      </div>
    </div>
  )
}
