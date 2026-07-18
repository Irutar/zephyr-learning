#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>

#define DUAL_LOG_TAG "main"
#include <app/wifi_log.h>

LOG_MODULE_REGISTER(my_app, LOG_LEVEL_DBG);


static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);
static const struct device *vmon = DEVICE_DT_GET(DT_NODELABEL(vmon));

static void vmon_print(const struct device *dev)
{
	int err = sensor_sample_fetch(dev);
	if (err) {
		log_dual_err("VMON fetch failed: %d", err);
		return;
	}

	struct sensor_value val;
	sensor_channel_get(dev, SENSOR_CHAN_VOLTAGE, &val);

	if (val.val2 == 0xFFFF) {
		/* I2C backend: val1 = mV, val2 = sentinel */
		log_dual_inf("[VMON] %d mV", val.val1);
	} else {
		/* ADC backend: val1 = V, val2 = µV */
		log_dual_inf("[VMON] %d.%03d V  (%d mV)",
			     val.val1, val.val2 / 1000,
			     val.val1 * 1000 + val.val2 / 1000);
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

int main(void)
{
	uint32_t tick = 0;

	log_dual_inf("========================================");
	log_dual_inf("  my_app — Zephyr Trace Application");
	log_dual_inf("  Target: " CONFIG_BOARD);
	log_dual_inf("========================================");

	log_dual_dbg("Debug trace enabled");
	print_system_info();

	if (!device_is_ready(vmon)) {
		log_dual_err("VMon not ready");
	} else {
		log_dual_inf("VMon ready");
	}

	if (!gpio_is_ready_dt(&led)) {
		log_dual_err("LED not ready");
	} else {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	}

	int ret = wifi_log_init();
	if (ret < 0) {
		log_dual_err("Wi-Fi init: %d", ret);
	}

	while (1) {
		tick++;

		gpio_pin_toggle_dt(&led);

		if ((tick % (1000 / CONFIG_APP_LOOP_PERIOD_MS)) == 0) {
			vmon_print(vmon);
		}

		if ((tick % CONFIG_APP_SYSTEM_INFO_INTERVAL) == 0) {
			log_dual_wrn("Snapshot #%u", tick);
			print_system_info();
		}

		k_sleep(K_MSEC(CONFIG_APP_LOOP_PERIOD_MS));
	}
	return 0;
}
