/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */


#include <dfu/flash_img.h>
#include <dfu/mcuboot.h>
#include <logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <net/download_client.h>
#include <pm_config.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

K_SEM_DEFINE(fota_finished, 0, 1);

LOG_MODULE_REGISTER(fota_download, CONFIG_FOTA_DOWNLOAD_LOG_LEVEL);

#define PROGRESS_WIDTH 50
#define STARTING_OFFSET 0

#define TEST PM_MCUBOOT_SECONDARY_SIZE

static struct	flash_img_context flash_img;
static struct	download_client dfu;

static void do_reboot()
{
	LOG_INF("Rebooting after update");
	int ret = boot_request_upgrade(BOOT_UPGRADE_TEST);
	if (ret)
	{
		LOG_ERR("Error attempting reboot: %d", ret);
	}

	sys_reboot(0);
}


static struct download_client_cfg config = {
	.sec_tag = -1,
};

static int64_t ref_time;

static void progress_print(size_t downloaded, size_t file_size)
{
	const int percent = (downloaded * 100) / file_size;
	size_t lpad = (percent * PROGRESS_WIDTH) / 100;
	size_t rpad = PROGRESS_WIDTH - lpad;

	printk("\r[ %3d%% ] |", percent);
	for (size_t i = 0; i < lpad; i++) {
		printk("=");
	}
	for (size_t i = 0; i < rpad; i++) {
		printk(" ");
	}
	printk("| (%d/%d bytes)\n", downloaded, file_size);
}

static void handle_download_error(const struct download_client_evt *event)
{
	printk("Error %d during download\n", event->error);
	// lte_lc_power_off();
}

static int callback(const struct download_client_evt *event)
{
	static size_t downloaded;
	static size_t file_size;
	uint32_t speed;
	int64_t ms_elapsed;
	int err;

	if (downloaded == 0) {
		err = download_client_file_size_get(&dfu, &file_size);

		if (err != 0) {
			LOG_ERR("download_client_file_size_get error %d", err);
			handle_download_error(event);
			return err;
		}
		if (file_size > PM_MCUBOOT_SECONDARY_SIZE) {
			LOG_ERR("Requested file too big to fit in flash\n");
			handle_download_error(event);
			return -EFBIG;
		}

		downloaded += STARTING_OFFSET;
	}

	switch (event->id) {
	case DOWNLOAD_CLIENT_EVT_FRAGMENT:
		downloaded += event->fragment.len;
		if (file_size) {
			progress_print(downloaded, file_size);
		} else {
			printk("\r[ %d bytes ] ", downloaded);
		}

		err = flash_img_buffered_write(&flash_img, (uint8_t *)event->fragment.buf, event->fragment.len, false);
		if (err != 0) 
		{
			LOG_ERR("flash_img_buffered_write error %d", err);
			handle_download_error(event);
			return err;
		}

		return 0;

	case DOWNLOAD_CLIENT_EVT_DONE:
		ms_elapsed = k_uptime_delta(&ref_time);
		speed = ((float)file_size / ms_elapsed) * MSEC_PER_SEC;
		printk("\nDownload completed in %lld ms @ %d bytes per sec, total %d bytes\n",
		       ms_elapsed, speed, downloaded);


		// Write with 0 length to flush the write operation to flash. 
		err = flash_img_buffered_write(&flash_img, (uint8_t *)event->fragment.buf, 0, true);
		if (err != 0) {
			LOG_ERR("flash_img_buffered_write error %d", err);
			handle_download_error(event);
			return err;
		}

		err = boot_request_upgrade(BOOT_UPGRADE_TEST);
		if (err != 0) {
			LOG_ERR("boot_request_upgrade error %d", err);
			handle_download_error(event);
			return err;
		}
		err = download_client_disconnect(&dfu);
		if (err != 0) {
			LOG_ERR("download_client_disconncet error %d", err);
			handle_download_error(event);
			return err;
		}

		// lte_lc_power_off();

		printk("Bye\n");
		k_sem_give(&fota_finished);

		do_reboot();
		return 0;

	case DOWNLOAD_CLIENT_EVT_ERROR:
		handle_download_error(event);
		k_sem_give(&fota_finished);

		/* Stop download */
		return -1;
	}

	return 0;
}

bool download_firmware(char * endpoint)
{
	int err;

	printk("Download started\n");

	err = flash_img_init(&flash_img);

	if (err != 0) {
		LOG_ERR("flash_img_init error %d", err);
		return err;
	}

	err = boot_erase_img_bank(PM_MCUBOOT_SECONDARY_ID);
	if (err != 0) {
		LOG_ERR("Error erasing flash bank 1. Error: %d", err);
		return err;
	}

	err = download_client_init(&dfu, callback);
	if (err) {
		printk("Failed to initialize the client, err %d", err);
		return false;
	}

	err = download_client_connect(&dfu, endpoint, &config);
	if (err) {
		printk("Failed to connect, err %d", err);
		return false;
	}

	ref_time = k_uptime_get();

	err = download_client_start(&dfu, endpoint, STARTING_OFFSET);
	if (err) {
		printk("Failed to start the dfu downloader, err %d", err);
		return false;
	}


	printk("Downloading %s\n", endpoint);
	k_sem_take(&fota_finished, K_FOREVER);

    return true;
}
