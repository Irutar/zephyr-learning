/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Public API for the Wi-Fi log driver.
 */

#ifndef APP_WIFI_LOG_H
#define APP_WIFI_LOG_H

#include <zephyr/logging/log.h>

/*
 * Each source file should define DUAL_LOG_TAG before including this header,
 * e.g.:  #define DUAL_LOG_TAG "main"
 */
#ifndef DUAL_LOG_TAG
#define DUAL_LOG_TAG "?"
#endif

/**
 * @brief Initialize Wi-Fi + UDP. Messages sent before this are buffered.
 */
int wifi_log_init(void);

/**
 * @brief Send tagged + counted message over Wi-Fi (internal, use macros).
 */
void wifi_log_send_tagged(const char *level, const char *tag,
			  const char *fmt, ...);

/*
 * Dual-output macros: LOG subsystem (UART) + Wi-Fi (UDP).
 * Wi-Fi format: [LEVEL] #COUNTER [TAG] message
 *
 * Thread-safe (mutex). Messages before wifi_log_init() are ring-buffered.
 */

#define log_dual_inf(fmt, ...) \
	do { \
		LOG_INF(fmt, ##__VA_ARGS__); \
		wifi_log_send_tagged("INF", DUAL_LOG_TAG, fmt, ##__VA_ARGS__); \
	} while (0)

#define log_dual_wrn(fmt, ...) \
	do { \
		LOG_WRN(fmt, ##__VA_ARGS__); \
		wifi_log_send_tagged("WRN", DUAL_LOG_TAG, fmt, ##__VA_ARGS__); \
	} while (0)

#define log_dual_err(fmt, ...) \
	do { \
		LOG_ERR(fmt, ##__VA_ARGS__); \
		wifi_log_send_tagged("ERR", DUAL_LOG_TAG, fmt, ##__VA_ARGS__); \
	} while (0)

#define log_dual_dbg(fmt, ...) \
	do { \
		LOG_DBG(fmt, ##__VA_ARGS__); \
		wifi_log_send_tagged("DBG", DUAL_LOG_TAG, fmt, ##__VA_ARGS__); \
	} while (0)

#endif /* APP_WIFI_LOG_H */
