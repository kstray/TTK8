#ifndef MQTT_H
#define MQTT_H

extern struct k_msgq mqtt_msgq;

typedef struct {
    uint8_t data;
} mqtt_msg_type;

void mqtt_run();
int publish(uint8_t *data, size_t len);
void mqtt_request_weather();

#endif /* MQTT_H */