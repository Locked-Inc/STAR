idf_component_register(
    SRCS "src/star_sensor_mpu6050.c"
    INCLUDE_DIRS "include"
    REQUIRES star_bus star_error_handler
)
