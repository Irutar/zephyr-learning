#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <string.h>

#include "uart_comm.h"

#define DUAL_LOG_TAG "uart_comm"
#include <app/wifi_log.h>

LOG_MODULE_REGISTER(uart_comm, LOG_LEVEL_INF);

#define RECEIVE_CIRCULAR_SIZE   2048
#define TRANSMIT_CIRCULAR_SIZE  2048
#define LINE_SIZE                256

static uint8_t receive_circular[RECEIVE_CIRCULAR_SIZE];
static volatile uint32_t receive_write_index;
static uint32_t receive_read_index;

static uint8_t transmit_circular[TRANSMIT_CIRCULAR_SIZE];
static uint32_t transmit_write_index;
static volatile uint32_t transmit_read_index;

static const struct device *uart_device;

static uint8_t line_buffer[LINE_SIZE];
static size_t line_position;

static uart_comm_rx_callback_t receive_callback;
static void *receive_context;

static uint32_t circular_index_add(uint32_t index, uint32_t amount);
static void isr_receive(const struct device *device);
static void isr_transmit(const struct device *device);

static volatile uint32_t isr_byte_count;

static uint32_t circular_index_add(uint32_t index, uint32_t amount)
{
	return (index + amount) & (RECEIVE_CIRCULAR_SIZE - 1); /* The same as modulo */
}

static void isr_receive(const struct device *device)
{
	uint8_t chunk[16];
	uint32_t write_index;
	uint32_t space;
	int bytes_read;
	int byte_index;

	bytes_read = uart_fifo_read(device, chunk, sizeof(chunk));

	while (bytes_read > 0)
	{
		isr_byte_count += bytes_read;

		write_index = receive_write_index;
		space = RECEIVE_CIRCULAR_SIZE - 1 - circular_index_add(write_index - receive_read_index, 0);

		if (bytes_read > (int)space)
		{
			receive_read_index = circular_index_add(write_index,
							       bytes_read + 1);
		}

		for (byte_index = 0; byte_index < bytes_read; byte_index++)
		{
			if ('\r' != chunk[byte_index]) 
            {
				receive_circular[write_index] = chunk[byte_index];
				write_index = circular_index_add(write_index, 1);
			}
		}
		receive_write_index = write_index;

		bytes_read = uart_fifo_read(device, chunk, sizeof(chunk));
	}
}

static void isr_transmit(const struct device *device)
{
	uint32_t read_index;
	uint32_t available;
	uint32_t contiguous;
	uint32_t chunk;
	int written;

	while (uart_irq_tx_ready(device))
	{
		read_index = transmit_read_index;
		available = (transmit_write_index - read_index)	& (TRANSMIT_CIRCULAR_SIZE - 1);

		if (0 == available)
		{
			uart_irq_tx_disable(device);
			return;
		}

		contiguous = TRANSMIT_CIRCULAR_SIZE - read_index;
		chunk = available;
		if (chunk > contiguous)
		{
			chunk = contiguous;
		}

		if (chunk > 16)
		{
			chunk = 16;
		}

		written = uart_fifo_fill(device, &transmit_circular[read_index], chunk);
		read_index = (read_index + written)	& (TRANSMIT_CIRCULAR_SIZE - 1);
		transmit_read_index = read_index;
	}
}

static void uart_isr(const struct device *device, void *user_data)
{
	isr_receive(device);
	isr_transmit(device);
}

/* Communcation protocl requires '\n' terminated lines . This is simple protocol :) */
static int line_extract(uint8_t **output, size_t *output_length)
{
	uint32_t read_index;
	uint8_t character;
	int found;

	read_index = receive_read_index;
	found = 0;

	while (read_index != receive_write_index)
	{
		character = receive_circular[read_index];
		read_index = circular_index_add(read_index, 1);

		if ('\n' == character)
		{
			*output = line_buffer;
			*output_length = line_position;
			line_position = 0;
			found = 1;
			break;
		}

		if (line_position < sizeof(line_buffer) - 1)
		{
			line_buffer[line_position] = character;
			line_position = line_position + 1;
		}
	}

	receive_read_index = read_index;
	return found;
}

int uart_comm_init(const struct device *device)
{
	if (NULL == device)
	{
		return -EINVAL;
	}

	uart_device = device;

	log_dual_inf("Init done, RX enabled on %p", (void *)device);

	uart_irq_callback_set(device, uart_isr);
	uart_irq_rx_enable(device);

	return 0;
}

int uart_comm_send(const uint8_t *data, size_t length)
{
	uint32_t write_index;
	uint32_t space;
	size_t byte_index;

	if ((NULL == data) || (0 == length))
	{
		return -EINVAL;
	}

	write_index = transmit_write_index;
	space = (transmit_read_index - write_index - 1)
		& (TRANSMIT_CIRCULAR_SIZE - 1);

	if (length > space)
	{
		log_dual_err("TX overflow: need %u, have %u free",
			     (unsigned)length, (unsigned)space);
		return -ENOMEM;
	}

	log_dual_dbg("TX: %u bytes, free=%u",
		     (unsigned)length, (unsigned)space);

	for (byte_index = 0; byte_index < length; byte_index++)
	{
		transmit_circular[write_index] = data[byte_index];
		write_index = (write_index + 1)
			& (TRANSMIT_CIRCULAR_SIZE - 1);
	}

	transmit_write_index = write_index;

	uart_irq_tx_enable(uart_device);

	return 0;
}

void uart_comm_set_rx_callback(uart_comm_rx_callback_t callback, void *context)
{
	log_dual_dbg("Callback set: %p", (void *)callback);
	receive_callback = callback;
	receive_context = context;
}

void uart_comm_poll(void)
{
	static uint32_t last_byte_count;
	static uint32_t last_line_count;
	static uint32_t line_counter;
	uint8_t *line;
	size_t length;

	if (isr_byte_count != last_byte_count)
	{
		log_dual_dbg("RX bytes: %u (w=%u r=%u)",
			     isr_byte_count,
			     receive_write_index, receive_read_index);
		last_byte_count = isr_byte_count;
	}

	while (0 != line_extract(&line, &length))
	{
		line_counter++;
		log_dual_dbg("Line #%u extracted, len=%u: %.*s",
			     line_counter, (unsigned)length,
			     (int)length, line);
		last_line_count = line_counter;

		if ((length > 0) && (NULL != receive_callback))
		{
			receive_callback(line, length, receive_context);
		}
	}
}
