idf_component_register(
    SRCS
        "src/star_encoder.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_common
        esp_timer
)
