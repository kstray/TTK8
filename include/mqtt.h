#ifndef MQTT_H
#define MQTT_H



void mqtt_run();
int publish(uint8_t *data, size_t len);

#endif /* MQTT_H */