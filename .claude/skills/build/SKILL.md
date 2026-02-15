---
name: build
description: Build any STAR subsystem (proto, gateway, firmware, ROS2, UI, MATLAB, or generate documentation)
disable-model-invocation: false
allowed-tools: [Read, Bash]
context: main
---

# Build Skill

Build commands for all STAR subsystems. Use this when you need to compile, generate code, run tests, or build documentation.

## Protocol Buffers (`star-proto/`)

```bash
# Lint and format (run from star-proto/)
cd star-proto
buf lint proto/
buf format --diff proto/

# Generate code for all targets (run from workspace root)
buf generate star-proto/proto

# Alternative: Use Makefile
make proto-gen

# Run Go tests
cd star-proto/tests/go && go test ./...
```

## Gateway Service (`star-gateway/`)

```bash
# Build
cd star-gateway
go build ./cmd/star-gateway

# Test
go test ./...

# Run (on RPi5)
./star-gateway
```

## MATLAB (`matlab/`)

```bash
# Run in MATLAB
motor_model_1st_order   # Estimate transfer function
pid_design_velocity     # Design PID controller
pid_discretize          # Generate discrete coefficients for RX72N
```

## ROS2 Code Quality (`star-ros2/`)

```bash
# Format all ROS2 C++ files
./scripts/format-ros2.sh

# Check formatting (CI mode)
./scripts/format-ros2.sh --check

# Run automated code review
./scripts/review-ros2.sh

# Generate review report to file
./scripts/review-ros2.sh --report review.txt

# Install pre-commit hook (recommended)
cp scripts/pre-commit-ros2 .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

## Code Review (`coderabbit`)

CodeRabbit is an AI-powered code review tool that provides detailed feedback and fix suggestions for CI/dev workflows.

```bash
# AI-powered code review with detailed feedback and fix suggestions
coderabbit review --plain

# Token-efficient mode (minimal output for AI agents)
coderabbit review --prompt-only

# Review specific files
coderabbit review --plain path/to/file.go

# Shorthand alias
cr review --plain
```

**When to use:**
- Before commits to catch issues early
- After implementing features to get improvement suggestions
- During refactoring to ensure code quality
- Use `--plain` for human-readable analysis
- Use `--prompt-only` when working with AI agents to save tokens

**IMPORTANT - Automated Review Workflow:**
When completing plan mode implementations or any significant code changes:
1. Consider running `coderabbit review --prompt-only` after implementing changes
2. Optionally review feedback and apply suggested changes as appropriate
3. Iteratively improve your code based on the feedback
4. You may skip for trivial changes (typos, docs)

**Note:** Must run from repository root directory.
