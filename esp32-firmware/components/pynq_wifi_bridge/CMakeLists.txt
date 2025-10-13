# esp32-firmware/components/pynq_wifi_bridge/CMakeLists.txt

idf_component_register(SRCS "pynq_wifi_protocol.c"
                            "pynq_wifi_transport.c"
                            "pynq_wifi_handler.c"
                            "pynq_wifi_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio
                             star_bus
                    PRIV_REQUIRES esp_driver_uart
                                  esp_driver_spi
                                  esp_wifi
                                  esp_netif
                                  star_pin_validator
                                  star_error_handler)
