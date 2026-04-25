package controller

import (
	"log"
	"net/http"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"github.com/gorilla/websocket"
	"google.golang.org/protobuf/proto"
)

// processStart anchors a monotonic since-start clock for proto
// `timestamp_us` fields on outbound commands. See the gateway service
// package's monoMicrosSinceStart for rationale; we duplicate the helper
// locally to avoid an import cycle (service -> controller is allowed but
// controller -> service is the existing direction, so we keep this small).
var processStart = time.Now()

// monoMicrosSinceStart returns microseconds since this process started.
// Used for VelocityCommand.timestamp_us so the gateway and RX72N stay in
// the same "since boot/start" epoch convention.
func monoMicrosSinceStart() int64 {
	return time.Since(processStart).Microseconds()
}

type Handler struct {
	mu sync.Mutex

	lastState *starv1.ControllerState

	lastReceived time.Time

	gatewaySvc *service.GatewayService
}

func NewHandler() *Handler {
	return &Handler{}
}

func NewHandlerWithGateway(gatewaySvc *service.GatewayService) *Handler {
	return &Handler{
		gatewaySvc: gatewaySvc,
	}
}

func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	upgrader := websocket.Upgrader{
		CheckOrigin: func(_ *http.Request) bool { return true },
	}
	c, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("failed to accept websocket: %v", err)
		return
	}

	log.Printf("controller connected from %s", r.RemoteAddr)

	defer func() {
		deadline := time.Now().Add(time.Second)
		closePayload := websocket.FormatCloseMessage(websocket.CloseInternalServerErr, "internal error")
		if err := c.WriteControl(websocket.CloseMessage, closePayload, deadline); err != nil {
			// It's normal for Close to fail if connection is already closed
			log.Printf("websocket close: %v", err)
		}
		if err := c.Close(); err != nil {
			log.Printf("websocket close: %v", err)
		}
		log.Printf("controller disconnected from %s", r.RemoteAddr)
	}()

	var lastDebug bool

	for {
		typ, bytes, err := c.ReadMessage()
		if err != nil {
			log.Printf("failed to read: %v", err)
			break
		}

		if typ != websocket.BinaryMessage {
			continue
		}

		var msg starv1.ControllerState
		if err := proto.Unmarshal(bytes, &msg); err != nil {
			log.Printf("failed to unmarshal: %v", err)
			continue
		}

		// Handle debug state transitions
		if msg.Debug && !lastDebug {
			log.Println(">>> DEBUG MODE ENABLED")
		} else if !msg.Debug && lastDebug {
			log.Println(">>> DEBUG MODE DISABLED")
		}
		lastDebug = msg.Debug

		// Debug log to verify data reception
		if msg.Debug {
			log.Printf("Received: Linear=%.2f, Angular=%.2f", msg.LinearVel, msg.AngularVel)
		}

		// Convert ControllerState to VelocityCommand and forward to Gateway
		if h.gatewaySvc != nil {
			cmd := convertToVelocityCommand(&msg)
			h.gatewaySvc.UpdateTeleopCommand(cmd)
		}

		h.mu.Lock()
		h.lastState = &msg
		h.lastReceived = time.Now()
		h.mu.Unlock()
	}
}

func (h *Handler) GetLastState() *starv1.ControllerState {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.lastState
}

// GetSafeState returns the last state, or a zero state if the last message
// was received more than 200ms ago.
func (h *Handler) GetSafeState() *starv1.ControllerState {
	h.mu.Lock()
	defer h.mu.Unlock()

	if h.lastState == nil {
		return &starv1.ControllerState{}
	}

	if time.Since(h.lastReceived) > 200*time.Millisecond {
		// Watchdog triggered: return zero state
		return &starv1.ControllerState{}
	}

	return h.lastState
}

// convertToVelocityCommand converts arcade drive (linear, angular) to differential drive (left, right).
//
// Implements differential drive inverse kinematics for a 4-wheel robot:
//
//	vL = v - (omega * wheelBase) / 2
//	vR = v + (omega * wheelBase) / 2
//
// where v is linear velocity (m/s), omega is angular velocity (rad/s), and wheelBase is track width (m).
//
// Motor Layout (top view):
//
//	  Front
//	FL   FR
//	BL   BR
//	  Back
//
// For differential drive, left side (FL, BL) and right side (FR, BR) are coupled.
func convertToVelocityCommand(state *starv1.ControllerState) *starv1.VelocityCommand {
	const wheelBase float32 = 0.150 // meters (150mm track width)

	halfBase := wheelBase / 2.0
	vLeft := state.LinearVel - (state.AngularVel * halfBase)
	vRight := state.LinearVel + (state.AngularVel * halfBase)

	// For differential drive: left side motors get same velocity, right side motors get same velocity.
	// timestamp_us uses our monotonic since-start clock so round-trip latency math
	// stays consistent with the RX72N's boot-relative timestamps.
	return &starv1.VelocityCommand{
		FrontLeftVelocityMps:  float64(vLeft),
		FrontRightVelocityMps: float64(vRight),
		BackLeftVelocityMps:   float64(vLeft),
		BackRightVelocityMps:  float64(vRight),
		Sequence:              0,
		TimestampUs:           monoMicrosSinceStart(),
	}
}
