// Copyright (c) 2026 Locked Inc.
// SPDX-License-Identifier: MIT

package ws

import (
	"context"
	"io"
	"log/slog"
	"testing"
	"time"

	starv1 "github.com/Locked-Inc/star-proto/gen/go/star/v1"
	"google.golang.org/protobuf/proto"
)

// runLoopTick is the sleep duration used when waiting for the Hub's Run loop
// to process a channel event (e.g., registration). Small enough to keep tests
// fast; large enough to yield the scheduler so the goroutine actually runs.
const runLoopTick = 5 * time.Millisecond

// testMinimalBuffer is the send-channel capacity used in tests that verify
// drop behaviour when a client's buffer is at capacity. Named to avoid magic
// integer literals and make the intent self-documenting.
const testMinimalBuffer = 1

// testInterval is the throttle interval used in shouldThrottle table-driven tests.
const testInterval = 100 * time.Millisecond

// withinTelemetryOffset is a time advance that falls strictly within the telemetry
// throttle interval, ensuring a telemetry envelope is still suppressed.
const withinTelemetryOffset = 1 * time.Millisecond

// withinLidarOffset is a time advance that falls strictly within the lidar throttle
// interval, ensuring a lidar envelope is still suppressed.
const withinLidarOffset = 50 * time.Millisecond

// withinIntervalOffset is a time advance that falls strictly within testInterval,
// used in shouldThrottle table tests to assert throttling is still active.
const withinIntervalOffset = 10 * time.Millisecond

// pastIntervalOffset is a time offset that places lastSent strictly before the
// current time minus testInterval, ensuring throttling has expired.
const pastIntervalOffset = -200 * time.Millisecond

// registrationDeadline is the maximum time to wait for the Hub Run loop to
// acknowledge a client registration before failing the test.
const registrationDeadline = 50 * time.Millisecond

// shutdownTimeout is the maximum time to wait for Hub.Run to return after
// context cancellation before failing the test.
const shutdownTimeout = 500 * time.Millisecond

// closedChannelTimeout is the maximum time to wait for a client send channel
// to be closed (by unregister or graceful shutdown) before failing the test.
const closedChannelTimeout = 100 * time.Millisecond

// newTestHub returns a Hub wired with a discard logger suitable for unit tests.
func newTestHub(t *testing.T) *Hub {
	t.Helper()
	return NewHub(slog.New(slog.NewTextHandler(io.Discard, nil)))
}

// -- shouldThrottle ---------------------------------------------------------

func TestShouldThrottle_TableDriven(t *testing.T) {
	now := time.Now()

	tests := []struct {
		name            string
		initialLastSent time.Time
		now             time.Time
		interval        time.Duration
		wantThrottled   bool
	}{
		{
			name:            "FirstCall_NotThrottled",
			initialLastSent: time.Time{}, // zero value -- never sent before
			now:             now,
			interval:        testInterval,
			wantThrottled:   false,
		},
		{
			name:            "WithinInterval_Throttled",
			initialLastSent: now,                           // just sent
			now:             now.Add(withinIntervalOffset), // still within 100ms interval
			interval:        testInterval,
			wantThrottled:   true,
		},
		{
			name:            "AfterInterval_NotThrottled",
			initialLastSent: now.Add(pastIntervalOffset), // older than interval
			now:             now,
			interval:        testInterval,
			wantThrottled:   false,
		},
		{
			name:            "ExactlyAtBoundary_NotThrottled",
			initialLastSent: now.Add(-testInterval), // exactly one interval ago
			now:             now,
			interval:        testInterval,
			// now - lastSent == interval, which is NOT < interval, so allowed.
			wantThrottled: false,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			lastSent := tc.initialLastSent // local copy so we can take its address
			got := shouldThrottle(&lastSent, tc.now, tc.interval)
			if got != tc.wantThrottled {
				t.Errorf("shouldThrottle() = %v, want %v", got, tc.wantThrottled)
			}
			if !tc.wantThrottled {
				// When not throttled, lastSent must be updated to now.
				if lastSent != tc.now {
					t.Error("shouldThrottle must update *lastSent to now when allowing through")
				}
			}
		})
	}
}

// -- Broadcast -------------------------------------------------------------

func TestBroadcast_EnqueuesMessage(t *testing.T) {
	h := newTestHub(t)

	env := &starv1.STAREnvelope{Seq: 1}
	if err := h.Broadcast(context.Background(), env); err != nil {
		t.Fatalf("Broadcast returned unexpected error: %v", err)
	}

	select {
	case got := <-h.broadcast:
		if got.Seq != env.Seq {
			t.Errorf("got seq %d, want %d", got.Seq, env.Seq)
		}
	default:
		t.Fatal("broadcast channel is empty after Broadcast")
	}
}

func TestBroadcast_DropWhenFull(t *testing.T) {
	h := newTestHub(t)

	// Fill the buffer completely.
	for i := range defaultChannelBuffer {
		env := &starv1.STAREnvelope{Seq: uint64(i)}
		if err := h.Broadcast(context.Background(), env); err != nil {
			t.Fatalf("Broadcast %d returned unexpected error: %v", i, err)
		}
	}

	// Next call must return ErrBroadcastFull, not block.
	if err := h.Broadcast(context.Background(), &starv1.STAREnvelope{Seq: 999}); err != ErrBroadcastFull {
		t.Errorf("expected ErrBroadcastFull, got %v", err)
	}
}

// -- route -----------------------------------------------------------------

// routeResult captures the stampAndFanOut calls made to the hub during a
// route call by tracking the number of sends to its (empty) clients map.
// Since there are zero clients, we instead observe via seq increments.

func TestRoute_AlertPassesThrough(t *testing.T) {
	h := newTestHub(t)
	t0 := time.Now()
	var ts typeThrottleState

	before := h.seq.Load()
	h.route(&starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Alert{Alert: &starv1.Alert{}},
	}, t0, &ts, telemetryInterval, lidarInterval)
	after := h.seq.Load()

	if after <= before {
		t.Error("route(Alert) must call stampAndFanOut (seq must increase)")
	}
}

func TestRoute_TelemetryThrottled(t *testing.T) {
	h := newTestHub(t)
	t0 := time.Now()
	ts := typeThrottleState{
		telemetry: t0, // just updated -- within interval
	}

	before := h.seq.Load()
	h.route(&starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Telemetry{Telemetry: &starv1.TelemetryData{}},
	}, t0.Add(withinTelemetryOffset), &ts, telemetryInterval, lidarInterval)
	after := h.seq.Load()

	if after != before {
		t.Error("route(Telemetry) within interval must be throttled (seq must not change)")
	}
}

func TestRoute_LidarThrottled(t *testing.T) {
	h := newTestHub(t)
	t0 := time.Now()
	ts := typeThrottleState{
		lidar: t0, // just updated
	}

	before := h.seq.Load()
	h.route(&starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Lidar{Lidar: &starv1.LidarScan{}},
	}, t0.Add(withinLidarOffset), &ts, telemetryInterval, lidarInterval)
	after := h.seq.Load()

	if after != before {
		t.Error("route(Lidar) within lidar interval must be throttled")
	}
}

func TestRoute_LidarPassesAfterInterval(t *testing.T) {
	h := newTestHub(t)
	t0 := time.Now()
	ts := typeThrottleState{
		lidar: t0.Add(-lidarInterval - time.Millisecond), // older than interval
	}

	before := h.seq.Load()
	h.route(&starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Lidar{Lidar: &starv1.LidarScan{}},
	}, t0, &ts, telemetryInterval, lidarInterval)
	after := h.seq.Load()

	if after <= before {
		t.Error("route(Lidar) after lidar interval must call stampAndFanOut")
	}
}

func TestRoute_TelemetryPassesAfterInterval(t *testing.T) {
	h := newTestHub(t)
	t0 := time.Now()
	ts := typeThrottleState{
		telemetry: t0.Add(-telemetryInterval - time.Millisecond), // older than interval
	}

	before := h.seq.Load()
	h.route(&starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Telemetry{Telemetry: &starv1.TelemetryData{}},
	}, t0, &ts, telemetryInterval, lidarInterval)
	after := h.seq.Load()

	if after <= before {
		t.Error("route(Telemetry) after telemetry interval must call stampAndFanOut (seq must increase)")
	}
}

// TestRoute_UnknownPayload_DoesNotIncrement verifies that route does not call
// stampAndFanOut (and therefore does not increment seq) when the envelope
// carries a nil or unknown/unrecognised payload. This guards the default case
// in route's type-switch from silently broadcasting empty envelopes.
func TestRoute_UnknownPayload_DoesNotIncrement(t *testing.T) {
	h := newTestHub(t)
	t0 := time.Now()
	ts := typeThrottleState{}

	before := h.seq.Load()
	h.route(&starv1.STAREnvelope{Payload: nil}, t0, &ts, telemetryInterval, lidarInterval)
	after := h.seq.Load()

	if after != before {
		t.Errorf("route(nil payload) must not call stampAndFanOut: seq changed from %d to %d", before, after)
	}
}

// -- Hub lifecycle ----------------------------------------------------------

func TestHub_RegisterUnregister(t *testing.T) {
	h := newTestHub(t)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go h.Run(ctx)

	client := &Client{
		hub:  h,
		send: make(chan []byte, defaultChannelBuffer),
	}

	// Register the client.
	h.register <- client

	// Wait deterministically for Run to process the registration by polling
	// the atomic registeredClients counter. This avoids a flaky time.Sleep
	// that could race when the hub goroutine is slow to schedule.
	regDeadline := time.Now().Add(registrationDeadline)
	for time.Now().Before(regDeadline) && h.registeredClients.Load() == 0 {
		time.Sleep(runLoopTick)
	}
	if h.registeredClients.Load() == 0 {
		t.Fatal("client was not registered within deadline")
	}

	// Unregister the client -- Run should close its send channel.
	h.unregister <- client

	// After unregister the send channel must be closed.
	select {
	case _, ok := <-client.send:
		if ok {
			t.Error("send channel must be closed after unregister")
		}
	case <-time.After(registrationDeadline):
		t.Error("timed out waiting for send channel close after unregister")
	}
}

func TestHub_GracefulShutdown(t *testing.T) {
	h := newTestHub(t)
	ctx, cancel := context.WithCancel(context.Background())

	client := &Client{
		hub:  h,
		send: make(chan []byte, defaultChannelBuffer),
	}

	done := make(chan struct{})
	go func() {
		defer close(done)
		h.Run(ctx)
	}()

	h.register <- client

	// Wait deterministically for Run to process the registration by polling
	// the atomic registeredClients counter. This avoids a flaky time.Sleep
	// that could race when the hub goroutine is slow to schedule.
	regDeadline := time.Now().Add(registrationDeadline)
	for time.Now().Before(regDeadline) && h.registeredClients.Load() == 0 {
		time.Sleep(runLoopTick)
	}
	if h.registeredClients.Load() == 0 {
		t.Fatal("client was not registered before cancelling context")
	}

	// Cancel context -- Run should close all client send channels and return.
	cancel()

	select {
	case <-done:
	case <-time.After(shutdownTimeout):
		t.Fatal("hub.Run did not return after context cancellation")
	}

	// send channel must be closed by graceful shutdown.
	// Use closedChannelTimeout instead of a non-blocking default so the test
	// doesn't falsely fail while hub.Run / cancel() propagation is in-flight.
	select {
	case _, ok := <-client.send:
		if ok {
			t.Error("send channel must be closed on graceful shutdown")
		}
	case <-time.After(closedChannelTimeout):
		t.Error("send channel was not closed on graceful shutdown")
	}
}

// -- Unit tests -------------------------------------------------------------

// TestStampAndFanOut_DropsWhenClientFull verifies that stampAndFanOut is
// non-blocking: when a client's send channel is at capacity the message must
// be silently dropped rather than blocking the hub's event loop.
func TestStampAndFanOut_DropsWhenClientFull(t *testing.T) {
	h := newTestHub(t)

	// Create a client with send channel capacity testMinimalBuffer and pre-fill it.
	client := &Client{
		hub:  h,
		send: make(chan []byte, testMinimalBuffer),
	}
	client.send <- []byte("pre-filled") // channel is now full
	h.clients[client] = struct{}{}

	env := &starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Telemetry{Telemetry: &starv1.TelemetryData{}},
	}

	// stampAndFanOut must return without blocking even though the channel is full.
	done := make(chan struct{})
	go func() {
		h.stampAndFanOut(env, time.Now())
		close(done)
	}()

	select {
	case <-done:
		// Good: stampAndFanOut returned quickly.
	case <-time.After(closedChannelTimeout):
		t.Fatal("stampAndFanOut blocked when client send channel was full")
	}

	// The channel must still hold only the pre-filled message (new one dropped).
	if len(client.send) != 1 {
		t.Errorf("client.send length = %d, want 1 (drop should not enqueue new message)", len(client.send))
	}
	if msg := <-client.send; string(msg) != "pre-filled" {
		t.Errorf("client.send held unexpected message after drop")
	}
}

// -- Benchmarks -------------------------------------------------------------

// BenchmarkStampAndFanOut measures fan-out time with varying numbers of mock clients.
func BenchmarkStampAndFanOut_1Client(b *testing.B)    { benchStampAndFanOut(b, 1) }
func BenchmarkStampAndFanOut_10Clients(b *testing.B)  { benchStampAndFanOut(b, 10) }
func BenchmarkStampAndFanOut_100Clients(b *testing.B) { benchStampAndFanOut(b, 100) }

func benchStampAndFanOut(b *testing.B, n int) {
	b.Helper()
	h := NewHub(slog.New(slog.NewTextHandler(io.Discard, nil)))

	// Direct client population: hub.Run is NOT started in this benchmark so
	// there is no concurrent register/unregister processing and no event-loop
	// goroutine. Clients are inserted directly into h.clients to avoid the
	// overhead of spinning up hub.Run and sending on the register channel,
	// keeping the benchmark focused solely on stampAndFanOut throughput.
	for range n {
		c := &Client{
			hub:  h,
			send: make(chan []byte, defaultChannelBuffer),
		}
		h.clients[c] = struct{}{}
	}

	env := &starv1.STAREnvelope{
		Payload: &starv1.STAREnvelope_Telemetry{Telemetry: &starv1.TelemetryData{}},
	}
	now := time.Now()

	// Measure the marshalled size once so b.SetBytes reports throughput in B/op.
	basePayload, err := proto.Marshal(env)
	if err != nil {
		b.Fatalf("proto.Marshal: %v", err)
	}

	b.ReportAllocs()
	b.SetBytes(int64(len(basePayload)))
	b.ResetTimer()
	for range b.N {
		h.stampAndFanOut(env, now)
		// Drain send channels outside the timed region so only stampAndFanOut
		// is measured. Without draining, the next iteration's send would block
		// once the buffered channel fills, inflating the measured time.
		b.StopTimer()
		for c := range h.clients {
			select {
			case <-c.send:
			default:
			}
		}
		b.StartTimer()
	}
}

// BenchmarkRoute exercises the hub's route code paths for both throttled and
// non-throttled message types, with varying client counts, so hot-path
// regressions in the routing switch and stampAndFanOut fan-out are detectable.
func BenchmarkRoute_Alert_1Client(b *testing.B)        { benchRoute(b, 1, false) }
func BenchmarkRoute_Alert_10Clients(b *testing.B)      { benchRoute(b, 10, false) }
func BenchmarkRoute_Alert_100Clients(b *testing.B)     { benchRoute(b, 100, false) }
func BenchmarkRoute_Throttled_1Client(b *testing.B)    { benchRoute(b, 1, true) }
func BenchmarkRoute_Throttled_10Clients(b *testing.B)  { benchRoute(b, 10, true) }
func BenchmarkRoute_Throttled_100Clients(b *testing.B) { benchRoute(b, 100, true) }

func benchRoute(b *testing.B, n int, throttled bool) {
	b.Helper()
	h := NewHub(slog.New(slog.NewTextHandler(io.Discard, nil)))

	for range n {
		c := &Client{
			hub:  h,
			send: make(chan []byte, defaultChannelBuffer),
		}
		h.clients[c] = struct{}{}
	}

	var env *starv1.STAREnvelope
	var ts typeThrottleState
	t0 := time.Now()

	if throttled {
		// Telemetry with lastSent == t0 => always throttled within the interval.
		ts.telemetry = t0
		env = &starv1.STAREnvelope{
			Payload: &starv1.STAREnvelope_Telemetry{Telemetry: &starv1.TelemetryData{}},
		}
	} else {
		// Alert is never throttled.
		env = &starv1.STAREnvelope{
			Payload: &starv1.STAREnvelope_Alert{Alert: &starv1.Alert{}},
		}
	}

	b.ReportAllocs()
	b.ResetTimer()
	for range b.N {
		h.route(env, t0, &ts, telemetryInterval, lidarInterval)
		// Drain send channels outside the timed region.
		b.StopTimer()
		for c := range h.clients {
			select {
			case <-c.send:
			default:
			}
		}
		b.StartTimer()
	}
}
