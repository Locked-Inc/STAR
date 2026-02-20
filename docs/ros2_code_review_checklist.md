# ROS2 C++ Code Review Checklist

Use this checklist when reviewing ROS2 C++ code in the STAR project.

**Automation:** ~65% of these checks are automated via `./scripts/review-ros2.sh`.
- ? = Fully automated (checked by script)
- ? = Manual review required

## Naming Conventions

- [ ] ? Classes use `CamelCase` (e.g., `StarGatewayBridgeNode`)
- [ ] ? Methods use `snake_case` (e.g., `publish_telemetry()`) - same as C firmware
- [ ] ? Variables use `snake_case` (e.g., `encoder_ticks`)
- [ ] ? Member variables have trailing `_` (e.g., `grpc_channel_`)
- [ ] ? Constants use `ALL_CAPITALS` (e.g., `MAX_RETRIES`)
- [ ] ? Namespaces use `under_scored` and match package name (e.g., `star::spi_bridge`)

## File Organization

- [ ] ? C++ headers use `.hpp` extension (not `.h`)
- [ ] ? Include guards follow pattern: `PACKAGE_FILE_NAME_HPP_`
- [ ] ? Includes are organized in correct order (ROS2, system, project)
- [ ] ? No unnecessary includes (use forward declarations when possible)

## Formatting

- [ ] ? Code is formatted with `clang-format` (run formatter before commit)
- [ ] ? Line length <= 120 characters
- [ ] ? 2-space indentation (no tabs)
- [ ] ? Function braces on new line
- [ ] ? Control statement braces cuddled (`if (x) {`)
- [ ] ? No indentation inside namespaces

## ROS2 Patterns

- [ ] ? Node inherits from `rclcpp::Node` or `rclcpp_lifecycle::LifecycleNode`
- [ ] ? Publishers/subscribers use `SharedPtr` types
- [ ] ? Timer callbacks use `std::bind` or lambdas
- [ ] ? QoS settings are appropriate for use case

## Error Handling

- [ ] ? Exceptions used for error conditions (not return codes)
- [ ] ? All exceptions are caught in callbacks (prevent node crash)
- [ ] ? `RCLCPP_ERROR/WARN/INFO` used for logging (not `printf`/`cout`)
- [ ] ? Throttled logging used for high-frequency messages

## Documentation

- [ ] ? All public methods have Doxygen `/**` and `/**<` comments (same as C firmware)
- [ ] ? Class has `@brief` description
- [ ] ? Complex logic has inline comments explaining "why" (not "what")
- [ ] ? Parameter and return value documented with `@param` and `@return`

## Safety and Best Practices

- [ ] ? No blocking operations in callbacks (use timers or async)
- [ ] ? Resource cleanup in destructors (RAII pattern)
- [ ] ? Thread-safe access to shared data (use mutexes)
- [ ] ? Input validation for all external data (messages, parameters)
- [ ] ? Graceful shutdown implemented (cleanup on node termination)

## Testing

- [ ] ? Unit tests exist for new functionality (gtest)
- [ ] ? Integration tests for ROS2 interactions (launch tests)
- [ ] ? Edge cases tested (null pointers, invalid input, timeouts)
- [ ] ? Test coverage >= 80% for new code

## Specific to STAR Project

- [ ] ? Follows NASA Power of 10 rules where applicable (see CLAUDE.md)
- [ ] ? No dynamic allocation in critical paths (prefer static allocation)
- [ ] ? Constants defined as enums or `const` (not macros)
- [ ] ? Consistent with C firmware patterns (error handling philosophy)

## CI/CD

- [ ] ? Code passes `colcon build` without warnings
- [ ] ? Code passes `colcon test` (all tests green)
- [ ] ? Code passes `clang-format` check
- [ ] ? Code passes `ament_cppcheck` and `ament_cpplint`
