idf_component_register(
    SRCS "src/star_bms_bq7850.c"
    INCLUDE_DIRS "include"
    REQUIRES star_bus star_error_handler
)
