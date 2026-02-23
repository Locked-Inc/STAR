// LAYOUT ENGINE: Tiling WM (react-grid-layout)
import { useEffect, useRef, useState } from 'react';
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
import { useWindowStore, PRESETS } from './store/useWindowStore';
import type { Layouts } from './store/useWindowStore';
import { useDashboardStore } from './store/dashboardStore';

// react-grid-layout v2 native API
// @ts-ignore
import { ResponsiveGridLayout, useContainerWidth, verticalCompactor } from 'react-grid-layout';
import 'react-grid-layout/css/styles.css';
import 'react-resizable/css/styles.css';

const WS_PORT: number = Number(import.meta.env.VITE_WS_PORT) || 8080;
const WS_PROTOCOL = window.location.protocol === 'https:' ? 'wss' : 'ws';
const WS_URL = `${WS_PROTOCOL}://${window.location.hostname}:${WS_PORT}/ws`;

const TOPBAR_HEIGHT = 80; // approximate height of StatusBar accounting for padding
const TOTAL_ROW_UNITS = 18; // 24x18 grid is what our presets use

function App() {
  const { sendControllerState, sendEStop } = useSTARConnection(WS_URL);
  const { layouts, updateLayouts, applyPreset, resetLayout } = useWindowStore();
  const connectionState = useDashboardStore(s => s.connectionState);

  const containerRef = useRef<HTMLDivElement>(null);
  const [rowHeight, setRowHeight] = useState(64);

  // v2 hook for container width measurement
  const { width: gridWidth, containerRef: widthRef, mounted: widthMounted } = useContainerWidth();

  // Responsive layout row height calculation
  useEffect(() => {
    if (!containerRef.current) return;

    const dashboardRoot = containerRef.current;
    const ro = new ResizeObserver(([entry]) => {
      const { height } = entry.contentRect;
      // Recalculate rowHeight so 18 rows fill the screen perfectly
      const newRowHeight = Math.floor((height - TOPBAR_HEIGHT) / TOTAL_ROW_UNITS);
      setRowHeight(Math.max(newRowHeight, 20)); // Keep a minimum
    });

    ro.observe(dashboardRoot);
    return () => ro.disconnect();
  }, []);

  const handleResetLayout = () => {
    resetLayout();
  };

  const isLost = connectionState === 'disconnected';

  // Fallback to default if store is empty
  const hasLayout = layouts && Object.keys(layouts).length > 0;
  const activeLayouts = hasLayout ? (layouts as unknown as Layouts) : PRESETS.DEFAULT;

  return (
    <div ref={containerRef} style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      {/* Aurora Ambient Background */}
      <div className="aurora-bg" />

      {/* Global Connection Lost Warning Glow */}
      {isLost && <div className="connection-lost-glow" />}

      {/* Top Fixed Area */}
      <div style={{ zIndex: 1000, height: `${TOPBAR_HEIGHT}px` }}>
        <StatusBar sendEStop={sendEStop} onResetLayout={handleResetLayout} applyPreset={applyPreset} />
      </div>

      {/* Responsive Grid Area — v2 uses explicit width prop */}
      <div ref={widthRef} style={{ flex: 1, overflow: 'hidden' }}>
        {widthMounted && (
          <ResponsiveGridLayout
            className="layout"
            width={gridWidth}
            layouts={activeLayouts}
            onLayoutChange={(currentLayout: any, allLayouts: any) => updateLayouts(currentLayout, allLayouts)}
            breakpoints={{ lg: 1200, md: 996, sm: 768, xs: 480, xxs: 0 }}
            cols={{ lg: 24, md: 18, sm: 12, xs: 8, xxs: 6 }}
            rowHeight={rowHeight}
            margin={[0, 0]}
            containerPadding={[0, 0]}
            compactType="vertical"
            isBounded={true}
            isDraggable={true}
            isResizable={true}
            draggableHandle=".panel-header"
            resizeHandles={['se', 'sw', 'ne', 'nw', 'e', 'w', 's', 'n']}
          >
            <div key="movement" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.05s', borderRight: '0.5px solid rgba(255,255,255,0.1)', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <TeleopPanel sendControllerState={sendControllerState} />
            </div>
            <div key="motors" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.1s', borderRight: '0.5px solid rgba(255,255,255,0.1)', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <MotorPanel />
            </div>
            <div key="odometry" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.35s', borderRight: '0.5px solid rgba(255,255,255,0.1)', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <OdometryPanel />
            </div>
            <div key="lidar" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.15s', borderRight: '0.5px solid rgba(255,255,255,0.1)', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <LidarPanel />
            </div>
            <div key="battery" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.25s', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <BatteryPanel />
            </div>
            <div key="packet" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.2s', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <PacketAnalyzer />
            </div>
            <div key="alerts" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.3s' }}>
              <AlertsPanel />
            </div>
            <div key="imu" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.35s', borderRight: '0.5px solid rgba(255,255,255,0.1)', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <ImuPanel />
            </div>
            <div key="health" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.4s', borderRight: '0.5px solid rgba(255,255,255,0.1)', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <SystemHealthPanel />
            </div>
            <div key="camera" className="glass-panel panel-hover-container stagger-fade-in" style={{ animationDelay: '0.45s', borderBottom: '0.5px solid rgba(255,255,255,0.1)' }}>
              <CameraFeed />
            </div>
          </ResponsiveGridLayout>
        )}
      </div>
    </div>
  );
}

export default App;
