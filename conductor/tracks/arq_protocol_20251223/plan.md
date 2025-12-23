# Track Plan: ARQ Protocol Implementation

## Phase 1: Test Infrastructure and Basic Logic
- [ ] Task: Create mock dependencies for `Transport`, `Encoder`, and `Decoder` in `mock_transport_test.go` to facilitate isolated testing.
- [ ] Task: Create `arq_test.go` and implement tests for the `Send` method:
    - [ ] Sub-task: Write test for successful transmission (receiving ACK).
    - [ ] Sub-task: Implement test logic.
- [ ] Task: Implement tests for `Send` retransmission logic:
    - [ ] Sub-task: Write test for timeout and retry.
    - [ ] Sub-task: Write test for NACK and retry.
    - [ ] Sub-task: Write test for max retries exceeded.
    - [ ] Sub-task: Implement test logic.
- [ ] Task: Conductor - User Manual Verification 'Test Infrastructure and Basic Logic' (Protocol in workflow.md)

## Phase 2: Receive Logic and Edge Cases
- [ ] Task: Implement tests for the `Receive` method:
    - [ ] Sub-task: Write test for successful reception (sending ACK).
    - [ ] Sub-task: Write test for duplicate frame detection (sending ACK, discarding data).
    - [ ] Sub-task: Write test for out-of-order frame (sending NACK).
    - [ ] Sub-task: Implement test logic.
- [ ] Task: Implement tests for Sequence Number Wraparound:
    - [ ] Sub-task: Write test for sequence wrap from `DefaultSequenceMax` to 0.
    - [ ] Sub-task: Implement test logic.
- [ ] Task: Run `go test -race` and fix any detected race conditions.
- [ ] Task: Conductor - User Manual Verification 'Receive Logic and Edge Cases' (Protocol in workflow.md)

## Phase 3: Final Verification and Documentation
- [ ] Task: Generate code coverage report and ensure >80% coverage.
- [ ] Task: Verify thread safety of `Reset` and state getters.
- [ ] Task: Update `docs/sections/01_nanopb_protocol.tex` if any implementation details diverged from the spec.
- [ ] Task: Conductor - User Manual Verification 'Final Verification and Documentation' (Protocol in workflow.md)
