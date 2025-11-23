idf_component_register(
    SRCS "src/star_sensor_qmc5883l.c"
    INCLUDE_DIRS "include"
    REQUIRES star_bus star_error_handler
)
