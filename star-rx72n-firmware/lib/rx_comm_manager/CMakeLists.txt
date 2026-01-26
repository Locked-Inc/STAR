# lib/rx_comm_manager/CMakeLists.txt

add_library(rx_comm_manager STATIC
  src/rx_comm_manager.c
)

target_include_directories(rx_comm_manager PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(rx_comm_manager PUBLIC
  rx_frame
  rx_frame_ascii
  rx_usb
  rx_usb_comm
  rx_spi_comm
  rx_err
)

# Enforce C23 standard (NASA Power of 10 compliant syntax)
set_target_properties(rx_comm_manager PROPERTIES
  C_STANDARD 23
  C_STANDARD_REQUIRED ON
)

# Compiler flags are inherited from top-level CMakeLists.txt:
# - -Wall -Wextra -Wpedantic -Werror (NASA Power of 10 Rule 10)
# - -fshort-enums (embedded optimization)
# - Additional library-specific flags can be added via target_compile_options if needed
