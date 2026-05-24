# ============================
# C++ options
# ============================
set(CMAKE_CXX_STANDARD 17)
message("STATUS: Including user.cmake...")
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
    ${CMAKE_SOURCE_DIR}/Core/can
    ${CMAKE_SOURCE_DIR}/Core/usb
    ${CMAKE_SOURCE_DIR}/../pcan_common
)

# ============================
# Sources
# ============================
target_sources(${PROJECT_NAME} PRIVATE

    # ${CMAKE_SOURCE_DIR}/Core/Src/main.cpp  # Not used - using main.c instead
    ${CMAKE_SOURCE_DIR}/Core/Src/wchar_stubs.c


    # PCAN sources
    ${CMAKE_SOURCE_DIR}/Core/can/pcanpro_can.c
    ${CMAKE_SOURCE_DIR}/../pcan_common/pcanpro_led.c
    ${CMAKE_SOURCE_DIR}/../pcan_common/pcanpro_fd_protocol.c
    ${CMAKE_SOURCE_DIR}/../pcan_common/pcanpro_timestamp.c
    ${CMAKE_SOURCE_DIR}/Core/usb/pcanpro_usbd.c
    ${CMAKE_SOURCE_DIR}/Core/usb/pcan_usb.c

)

# Validates that CMAKE_OBJCOPY is set, otherwise tries to find it
if(NOT CMAKE_OBJCOPY)
    message(STATUS "Searching for objcopy...")
    find_program(CMAKE_OBJCOPY NAMES arm-none-eabi-objcopy objcopy)
endif()

if(CMAKE_OBJCOPY)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
        COMMENT "Generating .bin and .hex files..."
    )
else()
    message(WARNING "objcopy not found. .bin and .hex files will not be generated automatically.")
endif()
