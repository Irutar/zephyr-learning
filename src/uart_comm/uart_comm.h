/*
 * Copyright (c) 2026
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UART_COMM_H
#define UART_COMM_H

#include <stddef.h>
#include <stdint.h>

typedef void (*uart_comm_rx_callback_t)(const uint8_t *data, size_t length, void *context);

int uart_comm_init(const struct device *uart_device);
int uart_comm_send(const uint8_t *data, size_t length);
void uart_comm_set_rx_callback(uart_comm_rx_callback_t callback, void *context);
void uart_comm_poll(void);

#endif /* UART_COMM_H */
