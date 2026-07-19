# Copilot Agent Instructions

## Environment
- **WSL**: We are running inside Windows Subsystem for Linux (WSL2, Ubuntu 22.04).
- **WSL user**: `irutar`. Windows host user: `krystian`.
- **Zephyr base**: `/home/irutar/zephyrproject/zephyr`
- **West workspace**: `/home/irutar/zephyrproject`
- **Python venv**: `/home/irutar/zephyrproject/.venv`
- **SDK**: `/home/irutar/zephyr-sdk-1.0.1`

## Git Rules
- **NEVER** run `git add`, `git commit`, or `git push` unless the user
  explicitly says something like "zrób commit", "commitnij", "wypchnij", etc.
- `git status`, `git diff`, `git log` are fine anytime.

## Agent Memory Rules
- **All** project-specific instructions, rules, and context stay in **this file**
  (`.github/copilot-instructions.md`) or in the repo.
- Do **NOT** store repo-specific info in `/memories/` or any external location.
- When working on a different project, that project will have its own instructions.
- `/memories/` is only for global/personal preferences, not project rules.

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
