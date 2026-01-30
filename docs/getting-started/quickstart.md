# Quick Start Guide

Get up and running with the PCAN-USB Pro FD adapter in minutes!

## Prerequisites

Before you begin, ensure you have:

- STM32H743 development board or custom PCAN hardware
- USB cable for programming and communication
- ARM GCC toolchain installed
- CMake (version 3.22 or higher)
- ST-Link or compatible programmer

## Step 1: Clone the Repository

```bash
git clone https://github.com/yourusername/PCAN.git
cd PCAN
```

## Step 2: Build the Firmware

```bash
# Create build directory
mkdir -p build && cd build

# Configure the project
cmake ..

# Build (use all available CPU cores)
cmake --build . -j$(nproc)
```

Expected output:
```
[100%] Linking C executable PCAN.elf
   text    data     bss     dec     hex filename
  45678    1234    5678   52590    cd6e PCAN.elf
[100%] Built target PCAN
```

## Step 3: Flash the Firmware

Using OpenOCD:
```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program PCAN.elf verify reset exit"
```

Or using st-flash:
```bash
st-flash write PCAN.bin 0x8000000
```

## Step 4: Connect Hardware

1. **Connect CAN Bus**:
   - FDCAN1: PD0 (RX), PD1 (TX)
   - FDCAN2: PB5 (RX), PB6 (TX)
   - Add 120Ω termination resistors if needed

2. **Connect USB**:
   - Plug USB cable into your computer
   - Device should enumerate as USB HID

3. **Verify LEDs**:
   - LED_1 (PC7): CAN1 activity
   - LED_2 (PE2): CAN2 activity

## Step 5: Test Communication

### On Linux

```bash
# Check if device is detected
lsusb | grep PCAN

# Use SocketCAN (if driver is available)
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
```

### On Windows

1. Install PCAN-View software
2. Select the PCAN-USB Pro FD device
3. Configure bitrate (e.g., 500 kbps)
4. Start monitoring

## Next Steps

- [Configure CAN parameters](../usage/can-config.md)
- [Learn about the USB protocol](../software/usb-protocol.md)
- [Explore the API](../api/can.md)
- [Debug your setup](../build/debugging.md)

## Troubleshooting

!!! warning "Device not detected"
    - Check USB cable connection
    - Verify USB clock configuration (48 MHz)
    - Try a different USB port

!!! warning "Build fails"
    - Ensure ARM GCC toolchain is in PATH
    - Check CMake version (≥ 3.22)
    - Delete `build/` directory and try again

!!! warning "CAN messages not received"
    - Verify CAN bus termination (120Ω)
    - Check bitrate settings match your network
    - Ensure proper wiring (CAN_H, CAN_L)

## Getting Help

If you encounter issues:

1. Check the [Troubleshooting Guide](../usage/troubleshooting.md)
2. Review [GitHub Issues](https://github.com/yourusername/PCAN/issues)
3. Ask in [Discussions](https://github.com/yourusername/PCAN/discussions)
