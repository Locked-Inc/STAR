# CMakeLists.txt - nanopb C Serialization Tests
# STAR Project - Texas A&M University

cmake_minimum_required(VERSION 3.16)
project(star_proto_nanopb_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Unity test framework (fetched from GitHub)
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.5.2
)
FetchContent_MakeAvailable(unity)

# nanopb library (requires pip install nanopb)
find_program(NANOPB_GENERATOR nanopb_generator)
if(NOT NANOPB_GENERATOR)
    message(WARNING "nanopb_generator not found. Install with: pip install nanopb")
    message(WARNING "Tests will use placeholder stubs until nanopb is available.")
endif()

# Generated code directory
set(GEN_DIR "${CMAKE_SOURCE_DIR}/../../gen/nanopb")

# Source files
set(TEST_SOURCES
    test_serialization.c
)

# Include directories
include_directories(
    ${GEN_DIR}
    ${unity_SOURCE_DIR}/src
)

# Test executable
add_executable(star_proto_tests ${TEST_SOURCES})

# Link Unity
target_link_libraries(star_proto_tests unity)

# Enable testing
enable_testing()
add_test(NAME star_proto_tests COMMAND star_proto_tests)

# Custom target for running tests
add_custom_target(run_tests
    COMMAND star_proto_tests
    DEPENDS star_proto_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
