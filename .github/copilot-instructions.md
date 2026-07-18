# Copilot Agent Instructions

## Target Hardware
- **Board**: ESP32-DevKitC V4 (ESP32-WROVER-IE)
- **SoC**: ESP32 (Xtensa LX6, single core — PROCPU variant)
- **Flash**: 4 MB / **PSRAM**: 8 MB
- **Zephyr build target**: `esp32_devkitc/esp32/procpu`
- **Console UART**: UART0 (GPIO1 TX, GPIO3 RX, via onboard CP2102 → microUSB)
- **UART1**: GPIO10 TX, GPIO9 RX (available on goldpins)
- **UART2**: GPIO17 TX, GPIO16 RX (available on goldpins)
- **Toolchain**: Zephyr SDK 1.0.1

## Known Hardware Issues — DO NOT DEBUG

### Wi-Fi "not found" / zsock_sendto failures
- **Symptom**: `SSID 'domek_galazka' not found in scan results`, `zsock_sendto failed: -1`
- **Root cause**: ESP32 weak antenna. Router is physically distant. Signal unreliable.
- **Agent action**: **IGNORE**. Do NOT attempt to debug Wi-Fi connectivity, reconnect logic, DHCP, sockets, or suggest changes to `wifi_log.c` related to RF range. This is a known hardware limitation, not a software bug.
