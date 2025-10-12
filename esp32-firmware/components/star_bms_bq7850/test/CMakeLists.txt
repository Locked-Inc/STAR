# esp32-firmware/components/star_bms_bq7850/test/CMakeLists.txt

idf_component_register(
    SRCS "test_bq7850.c"
    INCLUDE_DIRS "."
    REQUIRES unity star_bms_bq7850 star_bus star_test
)
