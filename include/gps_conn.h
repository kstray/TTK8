#ifndef GPS_CONN_H
#define GPS_CONN_H


extern struct k_msgq gps_msgq;

typedef struct {
    double latitude;
    double longitude;
} gps_msg_type;

void gps_request_coordinates();
void gps_init();



#endif /* GPS_CONN_H */