# ROS2 C++ Code Review Checklist

Use this checklist when reviewing ROS2 C++ code in the STAR project.

**Automation:** A subset of these checks is automated via
`./scripts/ros2/review-ros2.sh`. Items below are tagged:
- `[auto]`   -- the script verifies it
- `[manual]` -- requires reviewer judgement

**Authoritative style references:**
`docs/sections/11_ros2_cpp_style_guide.tex` and the project root `CLAUDE.md`.
This file is a runtime checklist; if anything here disagrees with those
two, the .tex / CLAUDE.md wins.

## Naming Conventions

- [ ] `[auto]`   Classes use `CamelCase` (e.g., `StarGatewayBridgeNode`)
- [ ] `[auto]`   Methods use `snake_case` (e.g., `publish_telemetry()`)
- [ ] `[auto]`   Variables use `snake_case` (e.g., `encoder_ticks`)
- [ ] `[auto]`   Member variables have trailing `_` (e.g., `grpc_channel_`)
- [ ] `[manual]` Compile-time constants are `constexpr` or scoped `enum class`,
                 not `#define`. Spelling follows `kCamelCase` for `constexpr`
                 and `SCREAMING_SNAKE` for class-level public constants.
- [ ] `[auto]`   Namespaces use `under_scored` and match the package name
                 (e.g., `star::spi_bridge`)

## File Organization

- [ ] `[auto]`   C++ headers use `.hpp` extension (not `.h`).
- [ ] `[auto]`   **Headers use `#pragma once`** (CLAUDE.md mandate; not
                 traditional `PACKAGE__FILE_HPP_` triplets).
                 `scripts/ros2/fix-header-guards.sh` rewrites legacy guards.
- [ ] `[manual]` Includes are organized in correct order (system, ROS2,
                 project) per `docs/sections/11_ros2_cpp_style_guide.tex`.
- [ ] `[manual]` No unnecessary includes (use forward declarations when
                 possible).

## Formatting

- [ ] `[auto]`   Code is formatted with **`ament_uncrustify`** (NOT
                 `clang-format`; see `docs/ROS2_FORMATTING.md`).
- [ ] `[auto]`   Line length <= 120 characters.
- [ ] `[auto]`   2-space indentation (no tabs).
- [ ] `[auto]`   Function braces on new line; control-statement braces
                 cuddled (`if (x) {`).
- [ ] `[auto]`   No indentation inside namespaces.

## ROS2 Patterns

- [ ] `[manual]` Node inherits from `rclcpp::Node` or
                 `rclcpp_lifecycle::LifecycleNode`.
- [ ] `[manual]` Publishers/subscribers use `SharedPtr` types; **read-only
                 message access uses `ConstSharedPtr`**.
- [ ] `[manual]` Timer callbacks use `std::bind` or lambdas; lifetime is
                 owned by the node.
- [ ] `[manual]` QoS settings match the publisher/subscriber pair (no
                 best-effort vs reliable mismatches).

## Error Handling

- [ ] `[manual]` Exceptions are used for error conditions (not return
                 codes); all exceptions are caught in callbacks to prevent
                 node crash.
- [ ] `[auto]`   `RCLCPP_ERROR/WARN/INFO` used for logging (not `printf`,
                 `std::cout`, `std::cerr`).
- [ ] `[manual]` Throttled logging used for high-frequency messages.

## Documentation

- [ ] `[manual]` All public methods have Doxygen `/** ... */` comments;
                 struct fields use inline `///<` (per CLAUDE.md
                 "MAXIMUM documentation coverage" mandate).
- [ ] `[manual]` Class has `@brief` description.
- [ ] `[manual]` Complex logic has inline comments explaining the *why*,
                 not the *what*.
- [ ] `[manual]` Parameters and return values documented with `@param`,
                 `@return`, `@retval` where applicable.

## Safety and Best Practices

- [ ] `[manual]` No blocking operations in callbacks (use timers or
                 async).
- [ ] `[manual]` Resource cleanup in destructors (RAII pattern).
- [ ] `[manual]` Thread-safe access to shared data (mutex / atomic).
- [ ] `[manual]` Input validation for all external data (messages,
                 parameters, services).
- [ ] `[manual]` Graceful shutdown implemented (`on_shutdown` /
                 lifecycle transitions clean up resources).

## Testing

- [ ] `[manual]` Unit tests exist for new functionality (gtest).
- [ ] `[manual]` Integration tests for ROS2 interactions (launch tests).
- [ ] `[manual]` Edge cases tested (null pointers, invalid input,
                 timeouts).
- [ ] `[manual]` Coverage target: >= 80 % for new code (target, not CI-
                 enforced today).

## Specific to STAR Project

- [ ] `[manual]` Follows NASA Power of 10 rules where applicable
                 (see CLAUDE.md and `docs/sections/06_nasa_power_of_10.tex`).
- [ ] `[manual]` No dynamic allocation in critical paths (prefer static
                 allocation).
- [ ] `[manual]` Inclusive terminology -- Controller/Peripheral, COPI/CIPO
                 (NOT master/slave, MOSI/MISO).
- [ ] `[manual]` Consistent with C firmware patterns (error-handling
                 philosophy, naming where reasonable).

## CI/CD

- [ ] `[auto]`   Code passes `colcon build` without warnings.
- [ ] `[auto]`   Code passes `colcon test` (all tests green).
- [ ] `[auto]`   Code passes `ament_uncrustify` check.
- [ ] `[auto]`   Code passes `ament_cppcheck` and `ament_cpplint`.
