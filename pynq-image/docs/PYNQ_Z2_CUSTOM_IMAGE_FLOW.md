# PYNQ-Z2 Custom Image Design Flow

This document captures the end-to-end flow for crafting the STAR robot's custom PYNQ-Z2 SD card image. It links the build scripts, board configuration, and verification steps so anyone on the team can reproduce or evolve the image reliably.

## Flow at a Glance
1. **Prepare the build environment** on an Ubuntu 18.04/20.04 host and source required Xilinx toolchains.
2. **Stage the board definition** under `board-config/STAR-Z2` so the build system knows which overlays, packages, and boot files to include.
3. **Prime the prebuilt assets** (preferred) or commit to a full build depending on whether Stage 1/2 artifacts are available.
4. **Run the build orchestration** via `scripts/build.sh` or `make` and monitor the PYNQ `sdbuild` stages.
5. **Inspect build artifacts and logs**, run optional verification, then package the image for flashing.
6. **Flash and validate on hardware**, cycling findings back into board configuration or packages.

The sections below expand each step and highlight where customizations live inside this repository.

## 1. Prepare the Build Environment
- Host requirements: Ubuntu 18.04/20.04, 16–32GB RAM, 200GB SSD (see `README.md` for full specs).
- Install build dependencies and helper tools with `./scripts/setup-env.sh`.
- Source Xilinx toolchains that match the targeted PYNQ release:
  ```bash
  source /opt/Xilinx/Vivado/2022.1/settings64.sh
  source /opt/petalinux/2022.1/settings.sh
  ```
- Optional but recommended: build inside the Docker image defined under `scripts/docker/` for reproducible toolchains.

## 2. Stage the Board Definition
- The STAR-specific board lives in `board-config/STAR-Z2/`.
- Key touchpoints in `board-config/STAR-Z2/STAR-Z2.spec`:
  - `STAGE4_PACKAGES_*` for OS-level packages.
  - `STAGE4_PYTHON_PACKAGES_*` for Python dependencies deployed into the rootfs.
  - Overlay assignments pointing to `board-config/STAR-Z2/overlays/`.
- Custom package recipes reside in `board-config/STAR-Z2/packages/` with per-feature setup scripts (LiDAR, vision, networking, robot control).
- When adding new functionality, update the spec and include any support files so they are layered during Stage 4.

## 3. Prime Prebuilt Assets (Fast Path)
- The PYNQ `sdbuild` flow breaks into four stages. Stages 1–2 are time-consuming (rootfs bootstrap) and ideally reused.
- Drop or symlink prebuilt assets into `PYNQ/sdbuild/prebuilt/`:
  - `pynq_rootfs.arm.tar.gz` for the Stage 2 root filesystem (linking to `PYNQ/sdbuild/build/jammy.arm.stage2.tar.gz` is common).
  - `pynq_sdist.tar.gz` for the prebuilt PYNQ Python distribution (already tracked in this repo).
- If no prebuilt rootfs is available, expect a full build and plan for several hours.

## 4. Execute the Build
- Kick off from the repository root:
  ```bash
  ./scripts/build.sh prebuilt   # Uses Stage 1/2 cache when available
  ./scripts/build.sh full       # Forces Stage 1-4 rebuild
  # or equivalently
  make BOARDS=STAR-Z2 [PREBUILT=<img>]
  ```
- What to expect from each stage:
  | Stage | Description | Repository Inputs |
  |-------|-------------|-------------------|
  | 1 | Toolchain bootstrap & base system prep | PYNQ core scripts |
  | 2 | Root filesystem assembly | `PYNQ/sdbuild/prebuilt` (optional) |
  | 3 | Install PYNQ framework on the rootfs | `pynq_sdist.tar.gz` |
  | 4 | STAR-Z2 customization (packages, overlays, boot files) | `board-config/STAR-Z2/` |
- Monitor terminal output or tail generated logs (`PYNQ/sdbuild/output/build.log`).

## 5. Review Artifacts & Verify
- Successful builds drop artifacts under `PYNQ/sdbuild/output/`:
  - `STAR-Z2-<version>.img` — bootable SD card image.
  - `STAR-Z2-<version>.img.md5` — checksum.
  - `build.log` — full build transcript for diagnostics.
- Optional validation scripts:
  - `verify-image.sh` ensures image structure matches expectations.
  - Custom smoke tests can be staged under `test-scripts/` before flashing.
- Capture notable deltas in `CUSTOMIZATION_CHANGES.md` so image variations are traceable.

## 6. Flash & Hardware Validation
- Flash helper: `./flash.sh /dev/sdX` (wraps `dd` with safe defaults).
- Post-flash checklist:
  1. Boot with JP4 set to SD mode and confirm UART console output.
  2. Verify network reachability and SSH access (`star@<board-ip>`).
  3. Exercise critical overlays (`python3 -c "from pynq import Overlay; Overlay('base.bit')"`).
- Feed any regressions back into board packages or scripts, then iterate through the flow again.

## Automation & Release Notes
- CI: `.github/workflows/build-image.yml` can build and archive images on pushes, PRs, or tags; ensure caches are configured when enabling it.
- Versioning: follow the `vMAJOR.MINOR.PATCH` scheme documented in `README.md` and tag releases alongside generated images.
- Release checklist:
  - Update `PROJECT_STATUS.md` with the image version, toolchain versions, and notable changes.
  - Store generated artifacts in the internal release bucket or attach to GitHub releases.

## Reference Materials
- `docs/BUILD_INSTRUCTIONS.md` — detailed command-by-command guide.
- `docs/XILINX_TOOLS_SETUP.md` — validated Vivado/PetaLinux installation process.
- `BUILD_COMMANDS.md` — shortcuts for leveraging existing Stage 2 builds.
- `QUICKSTART.md` — minimal path to first image.
- Official PYNQ documentation: <https://pynq.readthedocs.io/>.
