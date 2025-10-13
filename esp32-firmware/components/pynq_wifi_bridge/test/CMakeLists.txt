# esp32-firmware/components/pynq_wifi_bridge/test/CMakeLists.txt

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES star_test pynq_wifi_bridge star_bus)
