# esp32-firmware/main/CMakeLists.txt

idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS ""
                    REQUIRES pynq_wifi_bridge nvs_flash star_pin_validator)
