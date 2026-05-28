idf_component_register(SRCS "main.c" "usb/usb_descriptors.c" "can/pcan_twai.c" "../../pcan_common/pcanpro_fd_protocol.c" "../../pcan_common/pcanpro_timestamp.c" "../../pcan_common/pcanpro_led.c"
                    INCLUDE_DIRS "." "usb" "../../pcan_common"
                    REQUIRES espressif__tinyusb driver esp_timer usb)

target_compile_definitions(${COMPONENT_LIB} PRIVATE PCAN_FD)

idf_component_get_property(tusb_lib espressif__tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/usb")
