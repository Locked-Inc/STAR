cmake_minimum_required(VERSION 3.8)
project(star_spi_bridge)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

find_package(Protobuf REQUIRED)

# ============================================
# Include generated protobuf headers
# ============================================

# Path to generated C++ protobuf code
set(STAR_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../star-proto/gen/cpp")

# Verify protobuf directory exists
if(NOT EXISTS "${STAR_PROTO_DIR}")
  message(FATAL_ERROR
    "Protobuf C++ code not found at ${STAR_PROTO_DIR}. "
    "Run 'buf generate' in star-proto/ directory first.")
endif()

include_directories(${STAR_PROTO_DIR})

# ============================================
# Collect generated protobuf source files
# ============================================

file(GLOB PROTO_SRCS "${STAR_PROTO_DIR}/star/v1/*.pb.cc")

# Include directories
include_directories(include)

# SPI Driver Library
add_library(spi_driver
  src/spi_driver.cpp
  src/spi_message_converter.cpp
  ${PROTO_SRCS}
)
target_include_directories(spi_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(spi_driver
  ${Protobuf_LIBRARIES}
  geometry_msgs::geometry_msgs
  nav_msgs::nav_msgs
  sensor_msgs::sensor_msgs
)
ament_target_dependencies(spi_driver
  rclcpp
  rclcpp_lifecycle
)

# Main Node Executable
add_executable(star_spi_bridge_node src/star_spi_driver_node.cpp src/main.cpp)
target_include_directories(star_spi_bridge_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(star_spi_bridge_node spi_driver)
ament_target_dependencies(star_spi_bridge_node
  rclcpp
  rclcpp_lifecycle
  geometry_msgs
  nav_msgs
  sensor_msgs
)

# Install rules
install(TARGETS spi_driver star_spi_bridge_node
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  
  add_executable(test_spi_driver test/test_spi_driver.cpp)
  target_link_libraries(test_spi_driver spi_driver ${Protobuf_LIBRARIES})
  ament_add_gtest_executable(test_spi_driver test_spi_driver)

  add_executable(test_spi_message_converter test/test_spi_message_converter.cpp)
  target_link_libraries(test_spi_message_converter spi_driver ${Protobuf_LIBRARIES})
  ament_add_gtest_executable(test_spi_message_converter test_spi_message_converter)
endif()

ament_package()
