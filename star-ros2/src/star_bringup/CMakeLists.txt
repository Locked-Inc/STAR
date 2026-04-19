cmake_minimum_required(VERSION 3.8)
project(star_bringup)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)

install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch
)

install(DIRECTORY config/
  DESTINATION share/${PROJECT_NAME}/config
)

install(DIRECTORY urdf/
  DESTINATION share/${PROJECT_NAME}/urdf
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # Launch-testing-ros stereo smoke test. Requires both IMX219-83
  # sensors on the Pi 5. Skips gracefully on CI where the hardware is
  # absent (see the HARDWARE_MARKER check inside the test file).
  find_package(launch_testing_ament_cmake REQUIRED)
  add_launch_test(test/launch_test_stereo_topics.py
    TIMEOUT 60
  )

  # Ship the test file into the install share dir so the launch_test
  # CLI can also invoke it manually on the robot.
  install(
    FILES test/launch_test_stereo_topics.py
    DESTINATION share/${PROJECT_NAME}/test
  )
endif()

ament_package()
