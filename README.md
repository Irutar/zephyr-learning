# my_app — Zephyr RTOS Learning Project

## About This Project

This is a **learning project** built alongside an AI assistant. The goal is to
learn Zephyr RTOS mechanisms — its build system, devicetree, driver model,
subsystems (networking, logging, sensors), and sysbuild with MCUboot.

This is **not** "vibe-coding". The engineer oversees every design decision,
understands every line of code, and uses AI only as a tool for faster
implementation and project navigation. AI writes boilerplate and suggests
approaches; the engineer evaluates, corrects, and integrates.

The goal is **education**, not a bug-free product. Bugs are expected and are
part of the learning process — they will **not** be fixed just because they
exist. Reported issues will not be addressed unless they align with a specific
learning objective.

---

## Implemented Features

Each feature is described here when it reaches a commit-worthy state.

### Voltage Monitor Sensor Driver (`drivers/sensor/voltage_monitor/`)

A custom Zephyr sensor driver that reads voltage from one of two selectable
backends, chosen at build time via Kconfig:

- **ADC backend** — reads raw ADC samples and converts them to millivolts using
  the Zephyr ADC API and board-specific calibration data.
- **I2C backend** — reads voltage as raw bytes from an external I2C sensor
  (e.g., INA219), assembling a little-endian value and applying a configurable
  scale factor (`NUM / DEN`) to obtain millivolts.

Both backends expose a unified `SENSOR_CHAN_VOLTAGE` channel through Zephyr's
`sensor` subsystem, making the backend transparent to application code.

The driver is defined in devicetree (`vmon.dtsi`) with the
`app,voltage-monitor` compatible string and instantiated via
`DT_INST_FOREACH_STATUS_OKAY`.

### Dual-output Logging — UART + Wi-Fi (`drivers/wifi_log/`)

A custom logging driver that sends log messages simultaneously over two
channels:

- **UART** — immediate output via Zephyr's UART log backend at 115200 baud.
  Available from boot, no network required.
- **Wi-Fi** — buffered log messages sent over a TCP socket to a remote syslog
  collector. Messages are queued in a ring buffer when the Wi-Fi link is down
  and drained automatically once the connection is re-established.

The driver connects to a configured Wi-Fi SSID, obtains an IP via DHCP, and
streams buffered logs to a remote server. This allows remote monitoring even
during intermittent connectivity.

The `log_dual_*()` macros (`log_dual_inf`, `log_dual_err`, etc.) route every
message to both backends transparently.

### MCUboot Bootloader + Dual-slot Flash Layout (`sysbuild/`, `dts/`)

The project builds with **MCUboot** as the first-stage bootloader via
`--sysbuild`. A custom flash partition table (`dts/partitions_4m.dtsi`)
replaces the vendor AMP layout:

| Offset | Size | Label | Purpose |
|--------|------|-------|---------|
| `0x1000` | 60 KB | `mcuboot` | Bootloader |
| `0x10000` | 64 KB | `sys` | PHY calibration (WiFi) |
| `0x20000` | 1856 KB | `image-0` (slot0) | Active application |
| `0x1F0000` | 1856 KB | `image-1` (slot1) | Backup / OTA target |
| `0x3C0000` | 256 KB | `storage` | User data |

### Image Update — Slot Sync + RTC Slot Selector (`src/image_update/`)

The standard MCUboot upgrade/swap path is **unusable on ESP32**: flash erase
and write operations inside MCUboot corrupt the THREADPTR register, causing
`EXCCAUSE 29 (store prohibited)` when the application kernel starts.  Instead,
the project uses a combination of **manual flash copy** and **RTC out-of-band
signalling** to achieve a working dual-slot setup.

#### self_copy — Slot0→Slot1 Synchronisation

On every boot the application compares the image headers of slot0 and slot1.
If they differ (i.e. after `west flash`), `self_copy` erases slot1 and copies
slot0's image **excluding the last 4 KB sector** — the MCUboot trailer (swap
state: magic, `image_ok`, `copy_done`).  The trailer sector is then explicitly
erased so MCUboot sees all-0xFF ("no swap state") on the next boot.  A reboot
follows the copy.

Without this trailer skip, a byte-for-byte copy would carry slot0's
`magic=GOOD, image_ok=SET` into slot1's trailer, which the MCUboot swap table
interprets as "pending permanent upgrade" — triggering the crash.

After the copy, `self_copy` re-reads both slots and computes a CRC32
checksum comparing the source and destination (trailer excluded).  If the
CRCs differ the copy is retried once; on persistent mismatch the device
halts rather than risking a boot from a corrupted slot.  The same CRC
check runs in `image_update_perform()` before switching to slot1.

Reboot is done via the ESP32 RTC_CNTL hardware register (`0x3FF48000`)
because `sys_reboot(SYS_REBOOT_COLD)` can hang after heavy flash I/O —
the flash cache is left in an inconsistent state after the copy/verify
pass, and the hardware reset bypasses all software layers.

#### RTC Slot Selector (`include/app/slot_selector.h`, `src/image_update/slot_selector.c`)

ESP32 RTC slow memory (8 KB at `0x50000000`, survives warm reboot, lost on
power-cycle) is used as an out-of-band channel between the application and
MCUboot:

| RTC Address | Purpose |
|-------------|---------|
| `0x50001FFC` | Magic word (`0x424F4F54` = "BOOT"). Application writes it before reboot; MCUboot (`arch/esp32.c` `do_boot()`) reads it and, when set, boots from slot1 instead of slot0. Cleared after use (single-shot). |
| `0x50001FF8` | Boot source (`0` = slot0, `1` = slot1). Written before reboot, read on next boot to display the active slot. Uninitialised RTC (cold boot) is detected and initialised to `0`. |

#### Boot Sequence

```
Boot 1  MCUboot → slot0  →  headers differ  →  self_copy slot0→slot1  →  reboot
Boot 2  MCUboot → slot0  →  headers match   →  RTC magic + reboot
Boot 3  MCUboot → slot1  →  "Running from slot1 — all good"
```

After a power-cycle the RTC magic is lost, so the device returns to slot0.
This is by design — the RTC selector is a one-shot test mechanism, not a
permanent slot preference.

#### MCUboot Modification

The only change outside the application is ~20 lines in
`bootloader/mcuboot/boot/zephyr/arch/esp32.c`:

```c
slot_sel = *(volatile uint32_t *)0x50001FFC;
if (0x424F4F54 == slot_sel) {
    slot = SECONDARY_SLOT;
    *(volatile uint32_t *)0x50001FFC = 0;
} else {
    slot = PRIMARY_SLOT;
}
```

MCUboot always boots slot0 unless it finds the RTC magic word — it never
considers `br_image_off` (which would carry the standard swap/upgrade
decision).  All slot switching is driven from the application via RTC memory.

---

## Hardware

| Item            | Description                     |
|-----------------|---------------------------------|
| Board           | ESP32-DevKitC V4                |
| Module          | ESP32-WROVER-IE (8 MB PSRAM)    |
| USB-UART Bridge | CP2102 (integrated)             |
| Console UART    | UART1: TX=GPIO10, RX=GPIO9      |
| Trace UART      | UART0: TX=GPIO1, RX=GPIO3       |
| Baud Rate       | 115200 8N1                      |

## Prerequisites

- [Zephyr RTOS](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
  development environment (Zephyr 4.x+)
- `west` tool installed and workspace initialised
- Zephyr SDK 1.0.1 (ESP32 Xtensa toolchain)

## Building

From the **Zephyr workspace root** (`/home/irutar/zephyrproject`):

```bash
west build -b esp32_devkitc/esp32/procpu my_app -p always --sysbuild
```

This builds both **MCUboot** (bootloader) and the application image.

## Flashing

```bash
west flash
```

## Monitoring

Connect to the console UART at **115200 8N1**:

```bash
screen /dev/ttyUSB0 115200
# or: minicom -D /dev/ttyUSB0 -b 115200
# or: picocom -b 115200 /dev/ttyUSB0
```

Logs also appear on the remote syslog collector when Wi-Fi is connected.

## Project Structure

```
my_app/
├── CMakeLists.txt                # Build definition, DTS overlays, sources
├── prj.conf                      # Kconfig application defaults
├── prj_local.conf                # Local secrets (Wi-Fi credentials) — gitignored
├── app.overlay                   # Application-wide devicetree overlay
├── sysbuild.conf                 # Sysbuild config (enables MCUboot)
├── Kconfig                       # Application-level Kconfig options
├── README.md                     # This file
├── .github/
│   └── copilot-instructions.md   # AI agent coding standards
├── boards/
│   └── esp32_devkitc_esp32_procpu.overlay
├── bootloader/                   # Local modifications to upstream MCUboot
│   └── mcuboot/
│       └── boot/zephyr/
│           └── arch/esp32.c      # RTC slot selector + THREADPTR clear
├── drivers/
│   ├── wifi_log/
│   │   └── wifi_log.c            # Dual-output logging driver (UART + Wi-Fi)
│   └── sensor/
│       └── voltage_monitor/
│           ├── voltage_monitor.c  # Voltage monitor sensor driver
│           ├── vmon.dtsi          # Module devicetree template
│           ├── Kconfig            # Backend selection (ADC / I2C)
│           └── README.md          # Driver documentation
├── dts/
│   ├── partitions_4m.dtsi        # Custom flash partition layout (shared)
│   └── bindings/
├── include/
│   └── app/
│       ├── wifi_log.h             # log_dual_*() macro definitions
│       ├── slot_selector.h        # RTC slot selector API
│       ├── self_copy.h            # Flash copy API
│       └── image_update.h         # Slot-management entry point
├── src/
│   ├── main.c                    # Thin entry — init + main loop
│   └── image_update/
│       ├── image_update.c         # Header compare → CRC verify → self_copy / switch
│       ├── self_copy.c           # Slot0→slot1 flash copy + CRC verify + retry
│       └── slot_selector.c       # RTC magic write + hardware reboot
├── support/
│   └── syslog_collector/         # Remote syslog receiver scripts
└── sysbuild/
    ├── CMakeLists.txt             # Sysbuild glue
    ├── mcuboot.conf               # MCUboot Kconfig overrides
    └── mcuboot.overlay            # MCUboot DTS overlay (partitions, UART)
```

