# ============================
# C++ options
# ============================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

target_compile_options(${PROJECT_NAME} PRIVATE
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    USE_CPP_LOGGER
)

target_link_options(${PROJECT_NAME} PRIVATE
    -u _printf_float
)

# ============================
# Include paths
# ============================
target_include_directories(${PROJECT_NAME} PRIVATE
    # ${CMAKE_SOURCE_DIR}/user/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
    ${CMAKE_SOURCE_DIR}/Core/pcan
)

# ============================
# Sources
# ============================
target_sources(${PROJECT_NAME} PRIVATE

    # ${CMAKE_SOURCE_DIR}/Core/Src/main.cpp  # Not used - using main.c instead
    ${CMAKE_SOURCE_DIR}/Core/Src/wchar_stubs.c

    # PCAN sources
    ${CMAKE_SOURCE_DIR}/Core/pcan/pcanpro_can.c
    ${CMAKE_SOURCE_DIR}/Core/pcan/pcanpro_led.c
    ${CMAKE_SOURCE_DIR}/Core/pcan/pcanpro_protocol.c
    ${CMAKE_SOURCE_DIR}/Core/pcan/pcanpro_timestamp.c
    ${CMAKE_SOURCE_DIR}/Core/pcan/pcanpro_usbd.c
    ${CMAKE_SOURCE_DIR}/Core/pcan/pcan_usb.c

)
