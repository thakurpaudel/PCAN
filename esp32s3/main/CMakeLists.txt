idf_component_register(SRCS "main.c" "usb_descriptors.c" "pcan_twai.c" "pcanpro_protocol.c" "pcanpro_timestamp.c"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__tinyusb driver esp_timer)

target_compile_definitions(${COMPONENT_LIB} PRIVATE PCAN_PRO)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
