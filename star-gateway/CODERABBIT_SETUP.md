# CodeRabbit Path Instructions Setup

This file contains path-specific instructions for CodeRabbit code review configuration.

For each section below:
1. Copy the **Path** glob pattern into the "Path" field
2. Copy the **Instructions** text into the "Instructions" text area

---

## 1. Go Gateway Service

### Path
```
star-gateway/**/*.go
```

### Instructions
```
Review Go gateway service code:
- Follow standard Go conventions (gofmt, golint)
- Verify interfaces are well-defined (Transport, HARQ, FEC)
- Check error handling (all errors must be checked or explicitly ignored)
- Verify layer separation: transport (L1), frame (L2), HARQ/FEC (L3), service (L5)
- Ensure thread-safe operations for concurrent gRPC handlers
- No magic numbers - use named constants
- Follow gateway architecture from docs/sections/07_gateway_architecture.tex
- Test coverage required for all new functions
- Benchmark performance-critical paths (SPI, FEC, HARQ)
```

---

## 2. C Firmware (RX72N)

### Path
```
star-rx72n-firmware/**/*.{c,h}
```

### Instructions
```
Review C firmware code with STRICT compliance checking:

NASA Power of 10 Rules (CRITICAL - REJECT violations):
- Rule 1: No goto, setjmp/longjmp, or recursion
- Rule 2: All loops must have statically provable bounds (use enums for limits)
- Rule 3: Zero malloc/free/realloc (static allocation only)
- Rule 4: Functions must be ≤60 lines
- Rule 5: Minimum 2 assertions per function (pre/post conditions using RX_CHECK macros)
- Rule 8: Enums for ALL integer constants (ZERO TOLERANCE for magic numbers)
- Rule 9: Function pointers ALLOWED for Dependency Inversion Principle (intentional deviation)
- Rule 10: Must compile with -Wall -Wextra -Werror (zero warnings)

Inclusive Terminology (CRITICAL - REJECT violations):
- FORBIDDEN: master/slave, MOSI/MISO, blacklist/whitelist
- REQUIRED: Controller/Peripheral, COPI/CIPO, denylist/allowlist

Code Style (MANDATORY):
- Naming: snake_case for functions/variables, SCREAMING_SNAKE_CASE for macros/enums
- Enums for ALL integer constants including: array indices, bit shifts, protocol offsets, bit masks
- const float ONLY for floating-point (enum cannot represent floats)
- Macros ONLY for: reducing code duplication, conditional compilation, build configuration flags
- Hardware register access: Use inline accessor functions with enum addresses (NEVER macros)
- No backward compatibility shims (project has no releases yet)
- Include guards: STAR_RX72N_FILENAME_H format
- Doxygen: /** for multi-line blocks, /**< for inline comments
- Line limit: 100 characters maximum

Magic Numbers (ZERO TOLERANCE):
- ALL numeric literals must be named enums or const
- This includes: 0, 1, array indices, bit positions, timeouts, buffer sizes
- Example: buf[k_idx_high_byte] not buf[0]
- Example: (val >> k_shift_byte) not (val >> 8)

SOLID Principles:
- Single Responsibility: One module = one purpose (e.g., rx_pid only does PID math)
- Open/Closed: Extensible via config structs (rx_pid_config_t)
- Liskov Substitution: Bus interface implementations interchangeable
- Interface Segregation: Small focused APIs (rx_pid has 7 functions)
- Dependency Inversion: Use function pointer interfaces for hardware abstraction

Validation Requirements:
- Pre-conditions: NULL checks, state validation, range checks
- Post-conditions: Output bounds, invariant validation
- Use RX_CHECK_NULL_PTR, RX_RETURN_ON_ERROR macros
```

---

## 3. ROS2 C++ Code

### Path
```
star-ros2/**/*.{cpp,hpp}
```

### Instructions
```
Review ROS2 C++ code:

Naming Conventions:
- Classes/Types: CamelCase (e.g., StarGatewayBridgeNode)
- Methods/Functions: snake_case (e.g., publish_telemetry, on_configure)
- Member variables: trailing underscore (e.g., telemetry_pub_, timer_)
- Constants: ALL_CAPITALS (e.g., MAX_RETRIES, DEFAULT_TIMEOUT_S)
- Namespaces: star::package_name::

File and Style:
- Headers use .hpp extension (not .h)
- Source files use .cpp extension
- Line limit: 120 characters maximum
- Include guard format: PACKAGE_FILENAME_HPP_
- Braces: Functions on new line, control statements cuddled
- Indentation: 2 spaces (no tabs)
- Filenames: under_scored (e.g., star_gateway_bridge_node.hpp)

ROS2 Patterns:
- Node inheritance: rclcpp::Node for basic nodes, rclcpp_lifecycle::LifecycleNode for safety-critical
- Publishers/Subscribers: Use SharedPtr and std::bind for callbacks
- Timers: Use create_wall_timer with std::chrono
- Logging: RCLCPP_INFO/WARN/ERROR/DEBUG (NEVER printf/cout/std::cerr)
- Throttled logging: RCLCPP_WARN_THROTTLE for high-frequency messages
- Exception handling: Catch exceptions in callbacks to prevent node crashes

Error Handling:
- Throw exceptions for errors (different from C firmware)
- Catch std::exception in all callbacks
- Log errors with RCLCPP_ERROR including context

Documentation:
- Doxygen comments: /** and /**< (same as C firmware)
- Document all public methods, classes, and non-obvious logic
- Include @brief, @param, @return tags

Formatting:
- Must pass clang-format check (star-ros2/.clang-format)
- Run: find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
- CI enforces formatting on all PRs

Inclusive Terminology (CRITICAL - REJECT violations):
- FORBIDDEN: master/slave, MOSI/MISO, blacklist/whitelist
- REQUIRED: Controller/Peripheral, COPI/CIPO, denylist/allowlist
```

---

## 4. Protocol Buffers

### Path
```
star-proto/**/*.proto
```

### Instructions
```
Review Protocol Buffer schemas (Boston Dynamics style):

Style Requirements:
- Proto3 syntax only (proto2 forbidden)
- Line limit: 100 characters maximum
- Indentation: 4 spaces (no tabs)
- Message names: PascalCase (e.g., MotorControlRequest)
- Field names: snake_case (e.g., velocity_mps, motor_id)
- Enum names: PascalCase (e.g., MotorState)
- Enum values: SCREAMING_SNAKE_CASE (e.g., MOTOR_STATE_IDLE)

Mandatory Conventions:
- Enum zero value MUST end with _UNKNOWN (e.g., MOTOR_STATE_UNKNOWN = 0)
- Units: MKS system with suffixes (_mps, _rad, _ma, _celsius, _hz)
- All RPC messages MUST include RequestHeader/ResponseHeader
- Field numbers: Reserve 1-15 for frequent fields (1-byte encoding)

nanopb Constraints (Embedded Target):
- Configure max_size in .options files for all strings and repeated fields
- Example: star.v1.RequestHeader.request_id max_size:64
- No dynamic allocation - all strings/arrays must have fixed max sizes
- Document size constraints in comments

Breaking Changes:
- NEVER change field numbers
- NEVER change field types
- NEVER remove required fields
- OK to add optional fields
- OK to add new enum values (not zero)
- OK to add new messages/services

Validation:
- Must pass: buf lint
- Must pass: buf format
- Must pass: buf build
- PRs: buf breaking check against main branch
- Generate targets: Go, TypeScript, nanopb (all three must build)

Documentation:
- Document all messages, fields, services, and RPCs
- Explain units, ranges, and constraints
- Reference related messages using fully-qualified names
```

---

## 5. LaTeX Documentation

### Path
```
docs/**/*.tex
```

### Instructions
```
Review LaTeX documentation:

Technical Accuracy:
- Verify measurements match actual hardware (voltages, currents, timings)
- Verify pin assignments match schematic/PCB design
- Verify protocol specifications match implementation code
- Cross-reference code when documenting APIs and algorithms

Content Organization:
- Keep docs/sections/*.tex as source of truth (not compiled PDF)
- Use consistent terminology with code (RequestHeader, ResponseHeader)
- Include code examples from actual source files
- Reference source files with relative paths

Inclusive Terminology (CRITICAL - REJECT violations):
- FORBIDDEN: master/slave, MOSI/MISO, blacklist/whitelist
- REQUIRED: Controller/Peripheral, COPI/CIPO, denylist/allowlist

Style:
- Use \texttt{} for code/identifiers
- Use \emph{} for emphasis (not \textbf{} unless critical)
- Tables: Use booktabs package (\toprule, \midrule, \bottomrule)
- Math: Use equation environments for formulas
- Citations: Use \cite{} for references

Consistency:
- Match constant names with code (k_max_payload_size, not MAX_PAYLOAD_SIZE)
- Match function names with code (rx_pid_compute, not rx_pid_update)
- Match type names with code (rx_err_t, not error_t)
```

---

## 6. Markdown Documentation

### Path
```
**/*.md
```

### Instructions
```
Review Markdown documentation:

AI Attribution (CRITICAL - REJECT violations):
- NO "Generated by Claude Code" footers
- NO "Co-Authored-By: Claude" signatures
- NO "Created with AI assistance" disclaimers
- Keep all text professional and human-written
- This applies to: commit messages, PR descriptions, README files, ALL markdown

Inclusive Terminology (CRITICAL - REJECT violations):
- FORBIDDEN: master/slave, MOSI/MISO, blacklist/whitelist
- REQUIRED: Controller/Peripheral, COPI/CIPO, denylist/allowlist

Technical Accuracy:
- Reference .tex files in docs/sections/ for technical details (not PDFs)
- Link to source code files for implementation details
- Use relative paths for internal repository links
- Verify commands/examples are correct and tested

Formatting:
- Use fenced code blocks with language identifiers (```c, ```go, ```bash)
- Use tables for structured information
- Use task lists for checklists (- [ ] item)
- Keep line length readable (aim for 80-100 chars, not strict)

Content:
- Focus on "why" and "what", not just "how"
- Include examples for non-obvious usage
- Document edge cases and limitations
- Link to related documentation
```

---

## 7. CMake Build Files

### Path
```
**/CMakeLists.txt
```

### Instructions
```
Review CMake build configuration:

Project Standards:
- Minimum CMake version: 3.20
- C standard: C11 (set(CMAKE_C_STANDARD 11))
- C++ standard: C++17 (set(CMAKE_CXX_STANDARD 17))
- Position independent code: ON

Compiler Flags (MANDATORY):
- C/C++: -Wall -Wextra -Werror (treat warnings as errors)
- C/C++: -Wpedantic (ISO standard compliance)
- Release: -O2 (or -O3 for non-safety-critical)
- Debug: -g -O0

Target Organization:
- Use target_include_directories(target PRIVATE/PUBLIC) not include_directories()
- Use target_link_libraries(target PRIVATE/PUBLIC) not link_libraries()
- Use target_compile_options(target PRIVATE) not add_compile_options()

Testing:
- Enable testing with enable_testing()
- Add tests with add_test()
- Use CTest for test execution

Dependencies:
- Document all find_package() calls
- Check package availability before use
- Provide clear error messages for missing dependencies
```

---

## 8. Shell Scripts

### Path
```
scripts/**/*.sh
```

### Instructions
```
Review shell scripts:

Safety and Robustness:
- Use bash shebang: #!/usr/bin/env bash
- Enable strict mode: set -euo pipefail
- Quote all variables: "$var" not $var
- Check command existence: command -v foo || error
- Validate inputs before use

Error Handling:
- Check exit codes: if ! command; then error; fi
- Provide meaningful error messages
- Clean up temporary files on error (trap)
- Exit with non-zero on failure

Style:
- Use long options for readability: --verbose not -v
- Document all functions with comments
- Use lowercase for local variables
- Use UPPERCASE for environment variables
- Indent with 2 spaces

Security:
- Never use eval
- Avoid executing user input
- Validate file paths before operations
- Use mktemp for temporary files
```

---

## 9. GitHub Workflows

### Path
```
.github/workflows/*.yml
```

### Instructions
```
Review GitHub Actions workflows:

Security:
- Pin action versions to commit SHA (not @v1 or @latest)
- Minimize permissions using permissions: key
- Never expose secrets in logs
- Use ${{ secrets.NAME }} for sensitive data

Best Practices:
- Use matrix strategy for multi-platform builds
- Cache dependencies (actions/cache)
- Fail fast when appropriate
- Set timeout-minutes to prevent hanging
- Use continue-on-error judiciously

Testing:
- Run tests before build
- Run linters and formatters
- Check for breaking changes (buf breaking for proto)
- Report test results

Artifacts:
- Upload build artifacts for debugging
- Upload test coverage reports
- Set retention-days appropriately
```

---

## 10. Test Files (All Languages)

### Path
```
**/*_test.{go,c,cpp}
```

### Instructions
```
Review test code:

Coverage Requirements:
- Test all public functions
- Test error paths and edge cases
- Test boundary conditions
- Test concurrent access (if applicable)

Test Organization:
- One test file per source file (_test.go, _test.c)
- Group related tests
- Use descriptive test names (TestFunctionName_Scenario_ExpectedBehavior)
- Use table-driven tests for multiple scenarios (Go)

Assertions:
- Use appropriate assertion macros/functions
- Provide descriptive failure messages
- Check all relevant outputs (return value, state changes, side effects)

Mocking:
- Mock external dependencies
- Use dependency injection for testability
- Verify mock expectations

Performance:
- Keep unit tests fast (<100ms per test)
- Use benchmarks for performance-critical code
- Set appropriate timeouts
```

---

## Additional Configuration

### Path Filters (Include)
```
star-gateway/**
star-rx72n-firmware/**
star-ros2/**
star-proto/**
docs/**
scripts/**
matlab/**
*.md
CMakeLists.txt
.github/workflows/**
```

### Path Filters (Exclude)
```
!**/node_modules/**
!**/dist/**
!**/build/**
!**/.cmake/**
!**/gen/**
!**/*.pb.go
!**/*.pb.h
!**/*.pb.c
!**/*.pb.ts
!**/*.pdf
!**/*.png
!**/*.jpg
!**/*.svg
!.git/**
!**/*.log
```

---

## General Review Instructions

Add this to the main CodeRabbit configuration (not path-specific):

```
General Code Review Guidelines for STAR Project:

CRITICAL RULES (REJECT PRs that violate):

1. Inclusive Terminology:
   - FORBIDDEN: master/slave, MOSI/MISO, blacklist/whitelist
   - REQUIRED: Controller/Peripheral, COPI/CIPO, denylist/allowlist
   - This applies to ALL files: code, docs, comments, commit messages

2. No Backward Compatibility:
   - Project has no releases - REJECT compatibility shims
   - Update all call sites directly when refactoring
   - NO deprecated function aliases
   - NO #define redirects for old names

3. AI Attribution:
   - REJECT commits with "Generated by Claude" footers
   - REJECT PRs with "Co-Authored-By: Claude" signatures
   - Keep all messages professional and human-written

4. Magic Numbers (C/C++ only):
   - Zero tolerance for numeric literals in code
   - ALL numbers must be named enums or const
   - Includes: array indices, bit shifts, offsets, masks, timeouts

5. Safety-Critical Standards (RX72N firmware only):
   - Follow NASA Power of 10 rules (see path-specific instructions)
   - Zero dynamic allocation (no malloc/free)
   - No recursion, no goto, no setjmp/longjmp
   - All loops must have compile-time provable bounds

Documentation Sources:
- docs/sections/*.tex - Source of truth for technical specs
- star-rx72n-firmware/CLAUDE.md - Firmware development guide
- star-gateway/CLAUDE.md - Gateway architecture and build guide
- CLAUDE.md (root) - Overall project guidelines

Review Focus Areas:
1. Correctness: Does the code work as intended?
2. Safety: Does it follow safety-critical guidelines?
3. Maintainability: Is the code clear and well-documented?
4. Performance: Are there obvious bottlenecks?
5. Testing: Is there adequate test coverage?
6. Security: Are there vulnerabilities (injection, overflow, etc.)?
```

---

## Suggested Labels Configuration

```
Suggest labels based on:

Component:
- gateway: Changes to star-gateway Go service
- firmware: Changes to star-rx72n-firmware C code
- ros2: Changes to star-ros2 C++ nodes
- proto: Changes to star-proto Protocol Buffers
- docs: Changes to documentation
- build: Changes to CMake, workflows, scripts

Type:
- bug: Bug fixes
- feature: New functionality
- refactor: Code restructuring without behavior change
- test: Test additions or modifications
- docs: Documentation updates
- style: Formatting, naming (no logic change)

Priority:
- critical: Safety violations, security issues, breaking changes
- high: Important bugs, blockers
- medium: Normal priority
- low: Minor improvements, nice-to-have

Standards Compliance:
- nasa-power-of-10: Related to NASA Power of 10 rules
- solid-principles: Related to SOLID design principles
- inclusive-terminology: Terminology updates
- backward-compatibility: Compatibility concerns (should be rare)
```

---

## Setup Instructions

1. Go to your CodeRabbit repository settings
2. Navigate to "Path Instructions" section
3. Click "+ Path Instructions" button
4. For each numbered section above (1-10):
   - Copy the **Path** glob pattern
   - Paste into the "Path" field
   - Copy the **Instructions** text
   - Paste into the "Instructions" text area
   - Click "Save" or "Add"
5. Add the **Path Filters** in the appropriate section
6. Add the **General Review Instructions** to the main configuration
7. Configure **Suggested Labels** as shown above

## Testing the Configuration

After setup, test with a sample PR that:
- Modifies a Go file in star-gateway
- Modifies a C file in star-rx72n-firmware
- Modifies a proto file in star-proto

Verify CodeRabbit applies the correct path-specific instructions for each file type.
