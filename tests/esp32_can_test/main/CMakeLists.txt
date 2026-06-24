idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_driver_gpio esp_driver_twai esp_timer)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=cpp" "-Wno-error=unused-function" "-Wno-unused-function")

