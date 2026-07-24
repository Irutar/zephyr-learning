/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define DUAL_LOG_TAG "gpio_event"
#include <app/wifi_log.h>

#include "gpio_event.h"

LOG_MODULE_REGISTER(gpio_event, LOG_LEVEL_INF);

static const struct gpio_dt_spec gpio_event_pin = GPIO_DT_SPEC_GET(DT_NODELABEL(gpio_event0), gpios);

static struct gpio_callback gpio_event_pin_callback_data;
static struct k_work_delayable gpio_event_delayable_work;
static gpio_event_callback_t user_callback;

static void gpio_event_work_handler(struct k_work *work)
{
	(void)work;

	if (user_callback != NULL)
	{
		user_callback();
	}
}

static void gpio_event_pin_interrupt_handler(const struct device *device,
			    struct gpio_callback *callback,
			    uint32_t pins)
{
	(void)device;
	(void)callback;
	(void)pins;

	k_work_schedule(&gpio_event_delayable_work, K_MSEC(50));
}

int gpio_event_initialize(gpio_event_callback_t callback)
{
	int error;

	user_callback = callback;

	if (false == gpio_is_ready_dt(&gpio_event_pin))
	{
		log_dual_err("Pin not ready");
		return -ENODEV;
	}

	error = gpio_pin_configure_dt(&gpio_event_pin, GPIO_INPUT);

	if (0 != error)
	{
		log_dual_err("Pin configure failed: %d", error);
		return error;
	}

	error = gpio_pin_interrupt_configure_dt(&gpio_event_pin, GPIO_INT_EDGE_RISING);

	if (0 != error)
	{
		log_dual_err("Interrupt configure failed: %d", error);
		return error;
	}

	gpio_init_callback(&gpio_event_pin_callback_data, gpio_event_pin_interrupt_handler, BIT(gpio_event_pin.pin));

	error = gpio_add_callback_dt(&gpio_event_pin, &gpio_event_pin_callback_data);

	if (0 != error)
	{
		log_dual_err("Callback add failed: %d", error);
		return error;
	}

	k_work_init_delayable(&gpio_event_delayable_work, gpio_event_work_handler);

	log_dual_inf("Pin %u ready, rising edge interrupt", gpio_event_pin.pin);

	return 0;
}
