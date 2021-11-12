

#include <zephyr.h>

#include "gpio_button.h"
#include "gps_conn.h"
#include "display_ssd16xx.h"
#include "mqtt.h"



void main(void) {


    gpio_button_init();
    //gps_start();

    display_init();


    //mqtt_run();

    while(1);
    

    
}

