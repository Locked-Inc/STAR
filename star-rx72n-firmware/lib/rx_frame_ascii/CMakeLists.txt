# lib/rx_frame_ascii/CMakeLists.txt

add_library(rx_frame_ascii STATIC
  src/rx_frame_ascii.c
)

target_include_directories(rx_frame_ascii PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(rx_frame_ascii PUBLIC
  rx_frame
  rx_err
)
