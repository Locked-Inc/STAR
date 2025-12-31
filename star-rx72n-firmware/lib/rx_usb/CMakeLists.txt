# =============================================================================
# rx_usb - USB CDC-ACM Driver for RX72N
#
# This library provides USB CDC-ACM (virtual COM port) functionality for
# communication with the Raspberry Pi 5 host.
#
# Architecture:
#   rx_usb.c      - Public API and state management
#   rx_usb_hw.c   - Hardware-level USB0 register access
#   rx_usb_cdc.c  - CDC class implementation (descriptors, requests)
#   rx_usb_isr.c  - Interrupt service routines
#
# Dependencies:
#   rx_core - Error handling and logging
#   rx_hal  - Hardware abstraction (system clock, interrupts)
# =============================================================================

add_library(rx_usb STATIC
    src/rx_usb.c
    src/rx_usb_hw.c
    src/rx_usb_cdc.c
    src/rx_usb_isr.c
)

target_include_directories(rx_usb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_link_libraries(rx_usb
    PUBLIC
        rx_core
        rx_hal
)

# Compiler warnings specific to USB driver
target_compile_options(rx_usb
    PRIVATE
        -Wno-unused-function  # Some FIT callbacks may not be used initially
)
