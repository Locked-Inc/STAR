# esp32-firmware/components/pynq_wifi_bridge/test/CMakeLists.txt

idf_component_register(SRCS "test_protocol.c"
                            # "test_transport.c"  # Disabled - requires hardware UART/SPI
                            # "test_transport_error_handler.c"  # Disabled - requires hardware transport
                            "test_ota_manager.c"
                            "test_ota_handler.c"
                            "test_http_handler.c"
                            "test_tcp_handler.c"
                            "test_udp_handler.c"
                            "test_wifi_handler.c"
                            "test_system_handler.c"
                            "test_transport_mock.c"
                       INCLUDE_DIRS "."
                       REQUIRES star_test pynq_wifi_bridge star_bus)

# Wrap transport_send for testing - allows test mocks to override real implementation
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=transport_send")
