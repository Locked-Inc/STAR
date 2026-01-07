cmake_minimum_required(VERSION 3.8)
project(star_bringup)

# Compiler options
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)

# Install launch files, config files, etc. (future)
# install(DIRECTORY launch config
#   DESTINATION share/${PROJECT_NAME}
# )

# Testing
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # Run all linters found in ament_lint_common
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
