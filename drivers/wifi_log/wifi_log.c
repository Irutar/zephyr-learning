#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/ring_buffer.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

LOG_MODULE_REGISTER(wifi_log, LOG_LEVEL_DBG);

#define DUAL_LOG_TAG "wifi"
#include <app/wifi_log.h>

static int sock = -1;
static struct sockaddr_in server_addr;
static bool wifi_ready;
static uint8_t msg_counter;
static int64_t last_reconnect_attempt;

K_MUTEX_DEFINE(wifi_log_mutex);
K_SEM_DEFINE(ip_ready, 0, 1);
K_SEM_DEFINE(scan_done, 0, 1);

static struct net_mgmt_event_callback ip_cb;
static struct net_mgmt_event_callback scan_cb;
static bool ssid_found;

static void ip_event_handler(struct net_mgmt_event_callback *cb,
			     uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD)
	{
		k_sem_give(&ip_ready);
	}
}

static void scan_event_handler(struct net_mgmt_event_callback *cb,
			       uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_SCAN_RESULT)
	{
		struct wifi_scan_result *r = (struct wifi_scan_result *)cb->info;
		if (r->ssid_length == strlen(CONFIG_APP_WIFI_LOG_SSID) &&
		    memcmp(r->ssid, CONFIG_APP_WIFI_LOG_SSID, r->ssid_length) == 0)
		{
			ssid_found = true;
		}
	}
	else if (mgmt_event == NET_EVENT_WIFI_SCAN_DONE)
	{
		k_sem_give(&scan_done);
	}
}

/* Ring buffer for messages before Wi-Fi is up */
#define RING_BUF_SIZE 4096
RING_BUF_DECLARE(log_ring, RING_BUF_SIZE);

static void flush_ring(void)
{
	uint8_t buffer[256];
	uint32_t length;
	int total;
	int sent;

	total = 0;

	length = ring_buf_get(&log_ring, buffer, sizeof(buffer));
	while (length > 0)
	{
		sent = zsock_sendto(sock, buffer, length, 0,
				    (struct sockaddr *)&server_addr,
				    sizeof(server_addr));
		if (0 > sent)
		{
			LOG_ERR("flush zsock_sendto failed: %d", sent);
			return;
		}
		total += sent;
		length = ring_buf_get(&log_ring, buffer, sizeof(buffer));
	}
	LOG_INF("Flushed %d bytes from ring buffer", total);
}

int wifi_log_init(void)
{
	struct net_if *iface;

	iface = net_if_get_default();
	if (NULL == iface)
	{
		log_dual_err("No network interface");
		return -ENODEV;
	}

	struct wifi_connect_req_params params = {
		.ssid        = CONFIG_APP_WIFI_LOG_SSID,
		.ssid_length = strlen(CONFIG_APP_WIFI_LOG_SSID),
		.psk         = CONFIG_APP_WIFI_LOG_PSK,
		.psk_length  = strlen(CONFIG_APP_WIFI_LOG_PSK),
		.channel     = WIFI_CHANNEL_ANY,
		.security    = WIFI_SECURITY_TYPE_PSK,
		.timeout     = 20,
	};

	log_dual_inf("Connecting to Wi-Fi: %s", CONFIG_APP_WIFI_LOG_SSID);

	log_dual_inf("Scanning for Wi-Fi networks...");
	net_mgmt_init_event_callback(&scan_cb, scan_event_handler,
				     NET_EVENT_WIFI_SCAN_RESULT |
				     NET_EVENT_WIFI_SCAN_DONE);
	net_mgmt_add_event_callback(&scan_cb);
	ssid_found = false;

	if (0 != net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0))
	{
		log_dual_err("Wi-Fi scan request failed");
		net_mgmt_del_event_callback(&scan_cb);
		return -EIO;
	}

	if (0 != k_sem_take(&scan_done, K_SECONDS(15)))
	{
		log_dual_err("Wi-Fi scan timed out");
		net_mgmt_del_event_callback(&scan_cb);
		return -ETIMEDOUT;
	}
	net_mgmt_del_event_callback(&scan_cb);

	if (false == ssid_found)
	{
		log_dual_err("SSID '%s' not found in scan results!", CONFIG_APP_WIFI_LOG_SSID);
		return -ENOENT;
	}
	log_dual_inf("SSID '%s' found, connecting...", CONFIG_APP_WIFI_LOG_SSID);


	net_mgmt_init_event_callback(&ip_cb, ip_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&ip_cb);

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
		     &params, sizeof(params)))
	{
		log_dual_err("Wi-Fi connect request failed");
		return -EIO;
	}

	log_dual_inf("Waiting for IP via DHCP...");

	if (0 != k_sem_take(&ip_ready, K_SECONDS(30)))
	{
		log_dual_err("DHCP timeout – no IP address");
		net_mgmt_del_event_callback(&ip_cb);
		return -ETIMEDOUT;
	}
	net_mgmt_del_event_callback(&ip_cb);
	log_dual_inf("Got IP address");

	sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (0 > sock)
	{
		log_dual_err("zsock_socket() failed: %d", sock);
		return sock;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(CONFIG_APP_WIFI_LOG_SERVER_PORT);
	zsock_inet_pton(AF_INET, CONFIG_APP_WIFI_LOG_SERVER_IP,
			&server_addr.sin_addr);

	log_dual_inf("UDP ready, sending to %s:%d",
		CONFIG_APP_WIFI_LOG_SERVER_IP,
		CONFIG_APP_WIFI_LOG_SERVER_PORT);

	wifi_ready = true;
	flush_ring();

	log_dual_inf("Wi-Fi log driver ready");
	return 0;
}

static void wifi_log_send_inner(const char *level, const char *tag,
				const char *fmt, va_list args)
{
	k_mutex_lock(&wifi_log_mutex, K_FOREVER);

	uint8_t sequence;
	int64_t uptime;
	char buffer[256];
	int offset;
	size_t length;
	int64_t now;

	sequence = msg_counter++;
	uptime = k_uptime_get();

	offset = snprintf(buffer, sizeof(buffer),
			  "[%02u:%02u:%02u.%03u] [%s] #%03u [%s] ",
			  (uint32_t)(uptime / 3600000),
			  (uint32_t)((uptime / 60000) % 60),
			  (uint32_t)((uptime / 1000) % 60),
			  (uint32_t)(uptime % 1000),
			  level, sequence, tag);
	vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);

	length = strlen(buffer);

	if (length > sizeof(buffer) - 3)
	{
		length = sizeof(buffer) - 3;
	}
	buffer[length] = '\r';
	buffer[length + 1] = '\n';
	buffer[length + 2] = '\0';
	length += 2;

	if (false == wifi_ready)
	{
		now = k_uptime_get();
		if (now - last_reconnect_attempt > 5000)
		{
			last_reconnect_attempt = now;
			if (sock >= 0)
			{
				zsock_close(sock);
				sock = -1;
			}
			sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (sock >= 0)
			{
				LOG_INF("Wi-Fi reconnected, flushing ring buffer");
				wifi_ready = true;
				flush_ring();
			}
		}
		if (false == wifi_ready)
		{
			ring_buf_put(&log_ring, (uint8_t *)buffer, length);
		}
		else
		{
			int send_result;
			send_result = zsock_sendto(sock, buffer, length, 0,
				     (struct sockaddr *)&server_addr,
				     sizeof(server_addr));
			if (0 > send_result)
			{
				LOG_ERR("zsock_sendto failed after reconnect: %d", send_result);
				wifi_ready = false;
				ring_buf_put(&log_ring, (uint8_t *)buffer, length);
			}
		}
	}
	else
	{
		int send_result;
		send_result = zsock_sendto(sock, buffer, length, 0,
			     (struct sockaddr *)&server_addr,
			     sizeof(server_addr));
		if (0 > send_result)
		{
			LOG_ERR("zsock_sendto failed: %d, falling back to ring buffer", send_result);
			wifi_ready = false;
			ring_buf_put(&log_ring, (uint8_t *)buffer, length);
		}
	}

	k_mutex_unlock(&wifi_log_mutex);
}

void wifi_log_send_tagged(const char *level, const char *tag,
			  const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wifi_log_send_inner(level, tag, fmt, args);
	va_end(args);
}