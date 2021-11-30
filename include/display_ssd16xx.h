#ifndef DISPLAY_SSD16XX_H
#define DISPLAY_SSD16XX_H



void display_init();
void display_print_placeholder();
void display_print_weather(char *weather, char *icon_id, char *temperature, char *location);


#endif /* DISPLAY_SSD16XX_H */