/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GPIO_EVENT_H
#define GPIO_EVENT_H

#include <stdint.h>

typedef void (*gpio_event_callback_t)(void);

int gpio_event_initialize(gpio_event_callback_t callback);

#endif /* GPIO_EVENT_H */
