#ifndef MQTT_H
#define MQTT_H


int mqtt_service_init();
void mqtt_service_start();
int publish_location(double latitude, double longitude);

#endif /* MQTT_H */