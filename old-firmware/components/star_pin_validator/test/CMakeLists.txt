# esp32-firmware/components/star_pin_validator/test/CMakeLists.txt

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES star_test star_pin_validator esp_driver_gpio)
