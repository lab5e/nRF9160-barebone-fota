#include "..\downloader\downloader.h"
#include "fota.h"
#include "fota_tlv.h"
#include <logging/log.h>
#include <net/coap.h>
#include <net/socket.h>
#include <stdio.h>
#include <zephyr.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/random/rand32.h>

LOG_MODULE_REGISTER(FOTA, CONFIG_FOTA_LOG_LEVEL);

#define BLOCK_SIZE COAP_BLOCK_256
#define BLOCK_BYTES 256
#define MAX_BUFFER_LEN 512

char CURRENT_COAP_BUFFER[128] = {0};

// This is the general buffer used by the FOTA and flash writing functions
static uint8_t request_buffer[MAX_BUFFER_LEN];
static uint8_t response_buffer[MAX_BUFFER_LEN];

static int fota_encode_simple_report(uint8_t *buffer, size_t *len)
{
	size_t sz = encode_tlv_string(buffer, FIRMWARE_VER_ID, FIRMWARE_VER);
	sz += encode_tlv_string(buffer + sz, MANUFACTURER_ID, MANUFACTURER);
	sz += encode_tlv_string(buffer + sz, SERIAL_NUMBER_ID, SERIAL_NUMBER);
	sz += encode_tlv_string(buffer + sz, MODEL_NUMBER_ID, MODEL_NUMBER);
	*len = sz;
	return 0;
}

static int fota_decode_simple_response(simple_fota_response_t *resp, const uint8_t *buf, size_t len)
{
	size_t idx = 0;
	int err = 0;
	while (idx < len)
	{
		uint8_t id = buf[idx++];
		switch (id)
		{
		case HOST_ID:
			err = decode_tlv_string(buf, &idx, resp->host);
			if (err)
			{
				return err;
			}
			break;
		case PORT_ID:
			err = decode_tlv_uint32(buf, &idx, &resp->port);
			if (err)
			{
				return err;
			}
			break;
		case PATH_ID:
			err = decode_tlv_string(buf, &idx, resp->path);
			if (err)
			{
				return err;
			}
			break;
		case AVAILABLE_ID:
			err = decode_tlv_bool(buf, &idx, &resp->scheduled_update);
			if (err)
			{
				return err;
			}
			break;
		default:
			LOG_ERR("Unknown field id in FOTA response: %d", id);
			return -1;
		}
	}
	return 0;
}

int fota_report_version(simple_fota_response_t *resp)
{
	struct coap_packet p;
	int err = 0;
#define TOKEN_SIZE 8
	char token[TOKEN_SIZE];
	sys_csrand_get(token, TOKEN_SIZE);

	if (coap_packet_init(&p, request_buffer, sizeof(request_buffer), 1, COAP_TYPE_CON,
						 TOKEN_SIZE, token, COAP_METHOD_POST,
						 coap_next_id()) < 0)
	{
		LOG_ERR("Unable to initialize CoAP packet");
		return -1;
	}
	if (coap_packet_append_option(&p, COAP_OPTION_URI_PATH,
								  CONFIG_FOTA_COAP_REPORT_PATH,
								  strlen(CONFIG_FOTA_COAP_REPORT_PATH)) < 0)
	{
		LOG_ERR("Could not append path option to packet");
		return -1;
	}
	if (coap_packet_append_payload_marker(&p) < 0)
	{
		LOG_ERR("Unable to append payload marker to packet");
		return -1;
	}

	uint8_t tmpBuf[256];
	size_t len;
	if (fota_encode_simple_report(tmpBuf, &len) < 0)
	{
		LOG_ERR("Unable to encode FOTA report");
		return -1;
	}
	if (coap_packet_append_payload(&p, tmpBuf, len) < 0)
	{
		LOG_ERR("Unable to append payload to CoAP packet");
		return -1;
	}

	LOG_DBG("Sending %d bytes to %s:%d", p.offset, log_strdup(CURRENT_COAP_BUFFER), CONFIG_COAP_SERVER_PORT);

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0)
	{
		LOG_ERR("Error opening socket: %d", sock);
		return sock;
	}

	struct sockaddr_in remote_addr = {
		sin_family : AF_INET,
	};
	remote_addr.sin_port = htons(CONFIG_COAP_SERVER_PORT);
	net_addr_pton(AF_INET, CONFIG_COAP_SERVER_ADDRESS, &remote_addr.sin_addr);

	err = sendto(sock, request_buffer, p.offset, 0, (const struct sockaddr *)&remote_addr, sizeof(remote_addr));
	if (err < 0)
	{
		LOG_ERR("Unable to send CoAP packet: %d", err);
		close(sock);
		return err;
	}
	LOG_DBG("Sent %d bytes, waiting for response", p.offset);

	int received = 0;

	if (!wait_for_response(sock))
	{
		close(sock);
		return -1;
	}
	received = recvfrom(sock, response_buffer, sizeof(response_buffer), 0, NULL, NULL);
	if (received < 0)
	{
		LOG_ERR("Error receiving data: %d", received);
		close(sock);
		return received;
	}
	close(sock);

	struct coap_packet reply;

	if (coap_packet_parse(&reply, response_buffer, received, NULL, 0) < 0)
	{
		LOG_ERR("Could not parse CoAP reply from server");
		return -1;
	}

	uint16_t payload_len = 0;
	const uint8_t *buf = coap_packet_get_payload(&reply, &payload_len);

	LOG_DBG("Decode response (%d bytes)", len);
	if (fota_decode_simple_response(resp, buf, payload_len) < 0)
	{
		LOG_ERR("Could not decode response from server");
		return -1;
	}

	return 0;
}

bool fota_run()
{
	simple_fota_response_t resp;
	int ret = fota_report_version(&resp);
	if (ret)
	{
		LOG_ERR("Error reporting version: %d", ret);
		return false;
	}
	if (resp.scheduled_update)
	{
		LOG_INF("A firmware update is scheduled for this device");
		char endpoint[128];
		sprintf(endpoint, "coap://%s:%d%s", resp.host, resp.port, resp.path);
		LOG_INF("Endpoint: %s", log_strdup(endpoint));

		return download_firmware(endpoint);
	}
	else
	{
		LOG_INF("A firmware update is not scheduled for this device");
	}
	return false;
}

bool wait_for_response(int sock)
{
	// Poll for response.
	struct pollfd poll_set[1];
	poll_set[0].fd = sock;
	poll_set[0].events = POLLIN;

	bool data = false;
	int wait = 0;
	const int poll_wait_ms = 500;
	const int max_wait_time_ms = 60000;
	while (!data)
	{
		poll(poll_set, 1, poll_wait_ms);
		if ((poll_set[0].revents & POLLIN) == POLLIN)
		{
			data = true;
			continue;
		}
		wait++;
		if (wait > (max_wait_time_ms / poll_wait_ms))
		{
			LOG_ERR("Server response timed out");
			return false;
		}
	}
	return true;
}