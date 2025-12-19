idf_component_register(
    SRCS
        "src/star_arq.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_common
        freertos
        star_frame
        star_bus
)
