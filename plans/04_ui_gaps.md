# UI Gaps: star-ui (TypeScript/React)

## Status Summary

The UI currently only implements gamepad control via WebSocket. This is a proof-of-concept. Everything else is missing — including critical safety features like an emergency stop button. The fundamental problem is that **gRPC services are inaccessible from the browser** without a gRPC-Web bridge.

| Gap | Severity | Effort |
|-----|----------|--------|
| gRPC-Web bridge (or WebSocket endpoints) | CRITICAL | 4-8 hrs |
| Emergency stop button | CRITICAL | 3 hrs |
| Telemetry display | CRITICAL | 10-15 hrs |
| Battery status display | CRITICAL | 5-8 hrs |
| Motor status display | HIGH | 8-12 hrs |
| Configuration panel (PID tuning) | HIGH | 15-20 hrs |
| UI tests (vitest) | HIGH | 10-15 hrs |
| State management (Zustand) | MEDIUM | 4-6 hrs |
| Camera feed | MEDIUM | 8-12 hrs |
| LiDAR visualization | LOW | 15-20 hrs |
| Navigation map | LOW | 20-30 hrs |

**Total Estimated Effort:** 110-175 hours for a complete operator UI

---

## The Core Problem: Browser Cannot Access gRPC

### Current Architecture
```
UI WebSocket ──→ /ws/controller ──→ Gateway (receives)
                 (ONLY endpoint)
```

### What Should Exist
```
UI gRPC-Web ──→ Gateway gRPC:50051
    ├── MotorControlService.StreamEncoders()
    ├── TelemetryService.StreamTelemetry()
    ├── BatteryManagementService.StreamBatteryState()
    └── ConfigurationService.GetConfiguration()
```

gRPC uses HTTP/2 with binary framing — browsers cannot use it directly. **Options:**

### Option A: WebSocket Telemetry Endpoints (Recommended - Simplest)

Add new WebSocket endpoints to the Gateway for streaming:

```go
// In star-gateway/internal/app/gateway.go, add routes:
mux.HandleFunc("/ws/telemetry", s.handleWebSocketTelemetry)
mux.HandleFunc("/ws/battery", s.handleWebSocketBattery)
mux.HandleFunc("/ws/encoders", s.handleWebSocketEncoders)
```

Gateway streams JSON/protobuf over WebSocket to UI.

**Pros:** Minimal new infrastructure, works with existing WebSocket client code.
**Cons:** More WebSocket endpoints to maintain, bidirectional limitation.

### Option B: gRPC-Web Proxy (Envoy)

Add Envoy reverse proxy that translates gRPC-Web (HTTP/1.1) to gRPC (HTTP/2).

```yaml
# docker-compose.yml (new file needed)
services:
  envoy:
    image: envoyproxy/envoy:v1.30
    ports:
      - "8081:8081"
    volumes:
      - ./envoy.yaml:/etc/envoy/envoy.yaml
```

**Pros:** UI can call any gRPC service natively using generated TypeScript clients.
**Cons:** Additional infrastructure, Envoy configuration complexity.

### Option C: REST/JSON HTTP Endpoints

Convert gRPC service calls to REST endpoints in the Gateway.

**Recommendation: Implement Option A first** (WebSocket streaming endpoints). This provides telemetry and battery data quickly. Migrate to gRPC-Web (Option B) when the UI becomes more complex.

---

## Gap 1: Emergency Stop Button (CRITICAL)

### Problem

No emergency stop button exists in the UI. This is a safety-critical gap.

### Implementation

```tsx
// src/components/EmergencyStopButton.tsx
import React, { useState, useCallback } from 'react';
import { ControllerService } from '../services/ControllerService';

interface EmergencyStopButtonProps {
    service: ControllerService;
}

export function EmergencyStopButton({ service }: EmergencyStopButtonProps) {
    const [stopped, setStopped] = useState(false);
    const [confirming, setConfirming] = useState(false);

    const handleEstop = useCallback(async () => {
        if (!confirming) {
            // Require double-click for confirmation
            setConfirming(true);
            setTimeout(() => setConfirming(false), 2000);
            return;
        }

        await service.sendEmergencyStop('User initiated from UI');
        setStopped(true);
    }, [confirming, service]);

    const handleReset = useCallback(async () => {
        await service.sendZeroVelocity();
        setStopped(false);
        setConfirming(false);
    }, [service]);

    return (
        <div className="estop-container">
            {!stopped ? (
                <button
                    className={`estop-button ${confirming ? 'confirming' : ''}`}
                    onClick={handleEstop}
                    aria-label="Emergency Stop"
                >
                    {confirming ? '⚠️ CONFIRM STOP' : '🛑 E-STOP'}
                </button>
            ) : (
                <div className="estop-stopped">
                    <span>🛑 STOPPED</span>
                    <button onClick={handleReset} className="estop-reset">
                        Reset
                    </button>
                </div>
            )}
        </div>
    );
}
```

### Gateway Side (new endpoint or gRPC forwarding)

`ControllerService.sendEmergencyStop()` must call:
- `POST /emergency-stop` (new HTTP endpoint), OR
- Send via WebSocket to gateway
- Gateway forwards to `MotorControlService.EmergencyStop()` RPC

### Estimated Effort: 3-4 hours

---

## Gap 2: Telemetry Display (CRITICAL)

### Problem

There is zero telemetry display. The operator cannot see:
- Motor encoder counts / velocities
- Robot orientation (no IMU data)
- Obstacle distances from HC-SR04 sensors
- System temperature (DS18B20)
- Network/transport link quality

### Implementation Plan

First, add WebSocket telemetry endpoint to Gateway (see Option A above).

Then create the UI component:

```tsx
// src/components/TelemetryDisplay.tsx
import React, { useEffect, useState } from 'react';
import { useTelemetryStream } from '../hooks/useTelemetryStream';

interface MotorStatus {
    motorId: number;
    velocityMps: number;
    encoderTicks: number;
    currentMa: number;
}

interface TelemetryState {
    timestamp: number;
    motors: MotorStatus[];
    temperature_celsius: number;
    sonarDistances: number[];  // 4 HC-SR04 sensors
    linkQuality: number;
    frameDropRate: number;
}

export function TelemetryDisplay() {
    const { telemetry, connected } = useTelemetryStream('ws://localhost:8080/ws/telemetry');

    if (!connected) {
        return <div className="telemetry-disconnected">Connecting to telemetry...</div>;
    }

    return (
        <div className="telemetry-panel">
            <h2>System Telemetry</h2>

            {/* Motor Status */}
            <section className="motors-section">
                <h3>Motors</h3>
                <div className="motor-grid">
                    {telemetry.motors.map(motor => (
                        <MotorCard key={motor.motorId} motor={motor} />
                    ))}
                </div>
            </section>

            {/* Obstacle Detection */}
            <section className="sonar-section">
                <h3>Proximity Sensors</h3>
                <SonarDisplay distances={telemetry.sonarDistances} />
            </section>

            {/* System Status */}
            <section className="system-section">
                <div>Temperature: {telemetry.temperature_celsius.toFixed(1)}°C</div>
                <div>Link Quality: {telemetry.linkQuality}%</div>
                <div>Frame Drop Rate: {(telemetry.frameDropRate * 100).toFixed(2)}%</div>
            </section>
        </div>
    );
}
```

### Hook for WebSocket Streaming

```tsx
// src/hooks/useTelemetryStream.ts
import { useState, useEffect, useRef } from 'react';
import { TelemetryData } from '../proto/star/v1/telemetry';

export function useTelemetryStream(url: string) {
    const [telemetry, setTelemetry] = useState<TelemetryData | null>(null);
    const [connected, setConnected] = useState(false);
    const wsRef = useRef<WebSocket | null>(null);

    useEffect(() => {
        const ws = new WebSocket(url);
        wsRef.current = ws;

        ws.onopen = () => setConnected(true);
        ws.onclose = () => {
            setConnected(false);
            // Reconnect after 1s
            setTimeout(() => { /* reconnect logic */ }, 1000);
        };

        ws.onmessage = (event) => {
            // Parse protobuf or JSON from gateway
            const data = JSON.parse(event.data) as TelemetryData;
            setTelemetry(data);
        };

        return () => ws.close();
    }, [url]);

    return { telemetry, connected };
}
```

### Estimated Effort: 10-15 hours

---

## Gap 3: Battery Status Display (CRITICAL)

### Implementation

```tsx
// src/components/BatteryStatus.tsx
import React from 'react';
import { useBatteryStream } from '../hooks/useBatteryStream';

export function BatteryStatus() {
    const { battery, connected } = useBatteryStream('ws://localhost:8080/ws/battery');

    if (!connected || !battery) {
        return <div className="battery-unknown">Battery: Unknown</div>;
    }

    const soc = battery.stateOfCharge;
    const color = soc > 0.5 ? 'green' : soc > 0.2 ? 'orange' : 'red';

    return (
        <div className={`battery-panel battery-${color}`}>
            <div className="battery-soc">
                <span className="battery-icon">🔋</span>
                <span>{(soc * 100).toFixed(0)}%</span>
            </div>
            <div className="battery-details">
                <div>{(battery.voltage / 1000).toFixed(2)} V</div>
                <div>{battery.current.toFixed(1)} A</div>
                <div>{battery.temperature.toFixed(1)} °C</div>
            </div>
            {soc < 0.1 && (
                <div className="battery-critical">⚠️ CRITICAL LOW BATTERY</div>
            )}
        </div>
    );
}
```

### Estimated Effort: 5-8 hours

---

## Gap 4: Configuration Panel (HIGH)

### Implementation

```tsx
// src/components/ConfigurationPanel.tsx
import React, { useState, useEffect } from 'react';

interface PidGains {
    motorId: number;
    kp: number;
    ki: number;
    kd: number;
}

export function ConfigurationPanel() {
    const [pidGains, setPidGains] = useState<PidGains[]>([
        { motorId: 0, kp: 0.286, ki: 8.01, kd: 0 },  // MATLAB defaults
        { motorId: 1, kp: 0.286, ki: 8.01, kd: 0 },
        { motorId: 2, kp: 0.286, ki: 8.01, kd: 0 },
        { motorId: 3, kp: 0.286, ki: 8.01, kd: 0 },
    ]);
    const [saving, setSaving] = useState(false);

    const handleSave = async (motorId: number) => {
        setSaving(true);
        const gains = pidGains[motorId];

        // POST to gateway (new endpoint needed, or WebSocket)
        const response = await fetch('/api/config/pid', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(gains),
        });

        setSaving(false);
    };

    return (
        <div className="config-panel">
            <h2>Motor Configuration</h2>

            {pidGains.map((gains, idx) => (
                <div key={idx} className="motor-config">
                    <h3>Motor {idx}</h3>
                    <div className="pid-inputs">
                        <label>
                            Kp: <input
                                type="number"
                                value={gains.kp}
                                step={0.001}
                                onChange={e => {
                                    const updated = [...pidGains];
                                    updated[idx] = { ...gains, kp: parseFloat(e.target.value) };
                                    setPidGains(updated);
                                }}
                            />
                        </label>
                        <label>
                            Ki: <input
                                type="number"
                                value={gains.ki}
                                step={0.01}
                                onChange={e => {
                                    const updated = [...pidGains];
                                    updated[idx] = { ...gains, ki: parseFloat(e.target.value) };
                                    setPidGains(updated);
                                }}
                            />
                        </label>
                        <label>
                            Kd: <input
                                type="number"
                                value={gains.kd}
                                step={0.0001}
                                onChange={e => {
                                    const updated = [...pidGains];
                                    updated[idx] = { ...gains, kd: parseFloat(e.target.value) };
                                    setPidGains(updated);
                                }}
                            />
                        </label>
                    </div>
                    <button onClick={() => handleSave(idx)} disabled={saving}>
                        {saving ? 'Saving...' : 'Apply Gains'}
                    </button>
                </div>
            ))}
        </div>
    );
}
```

### Estimated Effort: 15-20 hours

---

## Gap 5: UI Tests (HIGH)

### Problem

Vitest is configured but **zero test files exist**:

```
star-ui/src/test/setup.ts  ✅ (framework configured)
star-ui/src/**/*.test.tsx  ❌ (no test files)
```

### Required Test Coverage

```
tests/
├── components/
│   ├── ControllerView.test.tsx      (gamepad display)
│   ├── EmergencyStopButton.test.tsx (safety-critical)
│   ├── BatteryStatus.test.tsx       (status display)
│   └── TelemetryDisplay.test.tsx    (telemetry)
├── hooks/
│   ├── useGamepad.test.ts           (gamepad API)
│   ├── useControllerConnection.test.ts (WebSocket)
│   └── useTelemetryStream.test.ts   (streaming)
└── services/
    └── ControllerService.test.ts    (WebSocket protocol)
```

### Example Test

```tsx
// src/components/EmergencyStopButton.test.tsx
import { render, screen, fireEvent } from '@testing-library/react';
import { EmergencyStopButton } from './EmergencyStopButton';
import { vi } from 'vitest';

describe('EmergencyStopButton', () => {
    it('requires double-click for confirmation', async () => {
        const mockService = { sendEmergencyStop: vi.fn() };
        render(<EmergencyStopButton service={mockService as any} />);

        const button = screen.getByLabelText('Emergency Stop');

        // First click: confirm mode
        fireEvent.click(button);
        expect(screen.getByText('⚠️ CONFIRM STOP')).toBeTruthy();
        expect(mockService.sendEmergencyStop).not.toHaveBeenCalled();

        // Second click: actually stops
        fireEvent.click(button);
        expect(mockService.sendEmergencyStop).toHaveBeenCalledOnce();
    });

    it('shows reset button after stop', async () => {
        const mockService = {
            sendEmergencyStop: vi.fn().mockResolvedValue(undefined),
        };
        render(<EmergencyStopButton service={mockService as any} />);

        // Double click to stop
        const button = screen.getByLabelText('Emergency Stop');
        fireEvent.click(button);
        fireEvent.click(button);

        expect(screen.getByText('🛑 STOPPED')).toBeTruthy();
        expect(screen.getByText('Reset')).toBeTruthy();
    });
});
```

### Estimated Effort: 10-15 hours

---

## UI Architecture Plan

### State Management (Add Zustand)

```bash
npm install zustand
```

```tsx
// src/store/robotStore.ts
import { create } from 'zustand';
import { TelemetryData } from '../proto/star/v1/telemetry';
import { BatteryState } from '../proto/star/v1/battery_management';

interface RobotStore {
    // Connection state
    gatewayConnected: boolean;
    telemetryConnected: boolean;

    // Robot state (latest from streaming)
    telemetry: TelemetryData | null;
    battery: BatteryState | null;
    isEstopActive: boolean;

    // Actions
    setGatewayConnected: (connected: boolean) => void;
    setTelemetry: (data: TelemetryData) => void;
    setBattery: (data: BatteryState) => void;
    triggerEstop: () => void;
    resetEstop: () => void;
}

export const useRobotStore = create<RobotStore>((set) => ({
    gatewayConnected: false,
    telemetryConnected: false,
    telemetry: null,
    battery: null,
    isEstopActive: false,

    setGatewayConnected: (connected) => set({ gatewayConnected: connected }),
    setTelemetry: (data) => set({ telemetry: data }),
    setBattery: (data) => set({ battery: data }),
    triggerEstop: () => set({ isEstopActive: true }),
    resetEstop: () => set({ isEstopActive: false }),
}));
```

### App Layout

```tsx
// src/App.tsx (new layout with all panels)
function App() {
    return (
        <div className="app-layout">
            {/* Top bar: connection status + E-Stop */}
            <header className="app-header">
                <ConnectionStatus />
                <EmergencyStopButton />
                <BatteryStatus />
            </header>

            {/* Main content */}
            <main className="app-main">
                <aside className="app-sidebar">
                    <TelemetryDisplay />
                    <MotorStatus />
                </aside>

                <section className="app-center">
                    <ControllerView />
                </section>

                <aside className="app-config">
                    <ConfigurationPanel />
                </aside>
            </main>
        </div>
    );
}
```

---

## Complete UI Status

```
star-ui/src/
├── App.tsx                       ✅ Minimal (1 component)
├── components/
│   ├── ControllerView.tsx        ✅ Working (gamepad + velocities)
│   ├── EmergencyStopButton.tsx   ❌ Missing (critical safety)
│   ├── BatteryStatus.tsx         ❌ Missing (critical)
│   ├── TelemetryDisplay.tsx      ❌ Missing (critical)
│   ├── MotorStatus.tsx           ❌ Missing (high priority)
│   ├── ConfigurationPanel.tsx    ❌ Missing (high priority)
│   └── ConnectionStatus.tsx      ❌ Missing (low priority)
├── hooks/
│   ├── useGamepad.ts             ✅ Working
│   ├── useControllerConnection.ts ✅ Working
│   ├── useTelemetryStream.ts     ❌ Missing (needs gateway endpoint)
│   └── useBatteryStream.ts       ❌ Missing (needs gateway endpoint)
├── services/
│   └── ControllerService.ts      ✅ Working (WebSocket)
└── store/
    └── robotStore.ts             ❌ Missing (no state management)
```
