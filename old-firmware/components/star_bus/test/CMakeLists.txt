# esp32-firmware/components/star_bus/test/CMakeLists.txt

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES star_test star_bus esp_driver_gpio)
