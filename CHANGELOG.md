# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial project setup with STM32H743ZIT6
- Dual CAN-FD controller support (FDCAN1, FDCAN2)
- USB Custom HID device implementation
- PCAN-USB Pro FD protocol compatibility
- Hardware timestamping support
- LED indicators for CAN activity
- CMake build system
- Comprehensive documentation (README, CONTRIBUTING)
- Git configuration files (.gitignore, .gitattributes)

### Hardware
- STM32H743ZIT6 @ 480 MHz
- 2x FDCAN interfaces @ 2.5 Mbps
- USB 2.0 Full Speed
- Debug UART (USART1)
- Auxiliary UART (UART4)

## [0.1.0] - 2026-01-30

### Added
- Initial commit with STM32CubeMX generated code
- Basic project structure
- HAL driver integration
- USB device stack
- PCAN protocol implementation files

---

## Version History

### Version Numbering
- **MAJOR**: Incompatible API changes
- **MINOR**: New functionality (backwards-compatible)
- **PATCH**: Bug fixes (backwards-compatible)

### Categories
- **Added**: New features
- **Changed**: Changes to existing functionality
- **Deprecated**: Soon-to-be removed features
- **Removed**: Removed features
- **Fixed**: Bug fixes
- **Security**: Security fixes
