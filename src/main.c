

#include <zephyr.h>
#include <stdio.h>
#include <modem/lte_lc.h>

#include "gpio_button.h"
#include "gpio_led.h"
#include "gps_location.h"
#include "display_ssd16xx.h"
#include "mqtt_service.h"





void main(void) {

    /* Initialize modules */
    display_init();
    gpio_led_init();
    mqtt_service_init();
    gps_init();
    gpio_button_init();

    /* Init LTE connection */
    printk("Disabling PSM and eDRX\n");
    lte_lc_psm_req(false);
    lte_lc_edrx_req(false);

    int err;
    printk("LTE Link Connecting...\n");
    err = lte_lc_init_and_connect();
    if (err) {
        printk("Failed to establish LTE connection: %d\n", err);
    }
    printk("LTE Link Connected/n");

    /* System ready to start */
    display_print_placeholder();


    mqtt_service_start();
    

    
}


