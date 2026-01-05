# Specification: PWA Gamepad Controller (`pwa_controller_20260105`)

## Overview
Implement a mobile-friendly Progressive Web App (PWA) controller within the existing `star-ui` project to enable low-latency robot control using the HTML5 Gamepad API. This track focuses on delivering a "zero-install" MVP targeted at the Retroid Pocket 2S, allowing for remote operation via a web browser over a local network.

## User Story
As a robot operator, I want to use a physical gamepad connected to my mobile device/handheld console to control the STAR robot through a web interface, so that I can have precise, tactile control without installing native applications.

## Functional Requirements
- **Gamepad Integration:**
    - Detect and initialize gamepads using the HTML5 Gamepad API.
    - Support for standard gamepad mappings.
    - Polling loop (approx. 60Hz) using `requestAnimationFrame` to capture stick and button states.
    - **Security Constraint:** UI must prompt user to "Press any button on controller" to activate the API per browser security rules.
- **Control Mapping (Arcade Drive):**
    - Map **Left Stick Y-axis** to Linear Velocity ($v$).
    - Map **Left Stick X-axis** to Angular Velocity ($\omega$).
- **Real-Time Communication:**
    - Establish a dedicated WebSocket connection to `star-gateway` at `/ws/controller`.
    - Serialize gamepad state into Protobuf messages (using `protobuf.js`).
    - Stream control packets at 60Hz.
    - **Dead Man's Switch:** Automatically "zero out" and send stop commands if the WebSocket connection is lost or the controller disconnects.
- **UI/UX:**
    - Add a new "Controller" view/route to `star-ui`.
    - Provide visual feedback for gamepad connection status and connection prompts.
    - Display real-time stick values for debugging.

## Non-Functional Requirements
- **Latency:** Aim for <50ms end-to-end latency (Gamepad input to Gateway processing).
- **Portability:** Must run on Android Chrome (Retroid Pocket 2S) and modern desktop browsers.
- **Zero-Allocation:** Minimize memory allocations in the 60Hz polling loop to prevent GC pauses.
- **Testability:** All hooks and components must be testable via Jest with mocked Gamepad API.

## Testing Strategy
- **Unit Tests (Jest):**
    - Mock `navigator.getGamepads` to simulate controller connection, disconnection, and input changes.
    - Test the `useGamepad` hook in isolation to verify state updates and polling logic.
    - Verify correct Protobuf message serialization.
- **Integration Tests:**
    - Verify the interaction between the Gamepad hook and the WebSocket service (mocked WS server).
    - Ensure 60Hz loop correctly triggers message sending.
    - **Safety Test:** Simulate WebSocket disconnection and verify stop commands are generated.
- **Manual Verification:**
    - Verify on physical Retroid Pocket 2S in "Gamepad Mode".

## Documentation
- **User Guide:** Update `star-ui/UI_GUIDE.pdf` (or create a markdown equivalent) explaining how to connect the gamepad and access the controller page.
- **Architecture:** Update `conductor/tech-stack.md` to reflect the new WebSocket control channel.
- **Code:** Add JSDoc comments to critical logic (polling loop, WebSocket handlers).

## Acceptance Criteria
- [ ] `useGamepad` hook is unit tested with >80% coverage using mocked API.
- [ ] UI explicitly prompts the user to press a button to activate the controller.
- [ ] Gamepad is successfully detected in the `star-ui` interface.
- [ ] Moving the left stick generates Arcade Drive commands (linear/angular).
- [ ] **Safety:** Robot stops moving if the WebSocket disconnects or the controller is unplugged.
- [ ] `star-gateway` correctly receives and logs control packets from the PWA.
- [ ] The PWA remains responsive during continuous 60Hz streaming.
- [ ] Documentation updated as per requirements.

## Out of Scope
- Custom button mapping configuration (hardcoded for MVP).
- Video streaming integration (separate track).
- Multi-controller support.
