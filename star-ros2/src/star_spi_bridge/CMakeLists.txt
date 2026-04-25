cmake_minimum_required(VERSION 3.20)
project(star_spi_bridge LANGUAGES C CXX)

# Language standard enforcement
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position-independent code for shared-library compatibility
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

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

# ============================================
# Collect generated protobuf source files
# ============================================

file(GLOB PROTO_SRCS CONFIGURE_DEPENDS "${STAR_PROTO_DIR}/star/v1/*.pb.cc")

# SPI Driver Library
add_library(spi_driver
  src/spi_driver.cpp
  src/spi_message_converter.cpp
  ${PROTO_SRCS}
)
target_include_directories(spi_driver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${STAR_PROTO_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_compile_options(spi_driver PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic -Werror>
)
target_link_libraries(spi_driver
  ${Protobuf_LIBRARIES}
)
ament_target_dependencies(spi_driver
  rclcpp
  rclcpp_lifecycle
  geometry_msgs
  nav_msgs
  sensor_msgs
  tf2
  tf2_geometry_msgs
)

# Main Node Executable
add_executable(star_spi_bridge_node src/star_spi_driver_node.cpp src/main.cpp)
target_include_directories(star_spi_bridge_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_options(star_spi_bridge_node PRIVATE
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic -Werror>
)
target_link_libraries(star_spi_bridge_node spi_driver)
ament_target_dependencies(star_spi_bridge_node
  rclcpp
  rclcpp_lifecycle
  geometry_msgs
  nav_msgs
  sensor_msgs
  std_msgs
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

  ament_add_gtest(test_spi_driver test/test_spi_driver.cpp)
  target_link_libraries(test_spi_driver spi_driver ${Protobuf_LIBRARIES})
  target_compile_options(test_spi_driver PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic -Werror>
  )

  ament_add_gtest(test_spi_message_converter test/test_spi_message_converter.cpp)
  target_link_libraries(test_spi_message_converter spi_driver ${Protobuf_LIBRARIES})
  target_compile_options(test_spi_message_converter PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall -Wextra -Wpedantic -Werror>
  )
endif()

ament_package()
