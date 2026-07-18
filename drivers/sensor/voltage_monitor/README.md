# Voltage Monitor — module porting guide

How to copy this module from one Zephyr project to another.

---

## 1. Copy files

```
new_project/
├── drivers/sensor/voltage_monitor/   ← copy this entire folder
│   ├── CMakeLists.txt
│   ├── Kconfig
│   ├── vmon.dtsi          ← module's devicetree (edit per board)
│   └── voltage_monitor.c  ← driver (do not edit)
├── dts/bindings/sensor/
│   └── app,voltage-monitor.yaml  ← copy this binding
```

## 2. Register in CMakeLists.txt

Add the module's path **before** `find_package(Zephyr)`:

```cmake
string(REPLACE "/" "_" BOARD_QUAL ${BOARD})
set(DTC_OVERLAY_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/app.overlay"
  "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_QUAL}.overlay"
  # ── module overlays ──────────────────────────────
  "${CMAKE_CURRENT_SOURCE_DIR}/drivers/sensor/voltage_monitor/vmon.dtsi"
)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
```

Also add the driver source:

```cmake
target_sources(app PRIVATE
  drivers/sensor/voltage_monitor/voltage_monitor.c
)
```

And the Kconfig:

```cmake
# In your top-level Kconfig (or drivers/sensor/Kconfig):
rsource "drivers/sensor/voltage_monitor/Kconfig"
```

## 3. Configure `vmon.dtsi`

**Delete** the sections for peripherals your board does **not** have:

```devicetree
/ {
    vmon: voltage-monitor {
        compatible = "app,voltage-monitor";
        status = "okay";

        /* === ADC BACKEND — DELETE if board has no ADC === */
        io-channels = <&adc0 6>;        /* change channel */
        zephyr,gain = "ADC_GAIN_1_4";
        ...

        /* === I2C BACKEND — DELETE if board has no I2C === */
        i2c-bus = <&i2c0>;              /* change bus */
        i2c-addr = <0x20>;              /* change address */
    };
};
```

Board has **both** ADC and I2C? Keep both, pick backend in Kconfig.

## 4. Enable peripherals in board overlay

```devicetree
/* boards/your_board.overlay */

&adc0 { status = "okay"; };   /* only if board has ADC */
&i2c0 { status = "okay"; };   /* only if board has I2C */
```

No channel config here — that lives in `vmon.dtsi`.

## 5. Choose backend in `prj.conf`

```ini
# Enable the subsystem(s) your board has:
CONFIG_ADC=y
CONFIG_I2C=y
CONFIG_SENSOR=y

# Pick EXACTLY ONE backend:
CONFIG_VMON_BACKEND_ADC=y
# CONFIG_VMON_BACKEND_I2C=y

# I2C-specific (only if using I2C backend):
CONFIG_VMON_I2C_READ_BYTES=2
```

If neither backend is set → build fails with `#error "no backend selected"`.

## 6. Use in `main.c`

```c
#include <zephyr/drivers/sensor.h>

static const struct device *vmon = DEVICE_DT_GET(DT_NODELABEL(vmon));

// Always the same, regardless of backend:
sensor_sample_fetch(vmon);
struct sensor_value val;
sensor_channel_get(vmon, SENSOR_CHAN_VOLTAGE, &val);

// ADC backend:  val.val1 = volts,  val.val2 = microvolts
// I2C backend:  val.val1 = mV,     val.val2 = 0xFFFF (raw not applicable)
```

## 7. Build

```bash
west build -b your_board -d build/your_board --pristine=always
```

---

## Backend reference

| Backend | Kconfig | DT properties | Output |
|---------|---------|---------------|--------|
| Internal ADC | `VMON_BACKEND_ADC=y` | `io-channels`, `zephyr,gain`, `zephyr,reference`, `zephyr,resolution` | V + µV |
| External I2C | `VMON_BACKEND_I2C=y` | `i2c-bus`, `i2c-addr` | mV + `0xFFFF` |
