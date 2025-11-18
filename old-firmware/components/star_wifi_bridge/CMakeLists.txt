# esp32-firmware/components/star_wifi_bridge/CMakeLists.txt

idf_component_register(SRCS "pynq_wifi_protocol.c"
                            "pynq_wifi_transport.c"
                            "pynq_wifi_handler.c"
                            "pynq_wifi_manager.c"
                            "pynq_ota_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio
                             star_bus
                    PRIV_REQUIRES esp_driver_uart
                                  esp_driver_spi
                                  esp_wifi
                                  esp_netif
                                  esp_https_ota
                                  esp_http_client
                                  app_update
                                  json
                                  mbedtls
                                  star_pin_validator
                                  star_error_handler)
