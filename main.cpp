/*
 * Copyright 2021, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 */

#include "command_console.h"
#include "iperf_utility.h"
#include "bt_utility.h"
#include "wifi_utility.h"
#include "mbed.h"
#include "cyabs_rtos.h"

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
#define CONSOLE_COMMAND_MAX_PARAMS       (32)
#define CONSOLE_COMMAND_MAX_LENGTH       (85)
#define CONSOLE_COMMAND_HISTORY_LENGTH   (10)
#define CMD_CONSOLE_MAX_WIFI_RETRY_COUNT (3)

#define STA_INTERFACE
//#define AP_INTERFACE

#ifdef AP_INTERFACE
#include "WhdSoftAPInterface.h"
#define SOFTAP_IP_ADDRESS "192.165.100.2"
#define SOFTAP_NETMASK    "255.255.0.0"
#define SOFTAP_GATEWAY    "192.165.100.1"
#endif

const char* console_delimiter_string = " ";

static char command_buffer[CONSOLE_COMMAND_MAX_LENGTH];
static char command_history_buffer[CONSOLE_COMMAND_MAX_LENGTH * CONSOLE_COMMAND_HISTORY_LENGTH];

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Static Function Declarations
 ******************************************************/

/******************************************************
 *               Variable Definitions
 ******************************************************/
NetworkInterface *networkInterface;
/******************************************************
 *               Function Definitions
 ******************************************************/


UnbufferedSerial pc(USBTX, USBRX, 115200);
#ifdef __cplusplus
extern "C" {
#endif
extern void uart_callback();
#ifdef __cplusplus
}
#endif

int main(void)
{
	cy_command_console_cfg_t console_cfg;
	cy_rslt_t result;
	SocketAddress sockaddr;
#ifdef AP_INTERFACE
	int ret;
#endif
    printf( "Command console application \n");

    console_cfg.serial             = &pc;
    console_cfg.line_len           = sizeof(command_buffer);
    console_cfg.buffer             = command_buffer;
    console_cfg.history_len        = CONSOLE_COMMAND_HISTORY_LENGTH;
    console_cfg.history_buffer_ptr = command_history_buffer;
    console_cfg.delimiter_string   = console_delimiter_string;
    console_cfg.params_num         = CONSOLE_COMMAND_MAX_PARAMS;
    console_cfg.delimiter_string   = " ";
    console_cfg.thread_priority    = CY_RTOS_PRIORITY_NORMAL;

    /* Initialize command console library */
    result = cy_command_console_init(&console_cfg);
    if(result != CY_RSLT_SUCCESS)
    {
        printf("Error initializing command console library. Res:%lu\n", result);
        return -1;
    }
#ifdef STA_INTERFACE
    printf("Connecting to the network using Wifi...\r\n");
    networkInterface  = NetworkInterface::get_default_instance();
    if (!networkInterface)
    {
        printf("ERROR: No NetworkInterface found.\n");
        return -1;
    }
    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    nsapi_error_t net_status = -1;
    for(int tries = 0; tries < CMD_CONSOLE_MAX_WIFI_RETRY_COUNT; tries++)
    {
        net_status = networkInterface->connect();
        if (net_status == NSAPI_ERROR_OK)
        {
            networkInterface->get_ip_address(&sockaddr);
            printf("MAC: %s\n", networkInterface->get_mac_address());
            printf("IP: %s\n", sockaddr.get_ip_address());
            networkInterface->get_netmask(&sockaddr);
            printf("Netmask: %s\n", sockaddr.get_ip_address());
            networkInterface->get_gateway(&sockaddr);
            printf("Gateway: %s\n", sockaddr.get_ip_address());
            break;
        }
        else
        {
            printf("Unable to connect to network. Retrying...\r\n");
        }
    }

    if (net_status != NSAPI_ERROR_OK)
    {
        printf("ERROR: Connecting to the network failed (%d)!\r\n", net_status);
    }

    /* Wi-Fi utility library init */
    result = wifi_utility_init();
    if(result != CY_RSLT_SUCCESS)
    {
        printf("wifi_utility_init failed with error [%lu]\n", result);
        return -1;
    }

    /* Initialize iperf library */
    iperf_utility_init(networkInterface);
#else
    printf("\nWiFi SoftAP Mode\n");
    WhdSoftAPInterface *softap = WhdSoftAPInterface::get_default_instance();
    if (!softap) {
        printf("ERROR: No SoftAPInterface found.\n");
        return -1;
    }

    softap->set_network(SOFTAP_IP_ADDRESS, SOFTAP_NETMASK, SOFTAP_GATEWAY);

    ret = softap->start(MBED_CONF_APP_SOFTAP_WIFI_SSID, MBED_CONF_APP_SOFTAP_WIFI_PASSWORD,
    		            MBED_CONF_APP_SOFTAP_WIFI_SECURITY,	MBED_CONF_APP_SOFTAP_WIFI_CHANNEL);
    if( ret != 0)
    {
        printf("SoftAP start failed :%d\n", ret);
    }

    softap->get_mac_address();
    softap->get_ip_address(&sockaddr);
    printf("MAC: %s\n", softap->get_mac_address());
    printf("IP: %s\n", sockaddr.get_ip_address());
    softap->get_netmask(&sockaddr);
    printf("Netmask: %s\n", sockaddr.get_ip_address());
    softap->get_gateway(&sockaddr);
    printf("Gateway: %s\n", sockaddr.get_ip_address());

    /* Initialize iperf library */
    iperf_utility_init(softap);
#endif

    /* BT utility library init */
    bt_utility_init();

#ifndef ENABLE_UART_POLLING
    /* Register serial receive interrupt callback routine */
    pc.attach(&uart_callback);
#endif
    return result;
}
