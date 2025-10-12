# esp32-firmware/components/star_test/CMakeLists.txt

idf_component_register(SRCS "star_test.c"
                       INCLUDE_DIRS "include"
                       REQUIRES log)
