

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
    gpio_button_init();

    /* Init LTE connection */

    int err;
    printk("LTE Link Connecting...\n");
    err = lte_lc_init();
    if (err) {
        printk("Failed to init LTE connection: %d\n", err);
        return;
    }
    printk("LTE init\n");

    err = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);
    if (err) {
        printk("Could not set func mode\n");
        return;
    }

    printk("LTE connected\n");
    
    gps_init();

    k_sleep(K_SECONDS(7));

    /* System ready to start */
    display_print_placeholder();


    mqtt_service_start();
    

    
}


