# Plan: PWA Gamepad Controller (`pwa_controller_20260105`)

## Phase 1: Environment & Protocol Setup
- [x] Task: Define Gamepad Control Protobuf message (fields: linear_vel, angular_vel, timestamp) in `star-proto/proto/controller.proto`
- [x] Task: Generate code for Go and TypeScript using `buf generate`
- [x] Task: Verify generated artifacts in `star-gateway` and `star-ui`
- [~] Task: Conductor - User Manual Verification 'Phase 1: Environment & Protocol Setup' (Protocol in workflow.md)

## Phase 2: Gateway WebSocket Implementation (Go)
- [ ] Task: Create `internal/controller` package in `star-gateway` to handle arcade drive logic
- [ ] Task: Implement `/ws/controller` WebSocket endpoint in `star-gateway/cmd/star-gateway`
- [ ] Task: Write unit tests for WebSocket message deserialization (Red Phase)
- [ ] Task: Implement message handler to pass tests (Green Phase)
- [ ] Task: Implement safety watchdog: Auto-stop robot if no packet received for >200ms
- [ ] Task: Add logging for received control packets
- [ ] Task: Conductor - User Manual Verification 'Phase 2: Gateway WebSocket Implementation (Go)' (Protocol in workflow.md)

## Phase 3: UI Gamepad Integration (TypeScript)
- [ ] Task: Implement `useGamepad` custom hook in `star-ui/src/hooks/useGamepad.ts`
- [ ] Task: Write unit tests for `useGamepad` using mocked `navigator.getGamepads` (Red Phase)
- [ ] Task: Implement polling loop and connection logic to pass tests (Green Phase)
- [ ] Task: Create `ControllerView` component with "Press any button" prompt and stick visualization
- [ ] Task: Conductor - User Manual Verification 'Phase 3: UI Gamepad Integration (TypeScript)' (Protocol in workflow.md)

## Phase 4: UI WebSocket & Safety Integration (TypeScript)
- [ ] Task: Implement `ControllerService` to manage the `/ws/controller` connection
- [ ] Task: Integrate `protobuf.js` for message serialization in the 60Hz loop
- [ ] Task: Write integration tests for WebSocket streaming (Red Phase)
- [ ] Task: Implement streaming logic to pass tests (Green Phase)
- [ ] Task: Verify "Zero-Allocation" compliance using Chrome Performance tab (ensure no major GC spikes)
- [ ] Task: Implement "Dead Man's Switch" (send stop command on disconnect/unplug)
- [ ] Task: Verify safety logic with unit tests
- [ ] Task: Conductor - User Manual Verification 'Phase 5: UI WebSocket & Safety Integration (TypeScript)' (Protocol in workflow.md)

## Phase 5: Integration & Documentation
- [ ] Task: Perform end-to-end manual test with Retroid Pocket 2S (or Chrome Emulator)
- [ ] Task: Update `star-ui/UI_GUIDE.pdf` with controller instructions
- [ ] Task: Update `conductor/tech-stack.md` with WebSocket details
- [ ] Task: Add JSDoc to critical performance-sensitive paths (polling loop)
- [ ] Task: Conductor - User Manual Verification 'Phase 5: Integration & Documentation' (Protocol in workflow.md)
