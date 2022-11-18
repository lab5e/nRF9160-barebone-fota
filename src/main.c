#include <zephyr/kernel.h>
#include <stdio.h>
// #include <modem/lte_lc.h>
// #include <zephyr/net/socket.h>
#include <zephyr/dfu/mcuboot.h>

#include "lte/lte.h"
#include "fota/fota.h"

void main(void)
{
	printk("================================================\n");
	printk("nRF9160 FOTA sample has started (Version 1.0.XX)\n");
	printk("================================================\n");


	// Note:
	// If boot_write_img_confirmed() is not called, MCUBoot will revert to the previous
	// image on the next boot.
	boot_write_img_confirmed();

	lte_work_init();

	// Note: 
	// fota_run() will block until firmware download is complete

	fota_run();

	// Start any worker threads here.

	while (true)
	{
		// ..or process stuff in the main thread.
		k_sleep(K_MSEC(3000));
		printk("Ping...\n");
	}
}
