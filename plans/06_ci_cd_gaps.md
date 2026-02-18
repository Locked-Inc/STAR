# CI/CD Gaps

## Status Summary

The CI/CD pipeline has solid coverage for protocol buffers and the gateway, but completely lacks firmware and UI builds. A regression in the RX72N firmware or TypeScript UI would not be caught until someone manually tries to build.

| Gap | Severity | Effort |
|-----|----------|--------|
| Firmware build CI (CMake + GNURX) | HIGH | 4-6 hrs |
| Firmware unit test CI (Unity) | HIGH | 2-4 hrs |
| UI build CI (npm + Vite) | HIGH | 2-3 hrs |
| Documentation build CI (LaTeX) | MEDIUM | 2-3 hrs |
| Top-level orchestration workflow | HIGH | 4-6 hrs |
| ROS2 code review (make blocking) | MEDIUM | 1 hr |
| Docker image build + push CI | MEDIUM | 4-6 hrs |
| Security scanning (SAST) | LOW | 2-4 hrs |

---

## Existing CI/CD Workflows

```
.github/workflows/
├── proto.yml          ✅ 343 lines - 9 jobs (lint, generate, test all targets)
├── ros2.yml           ✅ 276 lines - build + non-blocking linting
├── gateway.yml        ✅ 482 lines - build, test, coverage (65%), security, benchmarks
└── proto-gen.yml      ✅ 65 lines  - standalone proto generation
```

**What's Covered:**
- Protocol Buffers: Lint, build, generate (Go + TypeScript + nanopb), test
- Gateway (Go): Build, test, coverage ≥65%, gosec security, cross-compile, benchmarks
- ROS2: Build + non-blocking lint/review

**What's Missing:**
- RX72N firmware: No CI workflow exists
- UI (TypeScript): No CI workflow exists
- Documentation (LaTeX): No CI workflow exists
- Top-level integration: No workflow that tests everything together

---

## Gap 1: Firmware Build CI (HIGH)

### New Workflow: `.github/workflows/firmware.yml`

```yaml
name: Firmware

on:
  push:
    paths:
      - 'e2-studio-star-rx72n-firmware/**'
      - '.github/workflows/firmware.yml'
  pull_request:
    paths:
      - 'e2-studio-star-rx72n-firmware/**'
      - 'star-proto/gen/nanopb/**'

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: ghcr.io/star-project/star-devenv:latest  # OR use Dockerfile

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install GNURX toolchain
        run: |
          # GNURX is installed in the devcontainer Dockerfile
          # For CI, install from the known location:
          export PATH="/opt/GNURX/bin:$PATH"
          which rx-elf-gcc || echo "GNURX not found, installing..."
          # TODO: Host GNURX somewhere accessible for CI
          # Currently: Download from Renesas (requires account - BAD for CI)
          # Better: Host in project S3 bucket or GitHub Releases

      - name: Build Firmware (Debug)
        run: |
          cd e2-studio-star-rx72n-firmware
          ./build.sh debug

      - name: Build Firmware (Release)
        run: |
          cd e2-studio-star-rx72n-firmware
          ./build.sh release

      - name: Upload Firmware Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: firmware-${{ github.sha }}
          path: |
            e2-studio-star-rx72n-firmware/build/star_rx72n.elf
            e2-studio-star-rx72n-firmware/build/star_rx72n.mot
          retention-days: 30

  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install Build Dependencies
        run: |
          sudo apt-get install -y gcc cmake git

      - name: Run Firmware Unit Tests (Host GCC)
        run: |
          cd e2-studio-star-rx72n-firmware
          ./tests/run_tests.sh

      - name: Upload Test Results
        uses: actions/upload-artifact@v4
        with:
          name: firmware-test-results
          path: e2-studio-star-rx72n-firmware/build/tests/
```

### GNURX Toolchain Problem

The GNURX toolchain requires a Renesas account to download. This blocks automated CI. Solutions:

**Option A:** Cache GNURX in GitHub Actions cache
```yaml
- name: Cache GNURX
  uses: actions/cache@v4
  with:
    path: /opt/GNURX
    key: gnurx-14.2.0.202511
```
(Still requires initial download - needs a CI runner with GNURX pre-installed)

**Option B:** Host GNURX in a private S3 bucket or GitHub Release artifact

**Option C:** Only run unit tests in CI (host GCC), skip cross-compile build in CI
- Unit tests use host GCC, no cross-compiler needed
- Cross-compile build is verified locally only
- **This is the pragmatic short-term solution**

### Recommended: Start with Option C

```yaml
# Simplified firmware.yml - unit tests only (no GNURX needed):
jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install build tools
        run: sudo apt-get install -y gcc g++ cmake git

      - name: Run unit tests
        run: |
          cd e2-studio-star-rx72n-firmware
          ./tests/run_tests.sh

      - name: Parse test results
        run: |
          cd e2-studio-star-rx72n-firmware/build
          ctest --output-on-failure
```

---

## Gap 2: UI Build CI (HIGH)

### New Workflow: `.github/workflows/ui.yml`

```yaml
name: UI

on:
  push:
    paths:
      - 'star-ui/**'
      - '.github/workflows/ui.yml'
  pull_request:
    paths:
      - 'star-ui/**'
      - 'star-proto/gen/typescript/**'

jobs:
  build-and-test:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup Node.js
        uses: actions/setup-node@v4
        with:
          node-version: '22'
          cache: 'npm'
          cache-dependency-path: star-ui/package-lock.json

      - name: Install dependencies
        run: |
          cd star-ui
          npm ci

      - name: TypeScript type check
        run: |
          cd star-ui
          npx tsc --noEmit

      - name: Lint
        run: |
          cd star-ui
          npm run lint

      - name: Run tests
        run: |
          cd star-ui
          npm test -- --reporter=junit --outputFile=test-results.xml

      - name: Build production bundle
        run: |
          cd star-ui
          npm run build

      - name: Upload test results
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: ui-test-results
          path: star-ui/test-results.xml

      - name: Upload build artifact
        uses: actions/upload-artifact@v4
        with:
          name: ui-dist-${{ github.sha }}
          path: star-ui/dist/
          retention-days: 7

  test-coverage:
    runs-on: ubuntu-latest
    needs: build-and-test

    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: '22'
          cache: 'npm'
          cache-dependency-path: star-ui/package-lock.json

      - run: cd star-ui && npm ci

      - name: Generate coverage report
        run: |
          cd star-ui
          npm test -- --coverage

      - name: Check coverage threshold
        run: |
          # Fail if coverage < 50%
          # (Will need to add coverage config to vitest.config.ts)
          cd star-ui
          npm test -- --coverage --coverage.thresholds.lines=50
```

**Note:** This CI will fail until UI tests are written (Gap 5 in plans/04_ui_gaps.md). Set up the workflow first, then write tests.

---

## Gap 3: Documentation Build CI (MEDIUM)

### New Workflow: `.github/workflows/docs.yml`

```yaml
name: Documentation

on:
  push:
    paths:
      - 'docs/**'
      - '.github/workflows/docs.yml'
  pull_request:
    paths:
      - 'docs/**'

jobs:
  build-latex:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install LaTeX
        run: |
          sudo apt-get install -y \
            texlive-full \
            latexmk \
            graphviz \
            plantuml

      - name: Build Documentation
        run: |
          cd docs
          make  # Runs 2-pass pdflatex

      - name: Check for LaTeX warnings
        run: |
          cd docs
          # Fail if there are any LaTeX errors
          grep -i "error" star_documentation.log && exit 1 || true

      - name: Upload PDF
        uses: actions/upload-artifact@v4
        with:
          name: star-documentation-${{ github.sha }}
          path: docs/star_documentation.pdf
          retention-days: 30

  doxygen:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Install Doxygen
        run: sudo apt-get install -y doxygen graphviz

      - name: Generate Doxygen (Firmware)
        run: |
          cd e2-studio-star-rx72n-firmware
          doxygen Doxyfile 2>&1 | tee doxygen_warnings.log

      - name: Check Doxygen warnings
        run: |
          cd e2-studio-star-rx72n-firmware
          # Fail on any Doxygen warnings (enforced by CLAUDE.md)
          grep -i "warning" doxygen_warnings.log && exit 1 || true

      - name: Upload Doxygen HTML
        uses: actions/upload-artifact@v4
        with:
          name: doxygen-firmware-${{ github.sha }}
          path: e2-studio-star-rx72n-firmware/docs/doxygen/html/
          retention-days: 7
```

---

## Gap 4: Top-Level Orchestration Workflow (HIGH)

### New Workflow: `.github/workflows/ci.yml`

This is the "master" CI workflow that runs all subsystems and provides a single status check.

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  # Detect which components changed
  changes:
    runs-on: ubuntu-latest
    outputs:
      firmware: ${{ steps.filter.outputs.firmware }}
      gateway: ${{ steps.filter.outputs.gateway }}
      ui: ${{ steps.filter.outputs.ui }}
      proto: ${{ steps.filter.outputs.proto }}
      ros2: ${{ steps.filter.outputs.ros2 }}
      docs: ${{ steps.filter.outputs.docs }}
    steps:
      - uses: actions/checkout@v4
      - uses: dorny/paths-filter@v3
        id: filter
        with:
          filters: |
            firmware:
              - 'e2-studio-star-rx72n-firmware/**'
            gateway:
              - 'star-gateway/**'
              - 'star-proto/gen/go/**'
            ui:
              - 'star-ui/**'
              - 'star-proto/gen/typescript/**'
            proto:
              - 'star-proto/**'
            ros2:
              - 'star-ros2/**'
            docs:
              - 'docs/**'

  # Run subsystem CIs in parallel
  proto-ci:
    needs: changes
    if: needs.changes.outputs.proto == 'true'
    uses: ./.github/workflows/proto.yml

  gateway-ci:
    needs: changes
    if: needs.changes.outputs.gateway == 'true'
    uses: ./.github/workflows/gateway.yml

  firmware-ci:
    needs: changes
    if: needs.changes.outputs.firmware == 'true'
    uses: ./.github/workflows/firmware.yml

  ui-ci:
    needs: changes
    if: needs.changes.outputs.ui == 'true'
    uses: ./.github/workflows/ui.yml

  ros2-ci:
    needs: changes
    if: needs.changes.outputs.ros2 == 'true'
    uses: ./.github/workflows/ros2.yml

  # Final status check
  ci-success:
    needs: [proto-ci, gateway-ci, firmware-ci, ui-ci, ros2-ci]
    if: always()
    runs-on: ubuntu-latest
    steps:
      - name: Check all CI passed
        run: |
          if [[ "${{ needs.proto-ci.result }}" == "failure" ]] || \
             [[ "${{ needs.gateway-ci.result }}" == "failure" ]] || \
             [[ "${{ needs.firmware-ci.result }}" == "failure" ]] || \
             [[ "${{ needs.ui-ci.result }}" == "failure" ]] || \
             [[ "${{ needs.ros2-ci.result }}" == "failure" ]]; then
            echo "One or more CI jobs failed"
            exit 1
          fi
          echo "All CI jobs passed (or skipped)"
```

---

## Gap 5: Make ROS2 Code Review Blocking (MEDIUM)

### Problem

In `ros2.yml`, the code review job has `continue-on-error: true`:

```yaml
# Current (non-blocking):
code-review:
  continue-on-error: true   # ← Allows merging even if review finds issues
  steps:
    - run: ./scripts/review-ros2.sh
```

### Fix

```yaml
# After ROS2 infrastructure stabilizes:
code-review:
  # Remove continue-on-error to make it blocking
  steps:
    - run: ./scripts/review-ros2.sh
```

Track this in the ROS2 implementation status document.

---

## Summary: CI/CD Matrix

| Component | Build CI | Test CI | Coverage | Lint | Docs CI |
|-----------|----------|---------|----------|------|---------|
| **Proto** | ✅ | ✅ Go/TS/nanopb | N/A | ✅ buf | N/A |
| **Gateway** | ✅ | ✅ 65%+ | ✅ Enforced | ✅ golangci | N/A |
| **ROS2** | ✅ | ✅ | N/A | ⚠️ Non-blocking | N/A |
| **Firmware** | ❌ Missing | ❌ Missing | N/A | N/A | ❌ Missing |
| **UI** | ❌ Missing | ❌ Missing | N/A | ❌ Missing | N/A |
| **Docs (LaTeX)** | ❌ Missing | N/A | N/A | N/A | N/A |
| **Docs (Doxygen)** | ❌ Missing | N/A | N/A | N/A | N/A |
| **Top-Level** | ❌ Missing | N/A | N/A | N/A | N/A |
