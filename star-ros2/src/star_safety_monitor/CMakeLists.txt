cmake_minimum_required(VERSION 3.8)
project(star_safety_monitor)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)

# Safety monitor node
add_executable(safety_monitor_node
  src/safety_monitor_node.cpp
  src/safety_monitor.cpp
)

target_include_directories(safety_monitor_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(safety_monitor_node
  rclcpp
  rclcpp_lifecycle
  std_msgs
  sensor_msgs
  geometry_msgs
  nav_msgs
  diagnostic_msgs
)

# Install targets
install(TARGETS safety_monitor_node
  DESTINATION lib/${PROJECT_NAME}
)

# Install launch files
install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

# Install config files
install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  # NOTE: Linting disabled -- ament_copyright requires full MIT license text in every
  # file header, which conflicts with STAR project's Doxygen-style header convention.
  # Consistent with star_gateway_bridge and star_spi_bridge which also disable lint_auto.
  # find_package(ament_lint_auto REQUIRED)
  # ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  find_package(lifecycle_msgs REQUIRED)

  # Create library for shared code
  add_library(safety_monitor_lib
    src/safety_monitor.cpp
  )
  target_compile_options(safety_monitor_lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_include_directories(safety_monitor_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  ament_target_dependencies(safety_monitor_lib
    rclcpp
    rclcpp_lifecycle
    std_msgs
    sensor_msgs
    geometry_msgs
    nav_msgs
    diagnostic_msgs
  )

  # Unit tests
  ament_add_gtest(${PROJECT_NAME}_test
    test/test_safety_monitor.cpp
  )
  target_include_directories(${PROJECT_NAME}_test PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_link_libraries(${PROJECT_NAME}_test
    safety_monitor_lib
  )
  ament_target_dependencies(${PROJECT_NAME}_test
    rclcpp
    rclcpp_lifecycle
    std_msgs
    sensor_msgs
    geometry_msgs
    nav_msgs
    diagnostic_msgs
  )
endif()

ament_package()
