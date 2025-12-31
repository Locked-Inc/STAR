# =============================================================================
# rx_usb_comm - USB CDC Communication Layer for RX72N
#
# High-level communication layer that integrates the frame protocol
# with the USB CDC driver. Provides the same framing as SPI but over
# USB virtual serial port.
#
# Dependencies:
#   rx_core   - Error handling and logging
#   rx_frame  - Frame encoding/decoding with CRC-32
#   rx_usb    - USB CDC driver
# =============================================================================

add_library(rx_usb_comm STATIC
    src/rx_usb_comm.c
)

target_include_directories(rx_usb_comm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(rx_usb_comm
    PUBLIC
        rx_core
        rx_frame
        rx_usb
)
