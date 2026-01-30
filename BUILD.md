# Build Instructions

This document provides detailed instructions for building the PCAN-USB Pro FD firmware.

## Quick Start

```bash
# Create build directory
mkdir -p build && cd build

# Configure
cmake ..

# Build
cmake --build . -j$(nproc)

# Flash (optional)
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
  -c "program PCAN.elf verify reset exit"
```

## Prerequisites

### Required Tools

1. **CMake** (≥ 3.22)
   ```bash
   cmake --version
   ```

2. **ARM GCC Toolchain**
   ```bash
   arm-none-eabi-gcc --version
   ```

3. **Make** or **Ninja** (build system)

### Optional Tools

- **OpenOCD**: For debugging and flashing
- **STM32CubeProgrammer**: Alternative flashing tool
- **STM32CubeMX**: For modifying hardware configuration

## Detailed Build Steps

### 1. Configure Build

#### Debug Build (Default)
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

#### Release Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

#### With Ninja
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

### 2. Compile

```bash
cmake --build build
```

Or with parallel jobs:
```bash
cmake --build build -j$(nproc)
```

### 3. Build Outputs

After successful build, you'll find in `build/`:

- **PCAN.elf** - ELF executable with debug symbols
- **PCAN.bin** - Raw binary for flashing
- **PCAN.hex** - Intel HEX format
- **PCAN.map** - Memory map file

## Build Configurations

### Compiler Flags

**Debug Mode:**
- `-O0` - No optimization
- `-g3` - Maximum debug info
- `-DDEBUG` - Debug macros enabled

**Release Mode:**
- `-O2` - Optimize for speed
- `-DNDEBUG` - Assertions disabled
- Smaller binary size

### Custom Defines

Edit `CMakeLists.txt` to add custom defines:
```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    PCAN_PRO_FD
    USE_CPP_LOGGER
    # Add your defines here
)
```

## Cleaning Build

```bash
# Remove build directory
rm -rf build

# Or clean within build
cmake --build build --target clean
```

## Troubleshooting

### Common Issues

**1. CMake not found**
```bash
sudo apt-get install cmake
```

**2. Toolchain not found**
```bash
# Ubuntu/Debian
sudo apt-get install gcc-arm-none-eabi

# macOS
brew install --cask gcc-arm-embedded
```

**3. Build fails with "No rule to make target"**
- Delete `build/` directory
- Reconfigure with `cmake -B build`

**4. Out of memory during compilation**
- Reduce parallel jobs: `cmake --build build -j2`

## Advanced Options

### Verbose Build
```bash
cmake --build build --verbose
```

### Specific Target
```bash
cmake --build build --target PCAN
```

### Generate compile_commands.json
Already enabled by default for clangd support.

## Cross-Platform Notes

### Linux
- Preferred platform
- All features supported

### macOS
- ARM GCC via Homebrew
- OpenOCD may require additional setup

### Windows
- Use MinGW or MSYS2
- Ensure ARM GCC is in PATH
- Consider WSL for easier setup

## Next Steps

After building:
1. [Flash the firmware](README.md#flashing-the-firmware)
2. [Test on hardware](README.md#usage)
3. [Debug if needed](README.md#debugging)
