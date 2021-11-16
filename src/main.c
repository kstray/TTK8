

#include <zephyr.h>
#include <stdio.h>

#include "gpio_button.h"
#include "gps_conn.h"
#include "display_ssd16xx.h"
#include "mqtt.h"





void main(void) {


    // mqtt_msg_type mqtt_msg;
    // gps_msg_type gps_msg;
    // char coordinates[10 * sizeof(gps_msg_type)];

    
    //gps_init();
    gpio_button_init();
    display_init();

    // k_msgq_get(&gps_msgq, &gps_msg, K_FOREVER);
    // /* Format GPS coordinates to "(latitude;longitude)" */
    // gps_msg.latitude = 63.09124712;
    // gps_msg.longitude = 15.12419553;
    // sprintf(coordinates, "%f;%f", gps_msg.latitude, gps_msg.longitude);
    // printk("Coordinates: %s\n", coordinates);

    // mqtt_msg.data = coordinates;
    // k_msgq_put(&mqtt_msgq, &mqtt_msg, K_NO_WAIT);

    while(1);
    

    
}


