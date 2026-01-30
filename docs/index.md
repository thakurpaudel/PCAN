# PCAN-USB Pro FD Adapter Documentation

Welcome to the PCAN-USB Pro FD Adapter firmware documentation!

## Overview

The PCAN-USB Pro FD Adapter is a high-performance CAN/CAN-FD to USB interface based on the STM32H743ZIT6 microcontroller. This project implements a firmware compatible with the PEAK PCAN-USB Pro FD protocol, enabling dual-channel CAN-FD communication over USB.

## Key Features

- **Dual CAN-FD Channels**: Two independent CAN-FD interfaces with up to 2.5 Mbps nominal bitrate
- **USB 2.0 Full Speed**: Custom HID device class for cross-platform compatibility
- **High Performance**: STM32H743 ARM Cortex-M7 @ 480 MHz
- **CAN-FD Support**: Full support for CAN-FD protocol with flexible data rate
- **Hardware Timestamping**: Precise message timing with microsecond resolution
- **LED Indicators**: Visual feedback for CAN bus activity
- **PCAN Protocol Compatible**: Works with existing PCAN software tools

## Hardware Specifications

| Component | Specification |
|-----------|---------------|
| **Microcontroller** | STM32H743ZIT6 |
| **Core** | ARM Cortex-M7 |
| **CPU Frequency** | 480 MHz |
| **Flash Memory** | 2 MB |
| **RAM** | 1 MB |
| **CAN Controllers** | 2x FDCAN |
| **USB** | USB OTG Full Speed |
| **Package** | LQFP144 |

## Quick Links

- [Getting Started Guide](getting-started/quickstart.md)
- [Build Instructions](build/instructions.md)
- [Hardware Specifications](hardware/specifications.md)
- [API Reference](api/can.md)
- [Contributing Guidelines](CONTRIBUTING.md)

## Project Structure

```
PCAN/
├── Core/               # Application code
│   ├── Inc/           # Headers
│   ├── Src/           # Source files
│   └── pcan/          # PCAN implementation
├── Drivers/           # STM32 HAL drivers
├── Middlewares/       # USB middleware
├── USB_DEVICE/        # USB device stack
├── cmake/             # CMake configuration
└── docs/              # Documentation
```

## Getting Started

1. **[Install Prerequisites](getting-started/prerequisites.md)** - Set up your development environment
2. **[Build the Firmware](build/instructions.md)** - Compile the project
3. **[Flash to Hardware](build/flashing.md)** - Program your STM32H743
4. **[Test CAN Communication](usage/testing.md)** - Verify functionality

## Support

- **Issues**: [GitHub Issues](https://github.com/yourusername/PCAN/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/PCAN/discussions)
- **Documentation**: This site!

## License

Copyright © 2026 Yatri Team. All rights reserved.

This software is licensed under terms that can be found in the LICENSE file.
