# esp32-firmware/components/star_bms_bq7850/CMakeLists.txt

idf_component_register(
    SRCS "star_bms_bq7850.c"
    INCLUDE_DIRS "include"
    REQUIRES star_bus star_error_handler
)
