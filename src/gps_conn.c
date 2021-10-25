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

#define AT_XSYSTEMMODE      "AT\%XSYSTEMMODE=0,0,1,0"
#define AT_ACTIVATE_GPS     "AT+CFUN=31"

#define AT_CMD_SIZE(x) (sizeof(x) - 1)


static const char update_indicator[] = {'\\', '|', '/', '-'};
static const char *const at_commands[] = {
    AT_XSYSTEMMODE,
    AT_ACTIVATE_GPS    
};

static struct nrf_modem_gnss_pvt_data_frame last_pvt;
static volatile bool gnss_blocked;

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

void nrf_modem_recoverable_error_handler(uint32_t error) {
    printk("Modem library recoverable error: %u\n", error);
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

static int init_gps(void) {
    if (setup_modem() != 0) {
        printk("Failed to initialize modem\n");
        return -1;
    }

    /* Initialize and configure GNSS */
    if (nrf_modem_gnss_init() != 0) {
        printk("Failed to initialize GNSS interface\n");
        return -1;
    }

    if (nrf_modem_gnss_event_handler_set(gnss_event_handler) != 0) {
        printk("Failed to set GNSS event handler\n");
        return -1;
    }

    if (nrf_modem_gnss_nmea_mask_set(NRF_MODEM_GNSS_NMEA_RMC_MASK |
					 NRF_MODEM_GNSS_NMEA_GGA_MASK |
					 NRF_MODEM_GNSS_NMEA_GLL_MASK |
					 NRF_MODEM_GNSS_NMEA_GSA_MASK |
					 NRF_MODEM_GNSS_NMEA_GSV_MASK) != 0) {
		printk("Failed to set GNSS NMEA mask\n");
		return -1;
	}

    if (nrf_modem_gnss_fix_retry_set(0) != 0) {
        printk("Failed to set GNSS fix retry\n");
        return -1;
    }

    if (nrf_modem_gnss_fix_interval_set(1) != 0) {
        printk("Failed to set GNSS fix interval\n");
        return -1;
    }

    if (nrf_modem_gnss_start() != 0) {
        printk("Failed to start GNSS\n");
        return -1;
    }

    return 0;
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

static void print_fix_data(struct nrf_modem_gnss_pvt_data_frame *pvt_data)
{
	printk("Latitude:   %.06f\n", pvt_data->latitude);
	printk("Longitude:  %.06f\n", pvt_data->longitude);
	printk("Altitude:   %.01f m\n", pvt_data->altitude);
	printk("Accuracy:   %.01f m\n", pvt_data->accuracy);
	printk("Speed:      %.01f m/s\n", pvt_data->speed);
	printk("Heading:    %.01f deg\n", pvt_data->heading);
	printk("Date:       %02u-%02u-%02u\n", pvt_data->datetime.year,
					       pvt_data->datetime.month,
					       pvt_data->datetime.day);
	printk("Time (UTC): %02u:%02u:%02u\n", pvt_data->datetime.hour,
					       pvt_data->datetime.minute,
					       pvt_data->datetime.seconds);
}


int gps_start(void) {
    uint8_t cnt = 0;
    uint64_t fix_timestamp = 0;
    struct nrf_modem_gnss_nmea_data_frame *nmea_data;

    printk("Initializing GPS\n");

    if (init_gps() != 0) {
        return -1;
    }

    printk("Getting GNSS data...\n");

    while(1) {
        (void)k_poll(events, 2, K_FOREVER);

        if (events[0].state == K_POLL_STATE_SEM_AVAILABLE &&
            k_sem_take(events[0].sem, K_NO_WAIT) == 0) {
            /* New PVT data available */

            if (!IS_ENABLED(CONFIG_GPS_SAMPLE_NMEA_ONLY)) {
                printk("\033[1;1H");
                printk("\033[2J");
                print_satellite_stats(&last_pvt);

                if (gnss_blocked) {
                    printk("GNSS operation blocked by LTE\n");
                }
                printk("---------------------------------\n");

                if (last_pvt.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
                    fix_timestamp = k_uptime_get();
                    print_fix_data(&last_pvt);
                } else {
                    printk("Seconds since last fix: %lld\n",
                            (k_uptime_get() - fix_timestamp) / 1000);
                    cnt++;
                    printk("Searching [%c]\n", update_indicator[cnt%4]);
                }
            }
        }

        if (events[1].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE &&
            k_msgq_get(events[1].msgq, &nmea_data, K_NO_WAIT) == 0) {
            /* New NMEA data available */

            printk("\nNMEA strings:\n\n%s\n", nmea_data->nmea_str);
            k_free(nmea_data);
        }

        events[0].state = K_POLL_STATE_NOT_READY;
        events[1].state = K_POLL_STATE_NOT_READY;
    }

    return 0;
}