# my_app - Zephyr Trace Application for ESP32-DevKitC-WROVER

## Overview

A minimal Zephyr RTOS application that sends trace, debug, and system status
messages over UART via the USB cable. Designed for the **ESP32-DevKitC-WROVER**
board (ESP32-WROVER-E-N4R8 module).

The application traces:
- System information (CPU frequency, uptime, firmware build date)
- Active thread list
- Periodic trace counters with iteration number and CPU cycle count
- Extended system snapshots every 10 seconds

## Hardware

| Item            | Description                     |
|-----------------|---------------------------------|
| Board           | ESP32-DevKitC                   |
| Module          | ESP32-WROVER-E-N4R8             |
| USB-UART Bridge | CP2102 (integrated)             |
| UART Pins       | TX=GPIO1, RX=GPIO3              |
| Baud Rate       | 115200 8N1                      |

Connect the board via USB. The trace output appears on `/dev/ttyUSB0` (Linux)
or the corresponding COM port (Windows / macOS).

## Prerequisites

- [Zephyr RTOS](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
  development environment (tested with Zephyr 4.x+)
- `west` tool installed and workspace initialised
- ESP32 toolchain installed (`esp32c3` or `xtensa`)

## Building

From the **Zephyr workspace root** (`/home/irutar/zephyrproject`):

```bash
west build -b esp32_devkitc/esp32/procpu my_app
```

Or from inside `my_app/`:

```bash
cd /home/irutar/zephyrproject/my_app
west build -b esp32_devkitc/esp32/procpu
```

## Flashing

```bash
west flash
```

## Monitoring

Use any serial terminal at **115200 8N1**:

```bash
# Linux
screen /dev/ttyUSB0 115200

# or
minicom -D /dev/ttyUSB0 -b 115200

# or
picocom -b 115200 /dev/ttyUSB0
```

## Example Output

```
========================================
  my_app - Zephyr Trace Application
  Target: ESP32-DevKitC-WROVER
  UART trace via USB (115200 8N1)
========================================

=== System Trace ===
Board   : ESP32-DevKitC-WROVER
SoC     : ESP32-WROVER-E-N4R8
CPU freq: 24000000 Hz
Uptime  : 0 hr 0 min 1 sec
Cycles  : 123456
Firmware: Jul 16 2026 12:00:00
====================

--- Active Threads ---
  [0x3ffb1234] idle                  prio=-1
  [0x3ffb5678] main                  prio=0
----------------------

[trace] iteration=1 uptime=1000 ms
[trace] iteration=2 uptime=2000 ms
...
```

## Files

```
my_app/
├── CMakeLists.txt     # Build definition
├── prj.conf           # Kconfig project configuration
├── README.md          # This file
└── src/
    └── main.c         # Application source code
```
