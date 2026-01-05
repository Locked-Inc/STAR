package controller

import (
	"log"
	"net/http"
	"sync"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"

	"google.golang.org/protobuf/proto"

	"nhooyr.io/websocket" //nolint:staticcheck
)

type Handler struct {
	mu sync.Mutex

	lastState *starv1.ControllerState

	lastReceived time.Time
}

func NewHandler() *Handler {
	return &Handler{}
}

func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	c, err := websocket.Accept(w, r, nil) //nolint:staticcheck // library is deprecated but migration is out of scope
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
