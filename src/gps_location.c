/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <nrf_modem_gnss.h>
#include <string.h>
#include <modem/at_cmd.h>

#include "gps_location.h"
#include "mqtt_service.h"

#define AT_XSYSTEMMODE      "AT\%XSYSTEMMODE=0,0,1,0"
#define AT_ACTIVATE_GPS     "AT+CFUN=31"

#define AT_CMD_SIZE(x) (sizeof(x) - 1)


static const char *const at_commands[] = {
    AT_XSYSTEMMODE,
    AT_ACTIVATE_GPS    
};

static struct nrf_modem_gnss_pvt_data_frame last_pvt;
static volatile bool gnss_blocked;

K_SEM_DEFINE(pvt_data_sem, 0, 1);
K_SEM_DEFINE(lte_ready, 0, 1);

struct k_poll_event events[1] = {
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
                    K_POLL_MODE_NOTIFY_ONLY,
                    &pvt_data_sem, 0)
};


void gps_work_handler(struct k_work *work) {

    printk("Getting GNSS data...\n");
    double latitude = last_pvt.latitude;
    double longitude = last_pvt.longitude;

    publish_location(latitude, longitude);

}

K_WORK_DEFINE(gps_work, gps_work_handler);

void gps_request_coordinates() {
    // if (nrf_modem_gnss_start() != 0) {
    //     printk("Failed to start GNSS\n");
    //     return;
    // }
    printk("Request coordinates\n");
    last_pvt.latitude = 63.00123034;
    last_pvt.longitude = 10.14964319;
    k_work_submit(&gps_work);
}


static int setup_modem(void) {
    for (int i = 0; i < ARRAY_SIZE(at_commands); i++) {
        if (at_cmd_write(at_commands[i], NULL, 0, NULL) != 0) {
            return -1;
        }
    }
    return 0;
}

static void gnss_event_handler(int event) {
    int retval;

    switch (event)
    {
    case NRF_MODEM_GNSS_EVT_PVT:
        retval = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
        if (retval == 0 && (last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID)) {
            k_work_submit(&gps_work);
        }
        break;
    
    case NRF_MODEM_GNSS_EVT_BLOCKED:
        gnss_blocked = true;
        break;
    
    case NRF_MODEM_GNSS_EVT_UNBLOCKED:
        gnss_blocked = false;
        break;

    default:
        break;
    }
}

void gps_init() {
    if (setup_modem() != 0) {
        printk("Failed to initialize modem\n");
        return;
    }

    /* Initialize and configure GNSS */
    if (nrf_modem_gnss_init() != 0) {
        printk("Failed to initialize GNSS interface\n");
        return;
    }

    if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
        printk("Failed to set GNSS event handler\n");
        return;
    }

    if (nrf_modem_gnss_fix_retry_set(0) != 0) {
        printk("Failed to set GNSS fix retry\n");
        return;
    }

    if (nrf_modem_gnss_fix_interval_set(0) != 0) {
        printk("Failed to set GNSS fix interval\n");
        return;
    }

    return;
}


