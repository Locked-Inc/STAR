###############################################################################################################################
# file:  CMakeLists.txt
# brief: BeagleBone Blue userspace firmware build configuration.
#
# usage: Cross-compile from x86 host:
#          cmake --preset cross-debug
#          cmake --build build-cross-debug -j$(nproc)
#
#        Native build on BeagleBone Blue:
#          cmake --preset native-debug
#          cmake --build build-native-debug -j$(nproc)
#
#        Run tests (host-only unit tests):
#          ctest --test-dir <build_dir>
###############################################################################################################################
cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Toolchain version gate -- C23 typed enums require gcc >= 13 or clang >= 15
# ---------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS "13.0")
    message(FATAL_ERROR "gcc >= 13.0 required for C23 typed enums (found ${CMAKE_C_COMPILER_VERSION})")
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS "15.0")
    message(FATAL_ERROR "clang >= 15.0 required for C23 typed enums (found ${CMAKE_C_COMPILER_VERSION})")
endif()

###################### PROJECT SETUP ######################################
project(star-beaglebone-blue C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

###################### NANOPB RUNTIME ######################################
include(FetchContent)
FetchContent_Declare(nanopb
    GIT_REPOSITORY https://github.com/nanopb/nanopb.git
    GIT_TAG        0.4.9.1
)
FetchContent_MakeAvailable(nanopb)

###################### GENERATED PROTOBUF SOURCES ##########################
set(NANOPB_GEN_DIR ${PROJECT_SOURCE_DIR}/../star-proto/gen/nanopb)

set(NANOPB_SOURCES
    ${NANOPB_GEN_DIR}/star/v1/wire.pb.c
    ${NANOPB_GEN_DIR}/star/v1/motor_control.pb.c
    ${NANOPB_GEN_DIR}/star/v1/telemetry.pb.c
    ${NANOPB_GEN_DIR}/star/v1/common.pb.c
    ${NANOPB_GEN_DIR}/star/v1/configuration.pb.c
    ${NANOPB_GEN_DIR}/star/v1/diagnostics.pb.c
    ${NANOPB_GEN_DIR}/google/protobuf/timestamp.pb.c
    ${NANOPB_GEN_DIR}/google/protobuf/duration.pb.c
)

###################### SOURCE FILES ######################################
set(PROJECT_SOURCES
    src/main.c
    src/hardware_init.c

    # Task implementations
    tasks/shared_data.c
    tasks/comm_task.c
    tasks/motor_control_task.c
    tasks/imu_task.c
    tasks/telemetry_task.c
    tasks/watchdog_task.c

    # Libraries
    libs/bb_usb_cdc/src/bb_usb_cdc.c
    libs/bb_crc/src/bb_crc.c
    libs/bb_frame/src/bb_frame.c
    libs/bb_pid/src/bb_pid.c

    # Generated protobuf
    ${NANOPB_SOURCES}
)

add_executable(star-beaglebone-blue ${PROJECT_SOURCES})

###################### INCLUDE PATHS ######################################
target_include_directories(star-beaglebone-blue PRIVATE
    src/inc
    tasks/inc
    libs/bb_err/inc
    libs/bb_usb_cdc/inc
    libs/bb_crc/inc
    libs/bb_frame/inc
    libs/bb_pid/inc
    ${nanopb_SOURCE_DIR}
    ${NANOPB_GEN_DIR}
)

###################### COMPILER FLAGS ######################################
target_compile_options(star-beaglebone-blue PRIVATE
    -std=gnu2x
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -fstack-protector-strong
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

###################### CROSS-COMPILE SYSROOT (optional) ####################
# Set BBB_EXTRA_INCLUDE / BBB_EXTRA_LIBDIR via CMakeUserPresets.json when
# cross-compiling without a full sysroot (e.g. Pi5 with only librobotcontrol).
if(BBB_EXTRA_INCLUDE)
    target_include_directories(star-beaglebone-blue SYSTEM PRIVATE ${BBB_EXTRA_INCLUDE})
endif()
if(BBB_EXTRA_LIBDIR)
    target_link_directories(star-beaglebone-blue PRIVATE ${BBB_EXTRA_LIBDIR})
endif()

###################### LINKER FLAGS ######################################
target_link_libraries(star-beaglebone-blue PRIVATE
    protobuf-nanopb-static  # nanopb runtime (pb_encode, pb_decode, pb_common)
    robotcontrol   # librobotcontrol: motor, encoder, IMU, servo
    pthread        # POSIX threads
    m              # libm: math functions
)

###################### SIZE REPORT ######################################
find_program(SIZE_TOOL size)
if(SIZE_TOOL)
    add_custom_command(TARGET star-beaglebone-blue POST_BUILD
        COMMAND ${SIZE_TOOL} $<TARGET_FILE:star-beaglebone-blue>
        COMMENT "Binary size report"
    )
endif()

###################### HOST UNIT TESTS ######################################
# Tests build for the host (x86_64) without librobotcontrol.
# Run: ctest --test-dir <build_dir>
enable_testing()

# Common test compile options
set(TEST_COMPILE_OPTIONS -std=gnu2x -Wall -Wextra -Wpedantic -Werror)
set(TEST_INCLUDE_DIRS
    src/inc
    tasks/inc
    libs/bb_err/inc
    libs/bb_crc/inc
    libs/bb_frame/inc
    libs/bb_pid/inc
)

# --- test_motor_control (shared_data tests) ---
add_executable(test_motor_control
    tests/test_motor_control.c
    tasks/shared_data.c
)
target_include_directories(test_motor_control PRIVATE ${TEST_INCLUDE_DIRS})
target_compile_options(test_motor_control PRIVATE ${TEST_COMPILE_OPTIONS})
target_link_libraries(test_motor_control PRIVATE pthread)
add_test(NAME shared_data_unit COMMAND test_motor_control)

# --- test_crc32 ---
add_executable(test_crc32
    tests/test_crc32.c
    libs/bb_crc/src/bb_crc.c
)
target_include_directories(test_crc32 PRIVATE ${TEST_INCLUDE_DIRS})
target_compile_options(test_crc32 PRIVATE ${TEST_COMPILE_OPTIONS})
add_test(NAME crc32_unit COMMAND test_crc32)

# --- test_frame ---
add_executable(test_frame
    tests/test_frame.c
    libs/bb_frame/src/bb_frame.c
    libs/bb_crc/src/bb_crc.c
)
target_include_directories(test_frame PRIVATE ${TEST_INCLUDE_DIRS})
target_compile_options(test_frame PRIVATE ${TEST_COMPILE_OPTIONS})
add_test(NAME frame_unit COMMAND test_frame)

# --- test_frame_decoder ---
add_executable(test_frame_decoder
    tests/test_frame_decoder.c
    libs/bb_frame/src/bb_frame.c
    libs/bb_crc/src/bb_crc.c
)
target_include_directories(test_frame_decoder PRIVATE ${TEST_INCLUDE_DIRS})
target_compile_options(test_frame_decoder PRIVATE ${TEST_COMPILE_OPTIONS})
add_test(NAME frame_decoder_unit COMMAND test_frame_decoder)

# --- test_pid ---
add_executable(test_pid
    tests/test_pid.c
    libs/bb_pid/src/bb_pid.c
)
target_include_directories(test_pid PRIVATE ${TEST_INCLUDE_DIRS})
target_compile_options(test_pid PRIVATE ${TEST_COMPILE_OPTIONS})
target_link_libraries(test_pid PRIVATE m)
add_test(NAME pid_unit COMMAND test_pid)

# --- test_watchdog ---
add_executable(test_watchdog
    tests/test_watchdog.c
    tasks/shared_data.c
)
target_include_directories(test_watchdog PRIVATE ${TEST_INCLUDE_DIRS})
target_compile_options(test_watchdog PRIVATE ${TEST_COMPILE_OPTIONS})
target_link_libraries(test_watchdog PRIVATE pthread)
add_test(NAME watchdog_unit COMMAND test_watchdog)
