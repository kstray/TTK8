

#include <zephyr.h>
#include <net/mqtt.h>

#include "gpio_button.h"
#include "gps_conn.h"
#include "display_ssd16xx.h"
#include "mqtt.h"


struct k_msgq button_msgq;
struct k_msgq display_msgq;
struct k_msgq mqtt_msgq;


void main(void) {


    gpio_button_init();
    //gps_start();

    //display_init();


    mqtt_run();
    

    
}

