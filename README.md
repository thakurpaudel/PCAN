# PCAN-USB Pro FD Adapter

A high-performance CAN/CAN-FD to USB adapter based on the STM32H743ZIT6 microcontroller, compatible with PEAK PCAN-USB Pro FD protocol.

## Features

- **Dual CAN-FD Channels**: Two independent CAN-FD interfaces with up to 2.5 Mbps nominal bitrate
- **USB 2.0 Full Speed**: Custom HID device class for cross-platform compatibility
- **High Performance**: STM32H743 ARM Cortex-M7 @ 480 MHz
- **CAN-FD Support**: Full support for CAN-FD protocol with flexible data rate
- **Timestamp**: Hardware timestamping for precise message timing
- **LED Indicators**: Visual feedback for CAN bus activity

## Hardware Specifications

### Microcontroller
- **MCU**: STM32H743ZIT6
- **Core**: ARM Cortex-M7
- **Frequency**: 480 MHz
- **Flash**: 2 MB
- **RAM**: 1 MB
- **Package**: LQFP144

### Peripherals
- **CAN Controllers**: 2x FDCAN (CAN-FD capable)
  - FDCAN1: PD0 (RX), PD1 (TX)
  - FDCAN2: PB5 (RX), PB6 (TX)
- **USB**: USB OTG FS (PA11/PA12)
- **Debug UART**: USART1 (PA9/PA10)
- **Auxiliary UART**: UART4 (PC10/PC11)
- **Timers**: TIM3, TIM8
- **LEDs**: 
  - LED_1: PC7
  - LED_2: PE2

### Clock Configuration
- **CPU Clock**: 480 MHz
- **AHB Clock**: 240 MHz
- **APB1/2/3/4**: 120 MHz
- **FDCAN Clock**: 120 MHz
- **USB Clock**: 48 MHz (HSI48)

## Project Structure

```
PCAN/
├── Core/
│   ├── Inc/              # STM32 HAL headers
│   ├── Src/              # STM32 HAL initialization code
│   └── pcan/             # PCAN-specific implementation
│       ├── pcanpro_can.c/h           # CAN driver abstraction
│       ├── pcanpro_protocol.c/h      # USB protocol handler
│       ├── pcanpro_fd_protocol.c     # CAN-FD protocol
│       ├── pcanpro_usbd.c/h          # USB device interface
│       ├── pcanpro_led.c/h           # LED control
│       └── pcanpro_timestamp.c/h     # Timestamp management
├── Drivers/              # STM32 HAL drivers
├── Middlewares/          # USB middleware
├── USB_DEVICE/           # USB device stack
├── cmake/                # CMake configuration
├── CMakeLists.txt        # Main CMake file
└── PCAN.ioc              # STM32CubeMX project file

```

## Prerequisites

### Software Requirements
- **CMake**: Version 3.22 or higher
- **ARM GCC Toolchain**: arm-none-eabi-gcc
- **STM32CubeMX**: (optional) For regenerating peripheral initialization code
- **OpenOCD** or **STM32CubeProgrammer**: For flashing

### Installing ARM GCC Toolchain

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install gcc-arm-none-eabi binutils-arm-none-eabi cmake
```

**macOS:**
```bash
brew install --cask gcc-arm-embedded
brew install cmake
```

**Windows:**
- Download from [ARM Developer](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm)
- Install CMake from [cmake.org](https://cmake.org/download/)

## Building the Project

### Using CMake (Recommended)

1. **Clone the repository:**
   ```bash
   cd /home/thakur/Documents/yatri/PCAN
   ```

2. **Create build directory:**
   ```bash
   mkdir -p build
   cd build
   ```

3. **Configure the project:**
   ```bash
   cmake ..
   ```

4. **Build:**
   ```bash
   cmake --build .
   ```

   Or for parallel build:
   ```bash
   cmake --build . -j$(nproc)
   ```

5. **Output files:**
   - `build/PCAN.elf` - ELF executable
   - `build/PCAN.bin` - Binary file for flashing
   - `build/PCAN.hex` - Intel HEX file

### Build Configurations

**Debug Build (default):**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

**Release Build:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Using Makefile

Alternatively, you can use the generated Makefile:
```bash
make -j$(nproc)
```

## Flashing the Firmware

### Using OpenOCD

```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program build/PCAN.elf verify reset exit"
```

### Using STM32CubeProgrammer

```bash
STM32_Programmer_CLI -c port=SWD -w build/PCAN.bin 0x08000000 -v -rst
```

### Using st-flash

```bash
st-flash write build/PCAN.bin 0x8000000
```

## Usage

### Connecting to PC

1. Connect the USB cable to your computer
2. The device should enumerate as a USB HID device
3. Use PCAN-compatible software to communicate with CAN buses

### CAN Bus Connections

**FDCAN1:**
- CAN_H: Connect to CAN bus high line
- CAN_L: Connect to CAN bus low line
- Add 120Ω termination resistor if needed

**FDCAN2:**
- CAN_H: Connect to second CAN bus high line
- CAN_L: Connect to second CAN bus low line
- Add 120Ω termination resistor if needed

### LED Indicators

- **LED_1 (PC7)**: CAN1 activity
- **LED_2 (PE2)**: CAN2 activity

## Development

### Code Style

- **C Standard**: C11
- **C++ Standard**: C++17 (for future extensions)
- **Indentation**: Spaces (configured in `.clangd`)

### Modifying Hardware Configuration

1. Open `PCAN.ioc` in STM32CubeMX
2. Make your changes
3. Regenerate code
4. Rebuild the project

### Adding Custom Code

Use the `USER CODE BEGIN` and `USER CODE END` sections in generated files to add custom code that won't be overwritten by STM32CubeMX.

## Debugging

### Using GDB with OpenOCD

1. **Start OpenOCD:**
   ```bash
   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg
   ```

2. **In another terminal, start GDB:**
   ```bash
   arm-none-eabi-gdb build/PCAN.elf
   ```

3. **Connect to OpenOCD:**
   ```
   (gdb) target extended-remote localhost:3333
   (gdb) monitor reset halt
   (gdb) load
   (gdb) continue
   ```

### Serial Debug Output

Connect to USART1 (PA9/PA10) at 115200 baud for debug output.

## Protocol Documentation

This adapter implements the PCAN-USB Pro FD protocol, compatible with:
- PCAN-View
- PCAN-Explorer
- SocketCAN (Linux)
- PCAN-Basic API

### Key Features

- **Message Filtering**: Hardware CAN ID filtering
- **Timestamps**: 32-bit microsecond timestamps
- **Error Frames**: Full error frame reporting
- **Bus Statistics**: TX/RX message counts, error counts

## Troubleshooting

### Build Issues

**Problem**: `arm-none-eabi-gcc not found`
- **Solution**: Install ARM GCC toolchain (see Prerequisites)

**Problem**: CMake configuration fails
- **Solution**: Ensure CMake version is 3.22 or higher

### Flashing Issues

**Problem**: Cannot connect to target
- **Solution**: Check ST-Link connection, ensure jumpers are set correctly

**Problem**: Flash verification failed
- **Solution**: Erase flash completely before programming

### Runtime Issues

**Problem**: USB device not recognized
- **Solution**: Check USB cable, try different USB port, verify USB clock configuration

**Problem**: CAN messages not received
- **Solution**: Check CAN bus termination, verify bitrate settings, check wiring

## License

Copyright (c) 2026 STMicroelectronics.
All rights reserved.

This software is licensed under terms that can be found in the LICENSE file in the root directory of this software component.

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Support

For issues and questions:
- Open an issue on GitHub
- Check the [STM32H7 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0433-stm32h742-stm32h743753-and-stm32h750-value-line-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- Consult [PCAN-USB Pro FD documentation](https://www.peak-system.com/PCAN-USB-Pro-FD.366.0.html)

## Acknowledgments

- STMicroelectronics for STM32 HAL libraries
- PEAK-System for PCAN protocol documentation
- ARM for CMSIS libraries
