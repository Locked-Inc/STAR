idf_component_register(
    SRCS
        "src/star_sensor_ds18b20.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
        freertos
        star_bus
)
