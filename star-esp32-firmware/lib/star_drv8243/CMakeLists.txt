idf_component_register(
    SRCS
        "src/star_drv8243.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
        driver
        star_bus
        star_motor
)
