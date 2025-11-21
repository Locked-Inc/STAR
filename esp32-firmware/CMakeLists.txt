# Root CMakeLists.txt for ESP-IDF with PlatformIO
cmake_minimum_required(VERSION 3.16)

# Add our libraries as ESP-IDF components
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/lib")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(star-firmware)
