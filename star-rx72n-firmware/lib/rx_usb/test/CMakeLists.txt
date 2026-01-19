# =============================================================================
# RX USB Library - Standalone Unit Tests
# Host-compiled tests with mocked hardware dependencies
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(rx_usb_tests C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable all warnings and treat as errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
# Suppress unused variable warning (s_tag is unused when logging is mocked)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable")

# =============================================================================
# Define UNIT_TEST to expose STATIC_TESTABLE functions
# =============================================================================

add_compile_definitions(UNIT_TEST)

# =============================================================================
# Source Files
# =============================================================================

# Main test file
set(TEST_SOURCES
    test_rx_usb.c
)

# Mock hardware implementation
set(MOCK_SOURCES
    mocks/mock_hardware.c
)

# Unit under test (rx_usb.c only, hardware is mocked)
set(USB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/rx_usb.c
)

# =============================================================================
# Include Directories
#
# Order matters: mocks directory comes FIRST to override hardware headers
# =============================================================================

include_directories(BEFORE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/rx_core/inc
)

# =============================================================================
# Test Executable
# =============================================================================

add_executable(test_rx_usb
    ${TEST_SOURCES}
    ${MOCK_SOURCES}
    ${USB_SOURCES}
)

# =============================================================================
# CTest Integration
# =============================================================================

enable_testing()
add_test(NAME test_rx_usb COMMAND test_rx_usb)

# =============================================================================
# Convenience Target
# =============================================================================

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_rx_usb
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running USB library unit tests..."
)

# =============================================================================
# Build Instructions
# =============================================================================
#
# To build and run tests:
#   mkdir build && cd build
#   cmake ..
#   make
#   ./test_rx_usb
#
# Or using cmake test integration:
#   make run_tests
#
# Or using ctest:
#   ctest --output-on-failure
#
# =============================================================================
