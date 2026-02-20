import { BrowserRouter, Routes, Route } from 'react-router-dom'
import { GatewayProvider } from '@/components/GatewayProvider'
import { Layout } from '@/components/layout/Layout'
import { DashboardPage } from '@/pages/DashboardPage'
import { ControllerPage } from '@/pages/ControllerPage'
import { TelemetryPage } from '@/pages/TelemetryPage'
import { MotorsPage } from '@/pages/MotorsPage'
import { ConfigurationPage } from '@/pages/ConfigurationPage'
import { FirmwarePage } from '@/pages/FirmwarePage'

function App() {
  return (
    <BrowserRouter>
      <GatewayProvider>
        <Routes>
          <Route path="/" element={<Layout />}>
            <Route index element={<DashboardPage />} />
            <Route path="controller" element={<ControllerPage />} />
            <Route path="telemetry" element={<TelemetryPage />} />
            <Route path="motors" element={<MotorsPage />} />
            <Route path="configuration" element={<ConfigurationPage />} />
            <Route path="firmware" element={<FirmwarePage />} />
          </Route>
        </Routes>
      </GatewayProvider>
    </BrowserRouter>
  )
}

export default App
