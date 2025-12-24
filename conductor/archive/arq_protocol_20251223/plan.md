# Track Plan: ARQ Protocol Implementation

## Phase 1: Test Infrastructure and Basic Logic [checkpoint: e6d10ab7]
- [x] Task: Create mock dependencies for `Transport`, `Encoder`, and `Decoder` in `mock_transport_test.go` to facilitate isolated testing. f57e48d3
- [x] Task: Create `arq_test.go` and implement tests for the `Send` method: 33f07e6a
    - [x] Sub-task: Write test for successful transmission (receiving ACK).
    - [x] Sub-task: Implement test logic.
- [x] Task: Implement tests for `Send` retransmission logic: 33f07e6a
    - [x] Sub-task: Write test for timeout and retry.
    - [x] Sub-task: Write test for NACK and retry.
    - [x] Sub-task: Write test for max retries exceeded.
    - [x] Implement test logic.
- [x] Task: Conductor - User Manual Verification 'Test Infrastructure and Basic Logic' (Protocol in workflow.md) e6d10ab7

## Phase 2: Receive Logic and Edge Cases [checkpoint: e6d10ab7]
- [x] Task: Implement tests for the `Receive` method: 33f07e6a
    - [x] Sub-task: Write test for successful reception (sending ACK).
    - [x] Sub-task: Write test for duplicate frame detection (sending ACK, discarding data).
    - [x] Sub-task: Write test for out-of-order frame (sending NACK).
    - [x] Implement test logic.
- [x] Task: Implement tests for Sequence Number Wraparound: 33f07e6a
    - [x] Sub-task: Write test for sequence wrap from `DefaultSequenceMax` to 0.
    - [x] Implement test logic.
- [x] Task: Run `go test -race` and fix any detected race conditions. 33f07e6a
- [x] Task: Conductor - User Manual Verification 'Receive Logic and Edge Cases' (Protocol in workflow.md) e6d10ab7

## Phase 3: Final Verification and Documentation
- [x] Task: Generate code coverage report and ensure >80% coverage. 33f07e6a
- [x] Task: Verify thread safety of `Reset` and state getters. 33f07e6a
- [x] Task: Update `docs/sections/01_nanopb_protocol.tex` if any implementation details diverged from the spec. 22971bf3
- [x] Task: Conductor - User Manual Verification 'Final Verification and Documentation' (Protocol in workflow.md) be69687d
