# PCAN-USB Pro FD Adapter Firmware

A high-performance, open-source firmware implementation for STM32H7 microcontrollers that emulates the PEAK PCAN-USB Pro FD adapter. This project enables STM32 hardware to interface with standard PCAN software tools (PCAN-View, PCAN-Explorer, etc.) via USB.

## 🚀 Features

*   **Dual Channel Support**: Two independent CAN-FD channels (CAN1 & CAN2).
*   **CAN-FD Ready**: Supports Flexible Data-rate (FD) and standard CAN 2.0B.
*   **High Performance**: Powered by STM32H743 (Cortex-M7 @ 480 MHz).
*   **USB 2.0 High Speed**: Utilizes STM32 USB OTG interface for low-latency communication.
*   **Standard Compatibility**: 
    *   Fully compatible with official PEAK drivers (Linux & Windows).
    *   Works with PCAN-View and PCAN-Basic API.
*   **Precise Timing**: Hardware-based timestamping for accurate message logging.

## 🛠 Hardware Specifications

This firmware is designed for the **STM32H743ZIT6** (Nucleo-144 or custom board).

| Feature | Pin | Description |
| :--- | :--- | :--- |
| **USB** | `PA11` (DM), `PA12` (DP) | USB OTG FS (used as device) |
| **CAN1** | `PD0` (RX), `PD1` (TX) | FDCAN1 Interface |
| **CAN2** | `PB5` (RX), `PB6` (TX) | FDCAN2 Interface |
| **LED 1** | `PC7` | Activity Indicator (Channel 1) |
| **LED 2** | `PE2` | Activity Indicator (Channel 2) |
| **Debug** | `PA9` (TX), `PA10` (RX) | USART1 for printf logs (115200 8N1) |

## 📦 Project Structure

```
PCAN/
├── CMakeLists.txt         # Root build configuration
├── Core/                  # Main application source
│   ├── pcan/              # PCAN Protocol Implementation
│   └── Src/               # STM32 HAL Callbacks & Init
├── Drivers/               # STM32 HAL & Low-Layer Drivers
├── Middlewares/           # USB Device Library
├── USB_DEVICE/            # USB Class & Descriptor configurations
└── cmake/                 # Build scripts & toolchain files
```

## 🔨 Build Instructions

### Prerequisites
*   **CMake** (3.22+)
*   **ARM GCC Toolchain** (`arm-none-eabi-gcc`)
*   **Make** or **Ninja**

### 1. Clone the Repository
```bash
git clone https://github.com/thakurpaudel/PCAN-STM32H7.git
cd PCAN
```

### 2. Configure with CMake
You can choose between **Classic CAN** (default) or **CAN-FD** mode.

**For Standard CAN-FD Mode (Recommended):**
```bash
cmake -B build -DPCAN_CAN_FD_MODE=ON
```

**For Legacy Classic CAN Mode:**
```bash
cmake -B build -DPCAN_CAN_FD_MODE=OFF
```

### 3. Compile
```bash
cmake --build build -j$(nproc)
```

### 4. Output Files
The build process automatically generates the following in the `build/` directory:
*   `PCAN.elf`: The main executable (with debug symbols).
*   `PCAN.bin`: Binary file for flashing.
*   `PCAN.hex`: Intel HEX file for flashing.

## ⚡ Flashing

### Using STM32CubeProgrammer
```bash
STM32_Programmer_CLI -c port=SWD -w build/PCAN.bin 0x08000000 -v -rst
```

### Using OpenOCD
```bash
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg -c "program build/PCAN.elf verify reset exit"
```

## 🔌 Usage

1.  **Connect** the STM32 board to your PC via the USB User port.
2.  **Verify Enumeration**:
    *   **Unux**: Run `lsusb`. You should see a generic or PEAK device.
        *   *Note: On Linux, `socketcan` should automatically pick it up if you use the peak-linux-driver.*
    *   **Windows**: Device Manager should show "PCAN-USB Pro FD" (if drivers are installed).
3.  **Open PCAN-View**:
    *   Select the device from the list.
    *   Initialize the bitrate (e.g., 500 kbit/s / 2 Mbit/s Data).
    *   You should now see TX/RX traffic.

## 🤝 Contributing

Contributions are welcome! Please format code according to the project's `.clang-format` style.

## 📄 License

This software component is licensed by STMicroelectronics under the BSD-3-Clause license, requiring preservation of copyright notices. PCAN protocol compatibility is provided for educational and interoperability purposes.
