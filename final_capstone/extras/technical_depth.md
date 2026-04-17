# Technical Depth of the Proposed Solution

This document answers the "Technical Depth of Proposed Solution" rubric
line directly:

> Solution exceeds expectations in technical depth, offering insight,
> innovation, or advanced understanding of the problem. Design solution
> demonstrates appropriate rigor, detail, and use of engineering
> principles.

Every claim below points to a file, a measurement, or a standard already
committed to the repository. Nothing here is aspirational.

---

## 1. Scope of rigor (what is being measured)

STAR is not one artifact; it is a vertically integrated stack where each
layer was designed, justified, and validated against first principles:

| Layer | Engineering principle applied | Evidence |
|---|---|---|
| Motor control | Closed-loop discrete-time PID, derived from first-order plant model | `matlab/pid_discretize_250hz.m`, model `G(s) = 3.665 / (0.075s + 1)` |
| Firmware | NASA/JPL Power of 10 for safety-critical C, SOLID for C modules | `CLAUDE.md` Power-of-10 section, `star-rx72n-firmware/` |
| Communication | Controller-timed 10 Mbps SPI with HARQ Chase Combining + rate-1/2 convolutional FEC | `writeup/03_system_design.md` Section "SPI link", peak utilization 1.6% |
| Serialization | Proto3 + nanopb, zero dynamic allocation, CRC-32 IEEE 802.3 framing | `star-proto/`, `.options` files, `CLAUDE.md` "nanopb Considerations" |
| Perception | RANSAC plane fitting in Open3D cross-validated against inertial pitch | `compliance-engine/star_compliance/nodes/ramp_slope_node.py` |
| Localization | EKF sensor fusion of wheel odometry and BNO055 IMU | `star-ros2/star_bringup`, `robot_localization` config |
| Autonomy | slam_toolbox (async) + Nav2 + frontier exploration | 143/143 passing ROS2 tests |
| UI | TypeScript compile-time type safety against firmware's own protobuf schema | `star-ui/` + `@protobuf-ts/grpcweb-transport` |

The vertical integration is the depth: a ramp-slope measurement at the
top is traceable through a cross-validated perception node, an EKF, an
SPI transport with reliability guarantees, a hard-real-time 250 Hz PID
loop, and a PWM edge on a verified GPIO pin -- every boundary enforced
by a typed schema or a compile-time check.

---

## 2. Insight: the cross-validation that protects one honest claim

The capstone could have claimed seven ADA checks with synthesized
accuracy numbers. The decision made instead was to implement one check
to a measurable standard and architect the remaining six against the
same sensor stack (`writeup/04_compliance_engine.md`).

The implemented ADA 405.2 ramp-slope check is not a single sensor
reading. It computes two independent slope estimates from physically
distinct sensors and accepts the result only when they agree:

1. **Geometric estimate.** LiDAR returns within 2 m of the robot are
   accumulated in the `base_link` frame while the body is moving
   forward. Open3D RANSAC fits a plane; the surface normal's angle with
   gravity is slope estimate #1.
2. **Inertial estimate.** The BNO055 pitch trace over the last 500 ms is
   averaged. Because the BNO055 sits on the RX72N PCB and is
   mechanically co-located with the drive chassis, this pitch reading
   is the robot body's orientation on the ramp -- that is estimate #2.
3. **Acceptance gate.** If the two estimates agree within 0.5 degrees,
   the result is accepted. If they disagree, the row is flagged rather
   than silently emitted.

Two sensors on two distinct physical paths (optical geometry vs 9-DoF
inertial fusion) converging on the same quantity is the engineering
insight: it detects sensor failure modes that a single-sensor pipeline
cannot. Ground truth is a Wixey WR300 digital angle gauge (+/- 0.1
degrees), which is well below the claim precision (4.76 degrees), so
the instrument is not the dominant error source.

This same architecture extends to the six stretch/architected checks:
each has a primary sensor and an independent cross-validator specified
in `writeup/04_compliance_engine.md` and tabulated in
`extras/traceability.md`.

---

## 3. Innovation: HARQ + FEC on an embedded SPI link

Embedded robotics systems typically pick one reliability primitive on a
chassis-internal bus (CRC + retry, or an error-correcting code, or
neither). STAR layers both on its 10 Mbps SPI link between the Pi 5 and
the RX72N:

- **Frame layer.** SYNC 0x55AA, 16-bit sequence number, CRC-32 IEEE
  802.3 trailer, payload up to 1024 bytes, nanopb-encoded. DMA
  double-buffered on RSPI2.
- **HARQ (hybrid ARQ with Chase Combining).** Failed frames are
  re-transmitted; the receiver combines the soft metrics of retries
  with the original, so each retry lowers the effective error rate
  rather than merely re-rolling the dice.
- **FEC (convolutional, K=7, rate-1/2).** Sits under HARQ and corrects
  burst errors from motor-switching noise on the chassis-internal
  ribbon cable without burning a round trip.

The measured ceiling matters more than the architecture: peak
utilization at full telemetry is **1.6% of 10 Mbps**. There is a factor
of 60 headroom for additional sensor streams before either reliability
layer becomes a bottleneck. Picking the primitive mix and then proving
the headroom is the engineering principle at work -- not just layering
features for their own sake.

Alternatives considered and rejected (`extras/qa_prep.md`): CAN bus
(max ~1 Mbps on RPi, insufficient for 100 Hz command + full telemetry +
firmware update streams) and UART (no DMA double-buffering story on the
RPi5 side).

---

## 4. Rigor: safety-critical firmware held to an external standard

The RX72N firmware is written to NASA/JPL Power of 10 rules -- not as
guidance, as an enforced standard with one documented deviation.
`CLAUDE.md` walks the ten rules explicitly; the short version:

- **Rule 1 (simple control flow).** No goto, setjmp/longjmp, recursion.
- **Rule 2 (bounded loops).** Every loop has a statically provable
  upper bound; enums provide the bound.
- **Rule 3 (no dynamic memory after init).** Zero malloc/free in the
  RX72N firmware. All buffers statically allocated with
  enum-defined sizes; ThreadX stacks are static arrays.
- **Rule 4 (short functions).** Functions fit on one screen;
  `rx_pid_compute()` is 44 lines end-to-end.
- **Rule 5 (assertions / validation).** Minimum two validation checks
  per function (pre- and post-conditions); PID has four.
- **Rule 6 (smallest scope).** Loop counters declared in the for
  statement; file-scope vars carry an `s_` prefix.
- **Rule 7 (check every return).** All returns validated or explicitly
  cast to `(void)`; `RX_RETURN_ON_ERROR` propagates.
- **Rule 8 (limit preprocessor).** C23 typed enums for every integer
  constant; macros only for de-duplication, conditional compilation,
  and build flags. Hardware register access is via inline accessor
  functions, not `#define`d base addresses.
- **Rule 9 (pointer use).** Intentional deviation: function pointers
  allowed for Dependency Inversion (mock injection in unit tests).
  Documented, not silent.
- **Rule 10 (max warnings).** `-Wall -Wextra -Werror`; the build fails
  on any warning; CI/CD enforces it.

The rigor is not in the rules themselves; it is that the project
commits to them ahead of the schedule and absorbs the cost. The
"no magic numbers, every literal is a typed enum" clause is a direct
consequence of Rule 8 and shows up on every page of the firmware.

---

## 5. Rigor: PID designed, not tuned

The 250 Hz discrete-time PID controller on each wheel is derived from a
measured first-order motor model, not hand-tuned:

- **Plant identification.** `G(s) = 3.665 / (0.075s + 1)` -- gain
  3.665 rad/s/V, time constant tau = 75 ms. Identified from step
  response data on the real FIT0520 gearmotors (210 RPM no-load,
  34.02:1 gearbox, 341.2 PPR Hall encoders, 11,599 counts per output
  revolution).
- **Controller design.** Continuous-domain PID gains chosen for the
  closed-loop bandwidth and damping the MATLAB workflow encodes, then
  discretized with backward-Euler integration and a derivative
  low-pass filter in `matlab/pid_discretize_250hz.m`.
- **Firmware implementation.** `rx_pid_compute()` in the RX72N
  firmware runs the discrete-time algorithm at 250 Hz with anti-windup
  clamping, integral saturation bounds, and post-condition checks that
  the output is within the configured min/max.

The depth is that each gain on the RX72N has a derivation upstream in
a MATLAB script that references a measured plant model, not a value
a team member tried that "felt right." The `/pid-tune` skill
documents the full system-identification-to-firmware pipeline.

---

## 6. Rigor: test coverage is the integration artifact

Test counts are cheap. The STAR test status is load-bearing:

- **ROS2 layer: 143 tests, 0 failures** across four packages
  (`star_bringup`, `star_spi_bridge`, `star_gateway_bridge`,
  `star_safety_monitor`). See
  `star-ros2/IMPLEMENTATION_STATUS.md`.
- **Firmware layer.** Unit tests run against the mock bus interface
  (`bus_interface_t` with function-pointer dependency inversion), so
  PID, nanopb framing, and state machines are exercised on the x86_64
  host without hardware in the loop.
- **Proto layer.** `proto.yml` CI runs `buf format`, `buf lint`, `buf
  build`, `buf breaking` against main, then generates Go, TypeScript,
  and nanopb code and runs serialization round-trip tests on all three
  targets. A breaking change to a field type is caught before merge.
- **Encoding policy.** A pre-commit hook rejects any source file that
  contains a non-ASCII character. This is not cosmetic -- multi-byte
  UTF-8 breaks downstream static analyzers, MISRA checkers, and the
  Windows toolchains other teams depend on.

Taken together, this is the integration proof: a change to a protobuf
field ripples through three generated toolchains, a firmware unit test
suite, and a 143-test ROS2 suite, and any regression is caught by CI
on the branch that introduced it.

---

## 7. Advanced understanding: what was explicitly *not* built, and why

A solution exceeds expectations when the team can articulate what they
chose *not* to pursue, with reasons. These are the honest non-claims:

- **Seven implemented ADA checks.** Architected against the same
  sensor stack in `writeup/04_compliance_engine.md`; only ramp slope
  is implemented end-to-end. The tradeoff: one check validated
  against a digital angle gauge is worth more than seven with
  synthesized accuracy numbers. The traceability matrix
  (`extras/traceability.md`) shows exactly what each stretch check
  would require.
- **Stereo-based door measurement.** The IMX219-83 stereo pipeline
  is wired and the RTAB-Map integration is in progress (most recent
  commit on main: "Fuse lidar into RTAB-Map for 12m range 3D
  mapping"), but the door clear-width check is not validated against
  ground truth in this package.
- **Hand-tuned per-check MAE figures.** Not published. The only
  accuracy claim is on the one check where a Wixey angle gauge is the
  reference. A curated demo course with planted shims is likewise
  explicitly not part of the deliverable.
- **Jetson compute.** Considered and rejected: real-time is on the
  RX72N, so a GPU buys nothing at this scale, and the Pi 5's two
  dedicated MIPI CSI-2 ports are what the stereo pair needs. Cost
  delta was ~$500 with no accuracy return.

The depth here is the *discipline* of saying where the evidence
stops, not the bravado of claiming it extends further.

---

## 8. Where to look for each element of rigor

For a reviewer time-boxed to 10 minutes:

| Element of the rubric | Open this file |
|---|---|
| Cross-validated geometric check | `compliance-engine/star_compliance/nodes/ramp_slope_node.py` |
| HARQ + FEC transport | `writeup/03_system_design.md` Section "SPI link" |
| NASA Power of 10 enforcement | `CLAUDE.md` Section "NASA Power of 10 Rules" |
| PID derivation | `matlab/pid_discretize_250hz.m` + `/pid-tune` skill |
| Proto-to-firmware type safety | `star-proto/` + `CLAUDE.md` Section "Protocol Buffers" |
| ROS2 test coverage | `star-ros2/IMPLEMENTATION_STATUS.md` |
| Claim traceability | `research/source_verification.md` and `extras/traceability.md` |
| Scope discipline | `writeup/04_compliance_engine.md` `[IMPLEMENTED]` / `[STRETCH]` / `[ARCHITECTED]` labels |

Every row above resolves to a file in this repository. None of them are
placeholders.
