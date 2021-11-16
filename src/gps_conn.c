/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <nrf_modem_gnss.h>
#include <string.h>
#include <modem/at_cmd.h>
#include <modem/lte_lc.h>

#include "gps_conn.h"

#define AT_XSYSTEMMODE      "AT\%XSYSTEMMODE=0,0,1,0"
#define AT_ACTIVATE_GPS     "AT+CFUN=31"

#define AT_CMD_SIZE(x) (sizeof(x) - 1)


static const char *const at_commands[] = {
    AT_XSYSTEMMODE,
    AT_ACTIVATE_GPS    
};

static struct nrf_modem_gnss_pvt_data_frame last_pvt;
static volatile bool gnss_blocked;

K_MSGQ_DEFINE(gps_msgq, sizeof(gps_msg_type), 10, 4);
K_MSGQ_DEFINE(nmea_queue, sizeof(struct nrf_modem_gnss_nmea_data_frame *), 10, 4);
K_SEM_DEFINE(pvt_data_sem, 0, 1);
K_SEM_DEFINE(lte_ready, 0, 1);

struct k_poll_event events[2] = {
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
                    K_POLL_MODE_NOTIFY_ONLY,
                    &pvt_data_sem, 0),
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
                    K_POLL_MODE_NOTIFY_ONLY,
                    &nmea_queue, 0),
};



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
    struct nrf_modem_gnss_nmea_data_frame *nmea_data;

    switch (event)
    {
    case NRF_MODEM_GNSS_EVT_PVT:
        retval = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
        if (retval == 0) {
            k_sem_give(&pvt_data_sem);
        }
        break;
    
    case NRF_MODEM_GNSS_EVT_NMEA:
        nmea_data = k_malloc(sizeof(struct nrf_modem_gnss_nmea_data_frame));
        if (nmea_data == NULL) {
            printk("Failed to allocate memory for NMEA\n");
            break;
        }

        retval = nrf_modem_gnss_read(nmea_data,
                            sizeof(struct nrf_modem_gnss_nmea_data_frame),
                            NRF_MODEM_GNSS_DATA_NMEA);
        if (retval == 0) {
            retval = k_msgq_put(&nmea_queue, &nmea_data, K_NO_WAIT);
        }

        if (retval != 0) {
            k_free(nmea_data);
        }
        break;
    
    case NRF_MODEM_GNSS_EVT_AGPS_REQ:
        printk("SUPL client library not configured\n");
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

    if (nrf_modem_gnss_nmea_mask_set(NRF_MODEM_GNSS_NMEA_RMC_MASK |
					 NRF_MODEM_GNSS_NMEA_GGA_MASK |
					 NRF_MODEM_GNSS_NMEA_GLL_MASK |
					 NRF_MODEM_GNSS_NMEA_GSA_MASK |
					 NRF_MODEM_GNSS_NMEA_GSV_MASK) != 0) {
		printk("Failed to set GNSS NMEA mask\n");
		return;
	}

    if (nrf_modem_gnss_fix_retry_set(0) != 0) {
        printk("Failed to set GNSS fix retry\n");
        return;
    }

    if (nrf_modem_gnss_fix_interval_set(1) != 0) {
        printk("Failed to set GNSS fix interval\n");
        return;
    }

    if (nrf_modem_gnss_start() != 0) {
        printk("Failed to start GNSS\n");
        return;
    }

    return;
}



void gps_work_handler(struct k_work *work) {
    gps_msg_type gps_msg;
    gps_msg.latitude = 63.123344;
    gps_msg.longitude = 10.092357;
    k_msgq_put(&gps_msgq, &gps_msgq, K_NO_WAIT);

    // printk("Getting GNSS data...\n");

    // (void)k_poll(events, 2, K_FOREVER);

    // if (events[0].state == K_POLL_STATE_SEM_AVAILABLE &&
    //     k_sem_take(events[0].sem, K_NO_WAIT) == 0) {
    //     /* New PVT data available */
    //     /* Extract GPS coordinates and forward to MQTT handler */
    //     gps_msg.latitude = last_pvt.latitude;
    //     gps_msg.longitude = last_pvt.longitude;
    //     k_msgq_put(&gps_msgq, &gps_msgq, K_NO_WAIT);
    // }
}

K_WORK_DEFINE(gps_work, gps_work_handler);

void gps_request_coordinates() {
    k_work_submit(&gps_work);
}
