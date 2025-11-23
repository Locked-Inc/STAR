idf_component_register(
    SRCS "src/star_sensor_dht22.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer star_error_handler
)
