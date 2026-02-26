// LAYOUT ENGINE: Tiling WM (react-grid-layout) with view-based layout system
import { useEffect, useRef, useState, type ReactNode } from 'react';
import { useSTARConnection } from './hooks/useSTARConnection';
import { StatusBar } from './components/StatusBar';
import { TeleopPanel } from './components/TeleopPanel';
import { MotorPanel } from './components/MotorPanel';
import { BatteryPanel } from './components/BatteryPanel';
import { OdometryPanel } from './components/OdometryPanel';
import { LidarPanel } from './components/LidarPanel';
import { AlertsPanel } from './components/AlertsPanel';
import { PacketAnalyzer } from './components/PacketAnalyzer';
import { ImuPanel } from './components/ImuPanel';
import { SystemHealthPanel } from './components/SystemHealthPanel';
import { CameraFeed } from './components/CameraFeed';
import { GpsPanel } from './components/GpsPanel';
import { TimeSeriesPanel } from './components/TimeSeriesPanel';
import { TransportDiagPanel } from './components/TransportDiagPanel';
import { PidTuningPanel } from './components/PidTuningPanel';
import { FirmwarePanel } from './components/FirmwarePanel';
import { Nav2GoalPanel } from './components/Nav2GoalPanel';
import { DiagLogPanel } from './components/DiagLogPanel';
import { useWindowStore, views } from './store/useWindowStore';
import type { Layout, Layouts } from './store/useWindowStore';
import { useDashboardStore } from './store/dashboardStore';
import type { ControllerState } from './proto/star/v1/controller';

import { ResponsiveGridLayout, useContainerWidth } from 'react-grid-layout';
import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

const WS_PORT: number = Number(import.meta.env.VITE_WS_PORT) || 8080;
const WS_PROTOCOL = window.location.protocol === 'https:' ? 'wss' : 'ws';
const WS_URL = `${WS_PROTOCOL}://${window.location.hostname}:${WS_PORT}/ws`;

const TOPBAR_HEIGHT = 80;
const TOTAL_ROW_UNITS = 18;
const GRID_GAP = 12; // px gap between panels

// --- Panel registry - maps key to component ---
function usePanelRegistry(sendControllerState: (s: ControllerState) => void): Record<string, ReactNode> {
  return {
    movement: <TeleopPanel sendControllerState={sendControllerState} />,
    motors: <MotorPanel />,
    odometry: <OdometryPanel />,
    lidar: <LidarPanel />,
    battery: <BatteryPanel />,
    packet: <PacketAnalyzer />,
    alerts: <AlertsPanel />,
    imu: <ImuPanel />,
    health: <SystemHealthPanel />,
    camera: <CameraFeed />,
    gps: <GpsPanel />,
    timeseries: <TimeSeriesPanel />,
    transport: <TransportDiagPanel />,
    pid: <PidTuningPanel />,
    firmware: <FirmwarePanel />,
    nav2: <Nav2GoalPanel />,
    diaglog: <DiagLogPanel />,
  };
}

function App() {
  const { sendControllerState, sendEStop, sendEStopRelease } = useSTARConnection(WS_URL);
  const {
    activeView,
    viewLayouts,
    maximizedPanel,
    setView,
    updateLayouts,
    toggleMaximize,
    resetLayout,
  } = useWindowStore();
  const connectionState = useDashboardStore(s => s.connectionState);
  const panelRegistry = usePanelRegistry(sendControllerState);

  const containerRef = useRef<HTMLDivElement>(null);
  const [rowHeight, setRowHeight] = useState(64);

  const { width: gridWidth, containerRef: widthRef, mounted: widthMounted } = useContainerWidth();

  const view = views[activeView];
  const isScrollable = view.scrollable ?? false;

  // Responsive row height - only for non-scrollable views
  useEffect(() => {
    if (!containerRef.current || isScrollable) return;

    const dashboardRoot = containerRef.current;
    const ro = new ResizeObserver(([entry]) => {
      const { height } = entry.contentRect;
      // Account for gaps (17 inter-row gaps) + container padding (top + bottom)
      const gapTotal = (TOTAL_ROW_UNITS - 1) * GRID_GAP + 2 * GRID_GAP;
      const newRowHeight = Math.floor((height - TOPBAR_HEIGHT - gapTotal) / TOTAL_ROW_UNITS);
      setRowHeight(Math.max(newRowHeight, 20));
    });

    ro.observe(dashboardRoot);
    return () => ro.disconnect();
  }, [isScrollable]);

  // For scrollable (FULL) view, use a fixed row height
  const effectiveRowHeight = isScrollable ? 48 : rowHeight;

  // Resolve layouts: user overrides -> default from view definition
  const activeLayouts: Layouts = viewLayouts[activeView] ?? view.layouts;

  // Escape key to exit maximize
  useEffect(() => {
    if (!maximizedPanel) return;
    const onKey = (e: KeyboardEvent) => { if (e.key === 'Escape') toggleMaximize(maximizedPanel); };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [maximizedPanel, toggleMaximize]);

  const isLost = connectionState === 'disconnected';

  return (
    <div ref={containerRef} style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      {/* Aurora Ambient Background */}
      <div className="aurora-bg" />

      {/* Global Connection Lost Warning Glow */}
      {isLost && <div className="connection-lost-glow" />}

      {/* Top Fixed Area */}
      <div style={{ zIndex: 1000, height: `${TOPBAR_HEIGHT}px` }}>
        <StatusBar
          sendEStop={sendEStop}
          sendEStopRelease={sendEStopRelease}
          onResetLayout={resetLayout}
          activeView={activeView}
          setView={setView}
        />
      </div>

      {/* Responsive Grid Area */}
      <div ref={widthRef} style={{ flex: 1, overflow: isScrollable ? 'auto' : 'hidden' }}>
        {widthMounted && (
          <ResponsiveGridLayout
            className="layout"
            width={gridWidth}
            layouts={activeLayouts}
            onLayoutChange={(currentLayout, allLayouts) => updateLayouts(currentLayout as Layout[], allLayouts as Layouts)}
            breakpoints={{ lg: 1200, md: 996, sm: 768, xs: 480, xxs: 0 }}
            cols={{ lg: 24, md: 18, sm: 12, xs: 8, xxs: 6 }}
            rowHeight={effectiveRowHeight}
            margin={[GRID_GAP, GRID_GAP] as const}
            containerPadding={[GRID_GAP, GRID_GAP] as const}
            dragConfig={{ enabled: true, bounded: false, handle: '.panel-header' }}
            resizeConfig={{ enabled: true, handles: ['se', 'sw', 'ne', 'nw', 'e', 'w', 's', 'n'] as const }}
          >
            {view.panels.map((key, i) => (
              <div
                key={key}
                className="glass-panel panel-hover-container stagger-fade-in"
                style={{
                  animationDelay: `${0.05 + i * 0.05}s`,
                }}
              >
                {/* Maximize button in header */}
                <button
                  type="button"
                  className="panel-maximize-btn"
                  onClick={() => toggleMaximize(key)}
                  title="Maximize panel"
                  aria-label={`Maximize ${key} panel`}
                >
                  [+]
                </button>
                {panelRegistry[key]}
              </div>
            ))}
          </ResponsiveGridLayout>
        )}
      </div>

      {/* Maximized Panel Overlay */}
      {maximizedPanel && panelRegistry[maximizedPanel] && (
        <div
          className="panel-maximize-overlay"
          role="button"
          tabIndex={0}
          onClick={(e) => { if (e.target === e.currentTarget) toggleMaximize(maximizedPanel); }}
          onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') toggleMaximize(maximizedPanel); }}
        >
          <div className="glass-panel panel-maximize-content">
            <button
              type="button"
              className="panel-maximize-close"
              onClick={() => toggleMaximize(maximizedPanel)}
              title="Exit fullscreen (Esc)"
            >
              X
            </button>
            {panelRegistry[maximizedPanel]}
          </div>
        </div>
      )}
    </div>
  );
}

export default App;
