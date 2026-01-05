# Plan: PWA Gamepad Controller (`pwa_controller_20260105`)

## Phase 1: Environment & Protocol Setup
- [x] Task: Define Gamepad Control Protobuf message (fields: linear_vel, angular_vel, timestamp) in `star-proto/proto/controller.proto`
- [x] Task: Generate code for Go and TypeScript using `buf generate`
- [x] Task: Verify generated artifacts in `star-gateway` and `star-ui`
- [x] Task: Conductor - User Manual Verification 'Phase 1: Environment & Protocol Setup' (Protocol in workflow.md) <!-- 710c52e0c1ba4dba03d1ed26f54e8f4e3210b1f5 -->

## Phase 2: Gateway WebSocket Implementation (Go)
- [x] Task: Create `internal/controller` package in `star-gateway` to handle arcade drive logic
- [x] Task: Implement `/ws/controller` WebSocket endpoint in `star-gateway/cmd/star-gateway`
- [x] Task: Write unit tests for WebSocket message deserialization (Red Phase)
- [x] Task: Implement message handler to pass tests (Green Phase)
- [x] Task: Implement safety watchdog: Auto-stop robot if no packet received for >200ms
- [x] Task: Add logging for received control packets
- [x] Task: Conductor - User Manual Verification 'Phase 2: Gateway WebSocket Implementation (Go)' (Protocol in workflow.md) <!-- 215dab018859150692ac7de3e6691460bdac3529 -->

## Phase 3: UI Gamepad Integration (TypeScript)
- [x] Task: Implement `useGamepad` custom hook in `star-ui/src/hooks/useGamepad.ts`
- [x] Task: Write unit tests for `useGamepad` using mocked `navigator.getGamepads` (Red Phase)
- [x] Task: Implement polling loop and connection logic to pass tests (Green Phase)
- [x] Task: Create `ControllerView` component with "Press any button" prompt and stick visualization
- [x] Task: Conductor - User Manual Verification 'Phase 3: UI Gamepad Integration (TypeScript)' (Protocol in workflow.md) <!-- a1a79d620996841d955410d522b9c61050595063 -->

## Phase 4: UI WebSocket & Safety Integration (TypeScript)
- [x] Task: Implement `ControllerService` to manage the `/ws/controller` connection
- [x] Task: Integrate `protobuf.js` for message serialization in the 60Hz loop
- [x] Task: Write integration tests for WebSocket streaming (Red Phase)
- [x] Task: Implement streaming logic to pass tests (Green Phase)
- [x] Task: Verify "Zero-Allocation" compliance using Chrome Performance tab (ensure no major GC spikes)
- [x] Task: Implement "Dead Man's Switch" (send stop command on disconnect/unplug)
- [x] Task: Verify safety logic with unit tests
- [x] Task: Conductor - User Manual Verification 'Phase 4: UI WebSocket & Safety Integration (TypeScript)' (Protocol in workflow.md) <!-- 1a4a4bdba4907f05ade2d5fed5b98078162cb895 -->

## Phase 5: Integration & Documentation
- [x] Task: Perform end-to-end manual test with Retroid Pocket 2S (or Chrome Emulator)
- [x] Task: Update `star-ui/UI_GUIDE.pdf` with controller instructions
- [x] Task: Update `conductor/tech-stack.md` with WebSocket details
- [x] Task: Add JSDoc to critical performance-sensitive paths (polling loop)
- [x] Task: Conductor - User Manual Verification 'Phase 5: Integration & Documentation' (Protocol in workflow.md) <!-- 73a5a342f23b672d2c8862b84ba0293cd7526c57 -->
