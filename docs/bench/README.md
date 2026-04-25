# STAR bench setup

End-to-end workflow for building, flashing, and verifying RX72N firmware
on TOM's breakout board from an Apple Silicon Mac.

## What's on the bench

| Component                                    | Role                                  |
|----------------------------------------------|---------------------------------------|
| Mac (macOS 15 arm64)                         | Build host, flash host, capture host  |
| Renesas E2 Lite (USB `045B:82A0`)            | JTAG/FINE programmer for the RX72N    |
| RX72N on TOM's breakout PCB                  | Device under test                     |
| 7x Digilent Analog Discovery 2 (`0403:6014`) | 112 digital capture channels (7 x 16) |
| Raspberry Pi 5 over USB or ASIX USB-Ethernet | Robot control host (active)           |

## Tooling layout

Everything host-side is native arm64 -- no Rosetta, no emulation.

| Tool                          | Path                                            | Purpose                             |
|-------------------------------|-------------------------------------------------|-------------------------------------|
| `devcontainer` CLI            | `~/.local/bin/devcontainer`                     | Run the Linux/amd64 build container |
| `rfp-cli` v3.22.00            | `~/tools/rfp-cli/bin/rfp-cli` (wrapper)         | Flash the RX72N via E2 Lite         |
| WaveForms `dwf.framework`     | `/Library/Frameworks/dwf.framework` (3.25.1)    | AD2 runtime for `pydwf`             |
| `pydwf` 1.1.19 + Python venv  | `star-rx72n-firmware/gpio_test/host/venv/`      | AD2 capture + verify                |
| GNURX 14.2.0.202511           | inside dev container at `/opt/gnurx/bin/`       | `rx-elf-gcc` cross-compiler         |

The dev container (`.devcontainer/devcontainer.json` -> root `Dockerfile`)
forces `linux/amd64` under QEMU so the GNURX installer runs. That's slow
on arm64 hosts but the cached image is pre-built (`vsc-star-...`) so
`devcontainer up` is a fast container start, not a full build.

## Flashing workflow

```bash
# 1. Make sure the dev container is up (builds are cached; this is fast)
devcontainer up --workspace-folder <repo-root>

# 2. Cross-compile in the container
devcontainer exec --workspace-folder <repo-root> bash -lc \
    'cd star-rx72n-firmware/gpio_test && make'

# 3. Flash from the host (native arm64 rfp-cli talks to E2 Lite directly)
cd star-rx72n-firmware/gpio_test
make flash
```

Why the split: macOS Docker Desktop cannot pass USB devices into a
container, so `rfp-cli` can only see the E2 Lite from the host side.
GNURX for macOS does not ship a sane native arm64 toolchain, so
cross-compilation stays in the Linux container.

## Verification workflow

`gpio_test` drives every GPIO pin in sequence; the AD2 array (7 units,
112 channels) captures the transitions; a host script matches them
against the expected firmware order. See
[`../../star-rx72n-firmware/gpio_test/README.md`](../../star-rx72n-firmware/gpio_test/README.md)
for the detailed firmware architecture and `gpio_verify.py` modes.

Quick run:

```bash
cd star-rx72n-firmware/gpio_test/host
./venv/bin/python3 gpio_verify.py --verbose           # full verify
./venv/bin/python3 gpio_verify.py --auto-map          # re-derive wiring
```

Current wiring is recorded in
[`ad2_wiring.md`](ad2_wiring.md) (one table per AD2 plus a
combined paste-ready `CHANNEL_TO_PIN` dict).

## Artifacts in this directory

```text
docs/bench/
  README.md          -- this file (bench overview + workflow)
  ad2_wiring.md      -- AD2 probe-to-breakout-pin map + CHANNEL_TO_PIN
  kicad/             -- KiCad exports of the STAR_MCU schematic
    STAR_MCU.pdf         -- plotted schematic
    STAR_MCU_bom.csv     -- bill of materials
    STAR_MCU_erc.rpt     -- ERC report
```

## First-time host setup

Done once per Mac. All tools install user-local -- no `sudo`, nothing
in `/opt` or `/Library` (except WaveForms, which has a single global
system install).

1. **WaveForms (free download from Digilent)** -- drag
   `WaveForms.app` into `/Applications/` and copy `dwf.framework`
   into `/Library/Frameworks/`.
2. **`rfp-cli` for macOS arm64** (from the Renesas Flash Programmer
   download archive) -- expand to `~/tools/rfp-cli/`, strip the
   `com.apple.quarantine` attribute (`xattr -dr com.apple.quarantine
   ~/tools/rfp-cli`), and create a wrapper at
   `~/tools/rfp-cli/bin/rfp-cli` that sets
   `DYLD_FALLBACK_LIBRARY_PATH` so the binary can find its bundled
   `libRFP.dylib` and friends.
3. **`pydwf` Python venv** -- `cd gpio_test/host && python3 -m venv
   venv && ./venv/bin/pip install pydwf`.
4. **`devcontainer` CLI** -- `npm i -g @devcontainers/cli` (or via
   `brew install devcontainer`).

## Things that will fail

- **Container flash attempts**: `make flash` from inside the
  devcontainer cannot reach the E2 Lite (no USB passthrough on macOS
  Docker Desktop). Always flash from the host.
- **First `make flash` after board power-up** occasionally reports
  `E4000004: framing error`. Harmless, just re-run.
- **Pi5 SSH**: configured (see `docs/PI_DEPLOYMENT.md`). `en12` (ASIX
  USB-Eth) has no link when the Pi5 is off or the cable is unplugged.
