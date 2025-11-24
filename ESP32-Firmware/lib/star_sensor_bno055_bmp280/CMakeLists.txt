idf_component_register(
    SRCS "src/star_sensor_bno055_bmp280.c"
    INCLUDE_DIRS "include"
    REQUIRES star_bus star_error_handler
)
