

#include <zephyr.h>
#include <stdio.h>

#include "gpio_button.h"
#include "gps_location.h"
#include "display_ssd16xx.h"
#include "mqtt_service.h"





void main(void) {

    mqtt_service_init();
    display_init();
    gps_init();
    gpio_button_init();


    //mqtt_service_start();
    

    
}


