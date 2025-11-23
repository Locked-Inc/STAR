idf_component_register(
    SRCS "src/star_sensor_bh1750.c"
    INCLUDE_DIRS "include"
    REQUIRES star_bus star_error_handler
)
