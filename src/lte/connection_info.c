#include "connection_info.h"
#include <nrf_modem_at.h>
#include <string.h>
#include <zephyr/kernel.h>

#define AT_RESPONSE_BUFFER_SIZE 128
char at_response_buffer[AT_RESPONSE_BUFFER_SIZE];

/*
*   run_at_command runs a - wait for it - AT command and stores the response in the static at_response_buffer
*/
bool run_at_command(char * cmd, int retries)
{
	int err;

    for (int i=0; i<retries; i++)
    {
        memset(at_response_buffer, AT_RESPONSE_BUFFER_SIZE, 0);
        err = nrf_modem_at_cmd(at_response_buffer, AT_RESPONSE_BUFFER_SIZE, cmd);
        if (err) 
        {
            printk("ERROR: %s failed with error: %d\n", cmd, err);
            k_sleep(K_SECONDS(10));
            continue;
        }
        else{
            return true;
        }
    }
    return false;
}


/*
*   debug_connection_info runs a series of AT commands on the modem and logs IMEI, IMSI, IP address and modem information
*/
void debug_connection_info()
{
	printk("---------- CONNECTION INFO ----------\n");

    if (run_at_command("AT+CFUN=1", 1))
    {
        char sep[] = "\r\n";
        char * token = strtok(at_response_buffer, sep);
        if (token != NULL)
        {
            printk("IMEI      : %s\n", token);
        }
    }

    k_sleep(K_SECONDS(2));

    if (run_at_command("AT+CGSN", 1))
    {
        char sep[] = "\r\n";
        char * token = strtok(at_response_buffer, sep);
        if (token != NULL)
        {
            printk("IMEI      : %s\n", token);
        }
    }

    if (run_at_command("AT+CIMI", 1))
    {
        char sep[] = "\r\n";
        char * token = strtok(at_response_buffer, sep);
        if (token != NULL)
        {
            printk("IMSI      : %s\n", token);
        }
    }

    if (run_at_command("AT+CGPADDR", 1))
    {
        char sep[] = "\"";
        char * token = strtok(at_response_buffer, sep);
        if (token != NULL)
        {
            token = strtok(NULL, sep);
            if (token != NULL)
            {
        	    printk("IP        : %s\n", token);
            }
        }
    }

    if (run_at_command("AT+CGMR", 1))
    {
        char sep[] = "\r\n";
        char * token = strtok(at_response_buffer, sep);
        if (token != NULL)
        {
            printk("FW version: %s\n", token);
        }
    }

    if (run_at_command("AT+CCLK?", 1))
    {
        char sep[] = "\r\n";
        char * token = strtok(at_response_buffer, sep);
        if (token != NULL)
        {
            token += 8;
            printk("Time string: %s\n", token);
        }
    }

	printk("-------------------------------------\n");
}


