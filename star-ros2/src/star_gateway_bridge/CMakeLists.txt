cmake_minimum_required(VERSION 3.22)
project(star_gateway_bridge)

# C++17 required for gRPC and modern C++ features
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# Compiler warnings
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ============================================
# Find dependencies
# ============================================

# ROS2 packages
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(diagnostic_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(nav_msgs REQUIRED)

# System dependencies (Protobuf and gRPC)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

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
# Collect generated protobuf/grpc source files
# ============================================

file(GLOB PROTO_SRCS CONFIGURE_DEPENDS "${STAR_PROTO_DIR}/star/v1/*.pb.cc")
file(GLOB GRPC_SRCS CONFIGURE_DEPENDS "${STAR_PROTO_DIR}/star/v1/*.grpc.pb.cc")

# Verify we found protobuf files
list(LENGTH PROTO_SRCS PROTO_COUNT)
if(PROTO_COUNT EQUAL 0)
  message(FATAL_ERROR
    "No .pb.cc files found in ${STAR_PROTO_DIR}/star/v1/. "
    "Run 'buf generate' in star-proto/ directory first.")
endif()

message(STATUS "Found ${PROTO_COUNT} protobuf source files")

# ============================================
# Build node library (component)
# ============================================

add_library(star_gateway_bridge_component SHARED
  src/message_converter.cpp
  src/star_gateway_bridge_node.cpp
  ${PROTO_SRCS}
  ${GRPC_SRCS}
)

target_include_directories(star_gateway_bridge_component PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Link ROS2 dependencies
ament_target_dependencies(star_gateway_bridge_component
  rclcpp
  rclcpp_components
  diagnostic_msgs
  geometry_msgs
  nav_msgs
  std_msgs
  sensor_msgs
  std_srvs
)

# Link system dependencies
target_link_libraries(star_gateway_bridge_component
  protobuf::libprotobuf
  gRPC::grpc++
)

# Register as rclcpp component
rclcpp_components_register_nodes(star_gateway_bridge_component
  "star::star_gateway_bridge::StarGatewayBridgeNode")

# ============================================
# Build standalone executable
# ============================================

add_executable(star_gateway_bridge_main src/main.cpp)

target_link_libraries(star_gateway_bridge_main
  star_gateway_bridge_component
)

# ============================================
# Install
# ============================================

# Install library
install(TARGETS
  star_gateway_bridge_component
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION lib/${PROJECT_NAME}
)

# Install executable
install(TARGETS
  star_gateway_bridge_main
  DESTINATION lib/${PROJECT_NAME}
)

# Install headers
install(DIRECTORY include/
  DESTINATION include
)

# ============================================
# Testing
# ============================================

if(BUILD_TESTING)
  # NOTE: Linting disabled for infrastructure phase
  # ROS2 ament linting enforces different style than STAR project standards:
  # - Double-underscore include guards vs path-based guards
  # - Copyright format requirements
  # - Include ordering different from project conventions
  # Will re-enable when application code is added and aligned with ROS2 standards.
  # find_package(ament_lint_auto REQUIRED)
  # ament_lint_auto_find_test_dependencies()

  # Unit tests
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_message_converter test/test_message_converter.cpp)
  target_link_libraries(test_message_converter star_gateway_bridge_component)
  target_include_directories(test_message_converter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${STAR_PROTO_DIR}
  )
  ament_target_dependencies(test_message_converter
    geometry_msgs
    sensor_msgs
    std_msgs
    nav_msgs
  )
endif()

# ============================================
# Export dependencies
# ============================================

ament_package()
