#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>

#define DUAL_LOG_TAG "main"
#include <app/wifi_log.h>
#include <app/slot_selector.h>
#include <app/image_update.h>

LOG_MODULE_REGISTER(my_app, LOG_LEVEL_DBG);


static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
static const struct device *voltage_monitor = DEVICE_DT_GET(DT_NODELABEL(vmon));

static void voltage_monitor_print(const struct device *device)
{
	struct sensor_value sensor_value;
	int error;

	error = sensor_sample_fetch(device);
	if (0 != error) {
		log_dual_err("Voltage monitor fetch failed: %d", error);
		return;
	}

	sensor_channel_get(device, SENSOR_CHAN_VOLTAGE, &sensor_value);

	if (0xFFFF == sensor_value.val2) {
		log_dual_inf("[VMON] %d mV", sensor_value.val1);
	} else {
		log_dual_inf("[VMON] %d.%03d V  (%d mV)",
			     sensor_value.val1, sensor_value.val2 / 1000,
			     sensor_value.val1 * 1000 + sensor_value.val2 / 1000);
	}
}

static void print_system_info(void)
{
	log_dual_inf("--- System Info ---");
	log_dual_inf("Board   : %s", CONFIG_BOARD);
	log_dual_inf("CPU freq: %u Hz", sys_clock_hw_cycles_per_sec());
	log_dual_inf("Uptime  : %lld ms", k_uptime_get());
	log_dual_inf("Firmware: %s %s", __DATE__, __TIME__);
	log_dual_inf("--------------------");
}

/*
 * RTC slow memory holds garbage after cold boot.  Detect and default to slot0.
 */
int main(void)
{
	uint32_t tick = 0;
	uint32_t boot_source;
	int error;

	boot_source = slot_selector_boot_source_read();
	if ((0 != boot_source) && (1 != boot_source)) {
		boot_source = 0;
		slot_selector_boot_source_write(0);
	}

	log_dual_inf("========================================");
	log_dual_inf("  my_app — Zephyr Trace Application");
	log_dual_inf("  Target: " CONFIG_BOARD);
	log_dual_inf("  Boot slot: %u", boot_source);
	log_dual_inf("========================================");

	log_dual_dbg("Debug trace enabled");
	print_system_info();

	if (false == device_is_ready(voltage_monitor)) {
		log_dual_err("Voltage monitor not ready");
	} else {
		log_dual_inf("Voltage monitor ready");
	}

	if (false == gpio_is_ready_dt(&led)) {
		log_dual_err("LED not ready");
	} else {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	}

	error = wifi_log_init();
	if (0 > error) {
		log_dual_err("Wi-Fi init: %d", error);
	}

	image_update_perform(boot_source);

	while (1) {
		tick++;

		gpio_pin_toggle_dt(&led);

		if (0 == (tick % (1000 / CONFIG_APP_LOOP_PERIOD_MS))) {
			voltage_monitor_print(voltage_monitor);
		}

		if (0 == (tick % CONFIG_APP_SYSTEM_INFO_INTERVAL)) {
			log_dual_wrn("Snapshot #%u", tick);
			print_system_info();
		}

		k_sleep(K_MSEC(CONFIG_APP_LOOP_PERIOD_MS));
	}

	return 0;
}
