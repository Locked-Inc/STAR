idf_component_register(
    SRCS "src/star_ota.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update esp_partition
)
