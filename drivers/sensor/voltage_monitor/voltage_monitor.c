#define DT_DRV_COMPAT app_voltage_monitor

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#define DUAL_LOG_TAG "vmon"
#include <app/wifi_log.h>

LOG_MODULE_REGISTER(voltage_monitor, LOG_LEVEL_DBG);

/* ── Compile-time guard: exactly one backend must be selected ───────── */

#if defined(CONFIG_VMON_BACKEND_ADC) && defined(CONFIG_VMON_BACKEND_I2C)
#error "VMON: only one backend allowed — disable either VMON_BACKEND_ADC or VMON_BACKEND_I2C"
#endif

#if !defined(CONFIG_VMON_BACKEND_ADC) && !defined(CONFIG_VMON_BACKEND_I2C)
#error "VMON: no backend selected — enable VMON_BACKEND_ADC or VMON_BACKEND_I2C"
#endif

struct vmon_data {
	int32_t raw_mv;	/* voltage in mV (normalised, backend-independent) */
};

struct vmon_config {
#if defined(CONFIG_VMON_BACKEND_ADC)
	struct adc_dt_spec adc;
#elif defined(CONFIG_VMON_BACKEND_I2C)
	struct i2c_dt_spec i2c;
	uint8_t i2c_read_bytes;
#endif
};


#if defined(CONFIG_VMON_BACKEND_ADC)

static int vmon_fetch_adc(const struct device *dev)
{
	struct vmon_config *cfg = dev->config;
	struct vmon_data *data = dev->data;
	int16_t raw;
	struct adc_sequence seq = {
		.buffer = &raw,
		.buffer_size = sizeof(raw),
	};

	adc_sequence_init_dt(&cfg->adc, &seq);
	int err = adc_read(cfg->adc.dev, &seq);
	if (err < 0) {
		return err;
	}

	int32_t mv = raw;
	adc_raw_to_millivolts_dt(&cfg->adc, &mv);
	data->raw_mv = mv;
	return 0;
}

#endif /* CONFIG_VMON_BACKEND_ADC */


#if defined(CONFIG_VMON_BACKEND_I2C)

static int vmon_fetch_i2c(const struct device *dev)
{
	struct vmon_config *cfg = dev->config;
	struct vmon_data *data = dev->data;
	uint8_t buf[8] = { 0 };
	int n = cfg->i2c_read_bytes;

	if (n > (int)sizeof(buf)) {
		n = sizeof(buf);
	}

	int ret = i2c_read_dt(&cfg->i2c, buf, n);
	if (ret < 0) {
		log_dual_err("I2C read failed: %d", ret);
		return ret;
	}

	/* Assemble little-endian bytes into raw value */
	uint32_t raw = 0;
	for (int i = 0; i < n; i++) {
		raw |= ((uint32_t)buf[i]) << (i * 8);
	}

	/* Apply scaling: mV = raw * NUM / DEN */
	data->raw_mv = (int32_t)((raw * CONFIG_VMON_I2C_SCALE_NUM)
				/ CONFIG_VMON_I2C_SCALE_DEN);
	return 0;
}

#endif /* CONFIG_VMON_BACKEND_I2C */


static int vmon_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

#if defined(CONFIG_VMON_BACKEND_ADC)
	return vmon_fetch_adc(dev);
#elif defined(CONFIG_VMON_BACKEND_I2C)
	return vmon_fetch_i2c(dev);
#endif
}

static int vmon_channel_get(const struct device *dev, enum sensor_channel chan,
			    struct sensor_value *val)
{
	struct vmon_data *data = dev->data;

	if (chan != SENSOR_CHAN_VOLTAGE) {
		return -ENOTSUP;
	}

#if defined(CONFIG_VMON_BACKEND_ADC)
	/* Standard sensor_value: val1 = volts, val2 = microvolts */
	val->val1 = data->raw_mv / 1000;
	val->val2 = (data->raw_mv % 1000) * 1000;
#elif defined(CONFIG_VMON_BACKEND_I2C)
	/* I2C value is already in mV; raw ADC not applicable */
	val->val1 = data->raw_mv;
	val->val2 = 0xFFFF;
#endif

	return 0;
}

static DEVICE_API(sensor, vmon_api) = {
	.sample_fetch = vmon_sample_fetch,
	.channel_get  = vmon_channel_get,
};


static int vmon_init(const struct device *dev)
{
#if defined(CONFIG_VMON_BACKEND_ADC)
	struct vmon_config *cfg = dev->config;

	if (!adc_is_ready_dt(&cfg->adc)) {
		log_dual_err("ADC not ready");
		return -ENODEV;
	}

	int err = adc_channel_setup_dt(&cfg->adc);
	if (err < 0) {
		log_dual_err("ADC channel setup failed: %d", err);
		return err;
	}

	log_dual_inf("ready — ADC backend (ch=%d, res=%d, vref=%d mV)",
		     cfg->adc.channel_id, cfg->adc.resolution,
		     cfg->adc.vref_mv);

#elif defined(CONFIG_VMON_BACKEND_I2C)
	struct vmon_config *cfg = dev->config;

	if (!device_is_ready(cfg->i2c.bus)) {
		log_dual_err("I2C bus not ready");
		return -ENODEV;
	}

	log_dual_inf("ready — I2C backend (addr=0x%02x, %d bytes)",
		     cfg->i2c.addr, cfg->i2c_read_bytes);
#endif

	return 0;
}

#if defined(CONFIG_VMON_BACKEND_ADC)
#  define VMON_CFG_INIT(n) .adc = ADC_DT_SPEC_GET(DT_DRV_INST(n)),
#elif defined(CONFIG_VMON_BACKEND_I2C)
#  define VMON_CFG_INIT(n)						\
	.i2c = {							\
		.bus = DEVICE_DT_GET(					\
			DT_PHANDLE(DT_DRV_INST(n), i2c_bus)),		\
		.addr = DT_PROP(DT_DRV_INST(n), i2c_addr),		\
	},								\
	.i2c_read_bytes = CONFIG_VMON_I2C_READ_BYTES,
#endif

#define VMON_DEFINE(n)							\
	static struct vmon_data vmon_data_##n;				\
	static struct vmon_config vmon_config_##n = {			\
		VMON_CFG_INIT(n)						\
	};								\
	DEVICE_DT_INST_DEFINE(n, vmon_init, NULL,				\
			      &vmon_data_##n, &vmon_config_##n,		\
			      POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,	\
			      &vmon_api);

DT_INST_FOREACH_STATUS_OKAY(VMON_DEFINE)
