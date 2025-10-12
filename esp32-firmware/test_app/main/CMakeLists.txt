# esp32-firmware/test_app/main/CMakeLists.txt

idf_component_register(SRCS "test_main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES star_test)
