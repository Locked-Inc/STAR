# esp32-firmware/components/star_error_handler/test/CMakeLists.txt

idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "."
                       REQUIRES star_test star_error_handler)
