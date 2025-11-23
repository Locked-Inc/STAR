idf_component_register(
    SRCS "src/star_sensor_sick_tim561.c"
    INCLUDE_DIRS "include"
    REQUIRES star_error_handler lwip
)
