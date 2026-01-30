##########################################################################################################################
# STM32H7 PCAN Pro X Makefile
##########################################################################################################################

######################################
# target
######################################
TARGET = PCAN

######################################
# building variables
######################################
# debug build?
DEBUG = 1
# optimization
OPT = -Og

#######################################
# paths
#######################################
# Build path
BUILD_DIR = build

######################################
# source
######################################
# C sources (common)
C_SOURCES =  \
Core/Src/main.c \
Core/Src/gpio.c \
Core/Src/stm32h7xx_it.c \
Core/Src/stm32h7xx_hal_msp.c \
Core/pcan/pcanpro_can.c \
Core/pcan/pcanpro_led.c \
Core/pcan/pcanpro_timestamp.c \
Core/pcan/usbd_conf.c \
Core/pcan/usb_device.c \
Core/pcan/pcanpro_usbd.c \
Core/pcan/usbd_desc.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_fdcan.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd_ex.c \
Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usb.c \
Core/Src/system_stm32h7xx.c \
Core/Src/sysmem.c \
Core/Src/syscalls.c \
Core/Src/tim.c \
Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c \
Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c

# Protocol source (added dynamically by build target)
ifdef PROTO
C_SOURCES += $(PROTO)
endif

# ASM sources
ASM_SOURCES =  \
startup_stm32h743xx.s

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S
 
#######################################
# CFLAGS
#######################################
# cpu
CPU = -mcpu=cortex-m7

# fpu
FPU = -mfpu=fpv5-d16

# float-abi
FLOAT-ABI = -mfloat-abi=hard

# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# AS defines
AS_DEFS = 

# C defines
C_DEFS =  \
-DUSE_PWR_LDO_SUPPLY \
-DUSE_HAL_DRIVER \
-DSTM32H743xx

# AS includes
AS_INCLUDES = 

# C includes
C_INCLUDES =  \
-ICore/Inc \
-ICore/pcan \
-IDrivers/STM32H7xx_HAL_Driver/Inc \
-IMiddlewares/ST/STM32_USB_Device_Library/Core/Inc \
-IDrivers/STM32H7xx_HAL_Driver/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32H7xx/Include \
-IDrivers/CMSIS/Include

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

# IMPORTANT: Add BOARD_FLAGS to CFLAGS
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

# Add board-specific flags if defined
ifdef BOARD_FLAGS
CFLAGS += $(BOARD_FLAGS)
endif

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

#######################################
# LDFLAGS
#######################################
# link script
LDSCRIPT = STM32H743XX_FLASH.ld

# libraries
LIBS = -lc -lm -lnosys 
LIBDIR = 
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

#######################################
# Default target
#######################################
.DEFAULT_GOAL := help

help:
	@echo "STM32H7 PCAN Build System"
	@echo "========================="
	@echo ""
	@echo "Available targets:"
	@echo "  make pro        - Build PCAN-USB Pro (Classic CAN, 2 channels)"
	@echo "  make pro_fd     - Build PCAN-USB Pro FD (CAN FD, 2 channels)"
	@echo "  make fd         - Build PCAN-USB FD (CAN FD, 1 channel)"
	@echo "  make x6         - Build PCAN-USB X6 (CAN FD, 2 channels)"
	@echo "  make all_boards - Build all variants"
	@echo "  make clean      - Clean build directory"
	@echo ""
	@echo "Build options:"
	@echo "  DEBUG=1         - Build with debug symbols (default)"
	@echo "  DEBUG=0         - Build without debug"
	@echo "  OPT=-Os         - Optimization level"
	@echo ""

#######################################
# Build targets
#######################################

# PCAN-USB Pro (Classic CAN only, 2 channels)
pro:
	@echo "Building PCAN-USB Pro..."
	@$(MAKE) --no-print-directory \
	        BOARD=pro \
	        TARGET=pcan_pro \
	        DEBUG=0 \
	        OPT=-Os \
	        PROTO=Core/pcan/pcanpro_protocol.c \
	        BOARD_FLAGS='-DPCAN_PRO=1 -DINCLUDE_LIN_INTERFACE=1' \
	        build_firmware

# PCAN-USB Pro FD (CAN FD, 2 channels)
pro_fd:
	@echo "Building PCAN-USB Pro FD..."
	@$(MAKE) --no-print-directory \
	        BOARD=pro_fd \
	        TARGET=pcan_pro_fd \
	        DEBUG=0 \
	        OPT=-Os \
	        PROTO=Core/pcan/pcanpro_fd_protocol.c \
	        BOARD_FLAGS='-DPCAN_PRO_FD=1 -DINCLUDE_LIN_INTERFACE=1' \
	        build_firmware

# PCAN-USB FD (CAN FD, 1 channel)
fd:
	@echo "Building PCAN-USB FD..."
	@$(MAKE) --no-print-directory \
	        BOARD=fd \
	        TARGET=pcan_fd \
	        DEBUG=0 \
	        OPT=-Os \
	        PROTO=Core/pcan/pcanpro_fd_protocol.c \
	        BOARD_FLAGS='-DPCAN_FD=1 -DINCLUDE_LIN_INTERFACE=0' \
	        build_firmware

# PCAN-USB X6 (CAN FD, 2 channels)
x6:
	@echo "Building PCAN-USB X6..."
	@$(MAKE) --no-print-directory \
	        BOARD=pcan_x6 \
	        TARGET=pcan_x6 \
	        DEBUG=0 \
	        OPT=-Os \
	        PROTO=Core/pcan/pcanpro_fd_protocol.c \
	        BOARD_FLAGS='-DPCAN_X6=1 -DINCLUDE_LIN_INTERFACE=0' \
	        build_firmware

# Build all variants
all_boards: pro pro_fd fd x6

#######################################
# build the application
#######################################
# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR) 
	@echo "  CC    $<"
	@$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@echo "  AS    $<"
	@$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo "  LD    $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@echo ""
	@$(SZ) $@
	@echo ""

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@echo "  HEX   $@"
	@$(HEX) $< $@
	
$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@echo "  BIN   $@"
	@$(BIN) $< $@	
	
$(BUILD_DIR):
	@mkdir -p $@

#######################################
# Build firmware (internal target)
#######################################
build_firmware: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
	@echo ""
	@echo "Build complete: $(TARGET)"
	@echo "Output files:"
	@echo "  - $(BUILD_DIR)/$(TARGET).elf"
	@echo "  - $(BUILD_DIR)/$(TARGET).hex"
	@echo "  - $(BUILD_DIR)/$(TARGET).bin"
	@echo ""

#######################################
# clean up
#######################################
clean:
	@echo "Cleaning build directory..."
	@rm -fR $(BUILD_DIR)
	@echo "Done."
  
#######################################
# dependencies
#######################################
-include $(wildcard $(BUILD_DIR)/*.d)

#######################################
# Phony targets
#######################################
.PHONY: help clean pro pro_fd fd x6 all_boards build_firmware

# *** EOF ***