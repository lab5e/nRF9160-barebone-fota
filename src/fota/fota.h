#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MANUFACTURER "Lab5e"
#define MODEL_NUMBER "nrf9160 Fota Demo"
#define SERIAL_NUMBER "1"
#define FIRMWARE_VER "1.0.0"

typedef struct
{
	char host[25];
	uint32_t port;
	char path[25];
	bool scheduled_update;
} simple_fota_response_t;

/**
 * fota_run starts the FOTA process and blocks until it is complete
 */
bool fota_run();

/**
 * Reports currently running version information to Span
 */
int fota_report_version(simple_fota_response_t *resp);

/**
 * wait_for_response wait for response on a socket fd
 */
bool wait_for_response(int sock);
