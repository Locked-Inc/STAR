# STAR UI Backend -- Software Design Document

---

## Document Control

|Field|Value|
|---|---|
|**Document Title**|STAR UI Backend -- WebSocket & Dashboard Architecture|
|**Version**|1.0 (Final)|
|**Phase**|Software Architecture Design|
|**Revised**|2026-02-18|
|**Applies To**|`star-gateway`, `star-ui`, `star-proto`, `star-ros2`|

---

## Table of Contents

1. Overview
2. Goals and Non-Goals
3. Glossary
4. Current System State
5. Architecture Decision Records
6. System Architecture
7. Protocol Specification
8. Gateway WebSocket Hub (Go)
9. GatewayService Modifications (Go)
10. REST Endpoints
11. ROS2 Changes
12. Frontend Architecture
13. Error Handling
14. Interface Contracts and Invariants
15. Dependencies
16. Implementation Work Order
17. Testing Strategy
18. Future Work

---

## 1. Overview

STAR is a four-wheeled mobile robot platform controlled via an RPi5 running ROS2 Humble. An existing React/TypeScript UI communicates with a Go gateway over WebSocket. Currently the UI only sends gamepad commands (50 Hz) over `/ws/controller` -- there is no telemetry path from the robot back to the browser.

This document specifies the complete backend architecture for the UI dashboard: a multiplexed bidirectional WebSocket that carries all robot data (motor telemetry, battery state, odometry, LiDAR scans, alerts) from the gateway to the browser, and all control commands (joystick, E-stop) from the browser to the gateway. It covers the Go WebSocket hub, the protobuf envelope format, the React state management layer, the live charting approach, and the packet analyzer debug tool.

---

## 2. Goals and Non-Goals

## Goals

- Replace the single-purpose `/ws/controller` endpoint with one multiplexed `/ws` endpoint carrying all UI<->Gateway traffic
    
- Push all robot telemetry types to the UI at appropriate rates (<=10 Hz sensor data, <=2.5 Hz LiDAR, immediate alerts)
    
- Display motor telemetry, battery state, odometry, LiDAR scan, alerts, and manual control in an MVP dashboard
    
- Provide a packet analyzer debug panel showing the last 100 raw WebSocket frames with decoded previews
    
- Keep the SPI firmware path (RX72N <-> star_spi_bridge) completely untouched
    
- Keep the gRPC interface (star_gateway_bridge <-> star-gateway) functionally unchanged for MVP
    

## Non-Goals

- Production security hardening (CORS, TLS, auth) -- local network deployment only
    
- gRPC streaming migration -- deferred to post-MVP
    
- PID tuning controls -- deferred to post-MVP
    
- CSS polish and Tailwind migration -- deferred to post-MVP
    
- Multi-robot support
    

---

## 3. Glossary

| Term              | Definition                                                                              |
| ----------------- | --------------------------------------------------------------------------------------- |
| **Envelope**      | `STAREnvelope` -- the single protobuf wrapper message that carries every WebSocket frame |
| **Hub**           | Go struct that manages all connected WebSocket clients and fans out outbound frames     |
| **readPump**      | Per-client goroutine that owns all reads from a WebSocket connection                    |
| **writePump**     | Per-client goroutine that owns all writes to a WebSocket connection                     |
| **SoA**           | Structure-of-Arrays -- the layout used in `LidarScan` for packed float fields            |
| **seq**           | Monotonic sequence number stamped by the Hub on every outbound envelope                 |
| **Throttle**      | Hub-side rate limiter that drops excess frames per data type before broadcast           |
| **Ring buffer**   | Fixed-size circular array used for uPlot time-series and packet analyzer                |
| **Scratch array** | Pre-allocated typed array used to unwrap a ring buffer into contiguous order for uPlot  |

---

## 4. Current System State

## What Exists and Works

```text
UI (React/TypeScript)
  ^v WebSocket ws://host:8080/ws/controller -- binary Protobuf ControllerState at 50Hz
Go Gateway (port 8080 HTTP/WS, port 50051 gRPC)
  ^v gRPC unary localhost:50051
star_gateway_bridge (ROS2 node)
  ^v ROS2 topics (/cmd_vel, /battery_state, /robot_status, etc.)
star_spi_bridge (ROS2 node)
  ^v SPI 10 MHz, nanopb Protobuf
RX72N Firmware (motor controller)
```

## What Is Missing

- The Gateway has no telemetry push path to the browser
    
- The UI has no dashboard -- only a gamepad input view
    
- `GatewayService.ForwardTelemetry()` updates an in-memory cache but nothing reads from it for the UI
    
- No reconnection logic, no connection state, no stale-data indication in the UI
    

---

## 5. Architecture Decision Records

|#|Decision|Choice|Rationale|
|---|---|---|---|
|ADR-01|UI<->Gateway transport|Single WebSocket `/ws`|One reconnect loop, one handler, one binary framing path. Browser cannot do native gRPC client-streaming.|
|ADR-02|Wire format|Binary Protobuf `STAREnvelope` with `oneof`|Type-safe discriminated union in TypeScript, no double serialization, `oneof` enforces exactly-one-payload invariant|
|ADR-03|Go WebSocket library|`gorilla/websocket` v1.5.3 (pinned)|Unarchived July 2023 with active maintainers; fine for <10 local clients; already in the ecosystem|
|ADR-04|Hub broadcast model|Event-driven push on ROS2 update, per-type throttle using a typed struct|No stale-data ticker; each data type throttled independently via named struct fields -- zero reflection, zero map allocation|
|ADR-05|Slow client policy|Non-blocking send into 32-message per-client buffer; drop frame, never disconnect|Telemetry is latest-value-wins; disconnecting on slow tab would require user to reload|
|ADR-06|Alerts and E-stop|Always immediate, bypass throttle entirely|Alerts must not be delayed by a co-arriving 10 Hz telemetry burst|
|ADR-07|React state management|Zustand with individual selectors, one `set()` per switch case|Prevents cross-slice re-renders; no object-allocation anti-pattern|
|ADR-08|LiDAR in Zustand|Keep field in store for MVP; `LidarPanel` subscribes via `.subscribe()` + `useRef` to bypass React render cycle|At 2.5 Hz, selector evaluation cost is negligible; `useRef` pattern eliminates the React VDOM cycle for the canvas|
|ADR-09|Charting|uPlot + pre-allocated `Float64Array` ring buffers and scratch arrays outside React state|Canvas-based, imperative `.setData()`, no React re-render per tick; smallest bundle of all candidates|
|ADR-10|LiDAR visualization|Raw Canvas 2D API, no library|Polar->Cartesian transform is trivial; avoids any dependency for a single panel|
|ADR-11|WebSocket reconnect|Exponential backoff with jitter, stale-data indicator on disconnect|Standard pattern for robotics dashboards over local network|
|ADR-12|E-stop path|Synchronous call in `readPump` bypassing Hub inbound queue + independent REST fallback `POST /api/estop`|`readPump` block during E-stop is desired; REST fallback handles WS-disconnected case|
|ADR-13|E-stop context|`context.Background()` independent of connection lifetime|HTTP request context or WS connection context may be cancelled during network hiccup -- E-stop must never be cancelled|
|ADR-14|gRPC (ROS2<->Gateway)|Keep unary `ForwardTelemetry` for MVP|Existing bridge works; streaming migration adds risk with no MVP benefit|
|ADR-15|ROS2 QoS|`BEST_EFFORT` + `keep_last:1` for telemetry; `RELIABLE` for commands|Sensor data is latest-value-wins; commands must not be lost|
|ADR-16|HTTP server `WriteTimeout`|Not an issue -- `HandshakeTimeout: 0` (gorilla default) causes gorilla to call `netConn.SetDeadline(time.Time{})` unconditionally after upgrade, clearing any server-level deadline|Verified against gorilla `server.go` source|
|ADR-17|Ping mechanism|`WriteMessage(websocket.PingMessage, nil)` inside `writePump`, never `WriteControl`|Gorilla issue #841: `WriteControl` permanently mutates `WriteDeadline`; `WriteMessage` respects the write mutex correctly|
|ADR-18|`ws.binaryType`|Must be set to `'arraybuffer'` immediately after `new WebSocket()`|Browser default is `'blob'`; `Uint8Array` constructor cannot accept a `Blob`, which would silently break all protobuf decoding|
|ADR-19|SPI / firmware|`STAREnvelope` is UI-layer only, never serialized over SPI|nanopb path uses flat messages; this envelope is a UI transport concern only|
|ADR-20|CSS/styling|Plain CSS for MVP|No new tooling; Tailwind deferred to post-MVP polish phase|

---

## 6. System Architecture

## 6.1 Component Diagram

```text
+------------------------------------------------------------------+
|  star-ui  (React 19 + TypeScript, Vite, served on :5173 dev)    |
|                                                                   |
|  useSTARConnection (single WS hook)                              |
|    binaryType = 'arraybuffer'  <- MUST be set before first msg   |
|    +-- sends: ControllerState (50Hz, seq=0)                      |
|    +-- sends: EStopCommand    (on demand, seq=0)                 |
|    +-- receives: STAREnvelope (<=10Hz / <=2.5Hz, seq>0)           |
|                                                                   |
|  Zustand (useDashboardStore)                                     |
|    +-- connectionState, lastSeq, seqGapDetected, dataIsStale     |
|    +-- eStopActive                                               |
|    +-- telemetry, motors[4], battery, odometry, systemStatus     |
|    +-- lidarScan (store field; LidarPanel uses .subscribe+ref)   |
|    +-- alerts[]  (capped at 200)                                 |
|                                                                   |
|  Components                                                       |
|    StatusBar * TeleopPanel * MotorPanel * BatteryPanel           |
|    OdometryPanel * LidarPanel * AlertsPanel * PacketAnalyzer     |
+------------------------------------------------------------------+
         |  ws://host:8080/ws  (binary Protobuf, STAREnvelope)
         |  fetch POST /api/estop  (E-stop fallback)
+------------------------------------------------------------------+
|  star-gateway  (Go, :8080 HTTP/WS, :50051 gRPC)                 |
|                                                                   |
|  /ws  ->  ws.NewHandler(hub, motorService, logger)                |
|  POST /api/estop  ->  estopHandler(motorService)                  |
|  GET  /healthz   ->  (unchanged)                                  |
|                                                                   |
|  Hub  (internal/ws/hub.go)                                       |
|    clients map[*Client]struct{}                                  |
|    register / unregister channels                                |
|    broadcast chan *STAREnvelope  (buffered 32, from GWSvc)       |
|    inbound   chan *STAREnvelope  (buffered 32, UI->GW)            |
|    seq  uint64  (atomic, outbound only)                          |
|    throttle  typeThrottleState  (named struct, zero reflection)  |
|                                                                   |
|  Client  (internal/ws/client.go)                                 |
|    readPump   -- owns all reads; E-stop synchronous bypass        |
|    writePump  -- ONLY writer; ping ticker; SetWriteDeadline       |
|                                                                   |
|  GatewayService  (internal/service/gateway_service.go)          |
|    ForwardTelemetry() -- unary RPC, calls hub.Broadcast() per    |
|    data type after updating cache                                |
|    HubNotifier interface (for testability)                       |
+------------------------------------------------------------------+
         |  gRPC unary :50051
+------------------------------------------------------------------+
|  star-ros2  (ROS2 Humble)                                        |
|                                                                  |
|  star_gateway_bridge_node                                        |
|    ForwardTelemetry() -- unary, sends telemetry to Gateway        |
|    GetTeleopCommand() -- polls command cache                      |
|    QoS: BEST_EFFORT keep_last:1  (telemetry subs)                |
|         RELIABLE    keep_last:10 (command pubs)                  |
|                                                                  |
|  star_spi_bridge_node  -- UNTOUCHED BY THIS WORK                  |
+------------------------------------------------------------------+
         |  SPI 10 MHz, nanopb Protobuf  (UNTOUCHED)
+------------------------------------------------------------------+
|  RX72N Firmware  (motor controller)  -- UNTOUCHED                 |
+------------------------------------------------------------------+

```
## 6.2 Data Flow -- Control Command (UI -> Robot)

```text
1. User moves gamepad stick
2. useSTARConnection encodes ControllerState into STAREnvelope (seq=0)
3. ws.send(binaryFrame)
4. readPump receives frame, proto.Unmarshal -> STAREnvelope
5. Routes to hub.inbound channel
6. GatewayService polls hub.inbound, writes to command cache
7. star_gateway_bridge calls GetTeleopCommand() at 50Hz
8. Publishes Twist to /teleop/cmd_vel (RELIABLE QoS)
9. star_spi_bridge reads /cmd_vel, formats nanopb packet, writes SPI
10. RX72N executes motor command
```

## 6.3 Data Flow -- Telemetry (Robot -> UI)

```text
1. RX72N sends telemetry over SPI at 10Hz
2. star_spi_bridge decodes nanopb, publishes to ROS2 topics
3. star_gateway_bridge subscribes (BEST_EFFORT, keep_last:1)
4. star_gateway_bridge calls ForwardTelemetry() gRPC unary RPC
5. GatewayService updates in-memory cache
6. GatewayService calls hub.Broadcast() once per data type present
7. Hub run loop receives envelope from broadcast channel
8. Hub applies per-type throttle (typed struct, no reflection)
9. Hub stamps seq (atomic), ts_ms, calls proto.Marshal once
10. Hub fans out []byte to each client.send channel (non-blocking)
11. writePump drains client.send, calls SetWriteDeadline, WriteMessage
12. Browser receives binary frame
13. useSTARConnection: raw.byteLength captured, fromBinary() decodes
14. recordPacket() stores in useRef ring buffer (100 entries)
15. updateFromEnvelope() dispatches: one set() per oneofKind case
16. Zustand notifies only selectors for the updated field
17. Component re-renders (e.g. BatteryPanel if battery arrived)
```

## 6.4 Data Flow -- E-Stop (UI -> Robot, Priority Path)

```text
WebSocket path (primary):
1. User clicks E-stop button in StatusBar
2. GatewayService.ts encodes EStopCommand in STAREnvelope, ws.send()
3. readPump decodes, detects EStopCommand oneofKind
4. readPump calls motorService.EmergencyStop(reason) SYNCHRONOUSLY
   (context.Background() -- never cancelled)
5. readPump blocks until EmergencyStop returns
   (joystick commands do not process during this window -- desired)
6. readPump forwards envelope to hub.inbound for packet analyzer visibility

REST fallback (if WS disconnected):
1. useSTARConnection detects WS is not OPEN
2. fetch('POST /api/estop?reason=user_ui_button')
3. estopHandler calls motorService.EmergencyStop(reason)
```

---

## 7. Protocol Specification

## 7.1 `STAREnvelope` -- Complete Proto Definition

```protobuf
// star/v1/ui.proto
// UI transport layer ONLY. Never serialized over SPI.
// Applies to: star-gateway <-> star-ui WebSocket boundary.
syntax = "proto3";
package star.v1;

import "star/v1/telemetry.proto";
import "star/v1/motor.proto";
import "star/v1/battery.proto";

// STAREnvelope is the single multiplexed message for the /ws WebSocket.
//
// Field numbering rationale:
//   Fields 1-15 encode with a 1-byte wire tag (varint field number + type).
//   oneof payload uses 1-9 (most frequent, smallest encoding).
//   seq (14) and ts_ms (15) use the last two 1-byte slots.
//
// Sequence number contract:
//   Gateway -> UI: Hub atomically increments seq for every outbound envelope.
//   UI -> Gateway: Always seq=0. Gap detection ignores UI-originated envelopes.
message STAREnvelope {
  uint64 seq   = 14;   // Gateway-assigned monotonic counter
  int64  ts_ms = 15;   // Gateway wall-clock timestamp at send (ms)

  oneof payload {
    // Gateway -> UI  (high-frequency, 1-byte field tags)
    TelemetryData   telemetry  = 1;
    MotorStatusList motor      = 2;
    BatteryState    battery    = 3;
    OdometryData    odometry   = 4;
    LidarScan       lidar      = 5;
    Alert           alert      = 6;
    SystemStatus    system     = 7;

    // UI -> Gateway
    ControllerState controller = 8;
    EStopCommand    estop      = 9;
  }
}

// -- New messages --------------------------------------------------

// MotorStatusList wraps the repeated MotorStatus for oneof compatibility.
// Index contract: 0=FL, 1=FR, 2=BL, 3=BR. Always 4 entries.
message MotorStatusList {
  repeated MotorStatus motors = 1;
}

// OdometryData carries integrated pose and velocity from the robot.
message OdometryData {
  double x_m       = 1;   // X position in meters (robot frame)
  double y_m       = 2;   // Y position in meters (robot frame)
  double theta_rad = 3;   // Heading in radians
  double vx_mps   = 4;   // Forward velocity m/s
  double wz_rps   = 5;   // Angular velocity rad/s
}

// LidarScan uses Structure-of-Arrays layout for packed-float wire efficiency.
// INVARIANT: angles_rad.length == distances_m.length == qualities.length.
// TypeScript decoder MUST validate this before rendering.
message LidarScan {
  repeated float angles_rad  = 1 [packed=true];
  repeated float distances_m = 2 [packed=true];
  repeated float qualities   = 3 [packed=true];  // 0-255 signal quality
}

// Alert carries a timestamped event from any subsystem.
message Alert {
  AlertLevel level   = 1;
  string     source  = 2;   // "safety_monitor" | "battery" | "motor" | "ws"
  string     message = 3;
  int64      ts_ms   = 4;
}

enum AlertLevel {
  ALERT_LEVEL_UNKNOWN = 0;
  ALERT_LEVEL_INFO    = 1;
  ALERT_LEVEL_WARN    = 2;
  ALERT_LEVEL_ERROR   = 3;
  ALERT_LEVEL_ESTOP   = 4;
}

// EStopCommand is UI-initiated only. The reason string is for logging.
message EStopCommand {
  string reason = 1;   // e.g. "user_ui_button" | "user_http_fallback"
}
```

## 7.2 Wire Format

Every WebSocket frame is a single serialized `STAREnvelope`. There is no length-prefix, no frame header, and no JSON. The WebSocket frame boundary itself delimits the message. The Go side uses `proto.Marshal` / `proto.Unmarshal`. The TypeScript side uses `STAREnvelope.fromBinary(new Uint8Array(event.data))` where `event.data` is an `ArrayBuffer` (enforced by `ws.binaryType = 'arraybuffer'`).

## 7.3 Sequence Number Contract

|Direction|seq value|Purpose|
|---|---|---|
|Gateway -> UI|Atomic increment, starts at 1|Gap detection in UI|
|UI -> Gateway|Always `0`|Gap detection ignores these|

Gap detection logic in the UI:

```typescript
// In useSTARConnection.ts, on every received envelope:
const raw = event.data as ArrayBuffer;
const byteLength = raw.byteLength;  // capture BEFORE decode
const env = STAREnvelope.fromBinary(new Uint8Array(raw));

const { lastSeq } = store.getState();
if (env.seq > 0 && env.seq > lastSeq + 1) {
    store.addAlert({
        level: AlertLevel.WARN,
        source: 'ws',
        message: `Sequence gap: ${lastSeq} -> ${env.seq}`,
        tsMs: BigInt(Date.now()),
    });
}
// Always update lastSeq -- even on gap, to prevent infinite re-alerting
store.updateFromEnvelope(env);  // updates lastSeq inside
recordPacket(env, 'rx', byteLength);
```

## 7.4 Throttle Rates

|Message Type|Max Rate|Throttle Type|
|---|---|---|
|`telemetry`|<=10 Hz|per-type timestamp struct|
|`motor`|<=10 Hz|per-type timestamp struct|
|`battery`|<=10 Hz|per-type timestamp struct|
|`odometry`|<=10 Hz|per-type timestamp struct|
|`system`|<=10 Hz|per-type timestamp struct|
|`lidar`|<=2.5 Hz|per-type timestamp struct|
|`alert`|Unlimited|**No throttle -- always immediate**|
|`estop`|Unlimited|**No throttle -- always immediate**|
|`controller` (inbound)|50 Hz|No Hub throttle; Gateway cache overwrites|

---

## 8. Gateway WebSocket Hub (Go)

## 8.1 File Structure

```
star-gateway/internal/ws/
  hub.go        -- Hub struct, run loop, per-type throttle, stampAndFanOut
  client.go     -- Client struct, readPump, writePump
  handler.go    -- http.Handler that upgrades HTTP -> WS, creates Client
```

## 8.2 Interfaces

```go
// internal/ws/interfaces.go

// HubNotifier is the minimal interface GatewayService uses to push to the Hub.
// The real Hub implements this. Tests use a mock.
type HubNotifier interface {
    Broadcast(env *starv1.STAREnvelope)
}

// MotorController is the interface for motor safety operations.
// Implemented by the real MotorControlService; mocked in tests.
// The context enables timeout enforcement per ADR-13: call sites use
// context.WithTimeout(context.Background(), estopTimeout) so the motor
// controller cannot stall readPump indefinitely, while context.Background()
// as the root ensures connection-lifecycle cancellation cannot abort a
// safety-critical e-stop in flight.
type MotorController interface {
    EmergencyStop(ctx context.Context, reason string) error
}
```
## 8.3 Hub

```go
// internal/ws/hub.go

// typeThrottleState holds the last-broadcast time per throttled message type.
// Named struct fields eliminate reflection and map allocations in the hot path.
type typeThrottleState struct {
    telemetry time.Time
    motor     time.Time
    battery   time.Time
    odometry  time.Time
    system    time.Time
    lidar     time.Time
}

type Hub struct {
    clients    map[*Client]struct{}
    register   chan *Client
    unregister chan *Client
    broadcast  chan *starv1.STAREnvelope  // buffered 32, from GatewayService
    inbound    chan *starv1.STAREnvelope  // buffered 32, UI->GW messages

    seq          uint64         // atomic; outbound envelopes only
    motorService MotorController
    logger       *slog.Logger
}

func NewHub(motorService MotorController, logger *slog.Logger) *Hub {
    return &Hub{
        clients:      make(map[*Client]struct{}),
        register:     make(chan *Client),
        unregister:   make(chan *Client),
        broadcast:    make(chan *starv1.STAREnvelope, 32),
        inbound:      make(chan *starv1.STAREnvelope, 32),
        motorService: motorService,
        logger:       logger,
    }
}

// Broadcast is called by GatewayService. Non-blocking -- drops if buffer full.
func (h *Hub) Broadcast(env *starv1.STAREnvelope) {
    select {
    case h.broadcast <- env:
    default:
        h.logger.Warn("hub broadcast buffer full, dropping frame")
    }
}

func (h *Hub) Run() {
    const (
        telemetryInterval = 100 * time.Millisecond  // <=10 Hz
        lidarInterval     = 400 * time.Millisecond  // <=2.5 Hz
    )
    var t typeThrottleState

    for {
        select {
        case client := <-h.register:
            h.clients[client] = struct{}{}

        case client := <-h.unregister:
            if _, ok := h.clients[client]; ok {
                delete(h.clients, client)
                close(client.send)
            }

        case env := <-h.broadcast:
            now := time.Now()
            h.route(env, now, &t, telemetryInterval, lidarInterval)
        }
    }
}

func (h *Hub) route(
    env *starv1.STAREnvelope,
    now time.Time,
    t *typeThrottleState,
    telemetryInterval, lidarInterval time.Duration,
) {
    switch env.Payload.(type) {

    // -- Alerts and E-stop: ALWAYS immediate, no throttle --
    case *starv1.STAREnvelope_Alert, *starv1.STAREnvelope_Estop:
        h.stampAndFanOut(env, now)

    // -- LiDAR: independent throttle (large packed payload) --
    case *starv1.STAREnvelope_Lidar:
        if now.Sub(t.lidar) < lidarInterval {
            return
        }
        t.lidar = now
        h.stampAndFanOut(env, now)

    // -- Per-type throttle for remaining sensor data --
    case *starv1.STAREnvelope_Telemetry:
        if now.Sub(t.telemetry) < telemetryInterval { return }
        t.telemetry = now
        h.stampAndFanOut(env, now)

    case *starv1.STAREnvelope_Motor:
        if now.Sub(t.motor) < telemetryInterval { return }
        t.motor = now
        h.stampAndFanOut(env, now)

    case *starv1.STAREnvelope_Battery:
        if now.Sub(t.battery) < telemetryInterval { return }
        t.battery = now
        h.stampAndFanOut(env, now)

    case *starv1.STAREnvelope_Odometry:
        if now.Sub(t.odometry) < telemetryInterval { return }
        t.odometry = now
        h.stampAndFanOut(env, now)

    case *starv1.STAREnvelope_System:
        if now.Sub(t.system) < telemetryInterval { return }
        t.system = now
        h.stampAndFanOut(env, now)
    }
}

// stampAndFanOut assigns seq + ts_ms, marshals ONCE, sends to all clients.
func (h *Hub) stampAndFanOut(env *starv1.STAREnvelope, now time.Time) {
    env.Seq = atomic.AddUint64(&h.seq, 1)
    env.TsMs = now.UnixMilli()

    data, err := proto.Marshal(env)
    if err != nil {
        h.logger.Error("envelope marshal failed", slog.String("err", err.Error()))
        return
    }
    for client := range h.clients {
        select {
        case client.send <- data:
        default:
            // Client send buffer full -- drop this frame for this client.
            // Never disconnect, never block. Client catches up when foregrounded.
        }
    }
}
```
## 8.4 Client
```go
// internal/ws/client.go

type Client struct {
    hub          *Hub
    conn         *websocket.Conn
    send         chan []byte  // buffered 32 -- outbound frames
    motorService MotorController
    logger       *slog.Logger
}

// writePump is the ONLY goroutine that ever calls WriteMessage on conn.
// All writes go through this function. No exceptions.
func (c *Client) writePump() {
    const (
        writeWait  = 10 * time.Second
        pongWait   = 60 * time.Second
    )
    pingPeriod := (pongWait * 9) / 10  // 54s -- derived, not hardcoded

    ticker := time.NewTicker(pingPeriod)
    defer func() {
        ticker.Stop()
        c.conn.Close()
    }()

    for {
        select {
        case message, ok := <-c.send:
            // SetWriteDeadline before EVERY WriteMessage.
            c.conn.SetWriteDeadline(time.Now().Add(writeWait))
            if !ok {
                // Hub closed the channel -- send WS close frame and exit.
                c.conn.WriteMessage(websocket.CloseMessage, []byte{})
                return
            }
            if err := c.conn.WriteMessage(websocket.BinaryMessage, message); err != nil {
                return
            }

        case <-ticker.C:
            // Use WriteMessage for ping, NOT WriteControl.
            // WriteControl permanently mutates WriteDeadline (gorilla issue #841).
            c.conn.SetWriteDeadline(time.Now().Add(writeWait))
            if err := c.conn.WriteMessage(websocket.PingMessage, nil); err != nil {
                return
            }
        }
    }
}

// readPump owns all reads from conn. Runs until error or close.
// E-stop is handled SYNCHRONOUSLY here -- this is intentional.
// Joystick commands should not process while stop is propagating.
func (c *Client) readPump() {
    const (
        pongWait     = 60 * time.Second
        // estopTimeout is the maximum time allowed for an emergency stop command
        // to reach the motor controller before the context is cancelled.
        // context.Background() is the root so a dropped HTTP connection cannot
        // abort the safety command before it reaches the motor controller.
        estopTimeout = 5 * time.Second
    )

    defer func() {
        c.hub.unregister <- c
        c.conn.Close()
    }()

    // Hard cap: largest valid inbound message is ~100 bytes (ControllerState).
    // SetReadLimit prevents memory abuse from malformed or malicious frames.
    c.conn.SetReadLimit(4096)

    c.conn.SetReadDeadline(time.Now().Add(pongWait))
    c.conn.SetPongHandler(func(string) error {
        c.conn.SetReadDeadline(time.Now().Add(pongWait))
        return nil
    })

    for {
        _, data, err := c.conn.ReadMessage()
        if err != nil {
            // Covers: clean close, ping timeout, network drop, read limit exceeded.
            return
        }

        env := &starv1.STAREnvelope{}
        if err := proto.Unmarshal(data, env); err != nil {
            // Malformed frame -- log and continue. Do not kill the connection.
            c.logger.Warn("bad envelope from client",
                slog.String("err", err.Error()),
                slog.Int("bytes", len(data)),
            )
            continue
        }

        // -- E-stop priority path --
        if estop, ok := env.Payload.(*starv1.STAREnvelope_Estop); ok {
            // Use a short-lived context so a slow motor controller cannot
            // block readPump indefinitely. context.Background() is the root
            // so cancellation of connection context cannot abort an e-stop.
            ctx, cancel := context.WithTimeout(context.Background(), estopTimeout)
            if err := c.motorService.EmergencyStop(ctx, estop.Estop.Reason); err != nil {
                c.logger.Error("emergency stop failed",
                    slog.String("reason", estop.Estop.Reason),
                    slog.Any("error", err),
                )
            }
            cancel() // Release context resources immediately; do not defer inside a loop.
            // Forward for packet analyzer visibility (non-blocking)
            select {
            case c.hub.inbound <- env:
            default:
            }
            continue
        }

        // All other inbound messages
        select {
        case c.hub.inbound <- env:
        default:
            // Hub inbound full -- drop. Never block readPump.
        }
    }
}
```
## 8.5 HTTP Upgrade Handler
```go
// internal/ws/handler.go

var upgrader = websocket.Upgrader{
    ReadBufferSize:  1024,
    WriteBufferSize: 1024,
    // HandshakeTimeout defaults to 0.
    // gorilla source: when HandshakeTimeout==0, calls netConn.SetDeadline(time.Time{})
    // unconditionally after upgrade, clearing any server-level deadline. Safe as-is.
    // CheckOrigin is configured via strictOriginChecking parameter (defaults to true).
    // Can be overridden with WS_STRICT_ORIGIN=false for development only.
}

func NewHandler(hub *Hub, motorService MotorController, logger *slog.Logger, strictOriginChecking bool) http.Handler {
    // Configure origin checking based on parameter (secure by default)
    localUpgrader := upgrader
    localUpgrader.CheckOrigin = func(r *http.Request) bool {
        if !strictOriginChecking {
            return true  // UNSAFE: Allows cross-origin connections (dev only)
        }
        // Strict mode: only allow same-origin requests
        return r.Header.Get("Origin") == ""
    }
    
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        conn, err := localUpgrader.Upgrade(w, r, nil)
        if err != nil {
            logger.Error("websocket upgrade failed", slog.String("err", err.Error()))
            return
        }
        client := &Client{
            hub:          hub,
            conn:         conn,
            send:         make(chan []byte, 32),
            motorService: motorService,
            logger:       logger,
        }
        hub.register <- client
        go client.writePump()
        go client.readPump()
        // Handler returns immediately. readPump and writePump own the connection.
    })
}
```
---

## 9. GatewayService Modifications
```go
// internal/service/gateway_service.go  (additions only)

type GatewayService struct {
    // ... existing fields unchanged ...
    hub HubNotifier  // set via SetHub() after both are constructed; nil-safe
}

func (s *GatewayService) SetHub(h HubNotifier) {
    s.hub = h
}

// ForwardTelemetry -- signature unchanged.
// Now additionally broadcasts each present data type as a separate envelope
// so the Hub throttles them independently (lidar != telemetry rate).
func (s *GatewayService) ForwardTelemetry(
    ctx context.Context,
    req *gatewayv1.ForwardTelemetryRequest,
) (*gatewayv1.ForwardTelemetryResponse, error) {
    // ... existing cache update logic unchanged ...

    if s.hub == nil {
        return &gatewayv1.ForwardTelemetryResponse{}, nil
    }

    if req.Telemetry != nil {
        s.hub.Broadcast(&starv1.STAREnvelope{
            Payload: &starv1.STAREnvelope_Telemetry{Telemetry: req.Telemetry},
        })
    }
    if req.MotorStatus != nil {
        s.hub.Broadcast(&starv1.STAREnvelope{
            Payload: &starv1.STAREnvelope_Motor{
                Motor: &starv1.MotorStatusList{Motors: req.MotorStatus},
            },
        })
    }
    if req.BatteryState != nil {
        s.hub.Broadcast(&starv1.STAREnvelope{
            Payload: &starv1.STAREnvelope_Battery{Battery: req.BatteryState},
        })
    }
    if req.Odometry != nil {
        s.hub.Broadcast(&starv1.STAREnvelope{
            Payload: &starv1.STAREnvelope_Odometry{Odometry: req.Odometry},
        })
    }
    if req.LidarScan != nil {
        s.hub.Broadcast(&starv1.STAREnvelope{
            Payload: &starv1.STAREnvelope_Lidar{Lidar: req.LidarScan},
        })
    }
    if req.SystemStatus != nil {
        s.hub.Broadcast(&starv1.STAREnvelope{
            Payload: &starv1.STAREnvelope_System{System: req.SystemStatus},
        })
    }

    return &gatewayv1.ForwardTelemetryResponse{}, nil
}
```
---

## 10. REST Endpoints
```go
// internal/app/gateway.go  (route changes)

// REMOVE:
// mux.Handle("/ws/controller", controllerHandler)

// ADD:
hub := ws.NewHub(motorService, logger)
go hub.Run()

mux.Handle("/ws", ws.NewHandler(hub, motorService, logger))

mux.HandleFunc("POST /api/estop", func(w http.ResponseWriter, r *http.Request) {
    reason := r.URL.Query().Get("reason")
    if reason == "" {
        reason = "user_http_fallback"
    }
    // context.Background() -- same rationale as readPump; must not be cancelled
    estopCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()
    if err := motorService.EmergencyStop(estopCtx, reason); err != nil {
        logger.Error("REST emergency stop failed", slog.Any("err", err))
        http.Error(w, "emergency stop failed", http.StatusInternalServerError)
        return
    }
    w.WriteHeader(http.StatusOK)
})

mux.HandleFunc("/healthz", healthHandler)  // unchanged

gatewayService.SetHub(hub)
```
---

## 11. ROS2 Changes

**File:** `star_gateway_bridge_node.cpp`
```cpp
// Telemetry subscriptions -- latest value only; drop if consumer is slow
auto qos_telemetry = rclcpp::QoS(rclcpp::KeepLast(1))
    .best_effort()
    .durability_volatile();

// Command publishers -- reliable delivery; commands must not be lost
auto qos_commands = rclcpp::QoS(rclcpp::KeepLast(10))
    .reliable()
    .durability_volatile();

battery_sub_  = create_subscription<BatteryState>(
    "/battery_state",  qos_telemetry, ...);
motor_sub_    = create_subscription<MotorStatus>(
    "/motor_status",   qos_telemetry, ...);
odom_sub_     = create_subscription<Odometry>(
    "/odom",           qos_telemetry, ...);
lidar_sub_    = create_subscription<LaserScan>(
    "/scan",           qos_telemetry, ...);

cmd_vel_pub_  = create_publisher<Twist>(
    "/teleop/cmd_vel", qos_commands);
estop_pub_    = create_publisher<Bool>(
    "/estop",          qos_commands);
```

---

## 12. Frontend Architecture

## 12.1 Zustand Store
```typescript
// src/store/dashboardStore.ts
import { create } from 'zustand';

interface DashboardState {
    // Connection
    connectionState: 'disconnected' | 'connecting' | 'connected' | 'reconnecting';
    lastSeq:         number;
    seqGapDetected:  boolean;
    dataIsStale:     boolean;

    // Robot data -- individual slices
    telemetry:    TelemetryData   | null;
    motors:       MotorStatus[]   | null;   // length 4: FL, FR, BL, BR
    battery:      BatteryState    | null;
    odometry:     OdometryData    | null;
    lidarScan:    LidarScan       | null;   // LidarPanel uses .subscribe() + useRef
    systemStatus: SystemStatus    | null;
    eStopActive:  boolean;

    // Alerts -- capped array; oldest dropped at 200
    alerts: Alert[];

    // Actions
    setConnectionState: (s: DashboardState['connectionState']) => void;
    updateFromEnvelope: (env: STAREnvelope) => void;
    markStale:          () => void;
    addAlert:           (alert: Alert) => void;
    triggerEStop:       () => void;
}

export const useDashboardStore = create<DashboardState>()((set, get) => ({
    connectionState: 'disconnected',
    lastSeq:         0,
    seqGapDetected:  false,
    dataIsStale:     false,
    telemetry:       null,
    motors:          null,
    battery:         null,
    odometry:        null,
    lidarScan:       null,
    systemStatus:    null,
    eStopActive:     false,
    alerts:          [],

    setConnectionState: (s) => set({ connectionState: s }),

    // One set() per case -- ensures only the relevant selector fires.
    // Never merge multiple fields into one set() call across cases.
    updateFromEnvelope: (env) => {
        // Always advance lastSeq
        if (env.seq > 0) {
            set({ lastSeq: Number(env.seq) });
        }
        const p = env.payload;
        switch (p.oneofKind) {
            case 'telemetry':   set({ telemetry: p.telemetry, dataIsStale: false }); break;
            case 'motor':       set({ motors: p.motor.motors });                     break;
            case 'battery':     set({ battery: p.battery });                        break;
            case 'odometry':    set({ odometry: p.odometry });                      break;
            case 'lidar':       set({ lidarScan: p.lidar });                        break;
            case 'system':      set({ systemStatus: p.system });                    break;
            case 'alert':       get().addAlert(p.alert);                            break;
        }
    },

    markStale:    () => set({ dataIsStale: true }),

    addAlert: (alert) => set((state) => ({
        alerts: [alert, ...state.alerts].slice(0, 200),
    })),

    triggerEStop: () => set({ eStopActive: true }),
}));
```

**Selector pattern -- enforced:**

```ts
// [PASS] Correct -- one value per selector, no object allocation
const battery = useDashboardStore(s => s.battery);
const motors  = useDashboardStore(s => s.motors);

// [FAIL] Forbidden -- new object on every render, re-renders all consumers
const { battery, motors } = useDashboardStore(s => ({ battery: s.battery, motors: s.motors }));
```

## 12.2 WebSocket Hook
```ts
// src/hooks/useSTARConnection.ts
const BASE_DELAY_MS = 500;
const MAX_DELAY_MS  = 30_000;
const JITTER_FACTOR = 0.3;

function getReconnectDelay(attempt: number): number {
    const exp    = Math.min(BASE_DELAY_MS * 2 ** attempt, MAX_DELAY_MS);
    const jitter = exp * JITTER_FACTOR * (Math.random() * 2 - 1);
    return exp + jitter;
}

export function useSTARConnection(url: string) {
    const wsRef      = useRef<WebSocket | null>(null);
    const attemptRef = useRef(0);
    const store      = useDashboardStore;

    function connect() {
        store.getState().setConnectionState('connecting');
        const ws = new WebSocket(url);

        // CRITICAL: Must be set before any messages arrive.
        // Browser default is 'blob' -- Uint8Array cannot be constructed from Blob.
        ws.binaryType = 'arraybuffer';

        ws.onopen = () => {
            attemptRef.current = 0;
            store.getState().setConnectionState('connected');
        };

        ws.onmessage = (event) => {
            const raw        = event.data as ArrayBuffer;
            const byteLength = raw.byteLength;  // capture BEFORE decode
            let env: STAREnvelope;
            try {
                env = STAREnvelope.fromBinary(new Uint8Array(raw));
            } catch (e) {
                recordPacket(null, 'rx', byteLength, 'DECODE_ERROR');
                console.warn('STAREnvelope decode failed', e);
                return;
            }

            const { lastSeq } = store.getState();
            if (env.seq > 0 && Number(env.seq) > lastSeq + 1) {
                store.getState().addAlert({
                    level:   AlertLevel.WARN,
                    source:  'ws',
                    message: `Sequence gap: ${lastSeq} -> ${env.seq}`,
                    tsMs:    BigInt(Date.now()),
                });
            }

            store.getState().updateFromEnvelope(env);
            recordPacket(env, 'rx', byteLength);
        };

        ws.onclose = ws.onerror = () => {
            store.getState().markStale();
            store.getState().setConnectionState('reconnecting');
            const delay = getReconnectDelay(attemptRef.current++);
            setTimeout(connect, delay);
        };

        wsRef.current = ws;
    }

    // Send helpers
    function sendControllerState(state: ControllerState) {
        const env = STAREnvelope.create({ payload: { oneofKind: 'controller', controller: state } });
        send(env, 'controller');
    }

    function sendEStop(reason: string) {
        const ws = wsRef.current;
        if (ws?.readyState === WebSocket.OPEN) {
            const env = STAREnvelope.create({ payload: { oneofKind: 'estop', estop: { reason } } });
            send(env, 'estop');
        } else {
            // REST fallback -- WebSocket is not open
            fetch(`/api/estop?reason=${encodeURIComponent(reason)}`, { method: 'POST' });
        }
    }

    function send(env: STAREnvelope, kind: string) {
        const ws = wsRef.current;
        if (ws?.readyState !== WebSocket.OPEN) return;
        const bytes = STAREnvelope.toBinary(env);
        ws.send(bytes);
        recordPacket(env, 'tx', bytes.byteLength);
    }

    useEffect(() => {
        connect();
        return () => wsRef.current?.close();
    }, [url]);

    return { sendControllerState, sendEStop };
}
```
## 12.3 uPlot Ring Buffer Pattern
```ts
// src/components/MotorPanel.tsx
// Float64Arrays live OUTSIDE React state -- no GC pressure per tick.

const WINDOW_PTS = 300;  // 30 seconds @ 10 Hz

// One scratch array per series -- all must exist simultaneously for setData()
const timestamps  = new Float64Array(WINDOW_PTS);
const flVelocity  = new Float64Array(WINDOW_PTS);
const frVelocity  = new Float64Array(WINDOW_PTS);
const blVelocity  = new Float64Array(WINDOW_PTS);
const brVelocity  = new Float64Array(WINDOW_PTS);

// Pre-allocated scratch arrays -- one per series
const scratchTs   = new Float64Array(WINDOW_PTS);
const scratchFL   = new Float64Array(WINDOW_PTS);
const scratchFR   = new Float64Array(WINDOW_PTS);
const scratchBL   = new Float64Array(WINDOW_PTS);
const scratchBR   = new Float64Array(WINDOW_PTS);

let writeIdx = 0;

// Unwrap a ring buffer into chronological order, no allocation.
function rolledView(ring: Float64Array, wIdx: number, scratch: Float64Array): Float64Array {
    const tail = ring.subarray(wIdx);         // older half (wIdx -> end)
    const head = ring.subarray(0, wIdx);      // newer half (0 -> wIdx)
    scratch.set(tail, 0);
    scratch.set(head, tail.length);
    return scratch;
}

// Called from Zustand .subscribe(), NOT from a React render.
// useDashboardStore.subscribe(s => s.motors, onMotorUpdate)
function onMotorUpdate(motors: MotorStatus[] | null) {
    if (!motors) return;
    timestamps[writeIdx]  = Date.now() / 1000;
    flVelocity[writeIdx]  = motors[0]?.velocityMps ?? 0;
    frVelocity[writeIdx]  = motors[1]?.velocityMps ?? 0;
    blVelocity[writeIdx]  = motors[2]?.velocityMps ?? 0;
    brVelocity[writeIdx]  = motors[3]?.velocityMps ?? 0;
    writeIdx = (writeIdx + 1) % WINDOW_PTS;

    if (chartRef.current) {
        // Imperative update -- no React re-render triggered
        chartRef.current.setData([
            rolledView(timestamps, writeIdx, scratchTs),
            rolledView(flVelocity, writeIdx, scratchFL),
            rolledView(frVelocity, writeIdx, scratchFR),
            rolledView(blVelocity, writeIdx, scratchBL),
            rolledView(brVelocity, writeIdx, scratchBR),
        ]);
    }
}

```
## 12.4 LiDAR Canvas Pattern
```ts
// src/components/LidarPanel.tsx
// LidarPanel subscribes to the store via .subscribe(), feeds a useRef,
// and draws imperatively. React VDOM never involved in the render cycle.

export function LidarPanel() {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const scanRef   = useRef<LidarScan | null>(null);

    useEffect(() => {
        // Subscribe to Zustand without triggering React re-renders
        const unsub = useDashboardStore.subscribe(
            (state) => state.lidarScan,
            (scan) => {
                // Validate SoA invariant before storing
                if (
                    scan &&
                    scan.anglesRad.length === scan.distancesM.length &&
                    scan.anglesRad.length === scan.qualities.length
                ) {
                    scanRef.current = scan;
                    drawScan();
                }
            }
        );
        return unsub;
    }, []);

    function drawScan() {
        const canvas = canvasRef.current;
        const scan   = scanRef.current;
        if (!canvas || !scan) return;

        const ctx = canvas.getContext('2d')!;
        const cx  = canvas.width / 2;
        const cy  = canvas.height / 2;
        const scale = 80; // pixels per meter

        ctx.clearRect(0, 0, canvas.width, canvas.height);

        for (let i = 0; i < scan.anglesRad.length; i++) {
            const d = scan.distancesM[i];
            const a = scan.anglesRad[i];
            const q = scan.qualities[i] / 255;
            if (d <= 0 || d > 12) continue;  // filter invalid points

            const x = cx + Math.cos(a) * d * scale;
            const y = cy - Math.sin(a) * d * scale;

            ctx.fillStyle = `rgba(0, 220, 80, ${q.toFixed(2)})`;
            ctx.fillRect(x - 1, y - 1, 2, 2);
        }
    }

    return <canvas ref={canvasRef} width={400} height={400} />;
}
```
## 12.5 Packet Analyzer
```ts
// src/components/PacketAnalyzer.tsx

interface PacketRecord {
    seq:       number;
    type:      string;          // oneofKind or 'DECODE_ERROR'
    direction: 'rx' | 'tx';
    sizeBytes: number;          // from raw.byteLength BEFORE decode
    tsMs:      number;
    preview:   string;
}

// Ring buffer in useRef -- never in React state
const packetRingBuffer = useRef<PacketRecord[]>(new Array(100).fill(null));
const packetWriteIdx   = useRef(0);
const packetCount      = useRef(0);

export function recordPacket(
    env:       STAREnvelope | null,
    direction: 'rx' | 'tx',
    sizeBytes: number,
    errorType?: string,
) {
    packetRingBuffer.current[packetWriteIdx.current] = {
        seq:       env ? Number(env.seq) : 0,
        type:      errorType ?? env?.payload.oneofKind ?? 'unknown',
        direction,
        sizeBytes,
        tsMs:      Date.now(),
        preview:   env ? formatPreview(env) : errorType ?? '',
    };
    packetWriteIdx.current = (packetWriteIdx.current + 1) % 100;
    packetCount.current++;
}

// Decoded preview strings per message type
function formatPreview(env: STAREnvelope): string {
    const p = env.payload;
    switch (p.oneofKind) {
        case 'telemetry':
            return `cpu=${p.telemetry.cpuPercent?.toFixed(1)}% temp=${p.telemetry.tempC?.toFixed(1)}degC`;
        case 'motor':
            return p.motor.motors
                .map((m, i) => `${['FL','FR','BL','BR'][i]}:${m.velocityMps?.toFixed(2)}`)
                .join(' ');
        case 'battery':
            return `V=${p.battery.voltageV?.toFixed(1)}v I=${p.battery.currentA?.toFixed(1)}A SOC=${p.battery.socPercent?.toFixed(0)}%`;
        case 'odometry':
            return `x=${p.odometry.xM?.toFixed(2)} y=${p.odometry.yM?.toFixed(2)} theta=${p.odometry.thetaRad?.toFixed(2)}`;
        case 'lidar':
            return `${p.lidar.anglesRad.length} pts`;
        case 'alert':
            return `[${AlertLevel[p.alert.level]}] ${p.alert.source}: ${p.alert.message}`;
        case 'system':
            return `mode=${p.system.mode} uptime=${p.system.uptimeSec}s`;
        case 'controller':
            return `lin=${p.controller.linearVel?.toFixed(2)} ang=${p.controller.angularVel?.toFixed(2)} [TX]`;
        case 'estop':
            return `reason=${p.estop.reason} [TX]`;
        default:
            return '';
    }
}

```

**Display:**
```text
+- PACKET ANALYZER ------------------------------------ [CLEAR] [v] -+
| SEQ    TYPE         DIR  SIZE   AGE    DECODED PREVIEW              |
| 10042  telemetry    RX   48B    12ms   cpu=23.1% temp=41.0degC        |
| 10041  motor        RX   112B   112ms  FL:0.23 FR:0.23 BL:0.22 BR:0|
| 10040  battery      RX   64B    212ms  V=11.8v I=2.1A SOC=87%      |
| 10039  lidar        RX   1840B  412ms  360 pts                      |
| 0      controller   TX   24B    18ms   lin=0.50 ang=0.00 [TX]      |
| 0      estop        TX   16B    --      reason=user_ui_button [TX]  |
| --      DECODE_ERROR RX   7B     --                                   |
+---------------------------------------------------------------------+
```
## 12.6 Component Inventory

|Component|Data Source|Render Trigger|
|---|---|---|
|`StatusBar`|`connectionState`, `eStopActive`, `dataIsStale`|Zustand selector|
|`TeleopPanel`|`useSTARConnection.sendControllerState`|Gamepad event|
|`MotorPanel`|Zustand `.subscribe(s => s.motors)`|uPlot `.setData()` imperatively|
|`BatteryPanel`|`useDashboardStore(s => s.battery)`|Zustand selector|
|`OdometryPanel`|`useDashboardStore(s => s.odometry)`|Zustand selector -> Canvas draw|
|`LidarPanel`|Zustand `.subscribe(s => s.lidarScan)`|Canvas draw imperatively via `useRef`|
|`AlertsPanel`|`useDashboardStore(s => s.alerts)`|Zustand selector|
|`PacketAnalyzer`|`useRef` ring buffer|`setInterval` 200ms snapshot|

---

## 13. Error Handling

|Error|Where|Response|
|---|---|---|
|WebSocket connection refused|`useSTARConnection`|Exponential backoff reconnect; `markStale()`|
|Proto decode failure (browser)|`useSTARConnection.onmessage`|Log warning, record `DECODE_ERROR` in packet analyzer, drop frame, continue|
|Proto marshal failure (Gateway)|`stampAndFanOut`|Log error, do not send to any client|
|Proto unmarshal failure (Gateway)|`readPump`|Log warning, continue (do not close connection)|
|Client send buffer full|`stampAndFanOut`|Drop frame silently for that client; never block, never disconnect|
|Hub broadcast buffer full|`Hub.Broadcast()`|Drop frame, log warning|
|E-stop gRPC failure|`readPump` / REST handler|Log error; no retry (not safety-critical system)|
|LiDAR SoA invariant violation|`LidarPanel` subscribe callback|Discard scan, do not render|
|Sequence gap detected|`useSTARConnection.onmessage`|Add `WARN` alert; update `lastSeq` to prevent re-alerting|

---

## 14. Interface Contracts and Invariants

|Contract|Enforcement|
|---|---|
|`writePump` is the **only** writer|No goroutine except `writePump` ever calls `WriteMessage` on `conn`|
|`SetWriteDeadline` before **every** `WriteMessage`|Deadline is an absolute `time.Time`; must be reset before each call|
|Pings via `WriteMessage`, **never** `WriteControl`|`WriteControl` permanently mutates `WriteDeadline` (gorilla issue #841)|
|`SetReadLimit(4096)` in `readPump`|Prevents memory abuse from malformed frames|
|E-stop uses `context.Background()`|Never cancelled by connection or request lifecycle|
|Alerts are **never throttled**|Hub type switch handles `Alert` and `EStop` before throttle logic|
|`seq` is Gateway-stamped only|Hub atomically assigns `seq` in `stampAndFanOut`; UI always sends `seq=0`|
|`Hub.Broadcast()` is non-blocking|Uses `select/default`; never blocks `ForwardTelemetry` gRPC handler|
|Zustand selectors are individual|Never destructure objects from selectors; `zustand/shallow` only when genuinely comparing multiple fields|
|uPlot data lives outside React|`Float64Array` ring buffers in module scope; `setData()` called imperatively via `chartRef.current`|
|LiDAR bypasses React render cycle|`LidarPanel` uses `useDashboardStore.subscribe()` + `useRef`; never `useDashboardStore()` hook|
|`ws.binaryType = 'arraybuffer'`|Set immediately after `new WebSocket(url)`, before any messages arrive|
|`sizeBytes` from `raw.byteLength`|Captured from `ArrayBuffer` before `fromBinary()` -- no re-serialization|
|LiDAR SoA validated before render|`angles_rad.length === distances_m.length === qualities.length` -- discard on violation|
|`lastSeq` updated on every envelope|Prevents infinite re-alerting after a gap|
|Per-type throttle uses named struct|`typeThrottleState` -- zero reflection, zero map allocation in hot path|
|`STAREnvelope` is UI-layer only|Never passed to nanopb or SPI path|

---

## 15. Dependencies

## star-ui (new)

`npm install zustand uplot uplot-react`

|Package|Version|Purpose|
|---|---|---|
|`zustand`|^5.x|Global robot state store|
|`uplot`|^1.6.x|Canvas time-series charting|
|`uplot-react`|^1.2.x|React mount wrapper for uPlot|

## star-gateway (no new deps)

`gorilla/websocket` is already in `go.mod`. Pin to `v1.5.3`.

## star-proto (no new deps)

Existing protoc + protobuf-ts toolchain handles the new `ui.proto` file.

---

## 16. Implementation Work Order

## Step 1 -- star-proto _(unblocks everything)_

|Action|File|
|---|---|
|**ADD**|`star/v1/ui.proto` -- `STAREnvelope`, `MotorStatusList`, `OdometryData`, `LidarScan`, `Alert`, `AlertLevel`, `EStopCommand`|
|**RUN**|`/build proto` -- regenerates Go + TypeScript targets|
|**VERIFY**|Generated TypeScript has `oneofKind` discriminated union for `STAREnvelope.payload`|

## Step 2 -- star-gateway _(Go)_

| Action     | File                                  | Key detail                                                                                    |
| ---------- | ------------------------------------- | --------------------------------------------------------------------------------------------- |
| **ADD**    | `internal/ws/interfaces.go`           | `HubNotifier`, `MotorController` interfaces                                                   |
| **ADD**    | `internal/ws/hub.go`                  | `typeThrottleState` struct, `run()` loop, `route()`, `stampAndFanOut()`, `Broadcast()`        |
| **ADD**    | `internal/ws/client.go`               | `readPump`, `writePump`, ping via `WriteMessage`, `SetReadLimit(4096)`                        |
| **ADD**    | `internal/ws/handler.go`              | `NewHandler()`, origin checking configurable (defaults to strict, override via WS_STRICT_ORIGIN=false) |
| **MODIFY** | `internal/service/gateway_service.go` | Add `hub HubNotifier` field, `SetHub()`, per-type `Broadcast()` calls in `ForwardTelemetry()` |
| **MODIFY** | `internal/app/gateway.go`             | Remove `/ws/controller`, add `/ws`, add `POST /api/estop`, wire `hub.Run()` goroutine         |

## Step 3 -- star-ros2 _(minimal)_

|Action|File|Key detail|
|---|---|---|
|**MODIFY**|`star_gateway_bridge_node.cpp`|Update QoS: `BEST_EFFORT keep_last:1` for all telemetry subs; `RELIABLE keep_last:10` for command/estop pubs|

## Step 4 -- star-ui _(TypeScript)_

|Action|File|Key detail|
|---|---|---|
|**RUN**|`npm install zustand uplot uplot-react`|--|
|**ADD**|`src/store/dashboardStore.ts`|Individual selectors, one `set()` per `oneofKind` case, `lastSeq` always updated|
|**ADD**|`src/services/GatewayService.ts`|Envelope encode/decode, `recordPacket()`, `formatPreview()` per type|
|**ADD**|`src/hooks/useSTARConnection.ts`|`ws.binaryType = 'arraybuffer'`, exponential backoff, gap detection, E-stop REST fallback|
|**ADD**|`src/components/StatusBar.tsx`|Connection dot, mode badge, E-stop button|
|**ADD**|`src/components/TeleopPanel.tsx`|Existing gamepad code migrated to new `sendControllerState()`|
|**ADD**|`src/components/MotorPanel.tsx`|uPlot, 5 `Float64Array` ring buffers, 5 scratch arrays, `rolledView()`|
|**ADD**|`src/components/BatteryPanel.tsx`|Voltage, SOC%, current display|
|**ADD**|`src/components/OdometryPanel.tsx`|Canvas 2D X/Y trail + heading arrow|
|**ADD**|`src/components/LidarPanel.tsx`|Canvas 2D, `.subscribe()` + `useRef`, SoA length validation|
|**ADD**|`src/components/AlertsPanel.tsx`|Scrollable timestamped alert log|
|**ADD**|`src/components/PacketAnalyzer.tsx`|`useRef` ring buffer, `setInterval` 200ms snapshot, `formatPreview()`|
|**MODIFY**|`src/App.tsx`|Dashboard grid layout, wire all components|

---

## 17. Testing Strategy

|Layer|What to Test|How|
|---|---|---|
|Go Hub|Throttle drops excess frames per type; alerts always pass through|Unit test: send 5 same-type frames in <100ms, assert only 1 broadcast|
|Go Hub|Fan-out to multiple clients|Unit test: 3 clients, 1 broadcast -> assert all 3 receive|
|Go Hub|Slow client drops frames, is not disconnected|Unit test: fill `client.send` buffer, send one more, assert client still registered|
|Go readPump|E-stop calls `motorService.EmergencyStop` synchronously|Unit test with mock `MotorController`|
|Go readPump|Malformed proto logs warning and continues|Unit test: send garbage bytes, assert connection stays open|
|Go GatewayService|`ForwardTelemetry` calls `hub.Broadcast` once per non-nil field|Unit test with mock `HubNotifier`|
|TypeScript|`rolledView` returns correct chronological order|Unit test: fill ring with sequential values, verify unwrapped output|
|TypeScript|Gap detection fires alert on seq skip|Unit test: send seq=5 then seq=8, assert alert added|
|TypeScript|`updateFromEnvelope` only sets relevant field|Unit test: motor update should not change `battery` reference|
|TypeScript|`ws.binaryType` is set to `'arraybuffer'`|Integration test / code review|

---

## 18. Future Work

|Item|Priority|Notes|
|---|---|---|
|Migrate gRPC to bidirectional streaming|High|Eliminates 50 Hz `GetTeleopCommand` polling; reduces command latency from ~20ms avg to <1ms push|
|`WritePreparedMessage` for broadcast|Medium|Pre-encode WS frames once; important if per-message compression is enabled|
|Tailwind CSS|Low|Dashboard polish; deferred until MVP is functional|
|PID tuning controls|Low|Real-time motor PID parameter adjustment from UI|
|WebSocket per-message compression|Low|`EnableCompression: true` on upgrader; evaluate trade-off on RPi5 CPU|
|Multi-robot support|Not planned|Would require Hub to be keyed by robot ID|