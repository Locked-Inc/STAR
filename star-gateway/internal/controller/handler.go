package controller

import (
	"log"
	"net/http"
	"sync"
	"time"

	"github.com/Locked-Inc/STAR/star-gateway/internal/service"
	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"

	"google.golang.org/protobuf/proto"

	"nhooyr.io/websocket" //nolint:staticcheck
)

type Handler struct {
	mu sync.Mutex

	lastState *starv1.ControllerState

	lastReceived time.Time

	gatewaySvc *service.GatewayService
}

func NewHandlerWithGateway(gatewaySvc *service.GatewayService) *Handler {
	return &Handler{
		gatewaySvc: gatewaySvc,
	}
}

func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	c, err := websocket.Accept(w, r, &websocket.AcceptOptions{
		InsecureSkipVerify: true,
	}) //nolint:staticcheck // library is deprecated but migration is out of scope
	if err != nil {
		log.Printf("failed to accept websocket: %v", err)
		return
	}

	log.Printf("controller connected from %s", r.RemoteAddr)

	defer func() {
		if err := c.Close(websocket.StatusInternalError, "internal error"); err != nil { //nolint:staticcheck
			// It's normal for Close to fail if connection is already closed
			log.Printf("websocket close: %v", err)
		}
		log.Printf("controller disconnected from %s", r.RemoteAddr)
	}()

	ctx := r.Context()
	var lastDebug bool

	for {
		typ, bytes, err := c.Read(ctx) //nolint:staticcheck
		if err != nil {
			log.Printf("failed to read: %v", err)
			break
		}

		if typ != websocket.MessageBinary {
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
// Implements differential drive inverse kinematics:
//   vL = v - (ω * wheelBase) / 2
//   vR = v + (ω * wheelBase) / 2
//
// where v is linear velocity (m/s), ω is angular velocity (rad/s), and wheelBase is track width (m).
func convertToVelocityCommand(state *starv1.ControllerState) *starv1.VelocityCommand {
	const wheelBase float32 = 0.150 // meters (150mm track width)

	halfBase := wheelBase / 2.0
	vLeft := state.LinearVel - (state.AngularVel * halfBase)
	vRight := state.LinearVel + (state.AngularVel * halfBase)

	return &starv1.VelocityCommand{
		LeftVelocityMps:  float64(vLeft),
		RightVelocityMps: float64(vRight),
		Sequence:         0,
		TimestampUs:      time.Now().UnixMicro(),
	}
}
