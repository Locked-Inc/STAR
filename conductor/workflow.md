# Project Workflow

## Guiding Principles

1. **The Plan is the Source of Truth:** All work must be tracked in `plan.md`
2. **The Tech Stack is Deliberate:** Changes to the tech stack must be documented in `tech-stack.md` *before* implementation
3. **Branch-Based Development:** ALWAYS work on a dedicated feature/bugfix branch. NEVER commit directly to the main branch.
4. **Pull Request Workflow:** Once a track or a significant set of tasks is complete and verified, create a Pull Request for review and merging.
5. **Test-Driven Development:** Write unit tests before implementing functionality.
6. **Mandatory Testing:** Always ensure all tests pass before committing. Never merge code that breaks existing functionality.
7. **High Code Coverage:** Aim for >80% code coverage for all modules.
8. **Documentation is First-Class:** Significant changes to infrastructure or core logic MUST be documented in the `docs/` folder.
9. **Non-Interactive & CI-Aware:** Prefer non-interactive commands. Use `CI=true` for watch-mode tools (tests, linters) to ensure single execution.

## Task Workflow

All tasks follow a strict lifecycle:

### Standard Task Workflow

1. **Select Task:** Choose the next available task from `plan.md` in sequential order.

2. **Branching:** If starting a new track or major feature, create a new branch: `git checkout -b feature/track-name`.

3. **Mark In Progress:** Before beginning work, edit `plan.md` and change the task from `[ ]` to `[~]`.

4. **Write Failing Tests (Red Phase):**
   - Create a new test file for the feature or bug fix.
   - Write one or more unit tests that clearly define the expected behavior.
   - **CRITICAL:** Run the tests and confirm that they fail as expected.

5. **Implement to Pass Tests (Green Phase):**
   - Write the minimum amount of application code necessary to make the failing tests pass.
   - Run the test suite again and confirm that all tests now pass.

6. **Refactor:**
   - Refactor code for clarity and performance while keeping tests passing.

7. **Verify Coverage:** Run coverage reports. Target: >80% coverage for new code.

8. **Document Changes (Infrastructure & Core):**
   - If the change affects infrastructure, communication protocols, or core logic:
     - Analyze the `docs/` directory and `docs/sections/` to identify relevant `.tex` files.
     - Update the appropriate LaTeX documentation (e.g., `star_documentation.tex` or its sub-sections).
     - Ensure the `star_documentation.pdf` is recompiled if necessary.
   - Update `tech-stack.md` if any architectural deviations occurred.

9. **Commit Code Changes:**
   - Stage all code and documentation changes.
   - Commit with a clear message: `feat(scope): Description`.

10. **Attach Task Summary with Git Notes:**
    - Attach a detailed summary to the commit using `git notes add -m "<summary>" <commit_hash>`.

11. **Update Plan:** Mark the task as `[x]` in `plan.md` and append the commit SHA.

12. **PR Creation:** Once the track is complete and all tasks are marked `[x]`, push the branch and create a Pull Request.

### Phase Completion Verification and Checkpointing Protocol

**Trigger:** Executed immediately after a task finishes a phase in `plan.md`.

1. **Automated Verification:** Run the full test suite (`go test ./...`, etc.) and ensure 100% pass rate.
2. **Manual Verification:** Follow the manual verification steps defined in the task/phase.
3. **Documentation Audit:** Verify that all relevant documentation in `docs/` has been updated and reflects the new state.
4. **User Feedback:** Present the results to the user and await explicit approval.
5. **Checkpoint:** Create a checkpoint commit and tag the SHA in `plan.md`.

## Quality Gates

- [ ] All tests pass
- [ ] Code coverage >80%
- [ ] Documentation in `docs/` updated for infrastructure changes
- [ ] Code follows style guides in `conductor/code_styleguides/`
- [ ] No security vulnerabilities (no hardcoded secrets)
- [ ] Branch is ready for PR

## Development Commands

### Go (Gateway)
- **Test:** `go test ./...`
- **Build:** `go build ./cmd/star-gateway`
- **Lint:** `golangci-lint run`

### C (Firmware)
- **Build:** `./build.sh` (or `cmake --build build`)
- **Flash:** `./flash.sh`

### Documentation (LaTeX)
- **Compile:** `pdflatex docs/star_documentation.tex` (Run from root)

## Testing Requirements

- **Unit Tests:** Mandatory for all new logic.
- **Integration Tests:** Required for inter-service communication (e.g., Gateway ↔ ROS2).
- **Regression Testing:** Run existing tests to ensure no breaking changes.

## Definition of Done

1. Code implemented and passes all tests.
2. Branch created and updated.
3. Documentation in `docs/` and `tech-stack.md` updated.
4. Changes committed with Git Notes.
5. Plan updated with SHA.
6. Ready for PR or Phase Checkpoint.