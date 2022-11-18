#include "connection_info.h"
#include <modem/lte_lc.h>
#include <net/socket.h>
#include <net/net_mgmt.h>
#include <net/net_ip.h>
#include <net/udp.h>
#include <net/coap.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>

K_SEM_DEFINE(lte_connected, 0, 1);

/*
*	lte_init initializes the modem
*/
void lte_init(void)
{
	printk("LTE init...\n");

	printk("Current configuration\n");
	printk("---------------------\n");
	printk("Connecting to APN: %s\n", CONFIG_PDN_DEFAULT_APN);
	printk("UDP Server is : %s:%d\n", CONFIG_UDP_SERVER_ADDRESS, CONFIG_UDP_SERVER_PORT);
	printk("COAP Server is : %s:%d\n", CONFIG_COAP_SERVER_ADDRESS, CONFIG_COAP_SERVER_PORT);
	printk("COAP Report path is : %s\n", CONFIG_FOTA_COAP_REPORT_PATH);
	printk("COAP Update path is : %s\n", CONFIG_FOTA_COAP_UPDATE_PATH);

	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) 
	{
		/* Do nothing, modem is already configured and LTE connected. */
	} 
	else 
	{
		err = lte_lc_init();
		if (err) {
			printk("Modem initialization failed, error: %d\n", err);
			return;
		}
	}
}


int configure_low_power(void)
{
	int err;

#if defined(CONFIG_UDP_PSM_ENABLE)
	/** Power Saving Mode */
	err = lte_lc_psm_req(true);
	if (err) 
	{
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#else
	err = lte_lc_psm_req(false);
	if (err) 
	{
		printk("lte_lc_psm_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_UDP_EDRX_ENABLE)
	/** enhanced Discontinuous Reception */
	err = lte_lc_edrx_req(true);
	if (err) 
	{
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#else
	err = lte_lc_edrx_req(false);
	if (err) 
	{
		printk("lte_lc_edrx_req, error: %d\n", err);
	}
#endif

#if defined(CONFIG_UDP_RAI_ENABLE)
	/** Release Assistance Indication  */
	err = lte_lc_rai_req(true);
	if (err) 
	{
		printk("lte_lc_rai_req, error: %d\n", err);
	}
#endif

	return err;
}

/*
*	lte_handler is a callback function for modem events. Once a connection has been established, this will again
*   call update_connection_info in order to provide the box with imei, imsi, ip address and modem firmware version
*/
void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) 
	{
		case LTE_LC_EVT_NW_REG_STATUS:
			if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) && (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) 
			{
				break;
			}

			printk("Network registration status: %s\n", evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ? "Connected - home network" : "Connected - roaming\n");
			debug_connection_info();
			k_sem_give(&lte_connected);
			break;
		case LTE_LC_EVT_PSM_UPDATE:
			printk("PSM parameter update: TAU: %d, Active time: %d\n", evt->psm_cfg.tau, evt->psm_cfg.active_time);
			break;
		case LTE_LC_EVT_EDRX_UPDATE: 
		{
			char log_buf[60];
			ssize_t len;

			len = snprintf(log_buf, sizeof(log_buf), "eDRX parameter update: eDRX: %f, PTW: %f\n", evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
			if (len > 0) 
			{
				printk("%s\n", log_buf);
			}
			break;
		}
		case LTE_LC_EVT_RRC_UPDATE:
			if (evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED)
			{
				// printk("RRC mode: Connected\n");
			}
			else if (evt->rrc_mode == LTE_LC_RRC_MODE_IDLE) 
			{
				// printk("RRC mode: Idle\n");
			}

			break;
		case LTE_LC_EVT_CELL_UPDATE:
			printk("LTE cell changed: Cell ID: %d, Tracking area: %d\n", evt->cell.id, evt->cell.tac);
			if (evt->cell.id == -1)
			{
				// Handle offline scenario
			}
			break;
		default:
			break;
	}
}


void lte_connect(void)
{
	int err;

	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) 
	{
		/* Do nothing, modem is already configured and LTE connected. */
	} 
	else 
	{
		err = lte_lc_connect_async(lte_handler);
		if (err) 
		{
			printk("Connecting to LTE network failed, error: %d\n", err);
			return;
		}
	}
}


/*
* 	lte_work_init intializes the modem and a modem connection based on the following config settings:
*	CONFIG_PDN_DEFAULT_APN, CONFIG_PDN_DEFAULT_FAM_IPV4V6, CONFIG_UDP_SERVER_ADDRESS and CONFIG_UDP_SERVER_PORT.
* 	The function returns when the modem is online.
*/
void lte_work_init(void)
{
	printk("Connecting to LTE network...\n");
    int err;

	/* Initialize the modem before calling configure_low_power(). This is
	 * because the enabling of RAI is dependent on the
	 * configured network mode which is set during modem initialization.
	 */
	lte_init();

	err = configure_low_power();
	if (err) 
	{
		printk("Unable to set low power configuration, error: %d\n", err);
	}

	lte_connect();

	k_sem_take(&lte_connected, K_FOREVER);
}