# esp32-firmware/test_app/CMakeLists.txt

#
# Test application for running STAR tests on ESP32 hardware.

cmake_minimum_required(VERSION 3.16)

# Add parent components directory to component search path
set(EXTRA_COMPONENT_DIRS "../components")

# Enable testing
set(TEST_COMPONENTS "star_error_handler star_pin_validator star_bus star_wifi_bridge" CACHE STRING "Components to test")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(star_test_app)
