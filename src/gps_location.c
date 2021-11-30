/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <nrf_modem_gnss.h>
#include <string.h>

#include "gps_location.h"
#include "mqtt_service.h"
#include "gpio_led.h"


static struct nrf_modem_gnss_pvt_data_frame last_pvt;
static volatile bool gnss_blocked;


void gps_work_handler(struct k_work *work) {

    printk("Getting GNSS data...\n");
    double latitude = last_pvt.latitude;
    double longitude = last_pvt.longitude;

    int err = publish_location(latitude, longitude);
    if (err != 0) {
        printk("Could not publish location\n");
    }

}

K_WORK_DEFINE(gps_work, gps_work_handler);

void gps_request_coordinates() {
    if (nrf_modem_gnss_start() != 0) {
        printk("Failed to start GNSS\n");
        return;
    }

    int err = nrf_modem_gnss_prio_mode_enable();
    if (err != 0) {
        printk("priority mode error\n");
        return;
    }
}

static void print_satellite_stats(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	uint8_t tracked   = 0;
	uint8_t in_fix    = 0;
	uint8_t unhealthy = 0;

	for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
		if (pvt_data->sv[i].sv > 0) {
			tracked++;

			if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) {
				in_fix++;
			}

			if (pvt_data->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY) {
				unhealthy++;
			}
		}
	}

	printk("Tracking: %d Using: %d Unhealthy: %d\n", tracked, in_fix, unhealthy);
}


static void gnss_event_handler(int event) {
    int retval;

    static int count = 0;

    switch (event)
    {
    case NRF_MODEM_GNSS_EVT_PVT:
        count++;
        retval = nrf_modem_gnss_read(&last_pvt, sizeof(last_pvt), NRF_MODEM_GNSS_DATA_PVT);
        if (retval == 0 && (last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID)) {
            k_work_submit(&gps_work);
            gpio_led_on_off(0);
            return;
        }
        print_satellite_stats(&last_pvt);
        gpio_led_on_off(count%2);
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


